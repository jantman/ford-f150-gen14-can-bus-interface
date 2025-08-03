#include <Arduino.h>
#include "config.h"

// Module includes (will be created in subsequent steps)
#include "can_manager.h"
#include "gpio_controller.h"
#include "message_parser.h"
#include "state_manager.h"
// #include "logger.h"

// Global variables for application state
bool systemInitialized = false;
unsigned long lastHeartbeat = 0;
unsigned long lastCANStats = 0;
unsigned long lastOutputUpdate = 0;
const unsigned long HEARTBEAT_INTERVAL = 10000; // 10 seconds
const unsigned long CAN_STATS_INTERVAL = 30000; // 30 seconds
const unsigned long OUTPUT_UPDATE_INTERVAL = 100; // 100ms for output updates

// Output control state tracking
struct OutputState {
    bool bedlightActive;
    bool parkedLEDActive;
    bool unlockedLEDActive;
    bool prevBedlightActive;
    bool prevParkedLEDActive;
    bool prevUnlockedLEDActive;
};
static OutputState outputState = {false, false, false, false, false, false};

// Function declarations
void updateOutputControlLogic();
void printSystemInfo();

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
    if (!initializeCAN()) {
        LOG_ERROR("Failed to initialize CAN bus");
        return;
    }
    LOG_INFO("CAN bus initialization successful");
    
    // Initialize state management (Step 5)
    initializeStateManager();
    LOG_INFO("State management initialization successful");
    
    // Test message parsing functions
    if (!testBitExtraction()) {
        LOG_ERROR("Bit extraction test failed - message parsing may not work correctly");
    }
    
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
    
    // Periodic CAN statistics
    if (currentTime - lastCANStats >= CAN_STATS_INTERVAL) {
        if (isCANConnected()) {
            LOG_DEBUG("CAN bus status: Connected");
        } else {
            LOG_WARN("CAN bus status: Disconnected");
        }
        lastCANStats = currentTime;
    }
    
    // Process CAN messages (Step 3-4)
    processPendingCANMessages();
    
    // Parse received target messages (Step 4)
    CANMessage message;
    while (receiveCANMessage(message)) {
        if (isTargetCANMessage(message.id)) {
            // Parse the message based on its ID
            switch (message.id) {
                case BCM_LAMP_STAT_FD1_ID: {
                    BCMLampStatus lampStatus;
                    if (parseBCMLampStatus(message, lampStatus)) {
                        updateBCMLampState(lampStatus);
                        LOG_DEBUG("BCM Lamp Status updated: PudLamp=%d", lampStatus.pudLampRequest);
                    }
                    break;
                }
                case LOCKING_SYSTEMS_2_FD1_ID: {
                    LockingSystemsStatus lockStatus;
                    if (parseLockingSystemsStatus(message, lockStatus)) {
                        updateLockingSystemsState(lockStatus);
                        LOG_DEBUG("Lock Status updated: VehLock=%d", lockStatus.vehicleLockStatus);
                    }
                    break;
                }
                case POWERTRAIN_DATA_10_ID: {
                    PowertrainData powertrainData;
                    if (parsePowertrainData(message, powertrainData)) {
                        updatePowertrainState(powertrainData);
                        LOG_DEBUG("Powertrain Data updated: ParkStatus=%d", powertrainData.transmissionParkStatus);
                    }
                    break;
                }
                case BATTERY_MGMT_3_FD1_ID: {
                    BatteryManagement batteryData;
                    if (parseBatteryManagement(message, batteryData)) {
                        updateBatteryState(batteryData);
                        LOG_DEBUG("Battery Data updated: SOC=%d%%", batteryData.batterySOC);
                    }
                    break;
                }
            }
        }
    }
    
    // Update state management (Step 5)
    checkForStateChanges();
    
    // Handle button input (Step 6) 
    updateButtonState();
    
    // Process button events with enhanced debouncing
    if (isButtonPressed()) {
        if (shouldActivateToolbox()) {
            LOG_INFO("Toolbox activation requested - conditions met, activating toolbox opener");
            setToolboxOpener(true);
        } else {
            LOG_WARN("Toolbox activation requested but conditions not met (not ready/parked/unlocked)");
        }
    }
    
    // Log long button holds (for diagnostics)
    if (isButtonHeld() && (getButtonHoldDuration() % 5000) == 0) {
        LOG_DEBUG("Button held for %lu ms", getButtonHoldDuration());
    }
    
    // Update GPIO timing (toolbox opener auto-shutoff)
    updateToolboxOpenerTiming();
    
    // Update outputs based on vehicle state (Step 7)
    updateOutputControlLogic();
    
    // Small delay to prevent overwhelming the CPU
    delay(10);
}

// Output Control Logic (Step 7) - Update all GPIO outputs based on vehicle state
void updateOutputControlLogic() {
    if (!systemInitialized) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Throttle output updates to avoid excessive GPIO operations
    if (currentTime - lastOutputUpdate < OUTPUT_UPDATE_INTERVAL) {
        return;
    }
    lastOutputUpdate = currentTime;
    
    // Get current vehicle state
    VehicleState vehicleState = getCurrentState();
    
    // Store previous output states for change detection
    outputState.prevBedlightActive = outputState.bedlightActive;
    outputState.prevParkedLEDActive = outputState.parkedLEDActive;
    outputState.prevUnlockedLEDActive = outputState.unlockedLEDActive;
    
    // === Bed Light Control Logic ===
    // Control bedlight based on PudLamp_D_Rq signal from BCM_Lamp_Stat_FD1
    if (vehicleState.systemReady) {
        outputState.bedlightActive = vehicleState.bedlightShouldBeOn;
    } else {
        // If system not ready, turn off bedlight for safety
        outputState.bedlightActive = false;
    }
    
    // === Parked LED Control Logic ===
    // Control parked LED based on TrnPrkSys_D_Actl signal from PowertrainData_10
    if (vehicleState.systemReady) {
        outputState.parkedLEDActive = vehicleState.isParked;
    } else {
        // If system not ready, turn off parked LED
        outputState.parkedLEDActive = false;
    }
    
    // === Unlocked LED Control Logic ===
    // Control unlocked LED based on Veh_Lock_Status signal from Locking_Systems_2_FD1
    if (vehicleState.systemReady) {
        outputState.unlockedLEDActive = vehicleState.isUnlocked;
    } else {
        // If system not ready, turn off unlocked LED
        outputState.unlockedLEDActive = false;
    }
    
    // === Apply GPIO Changes (only if state changed to minimize GPIO operations) ===
    
    // Update bedlight if state changed
    if (outputState.bedlightActive != outputState.prevBedlightActive) {
        setBedlight(outputState.bedlightActive);
        LOG_INFO("Bedlight %s (PudLamp state: %d)", 
                 outputState.bedlightActive ? "ON" : "OFF", 
                 vehicleState.pudLampRequest);
    }
    
    // Update parked LED if state changed
    if (outputState.parkedLEDActive != outputState.prevParkedLEDActive) {
        setParkedLED(outputState.parkedLEDActive);
        LOG_INFO("Parked LED %s (transmission park status: %d)", 
                 outputState.parkedLEDActive ? "ON" : "OFF", 
                 vehicleState.transmissionParkStatus);
    }
    
    // Update unlocked LED if state changed
    if (outputState.unlockedLEDActive != outputState.prevUnlockedLEDActive) {
        setUnlockedLED(outputState.unlockedLEDActive);
        LOG_INFO("Unlocked LED %s (vehicle lock status: %d)", 
                 outputState.unlockedLEDActive ? "ON" : "OFF", 
                 vehicleState.vehicleLockStatus);
    }
    
    // === Toolbox Opener Logic ===
    // Toolbox opener is handled by button press events in the main loop
    // The timing shutoff is handled by updateToolboxOpenerTiming()
    // No additional logic needed here as it's event-driven
    
    // Periodic status logging (every 30 seconds when outputs are active)
    static unsigned long lastStatusLog = 0;
    if (currentTime - lastStatusLog >= 30000) {
        if (outputState.bedlightActive || outputState.parkedLEDActive || outputState.unlockedLEDActive) {
            LOG_DEBUG("Output Status: Bedlight=%s, ParkedLED=%s, UnlockedLED=%s, System=%s",
                      outputState.bedlightActive ? "ON" : "OFF",
                      outputState.parkedLEDActive ? "ON" : "OFF", 
                      outputState.unlockedLEDActive ? "ON" : "OFF",
                      vehicleState.systemReady ? "READY" : "NOT_READY");
        }
        lastStatusLog = currentTime;
    }
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
