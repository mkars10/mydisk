#pragma once
#include <Arduino.h>
typedef void* QueueHandle_t; typedef int BaseType_t; const int pdTRUE=1;
QueueHandle_t xQueueCreate(int,size_t); BaseType_t xQueueSend(QueueHandle_t,const void*,int); BaseType_t xQueueReceive(QueueHandle_t,void*,int);
struct BLEServer; struct BLECharacteristic;
struct BLEServerCallbacks{virtual void onConnect(BLEServer*){} virtual void onDisconnect(BLEServer*){} virtual ~BLEServerCallbacks(){}};
struct BLECharacteristicCallbacks{virtual void onWrite(BLECharacteristic*){} virtual ~BLECharacteristicCallbacks(){}};
struct BLEDescriptor{}; struct BLE2902: BLEDescriptor{};
struct BLECharacteristic{ enum{PROPERTY_READ=1,PROPERTY_WRITE=2,PROPERTY_NOTIFY=4}; std::string uuid; std::vector<uint8_t> value; std::vector<std::vector<uint8_t>> sent; BLECharacteristicCallbacks*cb=nullptr;
  void setCallbacks(BLECharacteristicCallbacks*c){cb=c;} void addDescriptor(BLEDescriptor*){}
  void setValue(uint8_t*d,size_t n){value.assign(d,d+n);} void notify(){sent.push_back(value);}
  std::string getValue(){return std::string(value.begin(),value.end());}};
struct BLEService{ std::vector<BLECharacteristic*> chars; BLECharacteristic* createCharacteristic(const char*u,uint32_t){auto*c=new BLECharacteristic;c->uuid=u;chars.push_back(c);return c;} void start(){} };
struct BLEServer{ BLEServerCallbacks*cb=nullptr; BLEService*svc=nullptr; void setCallbacks(BLEServerCallbacks*c){cb=c;} BLEService*createService(const char*){svc=new BLEService;return svc;} };
struct BLEAdvertising{ void addServiceUUID(const char*){} void setScanResponse(bool){} };
struct BLEDevice{ static BLEServer*server; static int advStarts; static void init(const char*){} static BLEServer*createServer(){server=new BLEServer;return server;} static BLEAdvertising*getAdvertising(){static BLEAdvertising a;return &a;} static void startAdvertising(){advStarts++;} };
