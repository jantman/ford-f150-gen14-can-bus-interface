#include "gpio_controller.h"

// Global GPIO state tracking
static GPIOState gpioState = {
    .bedlight = false,
    .parkedLED = false,
    .unlockedLED = false,
    .toolboxOpener = false,
    .toolboxButton = false,
    .systemReady = false,
    .toolboxOpenerStartTime = 0
};

bool initializeGPIO() {
    LOG_INFO("Initializing GPIO pins...");
    
    // Initialize output pins
    pinMode(BEDLIGHT_PIN, OUTPUT);
    pinMode(PARKED_LED_PIN, OUTPUT);
    pinMode(UNLOCKED_LED_PIN, OUTPUT);
    pinMode(TOOLBOX_OPENER_PIN, OUTPUT);
    pinMode(SYSTEM_READY_PIN, OUTPUT);
    
    // Initialize input pin with internal pullup
    pinMode(TOOLBOX_BUTTON_PIN, INPUT_PULLUP);
    
    // Set all outputs to known state (off)
    digitalWrite(BEDLIGHT_PIN, LOW);
    digitalWrite(PARKED_LED_PIN, LOW);
    digitalWrite(UNLOCKED_LED_PIN, LOW);
    digitalWrite(TOOLBOX_OPENER_PIN, LOW);
    digitalWrite(SYSTEM_READY_PIN, LOW);
    
    // Initialize state structure
    gpioState.bedlight = false;
    gpioState.parkedLED = false;
    gpioState.unlockedLED = false;
    gpioState.toolboxOpener = false;
    gpioState.systemReady = false;
    gpioState.toolboxButton = digitalRead(TOOLBOX_BUTTON_PIN) == LOW; // Active low with pullup
    gpioState.toolboxOpenerStartTime = 0;
    
    LOG_INFO("GPIO initialization complete");
    LOG_INFO("  BEDLIGHT_PIN (%d): OUTPUT, initial state: LOW", BEDLIGHT_PIN);
    LOG_INFO("  PARKED_LED_PIN (%d): OUTPUT, initial state: LOW", PARKED_LED_PIN);
    LOG_INFO("  UNLOCKED_LED_PIN (%d): OUTPUT, initial state: LOW", UNLOCKED_LED_PIN);
    LOG_INFO("  TOOLBOX_OPENER_PIN (%d): OUTPUT, initial state: LOW", TOOLBOX_OPENER_PIN);
    LOG_INFO("  SYSTEM_READY_PIN (%d): OUTPUT, initial state: LOW", SYSTEM_READY_PIN);
    LOG_INFO("  TOOLBOX_BUTTON_PIN (%d): INPUT_PULLUP, current state: %s", 
             TOOLBOX_BUTTON_PIN, gpioState.toolboxButton ? "PRESSED" : "RELEASED");
    
    return true;
}

void setBedlight(bool state) {
    if (gpioState.bedlight != state) {
        gpioState.bedlight = state;
        digitalWrite(BEDLIGHT_PIN, state ? HIGH : LOW);
        LOG_INFO("Bedlight changed to: %s", state ? "ON" : "OFF");
    }
}

void setParkedLED(bool state) {
    if (gpioState.parkedLED != state) {
        gpioState.parkedLED = state;
        digitalWrite(PARKED_LED_PIN, state ? HIGH : LOW);
        LOG_INFO("Parked LED changed to: %s", state ? "ON" : "OFF");
    }
}

void setUnlockedLED(bool state) {
    if (gpioState.unlockedLED != state) {
        gpioState.unlockedLED = state;
        digitalWrite(UNLOCKED_LED_PIN, state ? HIGH : LOW);
        LOG_INFO("Unlocked LED changed to: %s", state ? "ON" : "OFF");
    }
}

void setSystemReady(bool state) {
    if (gpioState.systemReady != state) {
        gpioState.systemReady = state;
        digitalWrite(SYSTEM_READY_PIN, state ? HIGH : LOW);
        LOG_INFO("System ready indicator changed to: %s", state ? "ON" : "OFF");
    }
}

void setToolboxOpener(bool state) {
    unsigned long currentTime = millis();
    
    if (state && !gpioState.toolboxOpener) {
        // Turning on toolbox opener
        gpioState.toolboxOpener = true;
        gpioState.toolboxOpenerStartTime = currentTime;
        digitalWrite(TOOLBOX_OPENER_PIN, HIGH);
        LOG_INFO("Toolbox opener activated for %d ms", TOOLBOX_OPENER_DURATION_MS);
    } else if (!state && gpioState.toolboxOpener) {
        // Manually turning off toolbox opener
        gpioState.toolboxOpener = false;
        gpioState.toolboxOpenerStartTime = 0;
        digitalWrite(TOOLBOX_OPENER_PIN, LOW);
        LOG_INFO("Toolbox opener deactivated (manual)");
    }
}

bool readToolboxButton() {
    // Read button state (active low with pullup)
    bool currentButtonState = digitalRead(TOOLBOX_BUTTON_PIN) == LOW;
    
    // Update stored state
    gpioState.toolboxButton = currentButtonState;
    
    return currentButtonState;
}

void updateToolboxOpenerTiming() {
    if (!gpioState.toolboxOpener) {
        return; // Not active, nothing to check
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - gpioState.toolboxOpenerStartTime;
    
    // Handle potential timer rollover (happens every ~50 days)
    if (currentTime < gpioState.toolboxOpenerStartTime) {
        elapsed = (ULONG_MAX - gpioState.toolboxOpenerStartTime) + currentTime + 1;
    }
    
    if (elapsed >= TOOLBOX_OPENER_DURATION_MS) {
        // Time's up, turn off the toolbox opener
        gpioState.toolboxOpener = false;
        gpioState.toolboxOpenerStartTime = 0;
        digitalWrite(TOOLBOX_OPENER_PIN, LOW);
        LOG_INFO("Toolbox opener timed out after %lu ms", elapsed);
    }
}

GPIOState getGPIOState() {
    // Update button state before returning
    gpioState.toolboxButton = digitalRead(TOOLBOX_BUTTON_PIN) == LOW;
    return gpioState;
}

// Additional utility functions for debugging
void printGPIOStatus() {
    GPIOState state = getGPIOState();
    
    LOG_DEBUG("GPIO Status:");
    LOG_DEBUG("  Bedlight: %s", state.bedlight ? "ON" : "OFF");
    LOG_DEBUG("  Parked LED: %s", state.parkedLED ? "ON" : "OFF");
    LOG_DEBUG("  Unlocked LED: %s", state.unlockedLED ? "ON" : "OFF");
    LOG_DEBUG("  System Ready: %s", state.systemReady ? "ON" : "OFF");
    LOG_DEBUG("  Toolbox Opener: %s", state.toolboxOpener ? "ACTIVE" : "INACTIVE");
    LOG_DEBUG("  Toolbox Button: %s", state.toolboxButton ? "PRESSED" : "RELEASED");
    
    if (state.toolboxOpener) {
        unsigned long elapsed = millis() - state.toolboxOpenerStartTime;
        LOG_DEBUG("  Toolbox Opener Time Remaining: %lu ms", 
                  TOOLBOX_OPENER_DURATION_MS > elapsed ? 
                  TOOLBOX_OPENER_DURATION_MS - elapsed : 0);
    }
}
