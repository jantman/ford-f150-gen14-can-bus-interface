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
    bool currentState;              // Current debounced button state
    bool previousState;             // Previous debounced button state
    bool rawState;                  // Raw button reading (before debouncing)
    bool pressed;                   // Flag: button was pressed (cleared after reading)
    bool released;                  // Flag: button was released (cleared after reading)
    bool isHeld;                    // Flag: button is currently being held
    unsigned long lastChangeTime;   // Time of last state change
    unsigned long lastPressTime;    // Time of last press event
    unsigned long lastReleaseTime;  // Time of last release event
    unsigned long pressCount;       // Total number of button presses
    unsigned long holdDuration;     // Current hold duration (if held)
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
bool isButtonReleased();
bool isButtonHeld();
unsigned long getButtonHoldDuration();
unsigned long getButtonPressCount();
void resetButtonPressCount();
ButtonState getButtonState();

#endif // STATE_MANAGER_H
