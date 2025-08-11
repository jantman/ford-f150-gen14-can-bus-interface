#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>  // For memcpy, memset
#include <map>
#include <string>

// Ensure Arduino.h compatibility
#ifndef ARDUINO_H
#define ARDUINO_H
#endif

// Mock Arduino constants
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Mock GPIO pins - use same values as config.h
#define BEDLIGHT_PIN 5
#define TOOLBOX_OPENER_PIN 4
#define TOOLBOX_BUTTON_PIN 17
#define SYSTEM_READY_PIN 18

// Mock Arduino types
typedef uint8_t byte;

// Mock Arduino functions with tracking
class ArduinoMock {
public:
    static ArduinoMock& instance() {
        static ArduinoMock inst;
        return inst;
    }
    
    // Time control for testing
    void setMillis(unsigned long time) { currentTime = time; }
    void advanceTime(unsigned long ms) { currentTime += ms; }
    unsigned long getMillis() const { return currentTime; }
    
    // GPIO state tracking
    void setPinMode(uint8_t pin, uint8_t mode) { pinModes[pin] = mode; }
    uint8_t getPinMode(uint8_t pin) const { 
        auto it = pinModes.find(pin);
        return it != pinModes.end() ? it->second : INPUT;
    }
    
    void setDigitalWrite(uint8_t pin, uint8_t value) { digitalStates[pin] = value; }
    uint8_t getDigitalState(uint8_t pin) const {
        auto it = digitalStates.find(pin);
        return it != digitalStates.end() ? it->second : LOW;
    }
    
    void setDigitalRead(uint8_t pin, uint8_t value) { digitalReads[pin] = value; }
    uint8_t getDigitalRead(uint8_t pin) const {
        auto it = digitalReads.find(pin);
        return it != digitalReads.end() ? it->second : HIGH; // Default pullup
    }
    
    // Reset for clean tests
    void reset() {
        currentTime = 0;
        pinModes.clear();
        digitalStates.clear();
        digitalReads.clear();
        serialOutput.clear();
    }
    
    // Serial output capture
    void addSerialOutput(const std::string& output) { serialOutput += output; }
    std::string getSerialOutput() const { return serialOutput; }
    void clearSerialOutput() { serialOutput.clear(); }

private:
    unsigned long currentTime = 0;
    std::map<uint8_t, uint8_t> pinModes;
    std::map<uint8_t, uint8_t> digitalStates;
    std::map<uint8_t, uint8_t> digitalReads;
    std::string serialOutput;
};

// Mock Arduino functions
inline unsigned long millis() {
    return ArduinoMock::instance().getMillis();
}

inline void pinMode(uint8_t pin, uint8_t mode) {
    ArduinoMock::instance().setPinMode(pin, mode);
}

inline void digitalWrite(uint8_t pin, uint8_t value) {
    ArduinoMock::instance().setDigitalWrite(pin, value);
}

inline uint8_t digitalRead(uint8_t pin) {
    return ArduinoMock::instance().getDigitalRead(pin);
}

// Mock Serial class for logging
class MockSerial {
public:
    template<typename... Args>
    void printf(const char* format, Args... args) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        ArduinoMock::instance().addSerialOutput(std::string(buffer));
    }
    
    void print(const char* str) {
        ArduinoMock::instance().addSerialOutput(std::string(str));
    }
    
    void println(const char* str) {
        ArduinoMock::instance().addSerialOutput(std::string(str) + "\n");
    }
};

extern MockSerial Serial;

// Mock decision logic functions with C linkage for test compatibility
extern "C" {
    bool shouldEnableBedlight(uint8_t pudLampRequest);
    bool isVehicleUnlocked(uint8_t vehicleLockStatus);
    bool isVehicleParked(uint8_t transmissionParkStatus);
    bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked);
    bool shouldActivateToolbox();
    bool isTargetCANMessage(uint32_t messageId);
}
