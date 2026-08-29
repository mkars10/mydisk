//**RFID**\\
unsigned char Write[32] = {0XAA, 0X00, 0X49, 0X00, 0X19, 0X00, 0X00, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X08, 0X42, 0X58, 0X34, 0X00, 0XE2, 0X00, 0X35, 0XA3, 0X15, 0XC3, 0X0C, 0XC8, 0XB5, 0X99, 0X99, 0X99, 0X38, 0XDD};
unsigned char Write2[] = { 0XAA, 0X00, 0X49, 0X00, 0X19, 0X00, 0X00, 0X00, 0X00, 0X01, 0X00, 0X00, 0X00, 0X08, 0X4F, 0XA8, 0X34, 0X00, 0XE2, 0X00, 0X35, 0XA3, 0X15, 0XC3, 0X0C, 0XC8, 0XB5, 0X67, 0X89, 0X02, 0XA3, 0XDD };

unsigned char ReadSingle[7] = {0XAA, 0X00, 0X22, 0X00, 0X00, 0X22, 0XDD};				//Command to read RFID once
unsigned char ReadMulti[10] = {0XAA,0X00,0X27,0X00,0X03,0X22,0XFF,0XFF,0X4A,0XDD};		//Command to read RFID until stopped
unsigned char ReadStop[7] = {0XAA, 0X00, 0X28, 0X00, 0X00, 0X28, 0XDD};					//Command to stop ReadMulti
unsigned char SetSerial[9] = {0XAA,0X00,0X11,0X00,0X02,0X00,0XC0,0XD3,0XDD};			//Command to set the communication serial of the R200 RFID module

unsigned int currentWriteCode = 0;														//How many writes you have done/ can be set to be working as incremental value in the EPC as well
unsigned char epccode[12] = "SUPERAltec12";												//EPC code you want to write. can be Hex or ASCII text. -"12characters"- or 0X01, 0X02, 0X03, 0X04, 0X05, 0X06, 0X07, 0X08, 0X09, 0X10, 0X11, 0X12, 
unsigned char readepc[12];																//Variabel to store what EPC is read (used to check if the correct EPC has been written)
// unsigned char epccode[12] = {0,1,0,1,0,1,0,1,0,1,0,0};
// unsigned char readepc[12] = {0,1,0,1,0,1,0,1,0,1,0,0};

unsigned char lastValidReadData[64];													//Last correctly read RFID code. correct because it comes after the header (AA) and the second char is 0X02 
unsigned char readData[64];																//Char to store all incomming reading data up to 64 characters
unsigned int currentReadlength = 0;														//Length of the received data (can be used to change specific characters or commands like Checksum or CRT values)
unsigned int validReadLength = 0;														//change the position the first incoming data should be placed in. Saves the incomming data during the loop
bool isReading = false;																	//Bool to say if a reading cycle has been started. Checked by the header (AA)
bool isValidRead = false;																//Telling the code if the read value is valid. used to check if the first character after (AA) is a valid response type (0X02 for response)
bool hasValidRead = false;																//Saving the state of incommig data. used to write untill the *readepc is the same as *epccode. and used to tell the code if it should feed stop or retract the motor
bool printFeedback = false;																//Used to say if you want to see the feedback the R200 RFID module gives back to the serial port on the serial monitor

int WriteTries=0;																		//The counter to see how many write tries have passed. This forces the motor to turn on if after the set ammount the EPC still is not correct

#include "RFIDLabel.hpp"																//Include the code writen in RFIDLabel.hpp
#include "motors.hpp"																	//Include the code writen in motors.hpp
RFIDLabel* label;																		//Makes a new pointer of type RFIDLabel
Motor* motor;																			//Makes a new pointer of type Motor

unsigned long int counter = 0;															//Used to count the loops to pulse the reading commands

unsigned char generateChecksum(int cStart, int cEnd, unsigned char* dataArray) {		//Calculates the Checksum can probably delete because it is placed in RFIDlabel.cpp
	long int dataCounter = 0;															//Can probably delete because it is placed in RFIDlabel.cpp
	for(int i = cStart; i < cEnd; i++) {												//Can probably delete because it is placed in RFIDlabel.cpp
		dataCounter += (int)dataArray[i];												//Can probably delete because it is placed in RFIDlabel.cpp
	}
	return dataCounter % 256;															//Can probably delete because it is placed in RFIDlabel.cpp
}

void handleInput() {																	//Starts the input handling loop
	while(Serial1.available()) {														//Checks if serial input is available 
		unsigned char rc = Serial1.read();												//Print the incoming data into *rc
    
		// Serial.print(rc, HEX);
		// Serial.print(" ");

		if(rc == 0XAA) { // Start reading												//Check if the incomming data is the beginning of a read code by header (AA)
			// Serial.println("Start reading");
			isReading = true;															//Set reading state to true
			currentReadlength = 0;														//Current length of the saved incomming RFID signal

			for(int i = 0; i < 64; i++) {												//loop *readData 64 times
				readData[i] = 0X00;														//Fill the *readData with 0X00 to clear all positions before saving new data (only for visual and inspection perposes)
				// lastValidReadData[i] = 0X00;
			}
		}

		if(isReading) {																	//Check if *isReading is true
			readData[currentReadlength] = rc;											//put the incomming data from *readData into the correct position (placed by *currentReadLength) into *rc
			
			if(currentReadlength == 1 && rc == 0X02) { // Valid response type			//Check if first character after header (AA) is 0X02. this makes sure the incomming data is a response
				// Serial.print("Valid: ");												
				validReadLength = 0;													//Change the position the first incoming data should be placed in
				isValidRead = true;														//The incomming read data is valid
				for(int i = 0; i < 64; i++) {											//Loop *lastValidReadData 64 times*
					lastValidReadData[i] = 0X00;										//Clear all chars 
				}
				lastValidReadData[0] = 0XAA;											//Write header to lastValidReadData 0XAA

			}

			if(isValidRead) {															//If incomming data is valid
				lastValidReadData[currentReadlength] = rc;								//Taking the data form *LastValidReadData and putting it in *rc at the position decided by currentReadLength
				validReadLength++;														//saving the length of the saved string
				// Serial.print(validReadLength);
				// Serial.print(" ");

				if(currentReadlength > 7 && currentReadlength < 20) {					//Taking out the EPC code from the whole string of incomming data after reading
          // Serial.write(rc);
					readepc[currentReadlength - 8] = rc;								//Removing the first 8 characters from the incomming data and saving this in *readepc
				}
			}
		}

		if(rc == 0XDD) {																//Checking if the read value is 0XDD %(IMPORTANT, if the read value has 0XDD somewhere before it is supposed to be a footer right now the code thinks it still is a footer)%
			// Serial.println("Stop reading");
			// Serial.println();
			if(isValidRead) {															//True when the incomming data started with 0XAA, 0X02. indicating it's the header and it is a response
				hasValidRead = true;													//Saving the state of an incomming valid read
			}

			if(printFeedback) {															//Set to true if you want to be able to see the feedback in the serial monitor
				Serial.print("Feedback: ");												//Print "Feedback: " on the serial monitor
				for(int i = 0; i < currentReadlength + 1; i++) {						//MARLO
					Serial.print(readData[i], HEX);										//print the readData on location *i on the serial monitor
					Serial.print(" ");													//print a space between the incomming HEX, on the serial monitor
				}
				Serial.println();														//print a ln after the whole feedback has been written to the serial monitor

				printFeedback = false;													//Is set to false to stop the feedback from being printed
			}

			isValidRead = false;														//Since the read data is 0XDD it acts as the footer of the string. That's why the possible incomming data shouldn't be considered as valid data
			isReading = false;															//Since the read data is 0XDD it acts as the footer of the string. That's why the incomming data shouldn't be considered as data that should be read
		}

		currentReadlength++;															//Change the position the read data should be placed in
	}
}

bool compareStrings(unsigned char* c1, unsigned char* c2, int length) {					//Check if the read EPC code is the same as the EPC code we wanted to write
	for(int i = 0; i < length; i++) {													//Loop through the whole length one by one
		if(c1[i] != c2[i]) return false;												//If the single char in the current specific location in the read string is not the same as the single char in the written string *compareString is false
	}
	return true;																		//if *c1 and *c2 ar true. *compareString is true
}

void setup() {
	Serial.begin(115200);																//Start the regular serial port for communication and viewing the serial monitor
	Serial.println("Started");															//Print "started" and a Ln on the serial monitor
	Serial1.begin(115200);																//Start Serial1 to send commands to the RFID module R200
	Serial.println("Started");															//Print "started" and a Ln on the serial monitor

	label = new RFIDLabel();															//Data that should be sent to RFIDLabel
	Serial.println("Started");															//Print "started" and a Ln on the serial monitor
	motor = new Motor(4,5);																//Motor pins should be 4 and 5
	Serial.println("Started");															//Print "started" and a Ln on the serial monitor
	motor->feed();																		//Start function feed in motors.hpp. This starts the motor to run (should be forwards, but switch cables or pin numbers arround if it moves backwards)
  
}

void loop() {																			
	handleInput();																		//start the handleInput function. Main function in rfid_test.ino to read the RFID data

	if(hasValidRead) {																	//if there was read a valid read saved in the *hasValidRead
    
		// Serial.print("readepc: ");
		// Serial.write(readepc, 12);
		// Serial.print("\nepccode: ");
		// Serial.write(epccode, 12);
		// // Serial.println("Istrue", epccode == readepc);
		// Serial.print("Is true: ");
		// Serial.println(compareStrings(readepc, epccode, 12) ? "true" : "false");

		// Write if code != written code
		if(WriteTries <=10 && !compareStrings(readepc, epccode, 12)) {
			motor->stopFeed();															//Stop the motor
			int incrementalEnds=false;													//Switch to True if you want the last 2 chars of the EPC to be incremental up from 48 in HEX(=0)									
			if(incrementalEnds==true){													//If *incrementalEnds is true
				double calculation = currentWriteCode / 10;								//calculation to change the last 2 EPC digtis
				epccode[10] = 48 + ((int)floor(calculation) % 10);						//calculation to change the last 2 EPC digtis
				epccode[11] = 48 + (int)(currentWriteCode % 10);						//calculation to change the last 2 EPC digtis
			}																			
			for(int i = 0; i < 12; i++) {												//Loop for the comming 12 chars
				label->EPC[i + 2] = epccode[i];											//Skips the first 2 chars in the 14 bit long EPC since those 2 should be skipped in our testing 
			}
			// Serial.write(label->EPC, 14);

			Serial.print("Read: ");														//Print "Read: "
			for(int i = 0; i < 64; i++) {												//Loop up to 64 times
				Serial.print(lastValidReadData[i], HEX);								//Print the lastValidReadData in the serial monitor as HEX
				Serial.print(" ");														//Print a space on the serial monitor
				
				if(lastValidReadData[i] == 0XDD) {										//If the last read data is 0XDD
					break;																//Breaks the loop
				}
			}
			Serial.println();															//Print a ln on the serial monitor
			printFeedback = true;														//Choose true or false for if you want to see the feedback in the serial monitor
			//delay(100);																//pause the compare and motor pause function for 1000ms
			label->overwrite(lastValidReadData, validReadLength + 1);					//MARLO
			WriteTries++;
		} 
		else {// If read code == written code number++ and motor drive					//If *compareStrings is not false, do the following
			currentWriteCode++;															//Increase the value of *currentWriteCode to change the last 2 bits of data in the EPC

			motor->feed();																//Start to run the motor forwards

			if(WriteTries >=1 && compareStrings(readepc, epccode, 12))
				{
				WriteTries=0;
				}

//			for(int i = 0; i < 12; i++) {												//Loop trough the 12 chars of the EPC saved in *readepc
//				readepc[i] = ' ';														//Clear the readepc
//				}
		}
    	hasValidRead = false;															//Set *hasValidRead to false. This resets the code to know it should wait for a valid read again.
	}

	if(counter % 3000 == 0 && !printFeedback) {											//Non blocking pause of ReadSingle commands
		Serial1.write(ReadSingle, 7);													//Send the ReadSingle command to the RFID module R200
	}

	counter++;																			//Counter that counts the times the whole code has been looped
}
