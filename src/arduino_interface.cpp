#include "arduino_interface.h"

#ifdef NATIVE_ENV
// For native testing, provide stub implementations
void ArduinoHardware::digitalWrite(uint8_t pin, uint8_t value) {
    // Stub implementation for native testing
}

uint8_t ArduinoHardware::digitalRead(uint8_t pin) {
    // Stub implementation for native testing - return HIGH by default
    return 1;
}

void ArduinoHardware::pinMode(uint8_t pin, uint8_t mode) {
    // Stub implementation for native testing
}

unsigned long ArduinoHardware::millis() {
    // Stub implementation for native testing - return a simple counter
    static unsigned long counter = 0;
    return ++counter;
}
#else
// For real hardware, use Arduino functions
#include <Arduino.h>

void ArduinoHardware::digitalWrite(uint8_t pin, uint8_t value) {
    ::digitalWrite(pin, value);
}

uint8_t ArduinoHardware::digitalRead(uint8_t pin) {
    return ::digitalRead(pin);
}

void ArduinoHardware::pinMode(uint8_t pin, uint8_t mode) {
    ::pinMode(pin, mode);
}

unsigned long ArduinoHardware::millis() {
    return ::millis();
}
#endif // NATIVE_ENV
