#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

// Additional logging utilities beyond the basic macros in config.h

// Function declarations
void logSystemStartup();
void logCANMessage(const char* direction, uint32_t id, const uint8_t* data, uint8_t length);
void logStateChange(const char* signal, const char* oldValue, const char* newValue);
void logGPIOChange(const char* pin, bool state);
void logButtonPress();
void logError(const char* component, const char* error);

// Utility functions for converting values to strings
const char* pudLampToString(uint8_t value);
const char* lockStatusToString(uint8_t value);
const char* parkStatusToString(uint8_t value);
const char* boolToString(bool value);

// Performance monitoring
void logMemoryUsage();
void logSystemPerformance();

#endif // LOGGER_H
