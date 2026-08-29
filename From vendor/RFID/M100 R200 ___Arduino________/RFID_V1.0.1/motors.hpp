#pragma once
#include "Arduino.h"

class Motor {
    unsigned int feedPort = 4;
    unsigned int retractPort = 5;

    public:
    Motor(unsigned int feedport = 6, unsigned int retractport = 7) {
        this->feedPort = feedport;
        this->retractPort = retractport;
        
        pinMode(this->feedPort, OUTPUT);
        pinMode(this->retractPort, OUTPUT);

        digitalWrite(this->retractPort, LOW);
        digitalWrite(this->feedPort, LOW);
    }

    void feed() {
        digitalWrite(this->retractPort, LOW);
        digitalWrite(this->feedPort, HIGH);
    }
    void stopFeed() {
        digitalWrite(this->feedPort, LOW);
    }
    void retract() {
        digitalWrite(this->retractPort, HIGH);
        digitalWrite(this->feedPort, LOW);
    }
    void stopRetract() {
        digitalWrite(this->retractPort, LOW);
    }
};
