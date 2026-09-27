// Host-side check of the firmware: fake reader frames in, fake phone on Bluetooth.
// Run with firmware/host_test/run.sh. Not built by the Arduino IDE.
#include <Arduino.h>
#include <BLEDevice.h>
#include <deque>
#include <cassert>
HardwareSerial Serial, Serial2;
static unsigned long now_ms=1000; unsigned long millis(){return now_ms;} void delay(unsigned long d){now_ms+=d;}
BLEServer* BLEDevice::server=nullptr; int BLEDevice::advStarts=0;
static std::deque<std::vector<uint8_t>> q;
QueueHandle_t xQueueCreate(int,size_t){return (void*)1;}
BaseType_t xQueueSend(QueueHandle_t,const void*p,int){auto*b=(const uint8_t*)p;q.push_back(std::vector<uint8_t>(b,b+17));return 1;}
BaseType_t xQueueReceive(QueueHandle_t,void*p,int){if(q.empty())return 0;memcpy(p,q.front().data(),17);q.pop_front();return 1;}
void setup(); void loop();

static void pushTagFrame(int8_t rssi){
  uint8_t epc[12]={0xE2,0x80,0x68,0x90,0,0,0x50,0x0E,0x88,0xC6,0xA4,0xDD}; // 0xDD inside EPC on purpose
  std::vector<uint8_t> p={(uint8_t)rssi,0x30,0x00}; p.insert(p.end(),epc,epc+12); p.push_back(0x12); p.push_back(0x34);
  std::vector<uint8_t> f={0xAA,0x02,0x22,0x00,(uint8_t)p.size()}; f.insert(f.end(),p.begin(),p.end());
  unsigned s=0; for(size_t i=1;i<f.size();i++) s+=f[i]; f.push_back(s&0xFF); f.push_back(0xDD);
  Serial2.rx.insert(Serial2.rx.end(),f.begin(),f.end());
}
static BLECharacteristic* ch(const char*suffix){for(auto*c:BLEDevice::server->svc->chars) if(c->uuid.find(suffix)!=std::string::npos) return c; return nullptr;}
static void phoneWrite(const char*s){auto*c=ch("-0002-");c->value.assign(s,s+strlen(s));c->cb->onWrite(c);}
static std::string hex(const std::vector<uint8_t>&v){std::string o;char b[4];for(auto x:v){snprintf(b,4,"%02X ",x);o+=b;}return o;}

int main(){
  setup();
  auto*status=ch("-0004-"); auto*tag=ch("-0003-");
  printf("boot status: %s\n", hex(status->value).c_str());
  assert(status->value==std::vector<uint8_t>({1,0,20}));
  BLEDevice::server->cb->onConnect(nullptr);
  Serial2.tx.clear();
  phoneWrite("P15"); phoneWrite("S"); loop();
  printf("reader frames after P15,S: %s\n", hex(Serial2.tx).c_str());
  printf("status notifies: %zu, last: %s\n", status->sent.size(), hex(status->sent.back()).c_str());
  assert(status->sent.back()==std::vector<uint8_t>({1,1,15}));
  // 10 reads over 1s at -50 dBm
  for(int i=0;i<10;i++){ now_ms+=100; pushTagFrame(-50); loop(); }
  printf("tag notifies: %zu, last: %s\n", tag->sent.size(), hex(tag->sent.back()).c_str());
  auto &p=tag->sent.back(); assert(p.size()==16 && (int8_t)p[0]==-50 && p[3]==12 && p[15]==0xDD);
  printf("  rssi=%d prox=%d rate=%d\n",(int8_t)p[0],p[1],p[2]);
  // tag disappears: proximity should fall to 0 then updates stop
  for(int i=0;i<20;i++){ now_ms+=200; loop(); }
  printf("after 4s silent: last prox=%d, notifies=%zu\n", tag->sent.back()[1], tag->sent.size());
  assert(tag->sent.back()[1]==0);
  size_t n=tag->sent.size(); now_ms+=1000; loop(); assert(tag->sent.size()==n);
  phoneWrite("P99"); loop(); assert(status->sent.back()==std::vector<uint8_t>({1,1,15}));
  BLEDevice::server->cb->onDisconnect(nullptr); int adv=BLEDevice::advStarts; loop();
  printf("after disconnect: status %s, re-advertised %d\n", hex(status->value).c_str(), BLEDevice::advStarts-adv);
  assert(status->value[1]==0 && BLEDevice::advStarts==adv+1);
  printf("--- serial log ---\n%s", Serial.out.substr(0,600).c_str());
  printf("ALL PASS\n");
}
