# Disc Tracker Prototype

Goal: find a disc (with a passive UHF RFID sticker on it) using the R200 reader
and an ESP32. No app required — a terminal display on a laptop over USB, or a
phone page served by the ESP32's own WiFi for field use. Both give Geiger-counter
clicks that speed up as you get closer.

## 1. Hardware

Everything in the build, so it can be rebuilt or debugged without guessing.

### Compute

**ESP32-WROOM-32E** on a DevKitC-style board. The 32E is the current revision
of the classic WROOM-32 and is pin-compatible with it, so the wiring photo in
`docs/` still applies.

It matters that this is a **WROOM and not a WROVER**: WROVER modules wire their
onboard PSRAM to GPIO16 and GPIO17, the exact pins we use for the reader's
UART. The WROOM-32E has no PSRAM, so those pins are free. If you ever swap to a
WROVER, move the UART — the ESP32 routes UARTs through a GPIO matrix, so any
two free pins work; change `R200_RX_PIN` / `R200_TX_PIN` in the sketch.

USB-serial is a CH340 or CP2102 depending on the board. macOS 11.3+ has a
built-in driver for both (`AppleUSBSerial.dext`) — the `CH341SER.EXE` in the
vendor folder is Windows-only and not needed.

### Reader

**YPD-R200** UHF RFID module (Shenzhen YanPoDo) on its carrier board.

| Spec | Value |
|---|---|
| Protocol | ISO18000-6C / EPC Class-1 Gen-2 |
| Frequency | 840–960 MHz |
| Supply | **3.6–5.5 V** — note the minimum; 3.3 V is below spec |
| Logic | UART TTL **3.3 V**, 115200 8N1 |
| Sensitivity | −69 dBm (1% packet error) |
| Typical read distance | ~1 m for the bare module; more with a larger antenna |
| Module size | 53 × 33 mm excluding SMA |

That 3.6 V minimum is the real reason the board can't run off the ESP32's 3V3
pin — it's undervoltage, not just a current limit.

**Two power variants exist**, and they behave differently:

| Variant | TX power range | Peak current @ 5 V |
|---|---|---|
| RPEUM-20 | 12.5–20 dBm, 1 dBm steps | ~180 mA |
| RPEUM-26 | 5–26 dBm, 1 dBm steps | ~380 mA |

To find out which you have: set power to 26 dBm from the UI or `P26` on serial.
If it's rejected or clamped, you have the 20 dBm part.

The carrier board also carries an SMA antenna connector, a CH340E with its own
micro-USB port (an alternate serial route — you can plug the reader straight
into a computer and skip the ESP32 entirely), and a beeper wired to 5 V through
a 0 Ω resistor at **R9**; desolder R9 to silence it.

Source: https://www.aliexpress.com/item/4000281733851.html (per the original
repo's README).

### Antenna

**TODO — record the exact part.** The original repo's author used a
60 × 70 × 7 mm 4 dBi panel antenna claiming 0–3 m; ours has not been recorded
here yet. Gain and physical size drive read range more than anything else in
this build, so it's worth writing down.

Always screw the antenna on before starting a scan — transmitting into an open
SMA reflects power back into the module's amplifier.

### Tags

Passive UHF (EPC Gen-2) adhesive labels — no battery, thin and light enough not
to disturb disc flight, which is the whole premise of the project.

**TODO — record the exact tag model and size.**

Note that the protocol has `CMD_KillTag` (0x65) and `CMD_LockLabel` (0x82),
which permanently and irreversibly brick a tag. This firmware never sends
either; keep it that way if you add EPC writing.

### Power

5 V USB battery pack. Estimated draw (datasheet figures plus typical ESP32
numbers — not measured):

| State | Average @ 5 V | Peak |
|---|---|---|
| Idle, AP up, not scanning | ~160 mA | ~280 mA |
| Scanning @ 12 dBm | ~250 mA | ~400 mA |
| Scanning @ 20 dBm (default) | ~330 mA | ~500 mA |
| Scanning @ 26 dBm | ~525 mA | ~700 mA |

A 10,000 mAh bank delivers roughly 6,600 mAh at 5 V after losses, so expect
12–20 hours depending on transmit power. Battery is not the limiting factor.

**TODO — record the battery pack model/capacity.**

### Toolchain

| Piece | Version |
|---|---|
| Dev machine | macOS 26.4 |
| Arduino core | `esp32` **3.3.11** by Espressif Systems |
| Board selection | ESP32 Dev Module (`esp32:esp32:esp32`) |
| Python | 3.14.6 (Homebrew), venv in `.venv/` |
| pyserial | 3.5 |

Use the Espressif `esp32` core, **not** the Arduino-branded `arduino/esp32`
package — that one is for the Nano ESP32 and won't offer the right board.

Current build size: 73% of program storage, 16% of dynamic memory.

### Still to record

- [ ] Antenna make, model, gain, dimensions
- [ ] Tag make, model, dimensions
- [ ] Which R200 variant (RPEUM-20 or RPEUM-26)
- [ ] Battery pack model and capacity
- [ ] Measured current draw (a $10 inline USB power meter settles it)
- [ ] Measured RSSI at edge of range and at the antenna, for tuning
      `RSSI_FLOOR` / `RSSI_CEIL` in `firmware/DiscTracker/tagtable.h`

## 2. Wiring

Same as the original repo (photo: [docs/R200_ESP32_wiring.jpg](docs/R200_ESP32_wiring.jpg)).
The R200's serial header (bottom edge, labelled `3V3 RXD TXD GND`) talks 3.3V
logic, which is what the ESP32 speaks natively — no level shifter needed.

| R200 pin            | ESP32 pin        | Note                                        |
|---------------------|------------------|---------------------------------------------|
| 5V (top header)     | VIN (5V)         | Module needs 3.6–5.5 V — 3.3 V is below its minimum |
| GND                 | GND              |                                             |
| TXD                 | GPIO16 (RX2)     | Reader → ESP32                              |
| RXD                 | GPIO17 (TX2)     | ESP32 → reader                              |

**Attach the antenna to the SMA connector before powering the board.** Running
a UHF transmitter with no antenna can damage the power amplifier.

Only the ESP32's USB cable goes to the laptop; the R200's micro-USB port stays
unused (it's an alternate CH340 serial interface — handy for the Windows demo
software, not needed here).

## 3. Flash the firmware

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

## 4. Run the tracker on your laptop

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

## 5. Field mode — phone over WiFi

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

## 6. How the "hotter/colder" metric works

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
