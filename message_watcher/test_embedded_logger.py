#!/usr/bin/env python3
"""
Test script for the embedded CAN logger signal extraction logic.
This validates that our bit manipulation matches the DBC signal definitions.
"""

import sys
import os
sys.path.append(os.path.dirname(__file__))

from can_embedded_logger import EmbeddedCANLogger

def test_signal_extraction():
    """Test signal extraction with known test vectors."""
    logger = EmbeddedCANLogger("test", 1.0)
    
    print("Testing signal extraction logic...")
    
    # Test BCM_Lamp_Stat_FD1 (0x3C3)
    # Example data where PudLamp_D_Rq = 2 (bits 11:10 = 10 binary)
    test_data_1 = bytearray(8)
    test_data_1[1] = 0x08  # Set bit 11 (0x0800 in little endian = byte 1 bit 3)
    
    value = logger.extract_signal_value(test_data_1, 11, 2)
    print(f"PudLamp_D_Rq extraction test: expected=2, got={value}")
    
    # Test Battery_Mgmt_3_FD1 BSBattSOC (start_bit=22, length=7)
    # Example: 50% battery = 50 decimal = 0x32
    test_data_2 = bytearray(8)
    # Bit 22 down to bit 16: need to set bits in the right position
    # For 50 (0x32 = 0110010), put in bits 22:16
    test_data_2[2] = 0x32  # 50 in bits 22:16 maps to byte 2
    
    value2 = logger.extract_signal_value(test_data_2, 22, 7)
    print(f"BSBattSOC extraction test: expected=50, got={value2}")
    
    # Test a full decode
    decoded = logger.decode_can_message(0x43C, test_data_2)
    if decoded:
        print(f"Full decode test: {decoded}")
    else:
        print("Full decode test: No result")
    
    print("Signal extraction tests completed.")

if __name__ == "__main__":
    test_signal_extraction()
