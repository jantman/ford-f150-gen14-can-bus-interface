#pragma once

#include <cstring>
#include <stdint.h>

// Include production types and utilities
extern "C" {
    #include "../../src/can_protocol.h"  // For CANFrame type
    #include "../../src/bit_utils.h"     // For setBits function
}

/**
 * Shared CAN test utilities to eliminate duplicate helper functions
 * 
 * This utility class consolidates all CAN frame creation and signal setting
 * functions that were previously duplicated across multiple test files.
 * 
 * All functions use production code for consistency and single source of truth.
 */
class CANTestUtils {
public:
    /**
     * Create a CAN frame with specified ID and data
     * Replaces duplicate createCANFrame() functions in test files
     * 
     * @param id CAN message ID
     * @param data 8-byte data array
     * @return CANFrame structure ready for parsing
     */
    static CANFrame createCANFrame(uint32_t id, const uint8_t data[8]) {
        CANFrame frame;
        frame.id = id;
        frame.length = 8;
        memcpy(frame.data, data, 8);
        return frame;
    }
    
    /**
     * Create a CAN frame with individual bytes specified
     * Replaces createFrame() function from test_can_protocol_integration.cpp
     * 
     * @param id CAN message ID
     * @param b0-b7 Individual bytes for the 8-byte data array
     * @return CANFrame structure ready for parsing
     */
    static CANFrame createCANFrame(uint32_t id, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                                   uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
        CANFrame frame;
        frame.id = id;
        frame.length = 8;
        frame.data[0] = b0; frame.data[1] = b1; frame.data[2] = b2; frame.data[3] = b3;
        frame.data[4] = b4; frame.data[5] = b5; frame.data[6] = b6; frame.data[7] = b7;
        return frame;
    }
    
    /**
     * Set a signal value in CAN data using DBC-style bit positioning
     * Replaces duplicate setSignalValue() and setBitPattern() functions
     * 
     * Uses production setBits() function for consistency
     * 
     * @param data 8-byte data array (will be cleared first)
     * @param startBit MSB bit position (DBC format, 0-63)
     * @param length Number of bits (1-16)
     * @param value Value to set
     */
    static void setSignalValue(uint8_t data[8], uint8_t startBit, uint8_t length, uint32_t value) {
        memset(data, 0, 8);  // Clear data first
        setBits(data, startBit, length, value);  // Use production setBits function
    }
    
    /**
     * Extract signal value from CAN data using DBC-style bit positioning
     * Replaces duplicate pythonExtractSignalValue() functions
     * 
     * Uses production extractBits() function for consistency
     * Note: Production extractBits returns uint8_t, this wrapper returns uint32_t for compatibility
     * 
     * @param data 8-byte data array
     * @param startBit MSB bit position (DBC format, 0-63)
     * @param length Number of bits (1-8 for uint8_t, 1-16 for uint16_t)
     * @return Extracted value
     */
    static uint32_t extractSignalValue(const uint8_t data[8], uint8_t startBit, uint8_t length) {
        if (length <= 8) {
            return (uint32_t)extractBits(data, startBit, length);
        } else {
            return (uint32_t)extractBits16(data, startBit, length);
        }
    }
    
    /**
     * Create test CAN data with a single signal set
     * Convenience function for common test pattern
     * 
     * @param startBit MSB bit position (DBC format, 0-63)
     * @param length Number of bits (1-16)
     * @param value Value to set
     * @param data Output: 8-byte data array
     */
    static void createTestData(uint8_t startBit, uint8_t length, uint32_t value, uint8_t data[8]) {
        setSignalValue(data, startBit, length, value);
    }
};

// Legacy compatibility macros for existing test code
// These allow gradual migration without breaking existing tests
#define CREATE_CAN_MESSAGE(id, data_bytes) CANTestUtils::createCANFrame(id, data_bytes)
