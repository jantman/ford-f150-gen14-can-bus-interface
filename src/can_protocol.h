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

// Pure message data structures (no timestamps, no Arduino types)
typedef struct {
    uint8_t pudLampRequest;
    uint8_t illuminatedEntryStatus;
    uint8_t drCourtesyLightStatus;
    bool valid;
} BCMLampData;

typedef struct {
    uint8_t vehicleLockStatus;
    bool valid;
} LockingSystemsData;

typedef struct {
    uint8_t transmissionParkStatus;
    bool valid;
} PowertrainData;

typedef struct {
    uint8_t batterySOC;
    bool valid;
} BatteryData;

// Pure bit manipulation functions (no Arduino dependencies)
uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length);
uint16_t extractBits16(const uint8_t* data, uint8_t startBit, uint8_t length);

// Pure message parsing functions (no Arduino dependencies)
BCMLampData parseBCMLampFrame(const CANFrame* frame);
LockingSystemsData parseLockingSystemsFrame(const CANFrame* frame);
PowertrainData parsePowertrainFrame(const CANFrame* frame);
BatteryData parseBatteryFrame(const CANFrame* frame);

// Pure decision logic functions (no Arduino dependencies)
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked);
bool shouldEnableBedlight(uint8_t pudLampRequest);
bool isVehicleUnlocked(uint8_t vehicleLockStatus);
bool isVehicleParked(uint8_t transmissionParkStatus);
bool isTargetCANMessage(uint32_t messageId);

#ifdef __cplusplus
}
#endif

#endif // CAN_PROTOCOL_H
