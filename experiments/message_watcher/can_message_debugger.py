#!/usr/bin/env python3
"""
CAN Message Debugger

This script allows debugging individual CAN messages using the same decoding logic
from can_embedded_logger.py. It parses CAN message strings from the command line
and outputs decoded signal values.

The debugger supports the same 4 specific CAN messages as the embedded logger:
- BCM_Lamp_Stat_FD1 (0x3C3): PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
- Locking_Systems_2_FD1 (0x331): Veh_Lock_Status  
- PowertrainData_10 (0x176): TrnPrkSys_D_Actl
- Battery_Mgmt_3_FD1 (0x43C): BSBattSOC

Usage:
    python can_message_debugger.py "CAN RX: ID=0x331 (817), Len=8, Data=[00 0F 00 00 05 20 44 10]"
    python can_message_debugger.py --id 0x331 --data "00 0F 00 00 05 20 44 10"
    python can_message_debugger.py --id 817 --data "00:0F:00:00:05:20:44:10"
"""

import argparse
import sys
import re

# Import CAN message definitions and decoding logic from the embedded logger
# CAN message definitions - hard-coded from minimal.dbc for efficiency
CAN_MESSAGES = {
    0x3C3: {  # BCM_Lamp_Stat_FD1 (963)
        'name': 'BCM_Lamp_Stat_FD1',
        'signals': {
            'PudLamp_D_Rq': {'start_bit': 11, 'length': 2, 'values': {0: "OFF", 1: "ON", 2: "RAMP_UP", 3: "RAMP_DOWN"}},
            'Illuminated_Entry_Stat': {'start_bit': 63, 'length': 2, 'values': {0: "Off", 1: "On", 2: "Unknown", 3: "Invalid"}},
            'Dr_Courtesy_Light_Stat': {'start_bit': 49, 'length': 2, 'values': {0: "Off", 1: "On", 2: "Unknown", 3: "Invalid"}}
        }
    },
    0x331: {  # Locking_Systems_2_FD1 (817)
        'name': 'Locking_Systems_2_FD1',
        'signals': {
            'Veh_Lock_Status': {'start_bit': 34, 'length': 2, 'values': {0: "LOCK_DBL", 1: "LOCK_ALL", 2: "UNLOCK_ALL", 3: "UNLOCK_DRV"}}
        }
    },
    0x176: {  # PowertrainData_10 (374)
        'name': 'PowertrainData_10',
        'signals': {
            'TrnPrkSys_D_Actl': {'start_bit': 31, 'length': 4, 'values': {
                0: "NotKnown", 1: "Park", 2: "TransitionCloseToPark", 3: "AtNoSpring", 
                4: "TransitionCloseToOutOfPark", 5: "OutOfPark", 6: "Override", 7: "OutOfRangeLow",
                8: "OutOfRangeHigh", 9: "FrequencyError", 15: "Faulty"
            }}
        }
    },
    0x43C: {  # Battery_Mgmt_3_FD1 (1084)
        'name': 'Battery_Mgmt_3_FD1', 
        'signals': {
            'BSBattSOC': {'start_bit': 22, 'length': 7, 'values': None}  # Raw percentage value
        }
    }
}


class CANMessageDebugger:
    """CAN message decoder for debugging individual messages."""
    
    def __init__(self):
        self.supported_ids = list(CAN_MESSAGES.keys())
    
    def extract_signal_value(self, data, start_bit, length):
        """
        Extract signal value from CAN data using DBC bit positioning.
        Uses Intel (little-endian) byte order as specified in DBC (@0+).
        
        Args:
            data: 8-byte CAN data payload
            start_bit: Starting bit position (DBC format)
            length: Number of bits
            
        Returns:
            int: Extracted signal value
        """
        # Convert 8 bytes to 64-bit integer (little-endian)
        data_int = int.from_bytes(data, byteorder='little')
        
        # Calculate bit position from MSB (DBC uses MSB bit numbering)
        bit_pos = start_bit - length + 1
        
        # Create mask and extract value
        mask = (1 << length) - 1
        value = (data_int >> bit_pos) & mask
        
        return value

    def decode_can_message(self, can_id, data):
        """
        Decode CAN message using hard-coded signal definitions.
        
        Args:
            can_id: CAN message ID
            data: 8-byte CAN data payload
            
        Returns:
            dict: Decoded signals or None if message not monitored
        """
        if can_id not in CAN_MESSAGES:
            return None
            
        msg_def = CAN_MESSAGES[can_id]
        decoded_signals = {}
        
        # Extract each signal
        for signal_name, signal_def in msg_def['signals'].items():
            raw_value = self.extract_signal_value(data, signal_def['start_bit'], signal_def['length'])
            
            # Apply value mapping if available
            if signal_def['values'] is not None:
                decoded_signals[signal_name] = signal_def['values'].get(raw_value, f"Unknown({raw_value})")
            else:
                decoded_signals[signal_name] = raw_value
        
        return {
            'message_name': msg_def['name'],
            'signals': decoded_signals
        }

    def parse_can_string(self, can_string):
        """
        Parse CAN message string in various formats.
        
        Supported formats:
        - "CAN RX: ID=0x331 (817), Len=8, Data=[00 0F 00 00 05 20 44 10]"
        - "ID=0x331, Data=00 0F 00 00 05 20 44 10"
        - "0x331: 00 0F 00 00 05 20 44 10"
        
        Args:
            can_string: String representation of CAN message
            
        Returns:
            tuple: (can_id, data_bytes) or (None, None) if parsing failed
        """
        # Remove extra whitespace and normalize
        can_string = can_string.strip()
        
        # Pattern 1: Full CAN RX format
        pattern1 = r'CAN\s+RX:\s+ID=0x([0-9A-Fa-f]+).*?Data=\[([0-9A-Fa-f\s]+)\]'
        match = re.search(pattern1, can_string)
        if match:
            can_id = int(match.group(1), 16)
            data_str = match.group(2)
            return self._parse_data_bytes(can_id, data_str)
        
        # Pattern 2: Simple ID=, Data= format
        pattern2 = r'ID=0x([0-9A-Fa-f]+).*?Data=([0-9A-Fa-f\s\:\[\]]+)'
        match = re.search(pattern2, can_string)
        if match:
            can_id = int(match.group(1), 16)
            data_str = match.group(2)
            return self._parse_data_bytes(can_id, data_str)
        
        # Pattern 3: Simple format "0x331: 00 0F ..."
        pattern3 = r'0x([0-9A-Fa-f]+):\s*([0-9A-Fa-f\s\:]+)'
        match = re.search(pattern3, can_string)
        if match:
            can_id = int(match.group(1), 16)
            data_str = match.group(2)
            return self._parse_data_bytes(can_id, data_str)
        
        # Pattern 4: Decimal ID format
        pattern4 = r'ID=(\d+).*?Data=([0-9A-Fa-f\s\:\[\]]+)'
        match = re.search(pattern4, can_string)
        if match:
            can_id = int(match.group(1))
            data_str = match.group(2)
            return self._parse_data_bytes(can_id, data_str)
        
        return None, None
    
    def _parse_data_bytes(self, can_id, data_str):
        """
        Parse data bytes from string format.
        
        Args:
            can_id: CAN ID
            data_str: String containing hex bytes (space, colon, or bracket separated)
            
        Returns:
            tuple: (can_id, data_bytes) or (None, None) if parsing failed
        """
        try:
            # Clean up the data string - remove brackets, normalize separators
            data_str = data_str.replace('[', '').replace(']', '')
            data_str = data_str.replace(':', ' ')
            
            # Split by whitespace and parse hex bytes
            hex_bytes = data_str.split()
            
            if len(hex_bytes) != 8:
                print(f"Error: Expected 8 data bytes, got {len(hex_bytes)}", file=sys.stderr)
                return None, None
            
            # Convert to bytes
            data_bytes = bytes([int(b, 16) for b in hex_bytes])
            return can_id, data_bytes
            
        except ValueError as e:
            print(f"Error parsing data bytes: {e}", file=sys.stderr)
            return None, None

    def format_signal_value(self, value):
        """Format signal value for output."""
        if value is None:
            return "N/A"
        elif isinstance(value, int):
            return str(value)
        else:
            return str(value)

    def debug_message(self, can_id, data_bytes):
        """
        Debug a single CAN message.
        
        Args:
            can_id: CAN message ID
            data_bytes: 8-byte CAN data payload
        """
        print(f"CAN Message Debug Analysis")
        print(f"{'='*50}")
        print(f"CAN ID: 0x{can_id:03X} ({can_id})")
        print(f"Data: {' '.join(f'{b:02X}' for b in data_bytes)}")
        print()
        
        # Check if this is a monitored message
        if can_id not in CAN_MESSAGES:
            print(f"❌ Message ID 0x{can_id:03X} is not monitored by this debugger")
            print(f"📋 Supported message IDs:")
            for msg_id, msg_def in CAN_MESSAGES.items():
                print(f"   • 0x{msg_id:03X} ({msg_id}): {msg_def['name']}")
            return False
        
        # Decode the message
        decoded_data = self.decode_can_message(can_id, data_bytes)
        
        if decoded_data:
            print(f"✅ Successfully decoded message: {decoded_data['message_name']}")
            print(f"📊 Decoded Signals:")
            
            # Get raw values for display alongside decoded values
            msg_def = CAN_MESSAGES[can_id]
            for signal_name, value in decoded_data['signals'].items():
                signal_def = msg_def['signals'][signal_name]
                raw_value = self.extract_signal_value(data_bytes, signal_def['start_bit'], signal_def['length'])
                formatted_value = self.format_signal_value(value)
                
                if signal_def['values'] is not None:
                    # Show both decoded value and raw numeric value
                    print(f"   • {signal_name} = {formatted_value} (raw: {raw_value})")
                else:
                    # For raw numeric signals, just show the value
                    print(f"   • {signal_name} = {formatted_value}")
            
            print()
            print(f"📝 Raw signal extraction details:")
            msg_def = CAN_MESSAGES[can_id]
            data_int = int.from_bytes(data_bytes, byteorder='little')
            print(f"   Data as 64-bit int (little-endian): 0x{data_int:016X}")
            
            for signal_name, signal_def in msg_def['signals'].items():
                start_bit = signal_def['start_bit']
                length = signal_def['length']
                bit_pos = start_bit - length + 1
                mask = (1 << length) - 1
                raw_value = (data_int >> bit_pos) & mask
                
                print(f"   • {signal_name}:")
                print(f"     - Start bit: {start_bit}, Length: {length} bits")
                print(f"     - Bit position: {bit_pos}, Mask: 0x{mask:X}")
                print(f"     - Raw value: {raw_value} (0x{raw_value:X})")
            
            return True
        else:
            print(f"❌ Failed to decode message")
            return False


def main():
    parser = argparse.ArgumentParser(
        description='CAN Message Debugger - Debug individual CAN messages using embedded logger logic',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Parse CAN log format
    python can_message_debugger.py "CAN RX: ID=0x331 (817), Len=8, Data=[00 0F 00 00 05 20 44 10]"
    
    # Use explicit ID and data
    python can_message_debugger.py --id 0x331 --data "00 0F 00 00 05 20 44 10"
    python can_message_debugger.py --id 817 --data "00:0F:00:00:05:20:44:10"
    
    # Simple format
    python can_message_debugger.py "0x331: 00 0F 00 00 05 20 44 10"
    
    # List supported messages
    python can_message_debugger.py --list

Supported Messages:
    • 0x3C3 (963): BCM_Lamp_Stat_FD1 - PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
    • 0x331 (817): Locking_Systems_2_FD1 - Veh_Lock_Status
    • 0x176 (374): PowertrainData_10 - TrnPrkSys_D_Actl
    • 0x43C (1084): Battery_Mgmt_3_FD1 - BSBattSOC
        """
    )
    
    parser.add_argument('message_string', nargs='?', help='CAN message string to parse and debug')
    parser.add_argument('--list', action='store_true', help='List supported CAN messages and exit')
    
    parser.add_argument('--id', type=str, help='CAN ID (hex with 0x prefix or decimal)')
    parser.add_argument('--data', type=str, help='CAN data bytes (hex, space or colon separated)')
    
    args = parser.parse_args()
    
    debugger = CANMessageDebugger()
    
    # Handle --list option
    if args.list:
        print("Supported CAN Messages:")
        print("="*50)
        for can_id, msg_def in CAN_MESSAGES.items():
            print(f"• 0x{can_id:03X} ({can_id}): {msg_def['name']}")
            for signal_name, signal_def in msg_def['signals'].items():
                if signal_def['values']:
                    values_str = ", ".join(f"{k}={v}" for k, v in list(signal_def['values'].items())[:3])
                    if len(signal_def['values']) > 3:
                        values_str += "..."
                    print(f"    - {signal_name}: {values_str}")
                else:
                    print(f"    - {signal_name}: Raw numeric value")
            print()
        return 0
    
    # Handle explicit --id and --data
    if args.id and args.data:
        try:
            # Parse CAN ID
            if args.id.startswith('0x') or args.id.startswith('0X'):
                can_id = int(args.id, 16)
            else:
                can_id = int(args.id)
            
            # Parse data
            can_id, data_bytes = debugger._parse_data_bytes(can_id, args.data)
            if can_id is None:
                return 1
                
        except ValueError as e:
            print(f"Error parsing ID or data: {e}", file=sys.stderr)
            return 1
    
    # Handle message string
    elif args.message_string:
        can_id, data_bytes = debugger.parse_can_string(args.message_string)
        if can_id is None:
            print(f"Error: Could not parse CAN message string", file=sys.stderr)
            print(f"Input: {args.message_string}", file=sys.stderr)
            return 1
    
    else:
        parser.print_help()
        return 1
    
    # Debug the message
    success = debugger.debug_message(can_id, data_bytes)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
