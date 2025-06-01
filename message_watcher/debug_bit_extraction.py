#!/usr/bin/env python3
"""
Debug script to compare bit extraction between cantools and embedded implementation
"""

import cantools
import sys

# Import our embedded implementation
from can_embedded_logger import EmbeddedDBCDatabase

def analyze_bit_positions():
    """Analyze bit positions in detail."""
    test_data = bytes([0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80])
    
    print("Test data analysis:")
    print("Data:", " ".join([f"{b:02X}" for b in test_data]))
    print()
    
    # Show bit layout
    print("Bit layout (bit position: byte[bit]):")
    for byte_idx, byte_val in enumerate(test_data):
        print(f"Byte {byte_idx}: {byte_val:08b} ({byte_val:02X})")
        for bit_idx in range(8):
            global_bit = byte_idx * 8 + (7 - bit_idx)  # Motorola numbering
            bit_value = (byte_val >> (7 - bit_idx)) & 1
            print(f"  Bit {global_bit:2d}: {bit_value}")
    print()
    
    # Analyze Illuminated_Entry_Stat specifically
    print("Illuminated_Entry_Stat (start=63, length=2):")
    print("Should use bits [62, 63]")
    
    # Bit 63 is in byte 7, bit position 7 (MSB)
    # Bit 62 is in byte 7, bit position 6
    byte7 = test_data[7]
    bit63 = (byte7 >> 7) & 1
    bit62 = (byte7 >> 6) & 1
    
    print(f"Bit 63 (byte 7, bit 7): {bit63}")
    print(f"Bit 62 (byte 7, bit 6): {bit62}")
    print(f"Expected value: {(bit63 << 1) | bit62} (bit63={bit63}, bit62={bit62})")
    
    return test_data

def test_bit_extraction():
    """Test bit extraction with known data."""
    
    # Load the minimal DBC with cantools
    db = cantools.database.load_file('minimal.dbc')
    
    # Load our embedded database
    embedded_db = EmbeddedDBCDatabase()
    
    test_data = analyze_bit_positions()
    
    # Get the message from both
    cantools_msg = db.get_message_by_name('BCM_Lamp_Stat_FD1')
    embedded_msg = embedded_db.get_message_by_name('BCM_Lamp_Stat_FD1')
    
    # Decode with cantools
    cantools_decoded = cantools_msg.decode(test_data)
    
    # Decode with our embedded implementation
    embedded_decoded = embedded_msg.decode(test_data)
    
    print("\nCOMPARISSON RESULTS:")
    print("=" * 60)
    
    for signal_name in ['PudLamp_D_Rq', 'Dr_Courtesy_Light_Stat', 'Illuminated_Entry_Stat']:
        cantools_value = cantools_decoded[signal_name]
        embedded_value = embedded_decoded[signal_name]
        
        print(f"\n{signal_name}:")
        print(f"  Cantools:  {cantools_value}")
        print(f"  Embedded:  {embedded_value}")
        
        # Compare raw values
        cantools_raw = cantools_value.value if hasattr(cantools_value, 'value') else cantools_value
        embedded_raw = embedded_value.value if hasattr(embedded_value, 'value') else embedded_value
        
        match = "✓ MATCH" if cantools_raw == embedded_raw else "✗ MISMATCH"
        print(f"  Raw values: {cantools_raw} vs {embedded_raw} - {match}")
        
        # Debug our extraction for mismatches
        if cantools_raw != embedded_raw:
            signal = embedded_msg.get_signal_by_name(signal_name)
            print(f"  Signal details: start={signal.start}, length={signal.length}")
            
            # Manual bit extraction debug
            print("  Manual extraction debug:")
            for i in range(signal.length):
                bit_pos = signal.start - i
                byte_index = bit_pos // 8
                bit_index = 7 - (bit_pos % 8)
                if byte_index < len(test_data):
                    bit_value = (test_data[byte_index] >> bit_index) & 1
                    print(f"    Bit {bit_pos}: byte[{byte_index}].bit[{bit_index}] = {bit_value}")

if __name__ == "__main__":
    test_bit_extraction()