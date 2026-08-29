#pragma once
#include "Arduino.h"

class RFIDLabel {
  static unsigned char writeCounter; 
  
	public:
	RFIDLabel() {  };

	unsigned char EPC[14] = { 'A', 'L', 'A', 'L', 'T', 'E', 'C', 'T', 'E', 'S', 'T', 'C', 'D', 'E' };
	unsigned char memBank = 0X01;
	
	void generateWriteCommand(unsigned char* arr, unsigned char* oldLabel, int length);
	static unsigned char generateChecksum(int cStart, int cEnd, unsigned char* dataArray);
	void overwrite(unsigned char* oldLabel, int length);
};

// #include "RFIDLabel.cpp"
