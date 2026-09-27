// USB serial link for the laptop (laptop/tracker.py) and the Arduino serial
// monitor. Output lines:
//
//   TAG,<epc-hex>,<rssi-dBm>,<millis>   one per tag read
//   INFO,<text>                         status messages
//   ERR,<text or hex code>              errors
#pragma once
#include <stdint.h>

namespace serial_link {

void begin();
void poll();   // reads newline-terminated commands and passes them to commands::handle

void printTag(const uint8_t *epc, uint8_t epcLen, int8_t rssi, uint32_t ms);
void printInfo(const char *text);
void printError(const char *text);
void printErrorCode(uint8_t code);

}  // namespace serial_link
