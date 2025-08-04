#include <Arduino.h>
#include "config.h"

// Module includes (will be created in subsequent steps)
#include "can_manager.h"
#include "gpio_controller.h"
#include "message_parser.h"
#include "state_manager.h"
#include "diagnostic_commands.h"
// #include "logger.h"

// Global variables for application state
bool systemInitialized = false;
unsigned long lastHeartbeat = 0;
unsigned long lastCANStats = 0;
unsigned long lastOutputUpdate = 0;
unsigned long lastWatchdog = 0;
unsigned long lastErrorRecovery = 0;

// Error tracking and recovery
SystemHealth systemHealth = {0, 0, 0, 0, 0, false, false};
const unsigned long HEARTBEAT_INTERVAL = 10000; // 10 seconds
const unsigned long CAN_STATS_INTERVAL = 30000; // 30 seconds
const unsigned long OUTPUT_UPDATE_INTERVAL = 100; // 100ms for output updates
const unsigned long WATCHDOG_INTERVAL = 60000; // 60 seconds - system health check
const unsigned long ERROR_RECOVERY_INTERVAL = 5000; // 5 seconds between recovery attempts
const unsigned long CRITICAL_ERROR_THRESHOLD = 10; // Max critical errors before system reset

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
void performSystemWatchdog();
void handleErrorRecovery();
void performSafeSystemShutdown();
void printSystemInfo();

void setup() {
    // Initialize serial communication
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000); // Allow serial to stabilize
    
    LOG_INFO("=== Ford F150 Gen14 CAN Bus Interface ===");
    LOG_INFO("Project: https://github.com/jantman/ford-f150-gen14-can-bus-interface");
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
    
    systemInitialized = true;
    
    // Initialize system health tracking
    unsigned long currentTime = millis();
    systemHealth.lastCanActivity = currentTime;
    systemHealth.lastSystemOK = currentTime;
    
    LOG_INFO("System initialization complete");
    
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
    
    // Process serial diagnostic commands
    processSerialCommands();
    
    // Heartbeat message
    unsigned long currentTime = millis();
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        LOG_DEBUG("Heartbeat - System running, free heap: %d bytes", ESP.getFreeHeap());
        lastHeartbeat = currentTime;
    }
    
    // Periodic CAN statistics and diagnostics
    if (currentTime - lastCANStats >= CAN_STATS_INTERVAL) {
        if (isCANConnected()) {
            LOG_DEBUG("CAN bus status: Connected");
        } else {
            LOG_WARN("CAN bus status: Disconnected");
            // Print detailed diagnostics when disconnected
            printCANStatistics();
            
            // Enable debug message monitoring when having connection issues
            LOG_INFO("=== DEBUG: Attempting to receive ANY CAN messages ===");
            debugReceiveAllMessages();
        }
        lastCANStats = currentTime;
    }
    
    // Process CAN messages with error handling (Step 3-4, enhanced in Step 8)
    try {
        // Parse received target messages (Step 4)
        CANMessage message;
        unsigned int messagesProcessed = 0;
        const unsigned int MAX_MESSAGES_PER_LOOP = 10; // Prevent loop blocking
        
        while (receiveCANMessage(message) && messagesProcessed < MAX_MESSAGES_PER_LOOP) {
            messagesProcessed++;
            systemHealth.lastCanActivity = currentTime;
            
            if (isTargetCANMessage(message.id)) {
                bool parseSuccess = false;
                // Parse the message based on its ID
                switch (message.id) {
                    case BCM_LAMP_STAT_FD1_ID: {
                        BCMLampStatus lampStatus;
                        if (parseBCMLampStatus(message, lampStatus)) {
                            updateBCMLampState(lampStatus);
                            LOG_DEBUG("BCM Lamp Status updated: PudLamp=%d", lampStatus.pudLampRequest);
                            parseSuccess = true;
                        }
                        break;
                    }
                    case LOCKING_SYSTEMS_2_FD1_ID: {
                        LockingSystemsStatus lockStatus;
                        if (parseLockingSystemsStatus(message, lockStatus)) {
                            updateLockingSystemsState(lockStatus);
                            LOG_DEBUG("Lock Status updated: VehLock=%d", lockStatus.vehicleLockStatus);
                            parseSuccess = true;
                        }
                        break;
                    }
                    case POWERTRAIN_DATA_10_ID: {
                        PowertrainData powertrainData;
                        if (parsePowertrainData(message, powertrainData)) {
                            updatePowertrainState(powertrainData);
                            LOG_DEBUG("Powertrain Data updated: ParkStatus=%d", powertrainData.transmissionParkStatus);
                            parseSuccess = true;
                        }
                        break;
                    }
                    case BATTERY_MGMT_3_FD1_ID: {
                        BatteryManagement batteryData;
                        if (parseBatteryManagement(message, batteryData)) {
                            updateBatteryState(batteryData);
                            LOG_DEBUG("Battery Data updated: SOC=%d%%", batteryData.batterySOC);
                            parseSuccess = true;
                        }
                        break;
                    }
                }
                
                // Track parsing errors
                if (!parseSuccess) {
                    systemHealth.parseErrors++;
                    LOG_WARN("Failed to parse CAN message ID 0x%03X", message.id);
                }
            }
        }
        
        // If we hit the message limit, log it
        if (messagesProcessed >= MAX_MESSAGES_PER_LOOP) {
            LOG_DEBUG("Message processing limit reached (%d messages), continuing next loop", messagesProcessed);
        }
        
    } catch (...) {
        systemHealth.canErrors++;
        systemHealth.criticalErrors++;
        LOG_ERROR("Critical error in CAN message processing (count: %lu)", systemHealth.criticalErrors);
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
    
    // System watchdog and error recovery (Step 8)
    performSystemWatchdog();
    handleErrorRecovery();
    
    // Update system health
    if (isCANConnected() && getCurrentState().systemReady) {
        systemHealth.lastSystemOK = currentTime;
    }
    
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

// System Watchdog (Step 8) - Monitor system health and detect failures
void performSystemWatchdog() {
    unsigned long currentTime = millis();
    
    // Only run watchdog check at specified intervals
    if (currentTime - lastWatchdog < WATCHDOG_INTERVAL) {
        return;
    }
    lastWatchdog = currentTime;
    
    bool systemHealthy = true;
    
    // Check 1: CAN activity timeout
    if (currentTime - systemHealth.lastCanActivity > 30000) { // 30 seconds
        LOG_ERROR("Watchdog: No CAN activity for %lu ms", currentTime - systemHealth.lastCanActivity);
        systemHealthy = false;
    }
    
    // Check 2: System ready timeout
    if (currentTime - systemHealth.lastSystemOK > 60000) { // 60 seconds
        LOG_ERROR("Watchdog: System not ready for %lu ms", currentTime - systemHealth.lastSystemOK);
        systemHealthy = false;
    }
    
    // Check 3: Critical error count
    if (systemHealth.criticalErrors >= CRITICAL_ERROR_THRESHOLD) {
        LOG_ERROR("Watchdog: Critical error threshold exceeded (%lu errors)", systemHealth.criticalErrors);
        systemHealthy = false;
    }
    
    // Check 4: Memory health
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) { // Less than 10KB free
        LOG_ERROR("Watchdog: Low memory warning (%lu bytes free)", freeHeap);
        systemHealthy = false;
    }
    
    // Update watchdog status
    if (!systemHealthy && !systemHealth.watchdogTriggered) {
        systemHealth.watchdogTriggered = true;
        systemHealth.recoveryMode = true;
        LOG_ERROR("=== WATCHDOG TRIGGERED - System entering recovery mode ===");
        
        // Log detailed system state for debugging
        LOG_ERROR("System Health Report:");
        LOG_ERROR("  CAN Errors: %lu", systemHealth.canErrors);
        LOG_ERROR("  Parse Errors: %lu", systemHealth.parseErrors);
        LOG_ERROR("  Critical Errors: %lu", systemHealth.criticalErrors);
        LOG_ERROR("  Last CAN Activity: %lu ms ago", currentTime - systemHealth.lastCanActivity);
        LOG_ERROR("  Last System OK: %lu ms ago", currentTime - systemHealth.lastSystemOK);
        LOG_ERROR("  Free Heap: %lu bytes", freeHeap);
        
    } else if (systemHealthy && systemHealth.watchdogTriggered) {
        // System recovered
        systemHealth.watchdogTriggered = false;
        systemHealth.recoveryMode = false;
        LOG_INFO("=== WATCHDOG CLEARED - System recovery successful ===");
    }
    
    // Regular health status (when system is healthy)
    if (systemHealthy) {
        LOG_DEBUG("Watchdog: System healthy - CAN:%lu Parse:%lu Critical:%lu Heap:%lu", 
                  systemHealth.canErrors, systemHealth.parseErrors, 
                  systemHealth.criticalErrors, freeHeap);
    }
}

// Error Recovery (Step 8) - Attempt to recover from system errors
void handleErrorRecovery() {
    if (!systemHealth.recoveryMode) {
        return; // Only run recovery when needed
    }
    
    unsigned long currentTime = millis();
    
    // Throttle recovery attempts
    if (currentTime - lastErrorRecovery < ERROR_RECOVERY_INTERVAL) {
        return;
    }
    lastErrorRecovery = currentTime;
    
    LOG_INFO("Attempting system recovery...");
    
    // Recovery Step 1: Restart CAN bus if needed
    if (!isCANConnected()) {
        LOG_INFO("Recovery: Performing full CAN system recovery...");
        if (recoverCANSystem()) {
            LOG_INFO("Recovery: CAN system recovered successfully");
            systemHealth.canErrors = 0; // Reset CAN error count
        } else {
            LOG_ERROR("Recovery: CAN system recovery failed");
        }
    }
    
    // Recovery Step 2: Reset state timeouts
    LOG_INFO("Recovery: Resetting state timeouts...");
    resetStateTimeouts();
    
    // Recovery Step 3: Reset GPIO if needed
    GPIOState gpioState = getGPIOState();
    if (!gpioState.bedlight && !gpioState.parkedLED && !gpioState.unlockedLED) {
        LOG_INFO("Recovery: Reinitializing GPIO...");
        if (initializeGPIO()) {
            LOG_INFO("Recovery: GPIO reinitialized successfully");
        } else {
            LOG_ERROR("Recovery: GPIO reinitialization failed");
        }
    }
    
    // Recovery Step 4: Clear error counters if recovery successful
    if (isCANConnected()) {
        if (systemHealth.parseErrors > 0 || systemHealth.canErrors > 0) {
            LOG_INFO("Recovery: Clearing error counters (CAN:%lu Parse:%lu)", 
                     systemHealth.canErrors, systemHealth.parseErrors);
            systemHealth.parseErrors = 0;
            systemHealth.canErrors = 0;
        }
    }
    
    // Recovery Step 5: Last resort - safe shutdown if too many critical errors
    if (systemHealth.criticalErrors >= CRITICAL_ERROR_THRESHOLD * 2) {
        LOG_ERROR("Recovery: Critical error threshold exceeded, initiating safe shutdown");
        performSafeSystemShutdown();
    }
}

// Safe System Shutdown (Step 8) - Safely disable all outputs and enter minimal mode
void performSafeSystemShutdown() {
    LOG_ERROR("=== PERFORMING SAFE SYSTEM SHUTDOWN ===");
    
    // Turn off all outputs for safety
    setBedlight(false);
    setParkedLED(false);
    setUnlockedLED(false);
    setToolboxOpener(false);
    
    // Log final system state
    LOG_ERROR("All outputs disabled for safety");
    LOG_ERROR("System entering minimal operation mode");
    LOG_ERROR("Manual reset required to restore full functionality");
    
    // Set system to minimal operation mode
    systemInitialized = false;
    systemHealth.recoveryMode = false;
    
    // Flash parked LED as error indicator
    for (int i = 0; i < 10; i++) {
        setParkedLED(true);
        delay(200);
        setParkedLED(false);
        delay(200);
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
