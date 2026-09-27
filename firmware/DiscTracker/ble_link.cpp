#include "ble_link.h"
#include "commands.h"
#include "tracker.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <string.h>

namespace ble_link {

// Keep in sync with docs/ble-interface.md and ios/MyDiskKit.
static const char *SERVICE_UUID = "576097de-0001-4b1f-9097-0d8ee45a7f07";
static const char *COMMAND_UUID = "576097de-0002-4b1f-9097-0d8ee45a7f07";
static const char *TAG_UUID     = "576097de-0003-4b1f-9097-0d8ee45a7f07";
static const char *STATUS_UUID  = "576097de-0004-4b1f-9097-0d8ee45a7f07";

static const uint8_t PROTOCOL_VERSION = 1;
static const uint32_t TAG_REPORT_MS = 200;   // 5 updates/sec per tag
// Keep reporting a tag for a second after it goes stale, so the app sees
// its proximity fall to 0 rather than just freeze.
static const uint32_t REPORT_WITHIN_MS = tracker::STALE_MS + 1000;
static const size_t MAX_COMMAND = 16;

static BLECharacteristic *tagChar = nullptr;
static BLECharacteristic *statusChar = nullptr;

// BLE callbacks run on the Bluetooth task, not the main loop. They only hand
// work over; everything that touches the reader happens in poll().
static QueueHandle_t commandQueue = nullptr;
static volatile bool connected = false;
static volatile bool justDisconnected = false;

static void (*disconnectHandler)() = nullptr;
static uint32_t lastTagReport = 0;
static commands::Status lastStatus = { false, -1 };

struct Command { char text[MAX_COMMAND + 1]; };

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { connected = true; }
  void onDisconnect(BLEServer *) override { connected = false; justDisconnected = true; }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    auto value = c->getValue();
    Command cmd;
    size_t n = value.length();
    if (n == 0) return;
    if (n > MAX_COMMAND) n = MAX_COMMAND;
    memcpy(cmd.text, value.c_str(), n);
    cmd.text[n] = '\0';
    xQueueSend(commandQueue, &cmd, 0);   // drop if full; the app can resend
  }
};

static void publishStatus(const commands::Status &s, bool notify) {
  uint8_t packet[3] = { PROTOCOL_VERSION, (uint8_t)(s.scanning ? 1 : 0), (uint8_t)s.txPower };
  statusChar->setValue(packet, sizeof(packet));
  if (notify && connected) statusChar->notify();
}

static void sendTagUpdates() {
  static tracker::TagState tags[tracker::MAX_TAGS];
  int n = tracker::snapshot(tags, tracker::MAX_TAGS, millis(), REPORT_WITHIN_MS);
  for (int i = 0; i < n; i++) {
    // rssi | proximity | reads/sec | epcLen | epc  (at most 20 bytes)
    uint8_t packet[4 + tracker::MAX_EPC];
    packet[0] = (uint8_t)tags[i].rssi;
    packet[1] = tags[i].proximity;
    packet[2] = tags[i].readsPerSec;
    packet[3] = tags[i].epcLen;
    memcpy(packet + 4, tags[i].epc, tags[i].epcLen);
    tagChar->setValue(packet, 4 + tags[i].epcLen);
    tagChar->notify();
  }
}

void begin(const char *deviceName, void (*onDisconnect)()) {
  disconnectHandler = onDisconnect;
  commandQueue = xQueueCreate(4, sizeof(Command));

  BLEDevice::init(deviceName);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *commandChar = service->createCharacteristic(
      COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  commandChar->setCallbacks(new CommandCallbacks());

  tagChar = service->createCharacteristic(TAG_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  tagChar->addDescriptor(new BLE2902());   // lets the phone subscribe

  statusChar = service->createCharacteristic(
      STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);   // name goes in the scan response; the 128-bit UUID fills the advert
  BLEDevice::startAdvertising();

  lastStatus = commands::status();
  publishStatus(lastStatus, false);
}

void poll() {
  Command cmd;
  while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
    commands::handle(cmd.text);   // result shows up in the status characteristic
  }

  if (justDisconnected) {
    justDisconnected = false;
    if (disconnectHandler) disconnectHandler();
    BLEDevice::startAdvertising();   // stay findable for the next connection
  }

  commands::Status s = commands::status();
  if (s.scanning != lastStatus.scanning || s.txPower != lastStatus.txPower) {
    lastStatus = s;
    publishStatus(s, true);
  }

  if (connected && millis() - lastTagReport >= TAG_REPORT_MS) {
    lastTagReport = millis();
    sendTagUpdates();
  }
}

bool isConnected() { return connected; }

}  // namespace ble_link
