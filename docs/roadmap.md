# Roadmap notes

Items parked for later. The main roadmap lives outside this repo; move these
over when convenient.

## Per-read beeping on the phone (like the R200's built-in beeper)

**Status:** parked. Decide after the first iPhone test (2026-09-26).

**Why.** The R200 board's own beeper (wired through R9, see README) sounds on
every tag read. That's a true Geiger counter: zero lag, and the beep rate *is*
the read rate, so it speeds up as you get closer. In testing it's the most
useful feedback. Once the reader is inside a housing (or R9 is removed) that
beeper is gone, so the phone should reproduce it.

The phone can't today: over Bluetooth it only gets the finder's processed
result, one update per tag every 200 ms (proximity 0-100, smoothed RSSI,
reads/sec). Individual reads never reach the phone. Beeping from the processed
values may or may not feel real-time enough; unknown until the iPhone test.

**When.** If beeping from the processed values feels laggy, or when we start
tuning the percentage calculation (raw reads on the phone make that tuning
possible without reflashing the ESP32).

**What it would take (bigger change, touches every layer):**

- Firmware (`ble_link.cpp`): send each raw read (EPC, RSSI, time) over
  Bluetooth, e.g. a new notify characteristic. Probably needs a filter on the
  finder so only the disc being searched for is sent: raw reads can reach 20+
  per second per tag, and a bag of discs multiplies that.
- Protocol (`docs/ble-interface.md`): version 2; the new characteristic and a
  command to pick the EPC to stream.
- MyDiskKit: decode raw reads and expose them (e.g. an `onRead` callback that
  fires per read, for one beep each). Optionally move or copy the proximity
  maths into Swift so it can be tuned in the app. The mock needs raw reads too.
- App: one beep per read, matching the board.

**Open questions:**

- Is beeping from the 5 Hz processed values good enough? Answer with the
  iPhone test first.
- Bluetooth adds some delay per read. Is it small enough that per-read beeps
  still feel instant?
- Where should the maths live long term: ESP32, phone, or both?

## Tune the proximity scaling from the app, without reflashing

**Status:** parked for later (2026-09-26).

**Why.** The numbers that turn signal strength and read rate into the 0-100
proximity are guesses, fixed in the firmware (`firmware/DiscTracker/tracker.h`):
`RSSI_FLOOR` (-80 dBm), `RSSI_CEIL` (-35 dBm), `RATE_CEIL` (20 reads/s),
`STALE_MS` (2 s), `EWMA_ALPHA` (0.3), plus the 50/50 blend in `tracker.cpp`.
Tuning them against real field numbers means a reflash for every change.

**Ways to do it (pick when we get there):**

- Keep the maths on the ESP32 and add a Bluetooth command or characteristic
  to read and set the parameters, saved in the ESP32's flash so they survive a
  reboot. Smaller change; the protocol gets a new message (version 2).
- Move the maths to the phone. Needs raw reads over Bluetooth, so it overlaps
  with the per-read beeping item above.

**Touches:** firmware (`tracker`, `ble_link`), `docs/ble-interface.md`,
MyDiskKit (a settings API), and a test control on the Finder Stats page.
