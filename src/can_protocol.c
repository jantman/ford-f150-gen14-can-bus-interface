#include "can_protocol.h"
#include "config.h"

// Pure bit manipulation (no Arduino dependencies)
uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 8 || startBit >= 64) return 0;
    
    uint8_t startByte = startBit / 8;
    uint8_t startBitInByte = startBit % 8;
    
    if (startByte >= 8) return 0;
    
    uint8_t result = 0;
    uint8_t bitsRemaining = length;
    uint8_t resultBitPos = 0;
    
    while (bitsRemaining > 0 && startByte < 8) {
        uint8_t bitsInThisByte = 8 - startBitInByte;
        uint8_t bitsToExtract = (bitsRemaining < bitsInThisByte) ? bitsRemaining : bitsInThisByte;
        
        uint8_t mask = ((1 << bitsToExtract) - 1) << startBitInByte;
        uint8_t extractedBits = (data[startByte] & mask) >> startBitInByte;
        
        result |= (extractedBits << resultBitPos);
        
        bitsRemaining -= bitsToExtract;
        resultBitPos += bitsToExtract;
        startByte++;
        startBitInByte = 0;
    }
    
    return result;
}

uint16_t extractBits16(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 16) return 0;
    
    uint8_t startByte = startBit / 8;
    uint8_t startBitInByte = startBit % 8;
    
    if (startByte >= 8) return 0;
    
    uint16_t result = 0;
    uint8_t bitsRemaining = length;
    uint8_t resultBitPos = 0;
    
    while (bitsRemaining > 0 && startByte < 8) {
        uint8_t bitsInThisByte = 8 - startBitInByte;
        uint8_t bitsToExtract = (bitsRemaining < bitsInThisByte) ? bitsRemaining : bitsInThisByte;
        
        uint8_t mask = ((1 << bitsToExtract) - 1) << startBitInByte;
        uint8_t extractedBits = (data[startByte] & mask) >> startBitInByte;
        
        result |= (extractedBits << resultBitPos);
        
        bitsRemaining -= bitsToExtract;
        resultBitPos += bitsToExtract;
        startByte++;
        startBitInByte = 0;
    }
    
    return result;
}

// Pure message parsing (no Arduino dependencies)
BCMLampData parseBCMLampFrame(const CANFrame* frame) {
    BCMLampData result = {0};
    
    if (!frame || frame->id != BCM_LAMP_STAT_FD1_ID || frame->length != 8) {
        result.valid = false;
        return result;
    }
    
    // Extract signals according to DBC specification
    result.pudLampRequest = extractBits(frame->data, 11, 2);  // Bits 11-12
    result.illuminatedEntryStatus = extractBits(frame->data, 13, 2);  
    result.drCourtesyLightStatus = extractBits(frame->data, 15, 2);   
    
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
    
    // Extract signals according to DBC specification  
    result.vehicleLockStatus = extractBits(frame->data, 34, 2);  // Bits 34-35
    
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
    
    // Extract signals according to DBC specification
    result.transmissionParkStatus = extractBits(frame->data, 31, 4);  // Bits 31-34
    
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
    
    // Extract signals according to DBC specification
    result.batterySOC = extractBits(frame->data, 22, 7);  // Bits 22-28, 7 bits for 0-127%
    
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
