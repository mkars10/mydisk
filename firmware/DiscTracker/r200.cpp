#include "r200.h"

namespace r200 {

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

static const unsigned long QUIET_REARM_MS = 3000;

static HardwareSerial *uart = nullptr;
static Listener listener = { nullptr, nullptr, nullptr };
static bool scanning = false;
static int power = 20;              // dBm; full power saturates at close range
static unsigned long lastFrameMs = 0;

// ---------------------------------------------------------------------------
// Outgoing commands
// ---------------------------------------------------------------------------
static void sendFrame(uint8_t cmd, const uint8_t *params, uint16_t paramLen) {
  uint8_t head[5] = { FRAME_HEADER, TYPE_COMMAND, cmd,
                      (uint8_t)(paramLen >> 8), (uint8_t)(paramLen & 0xFF) };
  uint16_t sum = 0;
  for (int i = 1; i < 5; i++) sum += head[i];
  for (uint16_t i = 0; i < paramLen; i++) sum += params[i];
  uart->write(head, 5);
  if (paramLen) uart->write(params, paramLen);
  uart->write((uint8_t)(sum & 0xFF));
  uart->write(FRAME_END);
}

static void sendMultiPoll() {
  // 0x22 reserved byte, then poll count 0xFFFF (re-armed by poll() below)
  uint8_t p[3] = { 0x22, 0xFF, 0xFF };
  sendFrame(CMD_MULTI_POLL, p, 3);
}

void requestModuleInfo() {
  uint8_t p = 0x00;  // 0x00 = hardware version info
  sendFrame(CMD_GET_MODULE_INFO, &p, 1);
}

void startScan() {
  scanning = true;
  sendMultiPoll();
  lastFrameMs = millis();
}

void stopScan() {
  scanning = false;
  sendFrame(CMD_STOP_MULTI_POLL, nullptr, 0);
}

bool isScanning() { return scanning; }

void setTxPower(int dBm) {
  power = dBm;
  uint16_t v = (uint16_t)(dBm * 100);  // module expects dBm * 100
  uint8_t p[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
  sendFrame(CMD_SET_TX_POWER, p, 2);
}

int txPower() { return power; }

// ---------------------------------------------------------------------------
// Incoming frame parser — byte-at-a-time state machine, never blocks.
// This deliberately does NOT scan for 0xDD to find the end of a frame
// (0xDD can legitimately appear inside an EPC); it uses the declared
// parameter length instead, then verifies checksum and end byte.
// ---------------------------------------------------------------------------
static const uint16_t MAX_PARAMS = 250;

enum ParseState { PS_HEADER, PS_TYPE, PS_CMD, PS_LEN_MSB, PS_LEN_LSB,
                  PS_PARAMS, PS_CHECKSUM, PS_END };

static ParseState psState = PS_HEADER;
static uint8_t  frType, frCmd, frChecksum;
static uint16_t frLen, frGot;
static uint8_t  frParams[MAX_PARAMS];

static void handleFrame() {
  if (frType == TYPE_NOTIFICATION && frCmd == CMD_SINGLE_POLL) {
    // Tag notification: RSSI(1) | PC(2) | EPC(len-5) | tagCRC(2)
    if (frLen < 6) return;
    if (listener.onTag) listener.onTag(&frParams[3], (uint8_t)(frLen - 5), (int8_t)frParams[0]);
  }
  else if (frType == TYPE_RESPONSE && frCmd == CMD_GET_MODULE_INFO) {
    char text[MAX_PARAMS];
    uint16_t n = 0;
    for (uint16_t i = 1; i < frLen && n < sizeof(text) - 1; i++) text[n++] = (char)frParams[i];
    text[n] = '\0';
    if (listener.onInfo) listener.onInfo(text);
  }
  else if (frType == TYPE_RESPONSE && frCmd == CMD_SET_TX_POWER) {
    if (listener.onInfo) listener.onInfo("tx power acknowledged");
  }
  else if (frCmd == CMD_FAILURE) {
    // 0x15 just means "no tag seen this round" — stay quiet about it
    if (frLen >= 1 && frParams[0] != ERR_INVENTORY_FAIL && listener.onError) {
      listener.onError(frParams[0]);
    }
  }
}

static void feedByte(uint8_t b) {
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
void begin(HardwareSerial &port, int rxPin, int txPin, const Listener &l) {
  uart = &port;
  listener = l;
  uart->begin(115200, SERIAL_8N1, rxPin, txPin);
  delay(500);  // let the module boot
  requestModuleInfo();
  delay(100);
  setTxPower(power);
  lastFrameMs = millis();
}

void poll() {
  while (uart->available()) feedByte((uint8_t)uart->read());

  // The 0xFFFF poll count eventually runs out, and the module can go quiet
  // if it hiccups — re-arm multi-poll if nothing arrived for a while.
  if (scanning && millis() - lastFrameMs > QUIET_REARM_MS) {
    sendMultiPoll();
    lastFrameMs = millis();
  }
}

}  // namespace r200
