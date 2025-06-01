#!/usr/bin/env python3
"""
DBC File Validator

This script validates DBC files using the cantools library.
It can be used to check syntax, structure, and signal definitions.

Usage:
    python validate_dbc.py <dbc_file>
    python validate_dbc.py minimal.dbc

The script will:
1. Parse the DBC file and report any syntax errors
2. List all messages and signals found
3. Check for common issues like overlapping signals
4. Validate signal bit positions and lengths
"""

import sys
import argparse
from pathlib import Path

try:
    import cantools
except ImportError:
    print("Error: cantools library not found. Install with: pip install cantools")
    sys.exit(1)


def validate_dbc_file(dbc_path):
    """
    Validate a DBC file and provide detailed feedback.
    
    Args:
        dbc_path: Path to the DBC file
        
    Returns:
        bool: True if validation passed, False otherwise
    """
    try:
        print(f"Validating DBC file: {dbc_path}")
        print("=" * 60)
        
        # Parse the DBC file
        db = cantools.database.load_file(dbc_path)
        
        print(f"✓ DBC file parsed successfully!")
        print(f"  - Database version: {getattr(db, 'version', 'Not specified')}")
        print(f"  - Messages: {len(db.messages)}")
        print(f"  - Nodes: {len(db.nodes)}")
        
        # Validate messages and signals
        validation_passed = True
        
        print("\nMessage Details:")
        print("-" * 40)
        
        for msg in db.messages:
            print(f"\nMessage: {msg.name} (ID: 0x{msg.frame_id:X}, DLC: {msg.length})")
            
            if len(msg.signals) == 0:
                print("  ⚠️  Warning: Message has no signals")
                continue
                
            print(f"  Signals ({len(msg.signals)}):")
            
            # Check for overlapping signals
            signal_positions = []
            
            for signal in msg.signals:
                start_bit = signal.start
                length = signal.length
                
                # Calculate bit range
                if signal.byte_order == 'big_endian':
                    # Motorola byte order (Ford uses this)
                    bit_range = list(range(start_bit - length + 1, start_bit + 1))
                else:
                    # Intel byte order
                    bit_range = list(range(start_bit, start_bit + length))
                
                signal_positions.append((signal.name, bit_range))
                
                print(f"    - {signal.name}:")
                print(f"      Start bit: {signal.start}, Length: {signal.length}")
                print(f"      Byte order: {signal.byte_order}")
                print(f"      Scale: {signal.scale}, Offset: {signal.offset}")
                print(f"      Range: [{signal.minimum}, {signal.maximum}]")
                print(f"      Unit: {signal.unit or 'None'}")
                
                if signal.choices:
                    print(f"      Enum values: {signal.choices}")
                
                # Validate bit position
                if signal.start < 0 or signal.start >= (msg.length * 8):
                    print(f"      ❌ Error: Start bit {signal.start} is out of range for {msg.length}-byte message")
                    validation_passed = False
                
                if signal.length <= 0 or signal.length > 64:
                    print(f"      ❌ Error: Invalid signal length {signal.length}")
                    validation_passed = False
            
            # Check for overlapping signals
            overlaps = find_signal_overlaps(signal_positions)
            if overlaps:
                print(f"  ❌ Error: Signal overlaps detected:")
                for overlap in overlaps:
                    print(f"    - {overlap}")
                validation_passed = False
        
        # Check for duplicate message IDs
        msg_ids = [msg.frame_id for msg in db.messages]
        if len(msg_ids) != len(set(msg_ids)):
            print("\n❌ Error: Duplicate message IDs detected")
            validation_passed = False
        
        print("\n" + "=" * 60)
        if validation_passed:
            print("✓ DBC validation PASSED - No errors found!")
        else:
            print("❌ DBC validation FAILED - Errors found above")
        
        return validation_passed
        
    except Exception as e:
        print(f"❌ Error parsing DBC file: {e}")
        return False


def find_signal_overlaps(signal_positions):
    """Find overlapping bit positions between signals."""
    overlaps = []
    
    for i, (name1, bits1) in enumerate(signal_positions):
        for j, (name2, bits2) in enumerate(signal_positions[i+1:], i+1):
            # Check for bit overlap
            overlap_bits = set(bits1) & set(bits2)
            if overlap_bits:
                overlaps.append(f"{name1} and {name2} overlap at bits {sorted(overlap_bits)}")
    
    return overlaps


def main():
    parser = argparse.ArgumentParser(
        description='Validate DBC files using cantools',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python validate_dbc.py minimal.dbc
    python validate_dbc.py ford_lincoln_base_pt.dbc
    
The validator will check:
- DBC file syntax and structure
- Message and signal definitions
- Bit position overlaps
- Signal range validation
- Duplicate message IDs
        """
    )
    
    parser.add_argument(
        'dbc_file',
        help='Path to the DBC file to validate'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    args = parser.parse_args()
    
    # Check if file exists
    dbc_path = Path(args.dbc_file)
    if not dbc_path.exists():
        print(f"Error: DBC file '{dbc_path}' not found")
        sys.exit(1)
    
    # Validate the DBC file
    success = validate_dbc_file(dbc_path)
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()