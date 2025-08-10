#include "can_protocol.h"
#include "config.h"

// Pure decision logic (no Arduino dependencies)
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked) {
    return systemReady && isParked && isUnlocked;
}

bool shouldEnableBedlight(uint8_t pudLampRequest) {
    return (pudLampRequest == 1 || pudLampRequest == 2);  // ON or RAMP_UP
}

bool isVehicleUnlocked(uint8_t vehicleLockStatus) {
    return (vehicleLockStatus == 2 || vehicleLockStatus == 3);  // UNLOCK_ALL or UNLOCK_DRV
}

bool isVehicleParked(uint8_t transmissionParkStatus) {
    return (transmissionParkStatus == 1);  // Park
}
