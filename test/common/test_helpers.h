#pragma once

#include <gtest/gtest.h>
#include "../mocks/mock_arduino.h"

// Test helper class for Arduino mock setup/teardown
class ArduinoTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    // Helper to advance time and simulate real-time passage
    void advanceTime(unsigned long ms) {
        ArduinoMock::instance().advanceTime(ms);
    }
    
    // Helper to set initial time
    void setTime(unsigned long time) {
        ArduinoMock::instance().setMillis(time);
    }
    
    // Helper to simulate button press
    void pressButton(uint8_t pin, bool pressed = true) {
        ArduinoMock::instance().setDigitalRead(pin, pressed ? LOW : HIGH);
    }
    
    // Helper to check GPIO output state
    bool isGPIOHigh(uint8_t pin) {
        return ArduinoMock::instance().getDigitalState(pin) == HIGH;
    }
    
    // Helper to get serial output for logging verification
    std::string getSerialOutput() {
        return ArduinoMock::instance().getSerialOutput();
    }
    
    void clearSerialOutput() {
        ArduinoMock::instance().clearSerialOutput();
    }
};

// Helper macros for common CAN message creation
#define CREATE_CAN_MESSAGE(id, data_bytes) \
    { .identifier = id, .data_length_code = 8, .data = data_bytes }

// Bit manipulation helpers for creating test CAN data
inline void setBits(uint8_t* data, int startBit, int length, uint32_t value) {
    int byteIndex = startBit / 8;
    int bitOffset = startBit % 8;
    
    // Clear existing bits
    uint32_t mask = ((1U << length) - 1) << bitOffset;
    for (int i = 0; i < (length + bitOffset + 7) / 8; i++) {
        if (byteIndex + i < 8) {
            data[byteIndex + i] &= ~(mask >> (i * 8));
        }
    }
    
    // Set new value  
    for (int i = 0; i < (length + bitOffset + 7) / 8; i++) {
        if (byteIndex + i < 8) {
            data[byteIndex + i] |= (value << bitOffset) >> (i * 8);
        }
    }
}

// Helper to create BCM_Lamp_Stat_FD1 test message
inline void createBCMLampMessage(uint8_t* data, uint32_t pudLampValue) {
    memset(data, 0, 8);
    setBits(data, 11, 2, pudLampValue);
}

// Helper to create Locking_Systems_2_FD1 test message  
inline void createLockingSystemsMessage(uint8_t* data, uint32_t lockStatusValue) {
    memset(data, 0, 8);
    setBits(data, 34, 2, lockStatusValue);
}

// Helper to create PowertrainData_10 test message
inline void createPowertrainMessage(uint8_t* data, uint32_t parkSystemValue) {
    memset(data, 0, 8);
    setBits(data, 31, 4, parkSystemValue);
}

// Helper to create Battery_Mgmt_3_FD1 test message
inline void createBatteryMessage(uint8_t* data, uint32_t socValue) {
    memset(data, 0, 8);
    setBits(data, 22, 7, socValue);
}
