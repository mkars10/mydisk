/**
 * DiscTracker firmware — ESP32 + R200 UHF RFID reader
 *
 * Boots IDLE - the reader transmits nothing until told to start, so
 * powering up with no antenna attached is safe.
 *
 * Two links, both driven by the same commands (see commands.h):
 *   - Bluetooth LE for the phone app ("MyDisk"). Protocol: docs/ble-interface.md
 *   - USB serial for the laptop (laptop/tracker.py) and the serial monitor
 *
 * Modules, each with one job:
 *   r200         reader driver: frames in and out, tag reads out
 *   tracker      hotter/colder maths: smoothed RSSI, read rate, 0-100 proximity
 *   commands     S / X / P<dBm> / ?, shared by both links
 *   serial_link  USB text protocol
 *   ble_link     Bluetooth protocol
 * This file only wires them together.
 *
 * Wiring (same as the original repo, see docs/R200_ESP32_wiring.jpg):
 *   R200 5V/VIN <- ESP32 VIN (5V from USB; the 3V3 pin can't supply enough current)
 *   R200 GND    <- ESP32 GND
 *   R200 TXD    -> ESP32 GPIO16 (RX2)
 *   R200 RXD    <- ESP32 GPIO17 (TX2)
 *   Antenna MUST be attached before starting a scan.
 */

#include "r200.h"
#include "tracker.h"
#include "commands.h"
#include "serial_link.h"
#include "ble_link.h"

static const char *DEVICE_NAME = "MyDisk";
static const int R200_RX_PIN = 16;   // ESP32 RX2  <- R200 TXD
static const int R200_TX_PIN = 17;   // ESP32 TX2  -> R200 RXD

static void onTagRead(const uint8_t *epc, uint8_t epcLen, int8_t rssi) {
  uint32_t now = millis();
  tracker::record(epc, epcLen, rssi, now);
  serial_link::printTag(epc, epcLen, rssi, now);
}

static void onPhoneDisconnected() {
  // Don't leave the transmitter running after the user walks away.
  if (commands::status().scanning) commands::handle("X");
  serial_link::printInfo("phone disconnected");
}

void setup() {
  serial_link::begin();
  serial_link::printInfo("DiscTracker boot - idle, no RF until 'S'");

  r200::Listener listener = { onTagRead, serial_link::printInfo, serial_link::printErrorCode };
  r200::begin(Serial2, R200_RX_PIN, R200_TX_PIN, listener);

  ble_link::begin(DEVICE_NAME, onPhoneDisconnected);
  serial_link::printInfo("bluetooth advertising as MyDisk");
}

void loop() {
  r200::poll();
  serial_link::poll();
  ble_link::poll();
}
