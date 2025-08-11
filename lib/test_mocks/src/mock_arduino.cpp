#include "mock_arduino.h"
#include "../../../src/config.h"

// Define the mock Serial instance
MockSerial Serial;

// Mock GPIO state for testing
static struct {
    bool bedlight;
    bool toolboxOpener;
    bool toolboxButton;
    bool systemReady;
    unsigned long toolboxOpenerStartTime;
} mockGPIOState = {false, false, false, false, 0};

// Mock GPIO controller functions
class ArduinoInterface; // Forward declaration

extern "C" {
    bool initializeGPIO() { return true; }
    
    // Decision logic utility functions with C linkage for test compatibility
    bool shouldEnableBedlight(uint8_t pudLampRequest) {
        return (pudLampRequest == 1 || pudLampRequest == 2);  // PUDLAMP_ON or PUDLAMP_RAMP_UP
    }

    bool isVehicleUnlocked(uint8_t vehicleLockStatus) {
        return (vehicleLockStatus == 2 || vehicleLockStatus == 3);  // VEH_UNLOCK_ALL or VEH_UNLOCK_DRV
    }

    bool isVehicleParked(uint8_t transmissionParkStatus) {
        return (transmissionParkStatus == 1);  // TRNPRKSTS_PARK
    }

    bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked) {
        return systemReady && isParked && isUnlocked;
    }

    bool shouldActivateToolbox() {
        // For testing, use default behavior - could be enhanced to use mock state
        return shouldActivateToolboxWithParams(true, true, true);
    }
    
    // Stub implementation of isTargetCANMessage from production code
    // This avoids the need to include can_manager.cpp with its SPI dependencies
    bool isTargetCANMessage(uint32_t messageId) {
        return (messageId == BCM_LAMP_STAT_FD1_ID ||
                messageId == LOCKING_SYSTEMS_2_FD1_ID ||
                messageId == POWERTRAIN_DATA_10_ID ||
                messageId == BATTERY_MGMT_3_FD1_ID);
    }
    
    void setBedlight(bool state) {
        mockGPIOState.bedlight = state;
    }
    
    void setToolboxOpener(bool state) {
        mockGPIOState.toolboxOpener = state;
        if (state) {
            mockGPIOState.toolboxOpenerStartTime = ArduinoMock::instance().getMillis();
        }
    }
    
    void setSystemReady(bool state) {
        mockGPIOState.systemReady = state;
    }
    
    bool readToolboxButton() {
        return mockGPIOState.toolboxButton;
    }
    
    void updateToolboxOpenerTiming() {
        // Auto-shutoff after 1 second
        if (mockGPIOState.toolboxOpener && 
            (ArduinoMock::instance().getMillis() - mockGPIOState.toolboxOpenerStartTime) >= 1000) {
            mockGPIOState.toolboxOpener = false;
        }
    }
    
    // Return the mock GPIO state
    struct GPIOState {
        bool bedlight;
        bool toolboxOpener;
        bool toolboxButton;
        bool systemReady;
        unsigned long toolboxOpenerStartTime;
    };
    
    struct GPIOState getGPIOState() {
        struct GPIOState state = {
            mockGPIOState.bedlight,
            mockGPIOState.toolboxOpener,
            mockGPIOState.toolboxButton,
            mockGPIOState.systemReady,
            mockGPIOState.toolboxOpenerStartTime
        };
        return state;
    }
}

// C++ functions
bool initializeGPIOWithInterface(ArduinoInterface* interface) {
    // Mock implementation - just initialize
    return true;
}

void setArduinoInterface(ArduinoInterface* arduino) {
    // Mock implementation
}
