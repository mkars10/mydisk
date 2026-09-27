#include "tracker.h"
#include <string.h>

namespace tracker {

struct TagRec {
  bool     used;
  uint8_t  epc[MAX_EPC];
  uint8_t  epcLen;
  float    rssiSmooth;
  uint32_t reads;
  uint32_t lastSeen;
  uint32_t winStart;   // sliding window for reads/sec
  uint16_t winCount;
};

static TagRec table[MAX_TAGS];

static float rate(const TagRec &t, uint32_t now) {
  uint32_t span = now - t.winStart;
  if (span < 100) span = 100;
  return (t.winCount * 1000.0f) / span;
}

static float proximity(const TagRec &t, uint32_t now) {
  if (now - t.lastSeen > STALE_MS) return 0.0f;
  float rn = (t.rssiSmooth - RSSI_FLOOR) / (RSSI_CEIL - RSSI_FLOOR);
  rn = rn < 0 ? 0 : (rn > 1 ? 1 : rn);
  float hn = rate(t, now) / RATE_CEIL;
  if (hn > 1.0f) hn = 1.0f;
  // Rate carries the fringe of range; RSSI carries the mid range.
  return 0.5f * rn + 0.5f * hn;
}

void record(const uint8_t *epc, uint8_t len, int8_t rssi, uint32_t now) {
  if (len > MAX_EPC) len = MAX_EPC;

  int slot = -1, freeSlot = -1, oldest = -1;
  uint32_t oldestSeen = 0xFFFFFFFF;
  for (int i = 0; i < MAX_TAGS; i++) {
    if (!table[i].used) { if (freeSlot < 0) freeSlot = i; continue; }
    if (table[i].epcLen == len && memcmp(table[i].epc, epc, len) == 0) { slot = i; break; }
    if (table[i].lastSeen < oldestSeen) { oldestSeen = table[i].lastSeen; oldest = i; }
  }
  if (slot < 0) slot = (freeSlot >= 0) ? freeSlot : oldest;
  if (slot < 0) return;

  TagRec &t = table[slot];
  if (!t.used || t.epcLen != len || memcmp(t.epc, epc, len) != 0) {
    t = TagRec();                       // reused slot: start clean
    t.used = true;
    memcpy(t.epc, epc, len);
    t.epcLen = len;
    t.rssiSmooth = rssi;
    t.winStart = now;
  }
  t.rssiSmooth = EWMA_ALPHA * rssi + (1.0f - EWMA_ALPHA) * t.rssiSmooth;
  t.reads++;
  t.lastSeen = now;
  // Decay the window rather than resetting, so the rate doesn't sawtooth
  if (now - t.winStart > 1500) { t.winStart = now - 750; t.winCount /= 2; }
  t.winCount++;
}

void clear() {
  for (int i = 0; i < MAX_TAGS; i++) table[i] = TagRec();
}

int snapshot(TagState *out, int max, uint32_t now, uint32_t withinMs) {
  int n = 0;
  for (int i = 0; i < MAX_TAGS && n < max; i++) {
    const TagRec &t = table[i];
    if (!t.used || now - t.lastSeen > withinMs) continue;
    TagState &s = out[n++];
    memcpy(s.epc, t.epc, t.epcLen);
    s.epcLen = t.epcLen;
    float r = t.rssiSmooth;
    s.rssi = (int8_t)(r < -128 ? -128 : (r > 127 ? 127 : r));
    s.proximity = (uint8_t)(proximity(t, now) * 100.0f + 0.5f);
    float hz = rate(t, now) + 0.5f;
    s.readsPerSec = (uint8_t)(hz > 255 ? 255 : hz);
    s.msSinceSeen = now - t.lastSeen;
  }
  return n;
}

}  // namespace tracker
