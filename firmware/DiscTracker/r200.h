// R200 reader driver: builds command frames, parses the reader's replies and
// reports tag reads through callbacks. Knows nothing about tracking maths,
// USB or Bluetooth.
#pragma once
#include <Arduino.h>

namespace r200 {

struct Listener {
  void (*onTag)(const uint8_t *epc, uint8_t epcLen, int8_t rssi);
  void (*onInfo)(const char *text);   // module info, acknowledgements
  void (*onError)(uint8_t code);      // reader error codes (0x15 "no tag" is filtered out)
};

// Boots idle: nothing is transmitted until startScan().
void begin(HardwareSerial &port, int rxPin, int txPin, const Listener &listener);

// Call every loop(): drains the UART and re-arms polling if the reader went quiet.
void poll();

void startScan();
void stopScan();
bool isScanning();

// dBm. RPEUM-20 accepts 12.5-20, RPEUM-26 accepts 5-26.
void setTxPower(int dBm);
int txPower();

void requestModuleInfo();

}  // namespace r200
