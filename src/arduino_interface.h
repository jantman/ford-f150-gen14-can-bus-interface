#pragma once

#include <stdint.h>

/**
 * Abstract interface for Arduino hardware functions
 * 
 * This interface allows dependency injection for testing while maintaining
 * a single implementation of GPIO logic in production code.
 */
class ArduinoInterface {
public:
    virtual ~ArduinoInterface() = default;
    
    // Digital I/O functions
    virtual void digitalWrite(uint8_t pin, uint8_t value) = 0;
    virtual uint8_t digitalRead(uint8_t pin) = 0;
    virtual void pinMode(uint8_t pin, uint8_t mode) = 0;
    
    // Timing functions
    virtual unsigned long millis() = 0;
};

/**
 * Production implementation using real Arduino functions
 */
class ArduinoHardware : public ArduinoInterface {
public:
    void digitalWrite(uint8_t pin, uint8_t value) override;
    uint8_t digitalRead(uint8_t pin) override;
    void pinMode(uint8_t pin, uint8_t mode) override;
    unsigned long millis() override;
};
