#include "mock_arduino.h"
#include "../../../src/config.h"

// Define the mock Serial instance
MockSerial Serial;

extern "C" {
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
}
