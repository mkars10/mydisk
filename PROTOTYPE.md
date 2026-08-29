# Disc Tracker Prototype

Goal: find a disc (with a passive UHF RFID sticker on it) using the R200 reader,
an ESP32, and your laptop. No app — just a terminal display and Geiger-counter
clicks that speed up as you get closer.

## 1. Wiring

Same as the original repo (photo: [docs/R200_ESP32_wiring.jpg](docs/R200_ESP32_wiring.jpg)).
The R200's serial header (bottom edge, labelled `3V3 RXD TXD GND`) talks 3.3V
logic, which is what the ESP32 speaks natively — no level shifter needed.

| R200 pin            | ESP32 pin        | Note                                        |
|---------------------|------------------|---------------------------------------------|
| 5V (top header)     | VIN (5V)         | Power from 5V — the 3V3 pin can't source enough current for the RF amp |
| GND                 | GND              |                                             |
| TXD                 | GPIO16 (RX2)     | Reader → ESP32                              |
| RXD                 | GPIO17 (TX2)     | ESP32 → reader                              |

**Attach the antenna to the SMA connector before powering the board.** Running
a UHF transmitter with no antenna can damage the power amplifier.

Only the ESP32's USB cable goes to the laptop; the R200's micro-USB port stays
unused (it's an alternate CH340 serial interface — handy for the Windows demo
software, not needed here).

## 2. Flash the firmware

Open [firmware/DiscTracker/DiscTracker.ino](firmware/DiscTracker/DiscTracker.ino)
in the Arduino IDE, select your ESP32 board (e.g. "ESP32 Dev Module"), and upload.

This sketch is self-contained (it doesn't use the R200 library in `R200/`).
It puts the reader into continuous multi-poll mode (`AA 00 27 00 03 22 FF FF 4A DD`,
the same command as the vendor's `ReadMulti`) and prints one line per detection:

```
TAG,E28068900000500E88C6A4A7,-57,123456
     └─ EPC                   └─ RSSI dBm  └─ millis
```

**The firmware boots idle** — it transmits nothing until told to, so plugging
in the USB with no antenna attached is safe. It accepts these commands (from
the serial monitor or the tracker): `S` start, `X` stop, `P20` set TX power to
20 dBm, `?` module info. Once started it re-arms polling automatically if the
reader goes quiet.

Sanity check: open the Arduino serial monitor at 115200 baud, send `S`, then
hold a tag near the antenna — you should see a stream of `TAG,...` lines (and hear the
board's beeper, unless you've desoldered R9 — see README). **Close the serial
monitor before running the tracker** — only one program can hold the port.

## 3. Run the tracker on your laptop

The Python environment is already set up in `.venv/` (git-ignored). Run it with:

```bash
.venv/bin/python laptop/tracker.py
```

That's the whole invocation — no `activate` needed, since calling the venv's
Python directly picks up its packages. If you'd rather activate the venv:

```bash
source .venv/bin/activate
```

To rebuild the environment from scratch on another machine:

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

You get a live bar per tag seen, plus clicks that speed up as proximity rises:

```
>E28068900000500E88C6A4A7 [████████████░░░...] 63%   -52.3 dBm  14.2/s  (312 reads)
```

Useful flags:

- `--tag 88C6` — only click for EPCs containing that hex substring (so the
  other discs in your bag don't set it off)
- `--power 12` — lower TX power; **this is the main knob**. At full power
  every tag in the room pins at 100% and you get no gradient at all. Defaults
  to 20 dBm; drop to 5-12 for indoor testing, raise for field range.
- `--quiet` — no clicks
- `--idle` — connect without starting the scan
- `--port /dev/cu.usbserial-0001` — if auto-detect picks the wrong port

While it's running: **space** toggles scanning, **+/-** adjust TX power live
(5-26 dBm), **q** quits. Sweeping power with `+`/`-` until the tag *just*
stops reading is the fastest way to gauge distance — see power ramping below.

Auto-detect looks for a port whose name contains `usbserial`, `wchusbserial`,
`SLAB`, `USB`, or `ACM`, which covers the usual CP2102 / CH340 chips on ESP32
dev boards. To see what's actually connected:

```bash
.venv/bin/python -m serial.tools.list_ports -v
```

## 4. Field mode — phone over WiFi

The same firmware also hosts its own WiFi access point, so you can walk the
park with a battery pack and your phone. No app, no internet, no cell signal.

1. Power the ESP32 from any 5V USB battery pack.
2. On your phone, join the WiFi network **`DiscTracker`**, password **`discgolf`**.
3. Open **http://192.168.4.1** (or **http://disc.local**).

The page updates in place — nothing scrolls, nothing accumulates:

- **Focus card** at the top: one tag, big proximity number, bar, RSSI,
  reads/sec, total reads. By default it follows the strongest tag; tap any tag
  in the list to lock onto it, tap again to unlock. A locked tag that goes out
  of range shows `—` and turns red rather than silently reading stale numbers.
- **START / STOP** — the same idle-safe control as the terminal.
- **Signal strength slider**, 5–26 dBm, live.
- **CLICKS** toggle — Geiger clicks from the phone's speaker via Web Audio.
  Browsers block audio until the user taps something, which is why it's a
  button rather than automatic.
- **Tag list**, sorted strongest-first, with a mini bar and RSSI each.

The tracking maths (RSSI smoothing, read rate, proximity) now runs on the
ESP32, so the phone is a thin display and the laptop is optional. Serial
output is unchanged — `tracker.py` still works over USB exactly as before.

### Battery notes

Running the WiFi AP adds roughly 100–150 mA on top of the reader, so budget
around 0.5 A peak at high transmit power. Any modest power bank will run this
for hours. Watch for banks that auto-shut-off under light load — the reader
usually draws enough to keep them awake, but it's the most likely field
annoyance.

## 5. How the "hotter/colder" metric works

Two signals are blended, because each is only good in part of the range:

- **Read rate** (detections/sec): at the fringe of range the tag only answers
  occasionally, and the success rate climbs smoothly as you approach. This is
  the dominant useful signal at distance — literally the Geiger-counter model.
- **RSSI** (dBm, reported per read): noisy and mostly saturated up close, but
  gives a usable gradient in the mid-range. It's smoothed with an EWMA.

Proximity = average of the two, each normalised to 0–1. Tuning constants are
at the top of `tracker.py` (`RSSI_FLOOR`, `RATE_CEIL`, etc.) — expect to adjust
them once you see real numbers with your 4dbi antenna outdoors.

### Next idea worth trying: power ramping

A known trick for ranging with these modules: find the *lowest power at which
the tag still reads*. That threshold correlates with distance much better than
RSSI does. You can now do this by hand with the `+`/`-` keys; automating the
sweep and reporting an estimated range band is the natural next step.

### Field-use notes

- The antenna is directional-ish: sweeping it side to side and watching for
  the rate peak gives you a bearing, not just a distance.
- Water absorbs UHF badly — a disc in wet grass will read at a much shorter
  range than one on your desk. Test outdoors early.
- Tags directly on plastic fly fine; if reads are weak, tag placement near the
  rim vs. center of the disc can change coupling noticeably.
