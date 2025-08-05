#pragma once

#include <stdint.h>

/**
 * Bit manipulation utilities for CAN message processing
 * 
 * These functions use DBC-style bit positioning:
 * - startBit: MSB bit position (DBC format)
 * - Intel (little-endian) byte order as specified in DBC (@0+)
 * - Bit numbering: 0-63 for 8-byte CAN messages
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extract bits from CAN data using DBC-style bit positioning
 * @param data: 8-byte CAN data array
 * @param startBit: MSB bit position (DBC format, 0-63)
 * @param length: Number of bits to extract (1-8)
 * @return Extracted value (0-255)
 */
uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length);

/**
 * Extract bits from CAN data using DBC-style bit positioning (16-bit version)
 * @param data: 8-byte CAN data array
 * @param startBit: MSB bit position (DBC format, 0-63)
 * @param length: Number of bits to extract (1-16)
 * @return Extracted value (0-65535)
 */
uint16_t extractBits16(const uint8_t* data, uint8_t startBit, uint8_t length);

/**
 * Set bits in CAN data using DBC-style bit positioning
 * @param data: 8-byte CAN data array (modified in place)
 * @param startBit: MSB bit position (DBC format, 0-63)
 * @param length: Number of bits to set (1-16)
 * @param value: Value to set
 */
void setBits(uint8_t* data, uint8_t startBit, uint8_t length, uint32_t value);

#ifdef __cplusplus
}
#endif
