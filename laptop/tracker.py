#!/usr/bin/env python3
"""
Disc tracker prototype — laptop side.

Reads the CSV stream from the DiscTracker ESP32 firmware and turns it into
a live "hotter/colder" display with Geiger-counter clicks.

The proximity metric blends two signals:
  * RSSI (dBm) — good gradient once the tag is solidly in range
  * read rate (detections/sec) — good gradient at the fringe of range,
    where RSSI is noisy but the read success rate falls off smoothly

Usage:
    pip install pyserial
    python3 tracker.py                    # auto-detect serial port
    python3 tracker.py --port /dev/cu.usbserial-0001
    python3 tracker.py --tag 9999        # only track EPCs containing "9999"
    python3 tracker.py --power 12        # lower TX power = usable gradient indoors
    python3 tracker.py --quiet           # no clicks, display only
    python3 tracker.py --idle            # connect without starting the scan

Keys: space = start/stop, +/- = TX power, q = quit (Ctrl-C also works).
"""

import argparse
import math
import os
import struct
import subprocess
import sys
import tempfile
import threading
import time
import wave
from collections import defaultdict, deque

try:
    import select
    import termios
    import tty
    _KEYS_AVAILABLE = True
except ImportError:      # non-POSIX; tracker still runs, just without hotkeys
    _KEYS_AVAILABLE = False

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("pyserial is required:  pip install pyserial")

# --- tuning -----------------------------------------------------------------
RSSI_FLOOR = -80.0      # dBm treated as "barely detectable"
RSSI_CEIL = -35.0       # dBm treated as "right on top of it"
RATE_CEIL = 20.0        # reads/sec treated as maximum
STALE_SEC = 2.0         # no reads for this long -> tag considered lost
RATE_WINDOW = 1.5       # seconds over which reads/sec is measured
EWMA_ALPHA = 0.3        # RSSI smoothing factor
MIN_CLICK_HZ = 0.5      # click rate when tag barely detectable
MAX_CLICK_HZ = 15.0     # click rate when tag is on top of the antenna
BAR_WIDTH = 40
CLICK_STEPS = 12        # distinct click rates rendered between MIN and MAX
CLICK_TRAIN_SEC = 4.0   # length of each pre-rendered click loop
POWER_MIN = 5           # dBm; RPEUM-26 floor (RPEUM-20 bottoms out at 12.5)
POWER_MAX = 26
POWER_DEFAULT = 20      # full power saturates indoors - start lower


class TagState:
    def __init__(self):
        self.rssi_smooth = None
        self.last_rssi = None
        self.read_times = deque()
        self.last_seen = 0.0
        self.total_reads = 0

    def record(self, rssi, now):
        self.last_rssi = rssi
        self.rssi_smooth = (
            rssi if self.rssi_smooth is None
            else EWMA_ALPHA * rssi + (1 - EWMA_ALPHA) * self.rssi_smooth
        )
        self.read_times.append(now)
        self.last_seen = now
        self.total_reads += 1

    def rate(self, now):
        while self.read_times and now - self.read_times[0] > RATE_WINDOW:
            self.read_times.popleft()
        return len(self.read_times) / RATE_WINDOW

    def proximity(self, now):
        """0.0 = lost, 1.0 = on top of the antenna."""
        if now - self.last_seen > STALE_SEC or self.rssi_smooth is None:
            return 0.0
        rssi_n = (self.rssi_smooth - RSSI_FLOOR) / (RSSI_CEIL - RSSI_FLOOR)
        rssi_n = min(1.0, max(0.0, rssi_n))
        rate_n = min(1.0, self.rate(now) / RATE_CEIL)
        # Fringe of range: rate dominates. In range: average the two.
        return 0.5 * rssi_n + 0.5 * rate_n


# --- audio ------------------------------------------------------------------
class Clicker(threading.Thread):
    """Geiger-style clicks.

    Spawning one player per click piles up processes faster than they finish
    once the rate climbs, so instead this pre-renders a short click *train*
    for each of CLICK_STEPS discrete rates and keeps at most one player
    alive, swapping files when the rate bucket changes.
    """

    def __init__(self):
        super().__init__(daemon=True)
        self.level = 0.0
        self.running = True
        self.enabled = sys.platform == "darwin"
        self._proc = None
        self._bucket = None
        self._trains = {}

    # -- rendering -----------------------------------------------------------
    @staticmethod
    def _click_samples(rate):
        """One tick: sharp attack, fast exponential decay."""
        n = int(rate * 0.012)
        return [
            int(20000 * math.exp(-i / (rate * 0.002))
                * math.sin(2 * math.pi * 3000 * i / rate))
            for i in range(n)
        ]

    def _train_path(self, bucket):
        """Path to a looping wav of clicks at this bucket's rate (cached)."""
        if bucket in self._trains:
            return self._trains[bucket]
        hz = MIN_CLICK_HZ + (bucket / (CLICK_STEPS - 1)) * (MAX_CLICK_HZ - MIN_CLICK_HZ)
        path = os.path.join(tempfile.gettempdir(), f"disctracker_click{bucket:02d}.wav")
        try:
            if not os.path.exists(path):
                rate = 22050
                total = int(rate * CLICK_TRAIN_SEC)
                buf = bytearray(total * 2)
                click = self._click_samples(rate)
                period = max(1, int(rate / hz))
                for start in range(0, total, period):
                    for i, sample in enumerate(click):
                        idx = start + i
                        if idx >= total:
                            break
                        struct.pack_into("<h", buf, idx * 2, sample)
                with wave.open(path, "wb") as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(rate)
                    w.writeframes(bytes(buf))
            self._trains[bucket] = path
            return path
        except Exception:
            return None

    # -- playback ------------------------------------------------------------
    def set_level(self, level):
        self.level = min(1.0, max(0.0, level))

    def _bucket_for(self, level):
        if level <= 0.0:
            return None
        return min(CLICK_STEPS - 1, int(level * CLICK_STEPS))

    def _kill(self):
        if self._proc and self._proc.poll() is None:
            try:
                self._proc.terminate()
            except Exception:
                pass
        self._proc = None

    def _play(self, bucket):
        path = self._train_path(bucket)
        if not path:
            return
        try:
            self._proc = subprocess.Popen(
                ["afplay", path],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
        except Exception:
            self._proc = None

    def run(self):
        if not self.enabled:
            # Fallback: terminal bell, one at a time
            while self.running:
                if self.level > 0.0:
                    sys.stdout.write("\a")
                    sys.stdout.flush()
                    hz = MIN_CLICK_HZ + self.level * (MAX_CLICK_HZ - MIN_CLICK_HZ)
                    time.sleep(1.0 / hz)
                else:
                    time.sleep(0.1)
            return

        while self.running:
            bucket = self._bucket_for(self.level)
            if bucket != self._bucket:
                self._kill()
                self._bucket = bucket
                if bucket is not None:
                    self._play(bucket)
            elif bucket is not None and (self._proc is None or self._proc.poll() is not None):
                self._play(bucket)          # loop finished; restart it
            time.sleep(0.05)
        self._kill()

    def shutdown(self):
        """Stop immediately - no clicks trailing after the user quits."""
        self.running = False
        self._kill()


# --- keyboard ---------------------------------------------------------------
class KeyReader:
    """Non-blocking single-keypress reader. No-op if stdin isn't a terminal."""

    def __init__(self):
        self.enabled = _KEYS_AVAILABLE and sys.stdin.isatty()
        self._saved = None

    def start(self):
        if self.enabled:
            self._saved = termios.tcgetattr(sys.stdin)
            # cbreak (not raw) keeps Ctrl-C working as a normal interrupt
            tty.setcbreak(sys.stdin.fileno())

    def stop(self):
        if self.enabled and self._saved is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self._saved)
            self._saved = None

    def get(self):
        if not self.enabled:
            return None
        if select.select([sys.stdin], [], [], 0)[0]:
            return sys.stdin.read(1)
        return None


# --- serial -----------------------------------------------------------------
def find_port():
    candidates = []
    for p in serial.tools.list_ports.comports():
        dev = p.device
        if any(s in dev for s in ("usbserial", "SLAB", "wchusbserial", "USB", "ACM")):
            candidates.append(dev)
    if not candidates:
        sys.exit(
            "No serial port found. Is the ESP32 plugged in?\n"
            "Specify one with --port (see: python3 -m serial.tools.list_ports)"
        )
    if len(candidates) > 1:
        print("Multiple ports found, using the first:")
        for c in candidates:
            print("  ", c)
    return candidates[0]


# --- display ----------------------------------------------------------------
def bar(fraction, width=BAR_WIDTH):
    n = int(round(fraction * width))
    return "█" * n + "░" * (width - n)


def render(tags, target_filter, now, lines_drawn, scanning, hotkeys, power):
    # Move cursor up to overwrite the previous frame
    if lines_drawn:
        sys.stdout.write(f"\x1b[{lines_drawn}A")
    out = []
    state = "● SCANNING" if scanning else "○ IDLE (no RF)"
    hint = "   [space] start/stop  [+/-] power  [q] quit" if hotkeys else ""
    out.append(f"{state}  {power:>2} dBm{hint}".ljust(110))
    shown = sorted(tags.items(), key=lambda kv: -kv[1].proximity(now))
    if not shown:
        out.append("Searching... no tags seen yet".ljust(90))
    for epc, st in shown:
        prox = st.proximity(now)
        lost = now - st.last_seen > STALE_SEC
        rssi = f"{st.rssi_smooth:6.1f} dBm" if st.rssi_smooth is not None else "   --  "
        rate = f"{st.rate(now):5.1f}/s"
        label = epc if len(epc) <= 24 else epc[:24]
        status = " LOST " if lost else f"{int(prox * 100):3d}%  "
        marker = ">" if (target_filter and target_filter in epc) else " "
        out.append(
            f"{marker}{label:<24} [{bar(prox)}] {status} {rssi}  {rate}  "
            f"({st.total_reads} reads)".ljust(110)
        )
    for line in out:
        sys.stdout.write(line + "\n")
    sys.stdout.flush()
    return len(out)


# --- main -------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="R200 disc tracker prototype")
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--tag", default=None,
                    help="only sound clicks for EPCs containing this hex substring")
    ap.add_argument("--power", type=int, default=None,
                    help=f"reader TX power in dBm ({POWER_MIN}-{POWER_MAX}), "
                         f"default {POWER_DEFAULT}; adjust live with +/-")
    ap.add_argument("--quiet", action="store_true", help="no clicks")
    ap.add_argument("--idle", action="store_true",
                    help="start with scanning off (press space to begin)")
    args = ap.parse_args()

    port = args.port or find_port()
    print(f"Opening {port} @ {args.baud}...")
    ser = serial.Serial(port, args.baud, timeout=0.1)
    time.sleep(1.5)  # opening the port resets the ESP32; let it boot

    power = args.power if args.power is not None else POWER_DEFAULT
    power = min(POWER_MAX, max(POWER_MIN, power))
    ser.write(f"P{power}\n".encode())
    time.sleep(0.1)

    scanning = not args.idle
    if scanning:
        ser.write(b"S\n")

    clicker = None
    if not args.quiet:
        clicker = Clicker()
        clicker.start()

    tags = defaultdict(TagState)
    target = args.tag.upper() if args.tag else None
    lines_drawn = 0
    last_render = 0.0

    keys = KeyReader()
    keys.start()
    if keys.enabled:
        print("Space = start/stop, +/- = power, q = quit.\n")
    else:
        print("Tracking. Ctrl-C to quit.\n")
    try:
        while True:
            key = keys.get()
            if key:
                if key == " ":
                    scanning = not scanning
                    ser.write(b"S\n" if scanning else b"X\n")
                    if not scanning:
                        tags.clear()   # reset the display; stale bars are noise
                elif key in ("+", "="):
                    power = min(POWER_MAX, power + 1)
                    ser.write(f"P{power}\n".encode())
                elif key in ("-", "_"):
                    power = max(POWER_MIN, power - 1)
                    ser.write(f"P{power}\n".encode())
                elif key in ("q", "Q"):
                    break

            line = ser.readline().decode(errors="replace").strip()
            now = time.monotonic()
            if line:
                parts = line.split(",")
                if parts[0] == "TAG" and len(parts) >= 3:
                    epc = parts[1].upper()
                    try:
                        rssi = float(parts[2])
                    except ValueError:
                        continue
                    tags[epc].record(rssi, now)
                elif parts[0] in ("INFO", "ERR"):
                    sys.stdout.write((" ".join(parts) + " " * 40)[:100] + "\n")
                    lines_drawn = 0  # log lines scroll; redraw fresh below them

            if clicker and not scanning:
                clicker.set_level(0.0)
            elif clicker:
                if target:
                    matches = [s for e, s in tags.items() if target in e]
                    level = max((s.proximity(now) for s in matches), default=0.0)
                else:
                    level = max((s.proximity(now) for s in tags.values()), default=0.0)
                clicker.set_level(level)

            if now - last_render > 0.15:
                lines_drawn = render(tags, target, now, lines_drawn,
                                     scanning, keys.enabled, power)
                last_render = now
    except KeyboardInterrupt:
        pass
    finally:
        keys.stop()
        if clicker:
            clicker.shutdown()
        try:
            ser.write(b"X\n")
        except Exception:
            pass
        ser.close()
        print("\nBye.")


if __name__ == "__main__":
    main()
