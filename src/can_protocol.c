#include "can_protocol.h"
#include "config.h"

// Pure bit manipulation using DBC-style bit positioning
// startBit: MSB bit position (DBC format), length: number of bits (1-8)
// Uses Intel (little-endian) byte order as specified in DBC (@0+)
uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 8 || startBit > 63) return 0; // Bounds check
    
    // Convert 8 bytes to 64-bit integer (little-endian) - matches Python implementation
    uint64_t data_int = 0;
    for (int i = 0; i < 8; i++) {
        data_int |= ((uint64_t)data[i]) << (i * 8);
    }
    
    // Calculate bit position from MSB (DBC uses MSB bit numbering) - matches Python
    uint8_t bit_pos = startBit - length + 1;
    
    // Create mask and extract value - matches Python
    uint64_t mask = (1ULL << length) - 1;
    uint8_t value = (data_int >> bit_pos) & mask;
    
    return value;
}

// 16-bit version for larger values using DBC-style bit positioning
uint16_t extractBits16(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 16 || startBit > 63) return 0; // Bounds check
    
    // Convert 8 bytes to 64-bit integer (little-endian) - matches Python implementation
    uint64_t data_int = 0;
    for (int i = 0; i < 8; i++) {
        data_int |= ((uint64_t)data[i]) << (i * 8);
    }
    
    // Calculate bit position from MSB (DBC uses MSB bit numbering) - matches Python
    uint8_t bit_pos = startBit - length + 1;
    
    // Create mask and extract value - matches Python
    uint64_t mask = (1ULL << length) - 1;
    uint16_t value = (data_int >> bit_pos) & mask;
    
    return value;
}

// Pure message parsing (no Arduino dependencies)
BCMLampData parseBCMLampFrame(const CANFrame* frame) {
    BCMLampData result = {0};
    
    if (!frame || frame->id != BCM_LAMP_STAT_FD1_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to DBC specification (using DBC MSB bit positions)
    result.pudLampRequest = extractBits(frame->data, 12, 2);  // Bits 11-12 (DBC MSB position 12)
    result.illuminatedEntryStatus = extractBits(frame->data, 64, 2);  // Bits 63-64 (DBC MSB position 64) 
    result.drCourtesyLightStatus = extractBits(frame->data, 50, 2);   // Bits 49-50 (DBC MSB position 50)   
    
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
    
    // Extract signals according to DBC specification (using DBC MSB bit positions)
    result.vehicleLockStatus = extractBits(frame->data, 35, 2);  // Bits 34-35 (DBC MSB position 35)
    
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
    result.transmissionParkStatus = extractBits(frame->data, 34, 4);  // Bits 31-34 (DBC MSB position 34)
    
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
    result.batterySOC = extractBits(frame->data, 28, 7);  // Bits 22-28 (DBC MSB position 28), 7 bits for 0-127%
    
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
