#ifndef GPIO_CONTROLLER_H
#define GPIO_CONTROLLER_H

#ifndef NATIVE_ENV
#include <Arduino.h>
#endif
#include "config.h"

#ifdef __cplusplus
#include "arduino_interface.h"
extern "C" {
#endif

// GPIO state tracking
struct GPIOState {
    bool bedlight;
    bool toolboxOpener;
    bool toolboxButton;
    bool systemReady;
    unsigned long toolboxOpenerStartTime;
};

// C-compatible function declarations
bool initializeGPIO();
void setBedlight(bool state);
void setToolboxOpener(bool state);
void setSystemReady(bool state);
bool readToolboxButton();
void updateToolboxOpenerTiming();
GPIOState getGPIOState();

// Utility functions for debugging
void printGPIOStatus();

#ifdef __cplusplus
}

// C++ only functions for dependency injection
bool initializeGPIOWithInterface(ArduinoInterface* arduino);
void setArduinoInterface(ArduinoInterface* arduino);
#endif

#endif // GPIO_CONTROLLER_H
