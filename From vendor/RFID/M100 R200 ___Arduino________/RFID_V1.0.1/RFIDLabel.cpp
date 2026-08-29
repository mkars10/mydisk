#pragma once																									//Run once
#include "RFIDLabel.hpp"																						//Include the RFIDLabel.hpp

unsigned char RFIDLabel::writeCounter = 33;																		//set the length for *writeCounter command length

unsigned char RFIDLabel::generateChecksum(int cStart, int cEnd, unsigned char* dataArray) {						//Run the checksum calculation
	long int dataCounter = 0;																					//Start the *dataCounter at 0. This adds up to be the total sum of all dataArray data.
	for(int i = cStart; i < cEnd; i++) {																		//Loop through the whole write data to calculate what the checksum should be
		dataCounter += (int)dataArray[i];																		//Add dataArray up as an int instead of a char at the value of the location placed by *i
	}
	return dataCounter % 256;																					//Devide datacounter bij 256 and take the rest. This way the number here can never be higher then 256
}

void RFIDLabel::generateWriteCommand(unsigned char* arr, unsigned char* oldLabel, int length) {
	//                              header	Type	Command	param len	Passcode				
	unsigned char writeData[32]; //{ 0XAA,	0X00, 	0X49, 	0X00, 0X19,	0X00, 0X00, 0X00, 0X00 };
	writeData[0] = 0XAA;					// Header
	writeData[1] = 0X00;					// Type
	writeData[2] = 0X49;					// Command (write)
	writeData[3] = 0X00;					// Parameter length (1/2)
	writeData[4] = 0X19;					// Parameter length (2/2) Total length is 25 -> Hex 0X19

	writeData[5] = 0X00;					// Passcode 1/4
	writeData[6] = 0X00;					// Passcode 2/4
	writeData[7] = 0X00;					// Passcode 3/4
	writeData[8] = 0X00;					// Passcode 4/4

	writeData[9] = this->memBank;			// Memory bank (0 is RFU, 1 is EPC, 2 is TID and 3 is USER)
	writeData[10] = 0X00;					// Memory address offset 1/2
	writeData[11] = 0X00;					// Memory address offset 2/2
	writeData[12] = 0X00;					// Data length 1/2
	writeData[13] = 0X08;					// Data length 2/2. Data length is in words (bytes / 2)

	// Serial.print("Oldlabel: ");
	// for(int i = 0; i < length; i++) {
	// 	// Serial.print("(");
	// 	// Serial.print(i);
	// 	// Serial.print(")");
	// 	Serial.print(oldLabel[i], HEX);
	// 	Serial.print(" ");
	// }

	writeData[14] = oldLabel[length - 4];	// CRC code 1/2
	writeData[15] = oldLabel[length - 3];	// CRC code 2/2

	for(int i = 0; i < 14; i++) {
		writeData[16 + i] = this->EPC[i];	// 14x EPC code
	}

	// writeData[19] = RFIDLabel::writeCounter;

	writeData[30] = RFIDLabel::generateChecksum(1, 30, writeData);
	writeData[31] = 0XDD;

	Serial.println("");
	Serial.print("Writing 1: ");
	for(int i = 0; i < 32; i++) {
		arr[i] = writeData[i];
		// Serial.print("(");
		// Serial.print(i);
		// Serial.print(")");
		Serial.print(writeData[i], HEX);
		Serial.print(" ");
	}
	Serial.println("");
	// RFIDLabel::writeCounter++;
}

void RFIDLabel::overwrite(unsigned char* oldLabel, int length) {
	unsigned char toWrite[32];
	this->generateWriteCommand(toWrite, oldLabel, length);
	// Serial.print("Writing 2: ");
	// for(int i = 0; i < 32; i++) {
	// 	Serial.print(toWrite[i], HEX);
	// 	Serial.print(" ");
	// }
	// Serial.println("");
	Serial1.write(toWrite, 32);
}
