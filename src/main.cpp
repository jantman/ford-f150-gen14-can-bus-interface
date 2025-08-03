#include <Arduino.h>
#include "config.h"

// Module includes (will be created in subsequent steps)
// #include "can_manager.h"
#include "gpio_controller.h"
// #include "message_parser.h"
// #include "state_manager.h"
// #include "logger.h"

// Global variables for application state
bool systemInitialized = false;
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 10000; // 10 seconds

void setup() {
    // Initialize serial communication
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000); // Allow serial to stabilize
    
    LOG_INFO("=== Ford F150 Gen14 CAN Bus Interface ===");
    LOG_INFO("Firmware Version: %s", FIRMWARE_VERSION);
    LOG_INFO("Build Date: %s %s", BUILD_DATE, BUILD_TIME);
    LOG_INFO("Starting initialization...");
    
    // Initialize GPIO (Step 2)
    if (!initializeGPIO()) {
        LOG_ERROR("Failed to initialize GPIO pins");
        return;
    }
    LOG_INFO("GPIO initialization successful");
    
    // Initialize CAN bus (Step 3)
    // TODO: Initialize CAN bus
    LOG_INFO("TODO: Initialize CAN bus");
    
    // Initialize state management (Step 5)
    // TODO: Initialize state management
    LOG_INFO("TODO: Initialize state management");
    
    systemInitialized = true;
    LOG_INFO("System initialization complete");
    
    // Uncomment the line below to test all GPIO outputs on startup
    // testAllOutputs();
    
    // Print pin configuration for verification
    LOG_INFO("Pin Configuration:");
    LOG_INFO("  BEDLIGHT_PIN: %d", BEDLIGHT_PIN);
    LOG_INFO("  PARKED_LED_PIN: %d", PARKED_LED_PIN);
    LOG_INFO("  UNLOCKED_LED_PIN: %d", UNLOCKED_LED_PIN);
    LOG_INFO("  TOOLBOX_OPENER_PIN: %d", TOOLBOX_OPENER_PIN);
    LOG_INFO("  TOOLBOX_BUTTON_PIN: %d", TOOLBOX_BUTTON_PIN);
}

void loop() {
    if (!systemInitialized) {
        delay(100);
        return;
    }
    
    // Heartbeat message
    unsigned long currentTime = millis();
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        LOG_DEBUG("Heartbeat - System running, free heap: %d bytes", ESP.getFreeHeap());
        lastHeartbeat = currentTime;
    }
    
    // Process CAN messages (Step 3-4)
    // TODO: Process incoming CAN messages
    
    // Update state management (Step 5)
    // TODO: Check for state changes and update accordingly
    
    // Handle button input (Step 6)
    // TODO: Process toolbox button with debouncing
    
    // Update GPIO timing (toolbox opener auto-shutoff)
    updateToolboxOpenerTiming();
    
    // Update outputs (Step 7)
    // TODO: Update all output pins based on current state
    
    // Small delay to prevent overwhelming the CPU
    delay(10);
}

// Helper function to print system information
void printSystemInfo() {
    LOG_INFO("System Information:");
    LOG_INFO("  Chip Model: %s", ESP.getChipModel());
    LOG_INFO("  Chip Revision: %d", ESP.getChipRevision());
    LOG_INFO("  CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
    LOG_INFO("  Flash Size: %d bytes", ESP.getFlashChipSize());
    LOG_INFO("  Free Heap: %d bytes", ESP.getFreeHeap());
    LOG_INFO("  SDK Version: %s", ESP.getSdkVersion());
}
