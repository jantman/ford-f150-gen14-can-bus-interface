#!/usr/bin/env python3
"""
DBC Configuration Generator for CAN Standalone Logger

This script parses a DBC file to extract message and signal definitions,
then generates the MESSAGE_CONFIG dictionary format needed by can_standalone_logger.py.

Usage:
    python dbc_config_generator.py --messages BCM_Lamp_Stat_FD1 Locking_Systems_2_FD1 PowertrainData_10
    python dbc_config_generator.py --all
    python dbc_config_generator.py --file my_messages.txt

The script will output the MESSAGE_CONFIG dictionary that can be copied into
can_standalone_logger.py to replace the hardcoded configuration.
"""

import argparse
import re
import sys
from pathlib import Path


class DBCParser:
    def __init__(self, dbc_file_path):
        self.dbc_file_path = Path(dbc_file_path)
        self.messages = {}
        self.value_tables = {}
        self.parsed = False
    
    def parse(self):
        """Parse the DBC file and extract message and signal definitions."""
        if not self.dbc_file_path.exists():
            raise FileNotFoundError(f"DBC file not found: {self.dbc_file_path}")
        
        print(f"Parsing DBC file: {self.dbc_file_path}", file=sys.stderr)
        
        with open(self.dbc_file_path, 'r') as f:
            content = f.read()
        
        # Parse value tables first
        self._parse_value_tables(content)
        
        # Parse messages and signals
        self._parse_messages(content)
        
        self.parsed = True
        print(f"Found {len(self.messages)} messages and {len(self.value_tables)} value tables", file=sys.stderr)
    
    def _parse_value_tables(self, content):
        """Parse VAL_TABLE_ definitions."""
        val_table_pattern = r'VAL_TABLE_\s+(\w+)\s+((?:\d+\s+"[^"]*"\s*)+);'
        
        for match in re.finditer(val_table_pattern, content):
            table_name = match.group(1)
            table_content = match.group(2)
            
            # Parse value-name pairs
            value_pairs = re.findall(r'(\d+)\s+"([^"]*)"', table_content)
            
            self.value_tables[table_name] = {
                int(value): name for value, name in value_pairs
            }
    
    def _parse_messages(self, content):
        """Parse BO_ (message) definitions and their signals."""
        # Pattern to match message definitions
        message_pattern = r'BO_\s+(\d+)\s+(\w+):\s+(\d+)\s+(\w+)\s*\n((?:\s+SG_[^\n]+\n?)*)'
        
        for match in re.finditer(message_pattern, content, re.MULTILINE):
            can_id = int(match.group(1))
            msg_name = match.group(2)
            dlc = int(match.group(3))
            sender = match.group(4)
            signals_block = match.group(5)
            
            # Parse signals for this message
            signals = self._parse_signals(signals_block)
            
            self.messages[msg_name] = {
                'can_id': can_id,
                'dlc': dlc,
                'sender': sender,
                'signals': signals
            }
    
    def _parse_signals(self, signals_block):
        """Parse SG_ (signal) definitions within a message."""
        signals = {}
        
        # Pattern to match signal definitions
        signal_pattern = r'SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(([^,]+),([^)]+)\)\s*\[([^|]*)\|([^\]]*)\]\s*"([^"]*)"(?:\s+(.+))?'
        
        for match in re.finditer(signal_pattern, signals_block):
            signal_name = match.group(1)
            start_bit = int(match.group(2))
            length = int(match.group(3))
            byte_order = int(match.group(4))  # 0=big endian, 1=little endian
            sign = match.group(5)  # + or -
            scale = float(match.group(6))
            offset = float(match.group(7))
            min_val = match.group(8).strip()
            max_val = match.group(9).strip()
            unit = match.group(10)
            receivers = match.group(11) if match.group(11) else ""
            
            # Convert to the format expected by can_standalone_logger.py
            # Note: Ford DBC appears to use Intel (little endian) format mostly
            signed = (sign == '-')
            
            signals[signal_name] = {
                'start_bit': start_bit,
                'length': length,
                'scale': scale,
                'offset': offset,
                'signed': signed,
                'byte_order': byte_order,
                'unit': unit,
                'min_val': min_val,
                'max_val': max_val
            }
        
        return signals
    
    def _parse_signal_values(self, content):
        """Parse VAL_ definitions that assign value tables to signals."""
        signal_values = {}
        
        # Pattern to match signal value assignments
        val_pattern = r'VAL_\s+(\d+)\s+(\w+)\s+((?:\d+\s+"[^"]*"\s*)+);'
        
        for match in re.finditer(val_pattern, content):
            can_id = int(match.group(1))
            signal_name = match.group(2)
            values_content = match.group(3)
            
            # Parse value-name pairs
            value_pairs = re.findall(r'(\d+)\s+"([^"]*)"', values_content)
            values_dict = {int(value): name for value, name in value_pairs}
            
            if can_id not in signal_values:
                signal_values[can_id] = {}
            signal_values[can_id][signal_name] = values_dict
        
        return signal_values
    
    def get_message_config(self, message_names=None):
        """Generate MESSAGE_CONFIG dictionary for specified messages."""
        if not self.parsed:
            self.parse()
        
        # Parse signal value assignments
        with open(self.dbc_file_path, 'r') as f:
            content = f.read()
        signal_values = self._parse_signal_values(content)
        
        config = {}
        
        # If no message names specified, use all messages
        if message_names is None:
            message_names = list(self.messages.keys())
        
        for msg_name in message_names:
            if msg_name not in self.messages:
                print(f"Warning: Message '{msg_name}' not found in DBC file", file=sys.stderr)
                continue
            
            msg_data = self.messages[msg_name]
            can_id = msg_data['can_id']
            
            # Convert signals to the format expected by can_standalone_logger.py
            signals_config = {}
            
            for signal_name, signal_data in msg_data['signals'].items():
                start_bit = signal_data['start_bit']
                length = signal_data['length']
                scale = signal_data['scale']
                offset = signal_data['offset']
                signed = signal_data['signed']
                
                # Check if this signal has value mappings
                enum_values = None
                if can_id in signal_values and signal_name in signal_values[can_id]:
                    enum_values = signal_values[can_id][signal_name]
                
                # Format: (start_bit, length, scale, offset, signed, enum_values)
                signals_config[signal_name] = (start_bit, length, scale, offset, signed, enum_values)
            
            config[msg_name] = {
                'can_id': can_id,
                'signals': signals_config
            }
        
        return config
    
    def format_config_output(self, config):
        """Format the config dictionary as Python code."""
        lines = ["MESSAGE_CONFIG = {"]
        
        for msg_name, msg_config in config.items():
            lines.append(f'    "{msg_name}": {{')
            lines.append(f'        "can_id": 0x{msg_config["can_id"]:X},')
            lines.append(f'        "signals": {{')
            
            for signal_name, signal_tuple in msg_config["signals"].items():
                start_bit, length, scale, offset, signed, enum_values = signal_tuple
                
                # Format enum_values nicely
                if enum_values:
                    enum_str = "{"
                    enum_items = []
                    for value, name in sorted(enum_values.items()):
                        enum_items.append(f'{value}: "{name}"')
                    enum_str += ", ".join(enum_items) + "}"
                else:
                    enum_str = "None"
                
                lines.append(f'            "{signal_name}": ({start_bit}, {length}, {scale}, {offset}, {signed}, {enum_str}),')
            
            lines.append(f'        }}')
            lines.append(f'    }},')
        
        lines.append("}")
        
        return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Generate MESSAGE_CONFIG for CAN Standalone Logger from DBC file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Generate config for specific messages
    python dbc_config_generator.py --messages BCM_Lamp_Stat_FD1 Locking_Systems_2_FD1
    
    # Generate config for all messages in DBC
    python dbc_config_generator.py --all
    
    # Read message names from file
    python dbc_config_generator.py --file message_list.txt
    
    # Use different DBC file
    python dbc_config_generator.py --dbc custom.dbc --messages BCM_Lamp_Stat_FD1
        """
    )
    
    parser.add_argument(
        '--dbc',
        default='ford_lincoln_base_pt.dbc',
        help='Path to DBC file (default: ford_lincoln_base_pt.dbc)'
    )
    
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        '--messages',
        nargs='+',
        help='List of message names to extract'
    )
    group.add_argument(
        '--all',
        action='store_true',
        help='Extract all messages from DBC file'
    )
    group.add_argument(
        '--file',
        help='File containing list of message names (one per line)'
    )
    
    parser.add_argument(
        '--output',
        help='Output file (default: stdout)'
    )
    
    args = parser.parse_args()
    
    try:
        # Initialize parser
        dbc_parser = DBCParser(args.dbc)
        
        # Determine which messages to extract
        if args.all:
            message_names = None  # Will extract all messages
        elif args.file:
            with open(args.file, 'r') as f:
                message_names = [line.strip() for line in f if line.strip()]
        else:
            message_names = args.messages
        
        # Generate configuration
        config = dbc_parser.get_message_config(message_names)
        
        if not config:
            print("No messages found or extracted", file=sys.stderr)
            return 1
        
        # Format output
        output_text = dbc_parser.format_config_output(config)
        
        # Write output
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output_text)
            print(f"Configuration written to {args.output}", file=sys.stderr)
        else:
            print(output_text)
        
        # Print summary to stderr
        print(f"\nGenerated configuration for {len(config)} messages:", file=sys.stderr)
        for msg_name, msg_config in config.items():
            signal_count = len(msg_config['signals'])
            can_id = msg_config['can_id']
            print(f"  - {msg_name} (0x{can_id:X}): {signal_count} signals", file=sys.stderr)
        
        return 0
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())