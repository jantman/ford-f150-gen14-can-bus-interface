#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pure CAN message structure (no Arduino dependencies)
typedef struct {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} CANFrame;

#include "bit_utils.h"

// Pure decision logic functions (no Arduino dependencies)
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked);
bool shouldEnableBedlight(uint8_t pudLampRequest);
bool isVehicleUnlocked(uint8_t vehicleLockStatus);
bool isVehicleParked(uint8_t transmissionParkStatus);

#ifdef __cplusplus
}
#endif

#endif // CAN_PROTOCOL_H
