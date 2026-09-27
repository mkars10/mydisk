// The one place host commands are interpreted. USB serial and Bluetooth both
// hand their text commands here, so the two links always behave the same.
//
//   S        start scanning (nothing is transmitted until this)
//   X        stop scanning
//   P<dBm>   set transmit power, e.g. P20 (5-26)
//   ?        request module info (reply arrives later from the reader)
#pragma once

namespace commands {

struct Result {
  bool ok;
  const char *message;   // short human-readable text for logs; never null
};

Result handle(const char *cmd);

struct Status {
  bool scanning;
  int txPower;   // dBm
};

Status status();

}  // namespace commands
