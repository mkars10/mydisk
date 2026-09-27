#!/bin/sh
# Builds the firmware modules on your computer (no ESP32 needed) against small
# stand-ins for the Arduino, UART and Bluetooth libraries, then plays a fake
# phone and a fake reader through them. Prints ALL PASS when everything checks.
#
#   sh firmware/host_test/run.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FW="$HERE/../DiscTracker"
OUT="$(mktemp -d)"
cp "$FW/DiscTracker.ino" "$OUT/sketch.cpp"
c++ -std=gnu++17 -I"$HERE/stubs" -I"$FW" \
  "$HERE/ble_link_test.cpp" "$OUT/sketch.cpp" \
  "$FW/r200.cpp" "$FW/tracker.cpp" "$FW/commands.cpp" "$FW/serial_link.cpp" "$FW/ble_link.cpp" \
  -o "$OUT/test"
"$OUT/test"
