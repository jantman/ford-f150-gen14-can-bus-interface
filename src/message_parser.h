#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <Arduino.h>
#include "config.h"
#include "can_manager.h"

// Parsed message structures based on minimal.dbc
struct BCMLampStatus {
    uint8_t pudLampRequest;
    uint8_t illuminatedEntryStatus;
    uint8_t drCourtesyLightStatus;
    bool valid;
    unsigned long timestamp;
};

struct LockingSystemsStatus {
    uint8_t vehicleLockStatus;
    bool valid;
    unsigned long timestamp;
};

struct PowertrainData {
    uint8_t transmissionParkStatus;
    bool valid;
    unsigned long timestamp;
};

struct BatteryManagement {
    uint8_t batterySOC;
    bool valid;
    unsigned long timestamp;
};

// Function declarations (to be implemented in Step 4)
bool parseBCMLampStatus(const CANMessage& message, BCMLampStatus& status);
bool parseLockingSystemsStatus(const CANMessage& message, LockingSystemsStatus& status);
bool parsePowertrainData(const CANMessage& message, PowertrainData& data);
bool parseBatteryManagement(const CANMessage& message, BatteryManagement& data);

#include "bit_utils.h"

// Debugging functions
void printCANMessageHex(const CANMessage& message);
void printCANMessageBinary(const CANMessage& message);

#endif // MESSAGE_PARSER_H
