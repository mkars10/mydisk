#include "commands.h"
#include "r200.h"
#include <stdio.h>
#include <stdlib.h>

namespace commands {

static const int MIN_POWER_DBM = 5;
static const int MAX_POWER_DBM = 26;

Result handle(const char *cmd) {
  switch (cmd[0]) {
    case 'S': case 's':
      r200::startScan();
      return { true, "polling started" };
    case 'X': case 'x':
      r200::stopScan();
      return { true, "polling stopped" };
    case 'P': case 'p': {
      int dBm = atoi(cmd + 1);
      if (dBm < MIN_POWER_DBM || dBm > MAX_POWER_DBM) return { false, "power out of range (5-26 dBm)" };
      r200::setTxPower(dBm);
      static char msg[32];
      snprintf(msg, sizeof(msg), "tx power set to %d dBm", dBm);
      return { true, msg };
    }
    case '?':
      r200::requestModuleInfo();
      return { true, "module info requested" };
    default:
      return { false, "unknown command" };
  }
}

Status status() {
  return { r200::isScanning(), r200::txPower() };
}

}  // namespace commands
