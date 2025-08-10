#include "can_protocol.h"
#include "config.h"

// Pure message parsing (no Arduino dependencies)
BCMLampData parseBCMLampFrame(const CANFrame* frame) {
    BCMLampData result = {0};
    
    if (!frame || frame->id != BCM_LAMP_STAT_FD1_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to DBC specification (using DBC MSB bit positions)
    result.pudLampRequest = extractBits(frame->data, 11, 2);  // Bits 10-11 (DBC start_bit 11, length 2)
    result.illuminatedEntryStatus = extractBits(frame->data, 63, 2);  // Bits 62-63 (DBC start_bit 63, length 2) 
    result.drCourtesyLightStatus = extractBits(frame->data, 49, 2);   // Bits 48-49 (DBC start_bit 49, length 2)   
    
    // Basic validation
    result.valid = (result.pudLampRequest <= 3);  // Valid range 0-3
    
    return result;
}

LockingSystemsData parseLockingSystemsFrame(const CANFrame* frame) {
    LockingSystemsData result = {0};
    
    if (!frame || frame->id != LOCKING_SYSTEMS_2_FD1_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to real CAN data analysis (corrected bit position)
    // Based on test analysis: LOCK_ALL=0x02 (byte 4) -> value 1, UNLOCK_ALL=0x05 (byte 4) -> value 2
    result.vehicleLockStatus = extractBits(frame->data, 34, 2);  // Bits 33-34 (original working position)
    
    // Basic validation
    result.valid = (result.vehicleLockStatus <= 3);  // Valid range 0-3
    
    return result;
}

PowertrainData parsePowertrainFrame(const CANFrame* frame) {
    PowertrainData result = {0};
    
    if (!frame || frame->id != POWERTRAIN_DATA_10_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to DBC specification (using DBC MSB bit positions)
    result.transmissionParkStatus = extractBits(frame->data, 31, 4);  // Bits 28-31 (DBC start_bit 31, length 4)
    
    // Basic validation
    result.valid = (result.transmissionParkStatus <= 15);  // Valid range 0-15
    
    return result;
}

BatteryData parseBatteryFrame(const CANFrame* frame) {
    BatteryData result = {0};
    
    if (!frame || frame->id != BATTERY_MGMT_3_FD1_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to DBC specification (using DBC MSB bit positions)
    result.batterySOC = extractBits(frame->data, 22, 7);  // Bits 16-22 (DBC start_bit 22, length 7)
    
    // Basic validation
    result.valid = (result.batterySOC <= 127);  // Valid range 0-127%
    
    return result;
}

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

// Check if a CAN message ID is one of our target messages (matching Python implementation)
bool isTargetCANMessage(uint32_t messageId) {
    return (messageId == BCM_LAMP_STAT_FD1_ID ||
            messageId == LOCKING_SYSTEMS_2_FD1_ID ||
            messageId == POWERTRAIN_DATA_10_ID ||
            messageId == BATTERY_MGMT_3_FD1_ID);
}
