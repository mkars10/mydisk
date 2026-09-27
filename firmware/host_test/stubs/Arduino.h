#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#define SERIAL_8N1 0
#define HEX 16
unsigned long millis();
void delay(unsigned long);
struct HardwareSerial {
  std::vector<uint8_t> rx, tx; size_t rxPos=0; std::string out;
  void begin(long, int=0, int=0, int=0) {}
  int available() { return rx.size()-rxPos; }
  int read() { return rx[rxPos++]; }
  size_t write(const uint8_t*b,size_t n){tx.insert(tx.end(),b,b+n);return n;}
  size_t write(uint8_t b){tx.push_back(b);return 1;}
  void print(const char*s){out+=s;} void print(char c){out+=c;}
  void print(int v,int base=10){char b[16];snprintf(b,16,base==16?"%X":"%d",v);out+=b;}
  void println(const char*s){out+=s;out+="\n";} void println(uint32_t v){out+=std::to_string(v)+"\n";}
  void println(int v,int base){print(v,base);out+="\n";}
};
extern HardwareSerial Serial, Serial2;
