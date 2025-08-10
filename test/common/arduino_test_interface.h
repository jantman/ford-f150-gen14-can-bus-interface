#pragma once

#include "../../src/arduino_interface.h"
#include "../mocks/mock_arduino.h"

/**
 * Test implementation using ArduinoMock for dependency injection
 */
class ArduinoTestInterface : public ArduinoInterface {
public:
    void digitalWrite(uint8_t pin, uint8_t value) override {
        ArduinoMock::instance().setDigitalWrite(pin, value);
    }
    
    uint8_t digitalRead(uint8_t pin) override {
        return ArduinoMock::instance().getDigitalRead(pin);
    }
    
    void pinMode(uint8_t pin, uint8_t mode) override {
        ArduinoMock::instance().setPinMode(pin, mode);
    }
    
    unsigned long millis() override {
        return ArduinoMock::instance().getMillis();
    }
};
