// Tag table types and tuning constants.
// These live in a header because the Arduino build injects function
// prototypes above the sketch body; a struct defined inside the .ino
// would not yet be visible to them.
#pragma once
#include <stdint.h>

#define MAX_TAGS 16
#define MAX_EPC  16

static const float RSSI_FLOOR = -80.0f;   // "barely detectable"
static const float RSSI_CEIL  = -35.0f;   // "on top of it"
static const float RATE_CEIL  = 20.0f;    // reads/sec treated as maximum
static const uint32_t STALE_MS = 2000;    // no reads for this long -> lost
static const float EWMA_ALPHA = 0.3f;

struct TagRec {
  bool     used;
  uint8_t  epc[MAX_EPC];
  uint8_t  epcLen;
  float    rssiSmooth;
  int8_t   lastRssi;
  uint32_t reads;
  uint32_t lastSeen;
  uint32_t winStart;   // sliding window for reads/sec
  uint16_t winCount;
};
TagRec tagTable[MAX_TAGS];

