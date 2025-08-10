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
    vehicleState.transmissionParkStatus = TRNPRKSTS_PARK;  // Default to PARK if POWERTRAIN_DATA_10 not seen yet
    vehicleState.batterySOC = 0;
    
    // Initialize previous values
    vehicleState.prevPudLampRequest = PUDLAMP_OFF;
    vehicleState.prevVehicleLockStatus = VEH_LOCK_UNKNOWN;
    vehicleState.prevTransmissionParkStatus = TRNPRKSTS_PARK;  // Default to PARK if POWERTRAIN_DATA_10 not seen yet
    vehicleState.prevBatterySOC = 0;
    
    // Initialize timestamps
    unsigned long currentTime = millis();
    vehicleState.lastBCMLampUpdate = 0;
    vehicleState.lastLockingSystemsUpdate = 0;
    vehicleState.lastPowertrainUpdate = 0;
    vehicleState.lastBatteryUpdate = 0;
    
    // Initialize state flags
    vehicleState.isUnlocked = false;
    vehicleState.isParked = true;  // Default to parked if POWERTRAIN_DATA_10 not seen yet
    vehicleState.bedlightShouldBeOn = false;
    vehicleState.systemReady = false;
    
    // Initialize manual bed light control
    vehicleState.bedlightManualOverride = false;
    vehicleState.bedlightManualState = false;
    
    // Initialize button state
    buttonState.currentState = false;
    buttonState.previousState = false;
    buttonState.rawState = false;
    buttonState.pressed = false;
    buttonState.released = false;
    buttonState.isHeld = false;
    buttonState.lastChangeTime = currentTime;
    buttonState.lastPressTime = 0;
    buttonState.lastReleaseTime = 0;
    buttonState.pressCount = 0;
    buttonState.holdDuration = 0;
    
    // Initialize double-click detection
    buttonState.doubleClickDetected = false;
    buttonState.secondToLastPressTime = 0;
    
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
    
    // Clear manual bed light override when vehicle is locked
    if (!vehicleState.isUnlocked && vehicleState.bedlightManualOverride) {
        vehicleState.bedlightManualOverride = false;
        vehicleState.bedlightManualState = false;
        LOG_INFO("Bed light manual override cleared due to vehicle lock");
    }
    
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
    
    // Check if we have recent data from ANY of the monitored systems
    bool hasBCMData = (currentTime - vehicleState.lastBCMLampUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
    bool hasLockingData = (currentTime - vehicleState.lastLockingSystemsUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
    bool hasPowertrainData = (currentTime - vehicleState.lastPowertrainUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
    bool hasBatteryData = (currentTime - vehicleState.lastBatteryUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
    
    // System is ready if we have recent data from ANY of the monitored systems
    vehicleState.systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
    
    // Log system readiness changes
    if (systemWasReady != vehicleState.systemReady) {
        LOG_INFO("System readiness changed: %s (BCM:%s, Lock:%s, PT:%s, Batt:%s)",
                 vehicleState.systemReady ? "READY" : "NOT_READY",
                 hasBCMData ? "OK" : "TIMEOUT",
                 hasLockingData ? "OK" : "TIMEOUT",
                 hasPowertrainData ? "OK" : "TIMEOUT",
                 hasBatteryData ? "OK" : "TIMEOUT");
    }
    
    // Log warnings for data timeouts (only if ALL systems are timed out and system is not ready)
    if (!vehicleState.systemReady) {
        if (!hasBCMData && (currentTime % 30000) < 100) { // Log every 30s
            LOG_WARN("BCM lamp data timeout (last update %lu ms ago)", currentTime - vehicleState.lastBCMLampUpdate);
        }
        if (!hasLockingData && (currentTime % 30000) < 100) {
            LOG_WARN("Locking systems data timeout (last update %lu ms ago)", currentTime - vehicleState.lastLockingSystemsUpdate);
        }
        if (!hasPowertrainData && (currentTime % 30000) < 100) {
            LOG_WARN("Powertrain data timeout (last update %lu ms ago)", currentTime - vehicleState.lastPowertrainUpdate);
        }
        if (!hasBatteryData && (currentTime % 30000) < 100) {
            LOG_WARN("Battery data timeout (last update %lu ms ago)", currentTime - vehicleState.lastBatteryUpdate);
        }
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

// Update button state (Step 6 functionality - enhanced debouncing)
void updateButtonState() {
    if (!stateManagerInitialized) {
        return;
    }
    
    // Read raw button state (active low with pullup)
    bool currentRawReading = !digitalRead(TOOLBOX_BUTTON_PIN);
    unsigned long currentTime = millis();
    
    // Store raw state
    buttonState.rawState = currentRawReading;
    
    // Store previous debounced state
    buttonState.previousState = buttonState.currentState;
    
    // Debouncing logic: require stable state for BUTTON_DEBOUNCE_MS
    if (currentRawReading != buttonState.currentState) {
        // State change detected, check if it's been stable long enough
        if ((currentTime - buttonState.lastChangeTime) >= BUTTON_DEBOUNCE_MS) {
            // State change confirmed, update debounced state
            buttonState.currentState = currentRawReading;
            buttonState.lastChangeTime = currentTime;
            
            // Detect press event (transition from false to true)
            if (buttonState.currentState && !buttonState.previousState) {
                buttonState.pressed = true;
                
                // Check for double-click: two presses within BUTTON_DOUBLE_CLICK_MS, but not held
                unsigned long timeSinceLastPress = currentTime - buttonState.lastPressTime;
                if (buttonState.pressCount > 0 && 
                    timeSinceLastPress <= BUTTON_DOUBLE_CLICK_MS && 
                    timeSinceLastPress > BUTTON_DEBOUNCE_MS) {
                    
                    buttonState.doubleClickDetected = true;
                    LOG_INFO("Toolbox button double-clicked (%lu ms between presses)", timeSinceLastPress);
                }
                
                // Update press timing tracking
                buttonState.secondToLastPressTime = buttonState.lastPressTime;
                buttonState.lastPressTime = currentTime;
                buttonState.pressCount++;
                buttonState.holdDuration = 0;
                
                LOG_INFO("Toolbox button pressed (count: %lu)", buttonState.pressCount);
            }
            
            // Detect release event (transition from true to false)
            if (!buttonState.currentState && buttonState.previousState) {
                buttonState.released = true;
                buttonState.lastReleaseTime = currentTime;
                buttonState.isHeld = false;
                buttonState.holdDuration = 0;
                
                unsigned long pressDuration = currentTime - buttonState.lastPressTime;
                LOG_INFO("Toolbox button released (held for %lu ms)", pressDuration);
            }
        }
    } else {
        // No state change, reset the debounce timer
        buttonState.lastChangeTime = currentTime;
    }
    
    // Update hold status and duration
    if (buttonState.currentState) {
        buttonState.holdDuration = currentTime - buttonState.lastPressTime;
        
        // Check if button is being held
        if (buttonState.holdDuration >= BUTTON_HOLD_THRESHOLD_MS && !buttonState.isHeld) {
            buttonState.isHeld = true;
            LOG_INFO("Toolbox button is being held (%lu ms)", buttonState.holdDuration);
        }
    } else {
        buttonState.holdDuration = 0;
        buttonState.isHeld = false;
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

// Check if button was released (and clear the flag)
bool isButtonReleased() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    bool wasReleased = buttonState.released;
    buttonState.released = false; // Clear flag after reading
    return wasReleased;
}

// Check if button is currently being held
bool isButtonHeld() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    return buttonState.isHeld;
}

// Get current button hold duration in milliseconds
unsigned long getButtonHoldDuration() {
    if (!stateManagerInitialized) {
        return 0;
    }
    
    return buttonState.holdDuration;
}

// Get total button press count
unsigned long getButtonPressCount() {
    if (!stateManagerInitialized) {
        return 0;
    }
    
    return buttonState.pressCount;
}

// Reset button press count (useful for diagnostics)
void resetButtonPressCount() {
    if (!stateManagerInitialized) {
        return;
    }
    
    LOG_INFO("Button press count reset (was %lu)", buttonState.pressCount);
    buttonState.pressCount = 0;
}

// Get button state (read-only copy)
ButtonState getButtonState() {
    return buttonState;
}

// Check if button was double-clicked (and clear the flag)
bool isButtonDoubleClicked() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    bool wasDoubleClicked = buttonState.doubleClickDetected;
    buttonState.doubleClickDetected = false; // Clear flag after reading
    return wasDoubleClicked;
}

// Check if button input should be processed (only when vehicle is unlocked for security)
bool shouldProcessButtonInput() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    // For security reasons, only process button input when the vehicle is unlocked
    return vehicleState.isUnlocked;
}

// Toggle bed light manual override
void toggleBedlightManualOverride() {
    if (!stateManagerInitialized) {
        return;
    }
    
    if (vehicleState.bedlightManualOverride) {
        // Currently in manual mode, toggle the manual state
        vehicleState.bedlightManualState = !vehicleState.bedlightManualState;
        LOG_INFO("Bed light manual override toggled: %s", 
                 vehicleState.bedlightManualState ? "ON" : "OFF");
    } else {
        // Not in manual mode, enter manual mode and set to opposite of current automatic state
        vehicleState.bedlightManualOverride = true;
        vehicleState.bedlightManualState = !vehicleState.bedlightShouldBeOn;
        LOG_INFO("Bed light manual override activated: %s (was automatic %s)", 
                 vehicleState.bedlightManualState ? "ON" : "OFF",
                 vehicleState.bedlightShouldBeOn ? "ON" : "OFF");
    }
}

// Check if bed light is manually overridden
bool isBedlightManuallyOverridden() {
    if (!stateManagerInitialized) {
        return false;
    }
    
    return vehicleState.bedlightManualOverride;
}

// Clear bed light manual override (return to automatic mode)
void clearBedlightManualOverride() {
    if (!stateManagerInitialized) {
        return;
    }
    
    if (vehicleState.bedlightManualOverride) {
        vehicleState.bedlightManualOverride = false;
        vehicleState.bedlightManualState = false;
        LOG_INFO("Bed light manual override cleared, returning to automatic mode");
    }
}
