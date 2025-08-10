#ifndef GPIO_CONTROLLER_H
#define GPIO_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// GPIO state tracking
struct GPIOState {
    bool bedlight;
    bool toolboxOpener;
    bool toolboxButton;
    bool systemReady;
    unsigned long toolboxOpenerStartTime;
};

// Function declarations (to be implemented in Step 2)
bool initializeGPIO();
void setBedlight(bool state);
void setToolboxOpener(bool state);
void setSystemReady(bool state);
bool readToolboxButton();
void updateToolboxOpenerTiming();
GPIOState getGPIOState();

// Utility functions for debugging
void printGPIOStatus();

#endif // GPIO_CONTROLLER_H
