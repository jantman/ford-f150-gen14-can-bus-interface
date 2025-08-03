#include "message_parser.h"

// Utility function to extract bits from CAN data
// startBit: bit position (0-63), length: number of bits (1-32)
// CAN data is typically stored in big-endian format
uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 8) return 0; // This function is for up to 8 bits
    
    uint8_t startByte = startBit / 8;
    uint8_t startBitInByte = startBit % 8;
    
    if (startByte >= 8) return 0; // Out of bounds
    
    uint8_t result = 0;
    uint8_t bitsRemaining = length;
    uint8_t resultBitPos = 0;
    
    while (bitsRemaining > 0 && startByte < 8) {
        uint8_t bitsInThisByte = 8 - startBitInByte;
        uint8_t bitsToExtract = (bitsRemaining < bitsInThisByte) ? bitsRemaining : bitsInThisByte;
        
        // Create mask for the bits we want
        uint8_t mask = ((1 << bitsToExtract) - 1) << startBitInByte;
        uint8_t extractedBits = (data[startByte] & mask) >> startBitInByte;
        
        // Place extracted bits in result
        result |= (extractedBits << resultBitPos);
        
        bitsRemaining -= bitsToExtract;
        resultBitPos += bitsToExtract;
        startByte++;
        startBitInByte = 0; // For subsequent bytes, start from bit 0
    }
    
    return result;
}

// 16-bit version for larger values
uint16_t extractBits16(const uint8_t* data, uint8_t startBit, uint8_t length) {
    if (length > 16) return 0; // This function is for up to 16 bits
    
    uint8_t startByte = startBit / 8;
    uint8_t startBitInByte = startBit % 8;
    
    if (startByte >= 8) return 0; // Out of bounds
    
    uint16_t result = 0;
    uint8_t bitsRemaining = length;
    uint8_t resultBitPos = 0;
    
    while (bitsRemaining > 0 && startByte < 8) {
        uint8_t bitsInThisByte = 8 - startBitInByte;
        uint8_t bitsToExtract = (bitsRemaining < bitsInThisByte) ? bitsRemaining : bitsInThisByte;
        
        // Create mask for the bits we want
        uint8_t mask = ((1 << bitsToExtract) - 1) << startBitInByte;
        uint8_t extractedBits = (data[startByte] & mask) >> startBitInByte;
        
        // Place extracted bits in result
        result |= ((uint16_t)extractedBits << resultBitPos);
        
        bitsRemaining -= bitsToExtract;
        resultBitPos += bitsToExtract;
        startByte++;
        startBitInByte = 0; // For subsequent bytes, start from bit 0
    }
    
    return result;
}

// Parse BCM_Lamp_Stat_FD1 message (ID: 963, 8 bytes)
// Signals: PudLamp_D_Rq (bits 11-12), Illuminated_Entry_Stat (bits 63-64), Dr_Courtesy_Light_Stat (bits 49-50)
bool parseBCMLampStatus(const CANMessage& message, BCMLampStatus& status) {
    if (message.id != BCM_LAMP_STAT_FD1_ID || message.length != 8) {
        LOG_WARN("Invalid BCM_Lamp_Stat_FD1 message: ID=0x%03X, Length=%d", message.id, message.length);
        return false;
    }
    
    // Extract signals according to DBC file
    status.pudLampRequest = extractBits(message.data, 11, 2);           // Bits 11-12, 2 bits
    status.illuminatedEntryStatus = extractBits(message.data, 63, 2);   // Bits 63-64, 2 bits  
    status.drCourtesyLightStatus = extractBits(message.data, 49, 2);    // Bits 49-50, 2 bits
    
    status.valid = true;
    status.timestamp = message.timestamp;
    
    LOG_DEBUG("Parsed BCM_Lamp_Stat_FD1: PudLamp=%d, IllumEntry=%d, CourtesyLight=%d", 
              status.pudLampRequest, status.illuminatedEntryStatus, status.drCourtesyLightStatus);
    
    return true;
}

// Parse Locking_Systems_2_FD1 message (ID: 817, 8 bytes)
// Signal: Veh_Lock_Status (bits 34-35)
bool parseLockingSystemsStatus(const CANMessage& message, LockingSystemsStatus& status) {
    if (message.id != LOCKING_SYSTEMS_2_FD1_ID || message.length != 8) {
        LOG_WARN("Invalid Locking_Systems_2_FD1 message: ID=0x%03X, Length=%d", message.id, message.length);
        return false;
    }
    
    // Extract vehicle lock status signal
    status.vehicleLockStatus = extractBits(message.data, 34, 2);  // Bits 34-35, 2 bits
    
    status.valid = true;
    status.timestamp = message.timestamp;
    
    LOG_DEBUG("Parsed Locking_Systems_2_FD1: VehLockStatus=%d", status.vehicleLockStatus);
    
    return true;
}

// Parse PowertrainData_10 message (ID: 374, 8 bytes)
// Signal: TrnPrkSys_D_Actl (bits 31-34)
bool parsePowertrainData(const CANMessage& message, PowertrainData& data) {
    if (message.id != POWERTRAIN_DATA_10_ID || message.length != 8) {
        LOG_WARN("Invalid PowertrainData_10 message: ID=0x%03X, Length=%d", message.id, message.length);
        return false;
    }
    
    // Extract transmission park system status
    data.transmissionParkStatus = extractBits(message.data, 31, 4);  // Bits 31-34, 4 bits
    
    data.valid = true;
    data.timestamp = message.timestamp;
    
    LOG_DEBUG("Parsed PowertrainData_10: TrnPrkSys=%d", data.transmissionParkStatus);
    
    return true;
}

// Parse Battery_Mgmt_3_FD1 message (ID: 1084, 8 bytes)
// Signal: BSBattSOC (bits 22-28)
bool parseBatteryManagement(const CANMessage& message, BatteryManagement& data) {
    if (message.id != BATTERY_MGMT_3_FD1_ID || message.length != 8) {
        LOG_WARN("Invalid Battery_Mgmt_3_FD1 message: ID=0x%03X, Length=%d", message.id, message.length);
        return false;
    }
    
    // Extract battery state of charge
    data.batterySOC = extractBits(message.data, 22, 7);  // Bits 22-28, 7 bits (0-127%)
    
    data.valid = true;
    data.timestamp = message.timestamp;
    
    LOG_DEBUG("Parsed Battery_Mgmt_3_FD1: BattSOC=%d%%", data.batterySOC);
    
    return true;
}

// Utility functions for debugging - print raw message data
void printCANMessageHex(const CANMessage& message) {
    LOG_DEBUG("CAN Message ID=0x%03X Length=%d Data: %02X %02X %02X %02X %02X %02X %02X %02X",
              message.id, message.length,
              message.data[0], message.data[1], message.data[2], message.data[3],
              message.data[4], message.data[5], message.data[6], message.data[7]);
}

// Print message data in binary format for bit analysis
void printCANMessageBinary(const CANMessage& message) {
    LOG_DEBUG("CAN Message ID=0x%03X Binary:", message.id);
    for (int i = 0; i < message.length && i < 8; i++) {
        char binaryStr[9];
        for (int bit = 7; bit >= 0; bit--) {
            binaryStr[7-bit] = (message.data[i] & (1 << bit)) ? '1' : '0';
        }
        binaryStr[8] = '\0';
        LOG_DEBUG("  Byte %d: %s (0x%02X)", i, binaryStr, message.data[i]);
    }
}
