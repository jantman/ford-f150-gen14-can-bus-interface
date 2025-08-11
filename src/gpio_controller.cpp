#include "gpio_controller.h"
#ifdef NATIVE_ENV
#include "native_arduino_compat.h"
#else
#include <climits>  // For ULONG_MAX
#endif

// Global GPIO state tracking
static GPIOState gpioState = {
    .bedlight = false,
    .toolboxOpener = false,
    .toolboxButton = false,
    .systemReady = false,
    .toolboxOpenerStartTime = 0
};

// Arduino interface for dependency injection
static ArduinoInterface* arduinoInterface = nullptr;
static ArduinoHardware defaultHardware;

// C++ functions for dependency injection
void setArduinoInterface(ArduinoInterface* arduino) {
    arduinoInterface = arduino;
}

bool initializeGPIOWithInterface(ArduinoInterface* arduino) {
    // Set the interface if provided
    if (arduino) {
        setArduinoInterface(arduino);
    }
    
    return initializeGPIO();
}

// Get the current Arduino interface (defaulting to hardware)
static ArduinoInterface* getArduinoInterface() {
    return arduinoInterface ? arduinoInterface : &defaultHardware;
}

// C-compatible wrapper functions
extern "C" {

bool initializeGPIO() {
    ArduinoInterface* hw = getArduinoInterface();
    
    LOG_INFO("Initializing GPIO pins...");
    
    // Initialize output pins
    hw->pinMode(BEDLIGHT_PIN, OUTPUT);
    hw->pinMode(TOOLBOX_OPENER_PIN, OUTPUT);
    hw->pinMode(SYSTEM_READY_PIN, OUTPUT);
    
    // Initialize input pin with internal pullup
    hw->pinMode(TOOLBOX_BUTTON_PIN, INPUT_PULLUP);
    
    // Set all outputs to known state (off)
    hw->digitalWrite(BEDLIGHT_PIN, LOW);
    hw->digitalWrite(TOOLBOX_OPENER_PIN, LOW);
    hw->digitalWrite(SYSTEM_READY_PIN, LOW);
    
    // Initialize state structure
    gpioState.bedlight = false;
    gpioState.toolboxOpener = false;
    gpioState.systemReady = false;
    gpioState.toolboxButton = hw->digitalRead(TOOLBOX_BUTTON_PIN) == LOW; // Active low with pullup
    gpioState.toolboxOpenerStartTime = 0;
    
    LOG_INFO("GPIO initialization complete");
    LOG_INFO("  BEDLIGHT_PIN (%d): OUTPUT, initial state: LOW", BEDLIGHT_PIN);
    LOG_INFO("  TOOLBOX_OPENER_PIN (%d): OUTPUT, initial state: LOW", TOOLBOX_OPENER_PIN);
    LOG_INFO("  SYSTEM_READY_PIN (%d): OUTPUT, initial state: LOW", SYSTEM_READY_PIN);
    LOG_INFO("  TOOLBOX_BUTTON_PIN (%d): INPUT_PULLUP, current state: %s", 
             TOOLBOX_BUTTON_PIN, gpioState.toolboxButton ? "PRESSED" : "RELEASED");
    
    return true;
}

void setBedlight(bool state) {
    ArduinoInterface* hw = getArduinoInterface();
    
    if (gpioState.bedlight != state) {
        gpioState.bedlight = state;
        hw->digitalWrite(BEDLIGHT_PIN, state ? HIGH : LOW);
        LOG_INFO("Bedlight changed to: %s", state ? "ON" : "OFF");
    }
}

void setSystemReady(bool state) {
    ArduinoInterface* hw = getArduinoInterface();
    
    if (gpioState.systemReady != state) {
        gpioState.systemReady = state;
        hw->digitalWrite(SYSTEM_READY_PIN, state ? HIGH : LOW);
        LOG_INFO("System ready indicator changed to: %s", state ? "ON" : "OFF");
    }
}

void setToolboxOpener(bool state) {
    ArduinoInterface* hw = getArduinoInterface();
    unsigned long currentTime = hw->millis();
    
    if (state && !gpioState.toolboxOpener) {
        // Turning on toolbox opener
        gpioState.toolboxOpener = true;
        gpioState.toolboxOpenerStartTime = currentTime;
        hw->digitalWrite(TOOLBOX_OPENER_PIN, HIGH);
        LOG_INFO("Toolbox opener activated for %d ms", TOOLBOX_OPENER_DURATION_MS);
    } else if (!state && gpioState.toolboxOpener) {
        // Manually turning off toolbox opener
        gpioState.toolboxOpener = false;
        gpioState.toolboxOpenerStartTime = 0;
        hw->digitalWrite(TOOLBOX_OPENER_PIN, LOW);
        LOG_INFO("Toolbox opener deactivated (manual)");
    }
}

bool readToolboxButton() {
    ArduinoInterface* hw = getArduinoInterface();
    
    // Read button state (active low with pullup)
    bool currentButtonState = hw->digitalRead(TOOLBOX_BUTTON_PIN) == LOW;
    
    // Update stored state
    gpioState.toolboxButton = currentButtonState;
    
    return currentButtonState;
}

void updateToolboxOpenerTiming() {
    if (!gpioState.toolboxOpener) {
        return; // Not active, nothing to check
    }
    
    ArduinoInterface* hw = getArduinoInterface();
    unsigned long currentTime = hw->millis();
    unsigned long elapsed = currentTime - gpioState.toolboxOpenerStartTime;
    
    // Handle potential timer rollover (happens every ~50 days)
    if (currentTime < gpioState.toolboxOpenerStartTime) {
        elapsed = (ULONG_MAX - gpioState.toolboxOpenerStartTime) + currentTime + 1;
    }
    
    if (elapsed >= TOOLBOX_OPENER_DURATION_MS) {
        // Time's up, turn off the toolbox opener
        gpioState.toolboxOpener = false;
        gpioState.toolboxOpenerStartTime = 0;
        hw->digitalWrite(TOOLBOX_OPENER_PIN, LOW);
        LOG_INFO("Toolbox opener timed out after %lu ms", elapsed);
    }
}

GPIOState getGPIOState() {
    ArduinoInterface* hw = getArduinoInterface();
    
    // Update button state before returning
    gpioState.toolboxButton = hw->digitalRead(TOOLBOX_BUTTON_PIN) == LOW;
    return gpioState;
}

// Additional utility functions for debugging
void printGPIOStatus() {
    GPIOState state = getGPIOState();
    
    LOG_DEBUG("GPIO Status:");
    LOG_DEBUG("  Bedlight: %s", state.bedlight ? "ON" : "OFF");
    LOG_DEBUG("  System Ready: %s", state.systemReady ? "ON" : "OFF");
    LOG_DEBUG("  Toolbox Opener: %s", state.toolboxOpener ? "ACTIVE" : "INACTIVE");
    LOG_DEBUG("  Toolbox Button: %s", state.toolboxButton ? "PRESSED" : "RELEASED");
    
    if (state.toolboxOpener) {
        ArduinoInterface* hw = getArduinoInterface();
        unsigned long elapsed = hw->millis() - state.toolboxOpenerStartTime;
        LOG_DEBUG("  Toolbox Opener Time Remaining: %lu ms", 
                  TOOLBOX_OPENER_DURATION_MS > elapsed ? 
                  TOOLBOX_OPENER_DURATION_MS - elapsed : 0);
    }
}

} // extern "C"
