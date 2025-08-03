#include "state_manager.h"

// Global state variables
static VehicleState vehicleState;
static ButtonState buttonState;
static bool stateManagerInitialized = false;

// Initialize the state manager
void initializeStateManager() {
    LOG_INFO("Initializing State Manager...");
    
    // Initialize vehicle state with default values
    vehicleState.pudLampRequest = PUDLAMP_OFF;
    vehicleState.vehicleLockStatus = VEH_LOCK_UNKNOWN;
    vehicleState.transmissionParkStatus = TRNPRKSTS_UNKNOWN;
    vehicleState.batterySOC = 0;
    
    // Initialize previous values
    vehicleState.prevPudLampRequest = PUDLAMP_OFF;
    vehicleState.prevVehicleLockStatus = VEH_LOCK_UNKNOWN;
    vehicleState.prevTransmissionParkStatus = TRNPRKSTS_UNKNOWN;
    vehicleState.prevBatterySOC = 0;
    
    // Initialize timestamps
    unsigned long currentTime = millis();
    vehicleState.lastBCMLampUpdate = 0;
    vehicleState.lastLockingSystemsUpdate = 0;
    vehicleState.lastPowertrainUpdate = 0;
    vehicleState.lastBatteryUpdate = 0;
    
    // Initialize state flags
    vehicleState.isUnlocked = false;
    vehicleState.isParked = false;
    vehicleState.bedlightShouldBeOn = false;
    vehicleState.systemReady = false;
    
    // Initialize button state
    buttonState.currentState = false;
    buttonState.previousState = false;
    buttonState.pressed = false;
    buttonState.lastChangeTime = currentTime;
    buttonState.lastPressTime = 0;
    
    stateManagerInitialized = true;
    LOG_INFO("State Manager initialized successfully");
}

// Update BCM lamp state and detect changes
void updateBCMLampState(const BCMLampStatus& status) {
    if (!stateManagerInitialized) {
        LOG_WARN("State manager not initialized");
        return;
    }
    
    if (!status.valid) {
        LOG_WARN("Invalid BCM lamp status received");
        return;
    }
    
    // Store previous value
    vehicleState.prevPudLampRequest = vehicleState.pudLampRequest;
    
    // Update current value
    vehicleState.pudLampRequest = status.pudLampRequest;
    vehicleState.lastBCMLampUpdate = millis();
    
    // Update derived state
    vehicleState.bedlightShouldBeOn = (vehicleState.pudLampRequest == PUDLAMP_ON || 
                                      vehicleState.pudLampRequest == PUDLAMP_RAMP_UP);
    
    // Log state changes
    if (vehicleState.prevPudLampRequest != vehicleState.pudLampRequest) {
        const char* stateNames[] = {"OFF", "ON", "RAMP_UP", "RAMP_DOWN"};
        const char* prevName = (vehicleState.prevPudLampRequest < 4) ? stateNames[vehicleState.prevPudLampRequest] : "UNKNOWN";
        const char* currName = (vehicleState.pudLampRequest < 4) ? stateNames[vehicleState.pudLampRequest] : "UNKNOWN";
        
        LOG_INFO("PudLamp state changed: %s -> %s (bedlight should be %s)", 
                 prevName, currName, vehicleState.bedlightShouldBeOn ? "ON" : "OFF");
    }
}

// Update locking systems state and detect changes
void updateLockingSystemsState(const LockingSystemsStatus& status) {
    if (!stateManagerInitialized) {
        LOG_WARN("State manager not initialized");
        return;
    }
    
    if (!status.valid) {
        LOG_WARN("Invalid locking systems status received");
        return;
    }
    
    // Store previous value
    vehicleState.prevVehicleLockStatus = vehicleState.vehicleLockStatus;
    
    // Update current value
    vehicleState.vehicleLockStatus = status.vehicleLockStatus;
    vehicleState.lastLockingSystemsUpdate = millis();
    
    // Update derived state
    vehicleState.isUnlocked = (vehicleState.vehicleLockStatus == VEH_UNLOCK_ALL || 
                               vehicleState.vehicleLockStatus == VEH_UNLOCK_DRV);
    
    // Log state changes
    if (vehicleState.prevVehicleLockStatus != vehicleState.vehicleLockStatus) {
        const char* stateNames[] = {"LOCK_DBL", "LOCK_ALL", "UNLOCK_ALL", "UNLOCK_DRV"};
        const char* prevName = (vehicleState.prevVehicleLockStatus < 4) ? stateNames[vehicleState.prevVehicleLockStatus] : "UNKNOWN";
        const char* currName = (vehicleState.vehicleLockStatus < 4) ? stateNames[vehicleState.vehicleLockStatus] : "UNKNOWN";
        
        LOG_INFO("Vehicle lock state changed: %s -> %s (unlocked: %s)", 
                 prevName, currName, vehicleState.isUnlocked ? "YES" : "NO");
    }
}

// Update powertrain state and detect changes
void updatePowertrainState(const PowertrainData& data) {
    if (!stateManagerInitialized) {
        LOG_WARN("State manager not initialized");
        return;
    }
    
    if (!data.valid) {
        LOG_WARN("Invalid powertrain data received");
        return;
    }
    
    // Store previous value
    vehicleState.prevTransmissionParkStatus = vehicleState.transmissionParkStatus;
    
    // Update current value
    vehicleState.transmissionParkStatus = data.transmissionParkStatus;
    vehicleState.lastPowertrainUpdate = millis();
    
    // Update derived state
    vehicleState.isParked = (vehicleState.transmissionParkStatus == TRNPRKSTS_PARK);
    
    // Log state changes
    if (vehicleState.prevTransmissionParkStatus != vehicleState.transmissionParkStatus) {
        const char* stateNames[] = {"UNKNOWN", "PARK", "REVERSE", "NEUTRAL", "DRIVE", "SPORT", "LOW"};
        const char* prevName = (vehicleState.prevTransmissionParkStatus < 7) ? stateNames[vehicleState.prevTransmissionParkStatus] : "INVALID";
        const char* currName = (vehicleState.transmissionParkStatus < 7) ? stateNames[vehicleState.transmissionParkStatus] : "INVALID";
        
        LOG_INFO("Transmission park state changed: %s -> %s (parked: %s)", 
                 prevName, currName, vehicleState.isParked ? "YES" : "NO");
    }
}

// Update battery state and detect changes
void updateBatteryState(const BatteryManagement& data) {
    if (!stateManagerInitialized) {
        LOG_WARN("State manager not initialized");
        return;
    }
    
    if (!data.valid) {
        LOG_WARN("Invalid battery management data received");
        return;
    }
    
    // Store previous value
    vehicleState.prevBatterySOC = vehicleState.batterySOC;
    
    // Update current value
    vehicleState.batterySOC = data.batterySOC;
    vehicleState.lastBatteryUpdate = millis();
    
    // Log significant battery changes (>5% change)
    if (abs((int)vehicleState.prevBatterySOC - (int)vehicleState.batterySOC) >= 5) {
        LOG_INFO("Battery SOC changed significantly: %d%% -> %d%%", 
                 vehicleState.prevBatterySOC, vehicleState.batterySOC);
    }
}

// Check for state changes and system health
void checkForStateChanges() {
    if (!stateManagerInitialized) {
        return;
    }
    
    unsigned long currentTime = millis();
    bool systemWasReady = vehicleState.systemReady;
    
    // Check if we have recent data from all critical systems
    bool hasBCMData = (currentTime - vehicleState.lastBCMLampUpdate) < STATE_TIMEOUT_MS;
    bool hasLockingData = (currentTime - vehicleState.lastLockingSystemsUpdate) < STATE_TIMEOUT_MS;
    bool hasPowertrainData = (currentTime - vehicleState.lastPowertrainUpdate) < STATE_TIMEOUT_MS;
    bool hasBatteryData = (currentTime - vehicleState.lastBatteryUpdate) < STATE_TIMEOUT_MS;
    
    // System is ready if we have recent data from at least the critical systems
    vehicleState.systemReady = hasBCMData && hasLockingData && hasPowertrainData;
    
    // Log system readiness changes
    if (systemWasReady != vehicleState.systemReady) {
        LOG_INFO("System readiness changed: %s (BCM:%s, Lock:%s, PT:%s, Batt:%s)",
                 vehicleState.systemReady ? "READY" : "NOT_READY",
                 hasBCMData ? "OK" : "TIMEOUT",
                 hasLockingData ? "OK" : "TIMEOUT",
                 hasPowertrainData ? "OK" : "TIMEOUT",
                 hasBatteryData ? "OK" : "TIMEOUT");
    }
    
    // Log warnings for data timeouts
    if (!hasBCMData && (currentTime % 30000) < 100) { // Log every 30s
        LOG_WARN("BCM lamp data timeout (last update %lu ms ago)", currentTime - vehicleState.lastBCMLampUpdate);
    }
    if (!hasLockingData && (currentTime % 30000) < 100) {
        LOG_WARN("Locking systems data timeout (last update %lu ms ago)", currentTime - vehicleState.lastLockingSystemsUpdate);
    }
    if (!hasPowertrainData && (currentTime % 30000) < 100) {
        LOG_WARN("Powertrain data timeout (last update %lu ms ago)", currentTime - vehicleState.lastPowertrainUpdate);
    }
}

// Determine if toolbox should be activated
bool shouldActivateToolbox() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    // Toolbox conditions:
    // 1. System must be ready
    // 2. Vehicle must be parked
    // 3. Vehicle must be unlocked
    // 4. Button must be pressed (checked separately)
    
    bool conditions = vehicleState.systemReady && 
                     vehicleState.isParked && 
                     vehicleState.isUnlocked;
    
    LOG_DEBUG("Toolbox activation conditions: ready=%s, parked=%s, unlocked=%s -> %s",
              vehicleState.systemReady ? "YES" : "NO",
              vehicleState.isParked ? "YES" : "NO", 
              vehicleState.isUnlocked ? "YES" : "NO",
              conditions ? "ALLOW" : "DENY");
    
    return conditions;
}

// Get current vehicle state (read-only copy)
VehicleState getCurrentState() {
    return vehicleState;
}

// Reset state timeouts (useful for testing)
void resetStateTimeouts() {
    if (!stateManagerInitialized) {
        return;
    }
    
    unsigned long currentTime = millis();
    vehicleState.lastBCMLampUpdate = currentTime;
    vehicleState.lastLockingSystemsUpdate = currentTime;
    vehicleState.lastPowertrainUpdate = currentTime;
    vehicleState.lastBatteryUpdate = currentTime;
    
    LOG_INFO("State timeouts reset");
}

// Update button state (Step 6 functionality - basic implementation for now)
void updateButtonState() {
    if (!stateManagerInitialized) {
        return;
    }
    
    // Read current button state (active low with pullup)
    bool currentReading = !digitalRead(TOOLBOX_BUTTON_PIN);
    unsigned long currentTime = millis();
    
    // Store previous state
    buttonState.previousState = buttonState.currentState;
    
    // Simple debouncing - require stable state for BUTTON_DEBOUNCE_MS
    if (currentReading != buttonState.currentState) {
        if ((currentTime - buttonState.lastChangeTime) > BUTTON_DEBOUNCE_MS) {
            buttonState.currentState = currentReading;
            buttonState.lastChangeTime = currentTime;
            
            // Detect press event (transition from false to true)
            if (buttonState.currentState && !buttonState.previousState) {
                buttonState.pressed = true;
                buttonState.lastPressTime = currentTime;
                LOG_INFO("Toolbox button pressed");
            }
        }
    }
}

// Check if button was pressed (and clear the flag)
bool isButtonPressed() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    bool wasPressed = buttonState.pressed;
    buttonState.pressed = false; // Clear flag after reading
    return wasPressed;
}

// Get button state (read-only copy)
ButtonState getButtonState() {
    return buttonState;
}
