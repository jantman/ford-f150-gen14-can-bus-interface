#ifndef GPIO_CONTROLLER_H
#define GPIO_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// GPIO state tracking
struct GPIOState {
    bool bedlight;
    bool parkedLED;
    bool unlockedLED;
    bool toolboxOpener;
    bool toolboxButton;
    unsigned long toolboxOpenerStartTime;
};

// Function declarations (to be implemented in Step 2)
bool initializeGPIO();
void setBedlight(bool state);
void setParkedLED(bool state);
void setUnlockedLED(bool state);
void setToolboxOpener(bool state);
bool readToolboxButton();
void updateToolboxOpenerTiming();
GPIOState getGPIOState();

// Utility functions for testing and debugging
void testAllOutputs();
void printGPIOStatus();

#endif // GPIO_CONTROLLER_H
