/**
 * DiscTracker firmware — ESP32 + R200 UHF RFID reader
 *
 * Boots IDLE - the reader transmits nothing until told to start, so
 * powering up with no antenna attached is safe. Once started, it runs the
 * R200 in continuous multi-poll mode and streams every tag detection over
 * USB serial as a CSV line the laptop can parse:
 *
 *   TAG,<epc-hex>,<rssi-dBm>,<millis>
 *   INFO,<text>            module info / status messages
 *   ERR,<hex code>         reader error (0x15 "no tag" is suppressed)
 *
 * Commands accepted from the laptop (newline-terminated):
 *   S        start continuous polling (nothing is transmitted until this)
 *   X        stop polling
 *   P<dBm>   set transmit power, e.g. P20 (RPEUM-20: 12.5-20; RPEUM-26: 5-26)
 *   ?        request module info
 *
 * Field mode: also hosts a WiFi access point ("DiscTracker" / "discgolf")
 * serving a phone UI at http://192.168.4.1 (or http://disc.local). Serial
 * output is unchanged, so laptop/tracker.py still works over USB.
 *
 * Wiring (same as the original repo, see docs/R200_ESP32_wiring.jpg):
 *   R200 5V/VIN <- ESP32 VIN (5V from USB; the 3V3 pin can't supply enough current)
 *   R200 GND    <- ESP32 GND
 *   R200 TXD    -> ESP32 GPIO16 (RX2)
 *   R200 RXD    <- ESP32 GPIO17 (TX2)
 *   Antenna MUST be attached before starting a scan.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "webpage.h"
#include "tagtable.h"

static const char *AP_SSID = "DiscTracker";
static const char *AP_PASS = "discgolf";     // WPA2 needs >= 8 chars
WebServer server(80);

static const int R200_RX_PIN = 16;   // ESP32 RX2  <- R200 TXD
static const int R200_TX_PIN = 17;   // ESP32 TX2  -> R200 RXD
static const long R200_BAUD = 115200;

// ---------------------------------------------------------------------------
// Frame constants (see docs/R200 user protocol V2.3.3.pdf)
// Frame: AA | type | cmd | lenMSB | lenLSB | params... | checksum | DD
// Checksum = LSB of sum(type..last param)
// ---------------------------------------------------------------------------
static const uint8_t FRAME_HEADER = 0xAA;
static const uint8_t FRAME_END    = 0xDD;

static const uint8_t TYPE_COMMAND      = 0x00;
static const uint8_t TYPE_RESPONSE     = 0x01;
static const uint8_t TYPE_NOTIFICATION = 0x02;

static const uint8_t CMD_GET_MODULE_INFO = 0x03;
static const uint8_t CMD_SINGLE_POLL     = 0x22;
static const uint8_t CMD_MULTI_POLL      = 0x27;
static const uint8_t CMD_STOP_MULTI_POLL = 0x28;
static const uint8_t CMD_SET_TX_POWER    = 0xB6;
static const uint8_t CMD_FAILURE         = 0xFF;

static const uint8_t ERR_INVENTORY_FAIL = 0x15;  // "no tag in range" — not an error

// ---------------------------------------------------------------------------
// Outgoing commands
// ---------------------------------------------------------------------------
void sendFrame(uint8_t cmd, const uint8_t *params, uint16_t paramLen) {
  uint8_t head[5] = { FRAME_HEADER, TYPE_COMMAND, cmd,
                      (uint8_t)(paramLen >> 8), (uint8_t)(paramLen & 0xFF) };
  uint16_t sum = 0;
  for (int i = 1; i < 5; i++) sum += head[i];
  for (uint16_t i = 0; i < paramLen; i++) sum += params[i];
  Serial2.write(head, 5);
  if (paramLen) Serial2.write(params, paramLen);
  Serial2.write((uint8_t)(sum & 0xFF));
  Serial2.write(FRAME_END);
}

void requestModuleInfo() {
  uint8_t p = 0x00;  // 0x00 = hardware version info
  sendFrame(CMD_GET_MODULE_INFO, &p, 1);
}

void startMultiPoll() {
  // 0x22 reserved byte, then poll count 0xFFFF (re-armed by the watchdog below)
  uint8_t p[3] = { 0x22, 0xFF, 0xFF };
  sendFrame(CMD_MULTI_POLL, p, 3);
}

void stopMultiPoll() {
  sendFrame(CMD_STOP_MULTI_POLL, nullptr, 0);
}

int txPower = 20;   // dBm; full power saturates at close range

void setTxPower(int dBm) {
  txPower = dBm;
  uint16_t v = (uint16_t)(dBm * 100);  // module expects dBm * 100
  uint8_t p[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
  sendFrame(CMD_SET_TX_POWER, p, 2);
  Serial.print("INFO,tx power set to ");
  Serial.print(dBm);
  Serial.println(" dBm");
}

// ---------------------------------------------------------------------------
// Tag table - the tracking maths that used to live in tracker.py, moved here
// so the phone UI can be a thin display.
// ---------------------------------------------------------------------------
float tagRate(const TagRec &t, uint32_t now) {
  uint32_t span = now - t.winStart;
  if (span < 100) span = 100;
  return (t.winCount * 1000.0f) / span;
}

float tagProximity(const TagRec &t, uint32_t now) {
  if (now - t.lastSeen > STALE_MS) return 0.0f;
  float rn = (t.rssiSmooth - RSSI_FLOOR) / (RSSI_CEIL - RSSI_FLOOR);
  rn = rn < 0 ? 0 : (rn > 1 ? 1 : rn);
  float hn = tagRate(t, now) / RATE_CEIL;
  if (hn > 1.0f) hn = 1.0f;
  // Rate carries the fringe of range; RSSI carries the mid range.
  return 0.5f * rn + 0.5f * hn;
}

void recordTag(const uint8_t *epc, uint8_t len, int8_t rssi) {
  uint32_t now = millis();
  if (len > MAX_EPC) len = MAX_EPC;

  int slot = -1, freeSlot = -1, oldest = -1;
  uint32_t oldestSeen = 0xFFFFFFFF;
  for (int i = 0; i < MAX_TAGS; i++) {
    if (!tagTable[i].used) { if (freeSlot < 0) freeSlot = i; continue; }
    if (tagTable[i].epcLen == len && memcmp(tagTable[i].epc, epc, len) == 0) { slot = i; break; }
    if (tagTable[i].lastSeen < oldestSeen) { oldestSeen = tagTable[i].lastSeen; oldest = i; }
  }
  if (slot < 0) slot = (freeSlot >= 0) ? freeSlot : oldest;
  if (slot < 0) return;

  TagRec &t = tagTable[slot];
  if (!t.used || t.epcLen != len || memcmp(t.epc, epc, len) != 0) {
    t = TagRec();                       // reused slot: start clean
    t.used = true;
    memcpy(t.epc, epc, len);
    t.epcLen = len;
    t.rssiSmooth = rssi;
    t.winStart = now;
  }
  t.lastRssi = rssi;
  t.rssiSmooth = EWMA_ALPHA * rssi + (1.0f - EWMA_ALPHA) * t.rssiSmooth;
  t.reads++;
  t.lastSeen = now;
  // Decay the window rather than resetting, so the rate doesn't sawtooth
  if (now - t.winStart > 1500) { t.winStart = now - 750; t.winCount /= 2; }
  t.winCount++;
}

void clearTags() {
  for (int i = 0; i < MAX_TAGS; i++) tagTable[i] = TagRec();
}

// ---------------------------------------------------------------------------
// Incoming frame parser — byte-at-a-time state machine, never blocks.
// This deliberately does NOT scan for 0xDD to find the end of a frame
// (0xDD can legitimately appear inside an EPC); it uses the declared
// parameter length instead, then verifies checksum and end byte.
// ---------------------------------------------------------------------------
static const uint16_t MAX_PARAMS = 250;

enum ParseState { PS_HEADER, PS_TYPE, PS_CMD, PS_LEN_MSB, PS_LEN_LSB,
                  PS_PARAMS, PS_CHECKSUM, PS_END };

ParseState psState = PS_HEADER;
uint8_t  frType, frCmd, frChecksum;
uint16_t frLen, frGot;
uint8_t  frParams[MAX_PARAMS];

bool pollingEnabled = false;   // boot idle: no RF until the host sends 'S'
unsigned long lastFrameMs = 0;
unsigned long tagCount = 0;

void handleFrame() {
  if (frType == TYPE_NOTIFICATION && frCmd == CMD_SINGLE_POLL) {
    // Tag notification: RSSI(1) | PC(2) | EPC(len-5) | tagCRC(2)
    if (frLen < 6) return;
    int8_t rssi = (int8_t)frParams[0];
    uint16_t epcLen = frLen - 5;
    recordTag(&frParams[3], (uint8_t)epcLen, rssi);
    Serial.print("TAG,");
    for (uint16_t i = 0; i < epcLen; i++) {
      uint8_t b = frParams[3 + i];
      if (b < 0x10) Serial.print('0');
      Serial.print(b, HEX);
    }
    Serial.print(',');
    Serial.print(rssi);
    Serial.print(',');
    Serial.println(millis());
    tagCount++;
  }
  else if (frType == TYPE_RESPONSE && frCmd == CMD_GET_MODULE_INFO) {
    Serial.print("INFO,");
    for (uint16_t i = 1; i < frLen; i++) Serial.print((char)frParams[i]);
    Serial.println();
  }
  else if (frType == TYPE_RESPONSE && frCmd == CMD_SET_TX_POWER) {
    Serial.println("INFO,tx power acknowledged");
  }
  else if (frCmd == CMD_FAILURE) {
    // 0x15 just means "no tag seen this round" — stay quiet about it
    if (frLen >= 1 && frParams[0] != ERR_INVENTORY_FAIL) {
      Serial.print("ERR,");
      Serial.println(frParams[0], HEX);
    }
  }
}

void feedByte(uint8_t b) {
  switch (psState) {
    case PS_HEADER:
      if (b == FRAME_HEADER) psState = PS_TYPE;
      break;
    case PS_TYPE:
      frType = b;
      psState = PS_CMD;
      break;
    case PS_CMD:
      frCmd = b;
      psState = PS_LEN_MSB;
      break;
    case PS_LEN_MSB:
      frLen = ((uint16_t)b) << 8;
      psState = PS_LEN_LSB;
      break;
    case PS_LEN_LSB:
      frLen |= b;
      frGot = 0;
      if (frLen > MAX_PARAMS) { psState = PS_HEADER; break; }  // garbage; resync
      psState = frLen ? PS_PARAMS : PS_CHECKSUM;
      break;
    case PS_PARAMS:
      frParams[frGot++] = b;
      if (frGot == frLen) psState = PS_CHECKSUM;
      break;
    case PS_CHECKSUM:
      frChecksum = b;
      psState = PS_END;
      break;
    case PS_END: {
      psState = PS_HEADER;
      if (b != FRAME_END) break;
      uint16_t sum = frType + frCmd + (frLen >> 8) + (frLen & 0xFF);
      for (uint16_t i = 0; i < frLen; i++) sum += frParams[i];
      if ((uint8_t)(sum & 0xFF) == frChecksum) {
        lastFrameMs = millis();
        handleFrame();
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Commands from the laptop
// ---------------------------------------------------------------------------
char cmdBuf[16];
uint8_t cmdLen = 0;

void handleHostCommand(const char *cmd) {
  switch (cmd[0]) {
    case 'S': case 's':
      pollingEnabled = true;
      startMultiPoll();
      Serial.println("INFO,polling started");
      break;
    case 'X': case 'x':
      pollingEnabled = false;
      stopMultiPoll();
      Serial.println("INFO,polling stopped");
      break;
    case 'P': case 'p': {
      int dBm = atoi(cmd + 1);
      if (dBm >= 5 && dBm <= 26) setTxPower(dBm);
      else Serial.println("ERR,power out of range (5-26 dBm)");
      break;
    }
    case '?':
      requestModuleInfo();
      break;
  }
}

// ---------------------------------------------------------------------------
// Web API
// ---------------------------------------------------------------------------
void handleState() {
  uint32_t now = millis();
  static char buf[3200];

  // Order by proximity so the phone list is sorted strongest-first
  int idx[MAX_TAGS], n = 0;
  for (int i = 0; i < MAX_TAGS; i++) if (tagTable[i].used) idx[n++] = i;
  for (int i = 1; i < n; i++) {
    int k = idx[i]; float kp = tagProximity(tagTable[k], now); int j = i - 1;
    while (j >= 0 && tagProximity(tagTable[idx[j]], now) < kp) { idx[j+1] = idx[j]; j--; }
    idx[j+1] = k;
  }

  int p = snprintf(buf, sizeof(buf), "{\"scan\":%d,\"pwr\":%d,\"n\":%d,\"t\":[",
                   pollingEnabled ? 1 : 0, txPower, n);
  for (int i = 0; i < n; i++) {
    const TagRec &t = tagTable[idx[i]];
    char hex[MAX_EPC * 2 + 1];
    for (int b = 0; b < t.epcLen; b++) sprintf(hex + b * 2, "%02X", t.epc[b]);
    hex[t.epcLen * 2] = 0;
    p += snprintf(buf + p, sizeof(buf) - p,
                  "%s{\"e\":\"%s\",\"r\":%d,\"h\":%.1f,\"p\":%d,\"c\":%u,\"a\":%u}",
                  i ? "," : "", hex, (int)t.rssiSmooth, tagRate(t, now),
                  (int)(tagProximity(t, now) * 100.0f),
                  (unsigned)t.reads, (unsigned)(now - t.lastSeen));
    if (p > (int)sizeof(buf) - 200) break;   // never overrun
  }
  snprintf(buf + p, sizeof(buf) - p, "]}");
  server.send(200, "application/json", buf);
}

void startWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  if (MDNS.begin("disc")) MDNS.addService("http", "tcp", 80);

  server.on("/", []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/state", handleState);
  server.on("/api/start", []() {
    pollingEnabled = true; startMultiPoll(); lastFrameMs = millis();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/stop", []() {
    pollingEnabled = false; stopMultiPoll();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/power", []() {
    int d = server.arg("dbm").toInt();
    if (d >= 5 && d <= 26) setTxPower(d);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/clear", []() { clearTags(); server.send(200, "text/plain", "ok"); });
  server.onNotFound([]() { server.send_P(200, "text/html", PAGE_HTML); });
  server.begin();

  Serial.print("INFO,wifi AP '"); Serial.print(AP_SSID);
  Serial.print("' up at http://"); Serial.print(WiFi.softAPIP());
  Serial.println("  (or http://disc.local)");
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(R200_BAUD, SERIAL_8N1, R200_RX_PIN, R200_TX_PIN);
  delay(500);  // let the module boot
  Serial.println("INFO,DiscTracker boot - idle, no RF until 'S'");
  requestModuleInfo();
  delay(100);
  setTxPower(txPower);
  startWiFi();
  lastFrameMs = millis();
}

void loop() {
  server.handleClient();

  // Drain everything the reader has sent
  while (Serial2.available()) {
    feedByte((uint8_t)Serial2.read());
  }

  // Host commands
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen) { cmdBuf[cmdLen] = '\0'; handleHostCommand(cmdBuf); cmdLen = 0; }
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }

  // Watchdog: the 0xFFFF poll count eventually runs out, and the module can
  // go quiet if it hiccups — re-arm multi-poll if nothing arrived for 3 s.
  if (pollingEnabled && millis() - lastFrameMs > 3000) {
    startMultiPoll();
    lastFrameMs = millis();
  }
}
