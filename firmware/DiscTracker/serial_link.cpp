#include "serial_link.h"
#include "commands.h"
#include <Arduino.h>

namespace serial_link {

static char cmdBuf[16];
static uint8_t cmdLen = 0;

void begin() {
  Serial.begin(115200);
}

void poll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (!cmdLen) continue;
      cmdBuf[cmdLen] = '\0';
      cmdLen = 0;
      commands::Result r = commands::handle(cmdBuf);
      if (r.ok) printInfo(r.message); else printError(r.message);
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

void printTag(const uint8_t *epc, uint8_t epcLen, int8_t rssi, uint32_t ms) {
  Serial.print("TAG,");
  for (uint8_t i = 0; i < epcLen; i++) {
    if (epc[i] < 0x10) Serial.print('0');
    Serial.print(epc[i], HEX);
  }
  Serial.print(',');
  Serial.print(rssi);
  Serial.print(',');
  Serial.println(ms);
}

void printInfo(const char *text) {
  Serial.print("INFO,");
  Serial.println(text);
}

void printError(const char *text) {
  Serial.print("ERR,");
  Serial.println(text);
}

void printErrorCode(uint8_t code) {
  Serial.print("ERR,");
  Serial.println(code, HEX);
}

}  // namespace serial_link
