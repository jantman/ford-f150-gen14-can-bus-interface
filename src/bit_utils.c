#include "bit_utils.h"

/**
 * Extract bits from CAN data using DBC-style bit positioning
 * 
 * This implementation matches the Python DBC extraction logic:
 * 1. Converts 8 bytes to 64-bit integer (little-endian)
 * 2. Calculates bit position from MSB: bit_pos = start_bit - length + 1
 * 3. Creates mask and extracts value
 */
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

/**
 * 16-bit version for larger values using DBC-style bit positioning
 */
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

/**
 * Set bits in CAN data using DBC-style bit positioning
 * 
 * This implementation complements the extractBits function using the same
 * DBC bit positioning scheme.
 */
void setBits(uint8_t* data, uint8_t startBit, uint8_t length, uint32_t value) {
    if (length > 16 || startBit > 63) return; // Bounds check
    
    // Convert existing data to 64-bit integer (little-endian)
    uint64_t dataValue = 0;
    for (int i = 0; i < 8; i++) {
        dataValue |= ((uint64_t)data[i]) << (i * 8);
    }
    
    // Calculate bit position (DBC uses MSB bit numbering)
    uint8_t bitPos = startBit - length + 1;
    
    // Clear the target bits
    uint64_t mask = ((1ULL << length) - 1) << bitPos;
    dataValue &= ~mask;
    
    // Set the new value at the correct bit position
    dataValue |= ((uint64_t)value) << bitPos;
    
    // Convert back to byte array (little-endian)
    for (int i = 0; i < 8; i++) {
        data[i] = (dataValue >> (i * 8)) & 0xFF;
    }
}
