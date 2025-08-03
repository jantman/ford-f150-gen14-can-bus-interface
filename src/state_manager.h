#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "message_parser.h"

// Vehicle state structure
struct VehicleState {
    // Current signal values
    uint8_t pudLampRequest;
    uint8_t vehicleLockStatus;
    uint8_t transmissionParkStatus;
    uint8_t batterySOC;
    
    // Previous values for change detection
    uint8_t prevPudLampRequest;
    uint8_t prevVehicleLockStatus;
    uint8_t prevTransmissionParkStatus;
    uint8_t prevBatterySOC;
    
    // Timestamps
    unsigned long lastBCMLampUpdate;
    unsigned long lastLockingSystemsUpdate;
    unsigned long lastPowertrainUpdate;
    unsigned long lastBatteryUpdate;
    
    // State flags
    bool isUnlocked;
    bool isParked;
    bool bedlightShouldBeOn;
    bool systemReady;
};

// Button state structure
struct ButtonState {
    bool currentState;
    bool previousState;
    bool pressed;
    unsigned long lastChangeTime;
    unsigned long lastPressTime;
};

// Function declarations (to be implemented in Step 5)
void initializeStateManager();
void updateBCMLampState(const BCMLampStatus& status);
void updateLockingSystemsState(const LockingSystemsStatus& status);
void updatePowertrainState(const PowertrainData& data);
void updateBatteryState(const BatteryManagement& data);
void checkForStateChanges();
bool shouldActivateToolbox();
VehicleState getCurrentState();
void resetStateTimeouts();

// Function declarations for button handling (Step 6)
void updateButtonState();
bool isButtonPressed();
ButtonState getButtonState();

#endif // STATE_MANAGER_H
