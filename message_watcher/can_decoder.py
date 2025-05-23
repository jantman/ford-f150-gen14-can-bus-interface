#!/usr/bin/env python3
"""
CAN Bus Message Decoder

This script listens to CAN messages on a SocketCAN interface, decodes them using
a DBC file, and prints the decoded messages to stdout while also logging them
to a file with timestamps.

Usage:
    python can_decoder.py <can_interface>

Examples:
    python can_decoder.py can0
    python can_decoder.py slcan0
    python can_decoder.py can0 --changes-only

The script will use the ford_lincoln_base_pt.dbc file in the same directory
to decode messages and log both raw and decoded messages to a timestamped log file.
"""

import argparse
import sys
import time
import json
import os
from datetime import datetime
import can
import cantools

# Module-level constant defining which DBC messages to filter for
# Add message names here to limit which messages are captured
FILTERED_MESSAGE_NAMES = [
    "BCM_Lamp_Stat_FD1",
    "Locking_Systems_2_FD1"
]


class CANDecoder:
    def __init__(self, can_interface, dbc_file="ford_lincoln_base_pt.dbc", changes_only=False):
        """
        Initialize the CAN decoder.

        Args:
            can_interface: CAN interface name (e.g., 'can0')
            dbc_file: Path to the DBC file for message decoding
            changes_only: If True, only display/log messages when content changes
        """
        self.can_interface = can_interface
        self.dbc_file = dbc_file
        self.changes_only = changes_only
        self.bus = None
        self.db = None
        self.log_file = None
        self.start_time = time.time()
        self.filtered_message_ids = set()
        
        # Track last seen message data for change detection
        self.last_message_data = {}  # {arbitration_id: bytes}
        
        # Track last seen decoded signals for signal-level change detection
        self.last_decoded_signals = {}  # {arbitration_id: {signal_name: value}}
        
        # Create log filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        mode_suffix = "_changes" if changes_only else ""
        self.log_filename = f"can_decoded_{timestamp}_{can_interface}{mode_suffix}.log"
        
        print(f"CAN Decoder initialized")
        print(f"Interface: {can_interface}")
        print(f"DBC file: {dbc_file}")
        print(f"Changes only mode: {'ON' if changes_only else 'OFF'}")
        print(f"Log file: {self.log_filename}")

    def has_message_changed(self, msg):
        """
        Check if a message's content has changed since last seen.
        
        Args:
            msg: python-can Message object
            
        Returns:
            bool: True if this is the first time seeing this message ID or if content changed
        """
        msg_id = msg.arbitration_id
        current_data = bytes(msg.data)
        
        # Check if we've seen this message ID before
        if msg_id not in self.last_message_data:
            # First time seeing this message ID
            self.last_message_data[msg_id] = current_data
            return True
        
        # Compare with last seen data
        if self.last_message_data[msg_id] != current_data:
            # Content has changed
            self.last_message_data[msg_id] = current_data
            return True
        
        # No change
        return False

    def get_changed_signals(self, msg, decoded_data):
        """
        Get the signals that have changed since the last time this message was seen.
        
        Args:
            msg: python-can Message object
            decoded_data: Decoded message data containing signals
            
        Returns:
            dict: Dictionary of changed signals {signal_name: value}
            bool: True if this is the first time seeing this message
        """
        msg_id = msg.arbitration_id
        
        # If no decoded signals, return empty
        if not decoded_data or 'signals' not in decoded_data:
            return {}, False
        
        current_signals = decoded_data['signals']
        
        # Check if we've seen this message ID before
        if msg_id not in self.last_decoded_signals:
            # First time seeing this message - all signals are "changed"
            self.last_decoded_signals[msg_id] = current_signals.copy()
            return current_signals, True
        
        # Find changed signals
        changed_signals = {}
        last_signals = self.last_decoded_signals[msg_id]
        
        for signal_name, current_value in current_signals.items():
            # Check if signal is new or value has changed
            if (signal_name not in last_signals or 
                last_signals[signal_name] != current_value):
                changed_signals[signal_name] = current_value
        
        # Update our stored signals
        self.last_decoded_signals[msg_id] = current_signals.copy()
        
        return changed_signals, False

    def load_dbc(self):
        """Load the DBC file for message decoding and build message filters."""
        try:
            if not os.path.exists(self.dbc_file):
                print(f"Error: DBC file '{self.dbc_file}' not found")
                return False
                
            print(f"Loading DBC file: {self.dbc_file}")
            self.db = cantools.database.load_file(self.dbc_file)
            print(f"Successfully loaded DBC with {len(self.db.messages)} message definitions")
            
            # Print some basic info about the DBC
            print(f"DBC version: {getattr(self.db, 'version', 'Unknown')}")
            print(f"Available messages: {len(self.db.messages)}")
            
            # Build filtered message IDs from the constant
            if FILTERED_MESSAGE_NAMES:
                print(f"\nBuilding message filters for {len(FILTERED_MESSAGE_NAMES)} message(s):")
                for msg_name in FILTERED_MESSAGE_NAMES:
                    try:
                        msg = self.db.get_message_by_name(msg_name)
                        self.filtered_message_ids.add(msg.frame_id)
                        print(f"  - {msg_name}: 0x{msg.frame_id:X} ({msg.frame_id})")
                    except KeyError:
                        print(f"  - WARNING: Message '{msg_name}' not found in DBC")
                
                if self.filtered_message_ids:
                    print(f"\nFiltering enabled: Will only capture {len(self.filtered_message_ids)} message ID(s)")
                else:
                    print(f"\nWARNING: No valid message IDs found for filtering - will capture all messages")
            else:
                print(f"\nNo message filtering configured - will capture all messages")
            
            return True
        except Exception as e:
            print(f"Error loading DBC file: {e}")
            return False

    def build_can_filters(self):
        """
        Build CAN filters for the python-can Bus.
        
        Returns:
            list: List of filter dictionaries for python-can
        """
        if not self.filtered_message_ids:
            # No filtering - return None to accept all messages
            return None
        
        # Build filters list - each filter accepts one specific CAN ID
        filters = []
        for can_id in self.filtered_message_ids:
            filters.append({
                "can_id": can_id,
                "can_mask": 0x7FF,  # Exact match for 11-bit CAN ID
                "extended": False
            })
        
        return filters

    def connect_can(self):
        """Connect to the CAN bus interface with message filtering."""
        try:
            # Build CAN filters based on our filtered message IDs
            can_filters = self.build_can_filters()
            
            if can_filters:
                print(f"Applying CAN filters for {len(can_filters)} message ID(s)")
                for f in can_filters:
                    print(f"  - Filter: CAN ID 0x{f['can_id']:X}")
            else:
                print(f"No CAN filters applied - listening to all messages")
            
            self.bus = can.interface.Bus(
                channel=self.can_interface,
                bustype='socketcan',
                can_filters=can_filters
            )
            print(f"Successfully connected to {self.can_interface}")
            return True
        except Exception as e:
            print(f"Error connecting to CAN bus: {e}")
            return False

    def open_log_file(self):
        """Open the log file for writing."""
        try:
            self.log_file = open(self.log_filename, 'w')
            # Write header
            header = {
                'log_start_time': datetime.now().isoformat(),
                'can_interface': self.can_interface,
                'dbc_file': self.dbc_file,
                'format': 'Each line contains a JSON object with timestamp, raw_message, and decoded_message (if available)'
            }
            self.log_file.write(f"# {json.dumps(header)}\n")
            self.log_file.flush()
            return True
        except Exception as e:
            print(f"Error opening log file: {e}")
            return False

    def decode_message(self, msg):
        """
        Decode a CAN message using the loaded DBC.

        Args:
            msg: python-can Message object

        Returns:
            dict: Decoded message data or None if not found in DBC
        """
        try:
            # Find the message definition in the DBC
            dbc_message = self.db.get_message_by_frame_id(msg.arbitration_id)
            
            # Decode the message
            decoded_data = dbc_message.decode(msg.data)
            
            return {
                'message_name': dbc_message.name,
                'frame_id': msg.arbitration_id,
                'frame_id_hex': f"0x{msg.arbitration_id:X}",
                'signals': decoded_data,
                'signal_count': len(decoded_data)
            }
            
        except KeyError:
            # Message ID not found in DBC
            return None
        except Exception as e:
            # Other decoding errors
            return {
                'error': f"Decoding error: {str(e)}",
                'frame_id': msg.arbitration_id,
                'frame_id_hex': f"0x{msg.arbitration_id:X}"
            }

    def format_message_output(self, msg, decoded_data, timestamp_rel, is_changed=None):
        """
        Format message for console output.

        Args:
            msg: python-can Message object
            decoded_data: Decoded message data from decode_message()
            timestamp_rel: Relative timestamp since start
            is_changed: Whether this message has changed (for display purposes)

        Returns:
            str: Formatted message string
        """
        timestamp_str = f"{timestamp_rel:8.3f}s"
        frame_id_str = f"0x{msg.arbitration_id:03X}"
        dlc_str = f"DLC:{msg.dlc}"
        data_str = " ".join([f"{b:02X}" for b in msg.data])
        
        # Add change indicator if in changes-only mode
        change_indicator = ""
        if self.changes_only and is_changed is not None:
            if msg.arbitration_id not in self.last_message_data or len(self.last_message_data) == 1:
                change_indicator = " [NEW]"
            else:
                change_indicator = " [CHANGED]"
        
        if decoded_data and 'message_name' in decoded_data:
            # Successfully decoded message
            msg_name = decoded_data['message_name']
            signal_count = decoded_data.get('signal_count', 0)
            
            output = f"{timestamp_str} {frame_id_str} {dlc_str} [{data_str}]{change_indicator} -> {msg_name} ({signal_count} signals)"
            
            # In changes-only mode, show only changed signals; otherwise show first 3
            if self.changes_only and 'signals' in decoded_data:
                changed_signals, is_first_time = self.get_changed_signals(msg, decoded_data)
                signals_to_show = changed_signals
                signal_type = "changed" if not is_first_time else "all"
            elif 'signals' in decoded_data:
                # Normal mode - show first 3 signals
                signals_to_show = dict(list(decoded_data['signals'].items())[:3])
                signal_type = "first 3"
            else:
                signals_to_show = {}
                signal_type = ""
            
            if signals_to_show:
                key_signals = []
                for signal_name, value in signals_to_show.items():
                    if isinstance(value, (int, float)):
                        if isinstance(value, float):
                            key_signals.append(f"{signal_name}={value:.2f}")
                        else:
                            key_signals.append(f"{signal_name}={value}")
                    else:
                        key_signals.append(f"{signal_name}={value}")
                
                output += f" | {', '.join(key_signals)}"
                
                # Add count info for clarity
                if self.changes_only:
                    if not is_first_time and len(changed_signals) < signal_count:
                        output += f" ({len(changed_signals)}/{signal_count} changed)"
                else:
                    if len(decoded_data['signals']) > 3:
                        output += f" (+{len(decoded_data['signals'])-3} more)"
            
        elif decoded_data and 'error' in decoded_data:
            # Decoding error
            output = f"{timestamp_str} {frame_id_str} {dlc_str} [{data_str}]{change_indicator} -> DECODE_ERROR: {decoded_data['error']}"
        else:
            # Unknown message (not in DBC)
            output = f"{timestamp_str} {frame_id_str} {dlc_str} [{data_str}]{change_indicator} -> UNKNOWN"
        
        return output

    def log_message(self, msg, decoded_data, timestamp_abs, timestamp_rel, is_changed=False):
        """
        Log message to file in JSON format.

        Args:
            msg: python-can Message object  
            decoded_data: Decoded message data
            timestamp_abs: Absolute timestamp
            timestamp_rel: Relative timestamp since start
            is_changed: Whether this message content has changed
        """
        try:
            log_entry = {
                'timestamp_abs': timestamp_abs,
                'timestamp_rel': timestamp_rel,
                'is_changed': is_changed,
                'raw_message': {
                    'arbitration_id': msg.arbitration_id,
                    'arbitration_id_hex': f"0x{msg.arbitration_id:X}",
                    'dlc': msg.dlc,
                    'data': list(msg.data),
                    'data_hex': " ".join([f"{b:02X}" for b in msg.data]),
                    'is_extended_id': msg.is_extended_id,
                    'is_remote_frame': msg.is_remote_frame,
                    'is_error_frame': msg.is_error_frame
                }
            }
            
            if decoded_data:
                # Make a copy of decoded_data and convert any NamedSignalValue objects
                json_safe_decoded = {}
                for key, value in decoded_data.items():
                    if key == 'signals' and isinstance(value, dict):
                        # Convert signals dictionary to JSON-safe format
                        json_safe_signals = {}
                        for signal_name, signal_value in value.items():
                            json_safe_signals[signal_name] = self._make_json_safe(signal_value)
                        json_safe_decoded[key] = json_safe_signals
                    else:
                        json_safe_decoded[key] = self._make_json_safe(value)
                
                log_entry['decoded_message'] = json_safe_decoded
            
            self.log_file.write(json.dumps(log_entry) + '\n')
            self.log_file.flush()
            
        except Exception as e:
            print(f"Error logging message: {e}")

    def _make_json_safe(self, value):
        """
        Convert values to JSON-serializable format.
        
        Args:
            value: Any value that might not be JSON serializable
            
        Returns:
            JSON-serializable version of the value
        """
        # Handle NamedSignalValue objects from cantools
        if hasattr(value, 'name') and hasattr(value, 'value'):
            # This is a NamedSignalValue - return both the name and numeric value
            return {
                'name': str(value.name),
                'value': value.value
            }
        elif hasattr(value, '__dict__'):
            # Other objects with attributes - try to convert to string
            return str(value)
        else:
            # Already JSON-safe (int, float, str, bool, None, list, dict)
            return value

    def run(self):
        """Main message processing loop."""
        if not self.load_dbc():
            return False
            
        if not self.connect_can():
            return False
            
        if not self.open_log_file():
            return False
        
        print("\n" + "="*80)
        print("CAN MESSAGE DECODER - Listening for messages...")
        if self.filtered_message_ids:
            print(f"FILTERING: Only capturing {len(self.filtered_message_ids)} specific message ID(s)")
            for msg_name in FILTERED_MESSAGE_NAMES:
                try:
                    msg = self.db.get_message_by_name(msg_name)
                    print(f"  - {msg_name} (0x{msg.frame_id:X})")
                except KeyError:
                    pass
        else:
            print("NO FILTERING: Capturing all CAN messages")
        
        if self.changes_only:
            print("CHANGES ONLY MODE: Only showing messages when content changes")
        
        print("Press Ctrl+C to stop")
        print("="*80)
        print(f"{'Time':>8} {'ID':>7} {'DLC':>6} {'Data':<24} {'Decoded Message'}")
        print("-"*80)
        
        message_count = 0
        decoded_count = 0
        displayed_count = 0  # Count of messages actually displayed (in changes-only mode)
        
        try:
            while True:
                # Receive message with 1 second timeout
                msg = self.bus.recv(timeout=1.0)
                
                if msg:
                    current_time = time.time()
                    timestamp_rel = current_time - self.start_time
                    timestamp_abs = datetime.now().isoformat()
                    
                    # Check if message has changed (if in changes-only mode)
                    message_changed = True  # Default to always show
                    if self.changes_only:
                        message_changed = self.has_message_changed(msg)
                    
                    # Only process and display if message should be shown
                    if message_changed or not self.changes_only:
                        # Decode the message
                        decoded_data = self.decode_message(msg)
                        
                        # Print to console
                        output = self.format_message_output(msg, decoded_data, timestamp_rel, message_changed)
                        print(output)
                        
                        # Log to file (always log when shown)
                        self.log_message(msg, decoded_data, timestamp_abs, timestamp_rel, message_changed)
                        
                        displayed_count += 1
                        if decoded_data and 'message_name' in decoded_data:
                            decoded_count += 1
                    
                    message_count += 1
                        
                    # Print statistics every 100 messages (based on total received)
                    if message_count % 100 == 0:
                        decode_rate = (decoded_count / displayed_count * 100) if displayed_count > 0 else 0
                        if self.changes_only:
                            print(f"\n--- Stats: {message_count} total, {displayed_count} displayed, {decoded_count} decoded ({decode_rate:.1f}%) ---\n")
                        else:
                            print(f"\n--- Stats: {message_count} messages, {decoded_count} decoded ({decode_rate:.1f}%) ---\n")
                        
        except KeyboardInterrupt:
            print(f"\n\nReceived Ctrl+C, stopping...")
            
        finally:
            decode_rate = (decoded_count / displayed_count * 100) if displayed_count > 0 else 0
            print(f"\n" + "="*80)
            print(f"SESSION SUMMARY")
            print(f"="*80)
            print(f"Total messages received: {message_count}")
            if self.changes_only:
                print(f"Messages displayed (changed): {displayed_count}")
                unique_ids = len(self.last_message_data)
                print(f"Unique message IDs seen: {unique_ids}")
            print(f"Successfully decoded: {decoded_count}")
            print(f"Decode rate: {decode_rate:.1f}%")
            print(f"Session duration: {time.time() - self.start_time:.1f} seconds")
            print(f"Log file: {self.log_filename}")
            
            # Close resources
            if self.log_file:
                self.log_file.close()
                print(f"Log file closed: {self.log_filename}")
                
            if self.bus:
                self.bus.shutdown()
                print(f"Disconnected from {self.can_interface}")
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description='CAN Bus Message Decoder using DBC file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_decoder.py can0
    python can_decoder.py slcan0
    python can_decoder.py can0 --changes-only

The script will use ford_lincoln_base_pt.dbc in the current directory to decode messages.
All messages (raw and decoded) are logged to a timestamped file.

With --changes-only, only messages that have changed content or are seen for the 
first time will be displayed and logged.
        """
    )
    
    parser.add_argument(
        'can_interface',
        help='CAN interface name (e.g., can0, slcan0)'
    )
    
    parser.add_argument(
        '--dbc',
        default='ford_lincoln_base_pt.dbc',
        help='DBC file path (default: ford_lincoln_base_pt.dbc)'
    )
    
    parser.add_argument(
        '--changes-only',
        action='store_true',
        help='Only display and log messages when their content changes or when first seen'
    )
    
    args = parser.parse_args()
    
    # Create and run the decoder
    decoder = CANDecoder(args.can_interface, args.dbc, args.changes_only)
    
    try:
        success = decoder.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()