// Tag tracker: the "hotter/colder" maths. Turns individual tag reads into a
// smoothed RSSI, a read rate and a 0-100 proximity per tag. Knows nothing
// about the reader hardware, USB or Bluetooth.
#pragma once
#include <stdint.h>

namespace tracker {

static const int MAX_TAGS = 16;
static const int MAX_EPC  = 16;

// Tuning. Expect to adjust once real outdoor numbers are in.
static const float    RSSI_FLOOR = -80.0f;   // "barely detectable"
static const float    RSSI_CEIL  = -35.0f;   // "on top of it"
static const float    RATE_CEIL  = 20.0f;    // reads/sec treated as maximum
static const uint32_t STALE_MS   = 2000;     // no reads for this long -> proximity 0
static const float    EWMA_ALPHA = 0.3f;

struct TagState {
  uint8_t epc[MAX_EPC];
  uint8_t epcLen;
  int8_t  rssi;          // smoothed, dBm
  uint8_t proximity;     // 0-100; 0 once the tag has gone stale
  uint8_t readsPerSec;   // rounded, capped at 255
  uint32_t msSinceSeen;
};

void record(const uint8_t *epc, uint8_t len, int8_t rssi, uint32_t nowMs);
void clear();

// Fills `out` with every tag seen within the last `withinMs`, in no
// particular order. Returns how many were written.
int snapshot(TagState *out, int max, uint32_t nowMs, uint32_t withinMs);

}  // namespace tracker
