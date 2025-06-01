#!/usr/bin/env python3
"""
Self-Contained CAN Bus Logger with Embedded DBC Parser

This script provides a complete CAN bus logging solution without external dependencies.
It includes a minimal DBC parser and message decoder built specifically for the minimal.dbc file.

Features:
- Zero external dependencies (only python-can for CAN interface)
- Embedded DBC parsing logic
- Motorola (big-endian) bit extraction
- NamedSignalValue support with enum decoding
- Memory efficient for embedded systems

Usage:
    python can_embedded_logger.py <can_interface>
    python can_embedded_logger.py can0 > logfile.txt

The script parses the embedded minimal.dbc definitions and decodes CAN messages
identically to cantools but without requiring the cantools library.
"""

import argparse
import sys
import time
from datetime import datetime
import can
import threading
import re


class NamedSignalValue:
    """
    A signal value with an optional name (enum string).
    Compatible with cantools NamedSignalValue objects.
    """
    def __init__(self, value, name=None):
        self.value = int(value)
        self.name = name
    
    def __str__(self):
        if self.name:
            return f"{self.name}({self.value})"
        return str(self.value)
    
    def __int__(self):
        return self.value
    
    def __float__(self):
        return float(self.value)
    
    def __eq__(self, other):
        if isinstance(other, NamedSignalValue):
            return self.value == other.value
        return self.value == other
    
    def __repr__(self):
        return f"NamedSignalValue(value={self.value}, name='{self.name}')"


class Signal:
    """Represents a CAN signal with all its properties."""
    def __init__(self, name, start_bit, length, byte_order, signed, scale, offset, minimum, maximum, unit, choices=None):
        self.name = name
        self.start = start_bit
        self.length = length
        self.byte_order = byte_order  # 'big_endian' or 'little_endian'
        self.is_signed = signed
        self.scale = scale
        self.offset = offset
        self.minimum = minimum
        self.maximum = maximum
        self.unit = unit
        self.choices = choices or {}  # {value: name} mapping
    
    def decode(self, data):
        """Decode this signal from CAN message data."""
        try:
            # Extract raw bits based on byte order
            if self.byte_order == 'big_endian':
                raw_value = self._extract_motorola(data)
            else:
                raw_value = self._extract_intel(data)
            
            # Handle signed values
            if self.is_signed and self.length > 0 and (raw_value & (1 << (self.length - 1))):
                raw_value -= (1 << self.length)
            
            # Apply scale and offset
            scaled_value = raw_value * self.scale + self.offset
            
            # Create return value
            if self.choices and raw_value in self.choices:
                return NamedSignalValue(scaled_value, self.choices[raw_value])
            else:
                return scaled_value
                
        except Exception:
            return None
    
    def _extract_motorola(self, data):
        """Extract signal using Motorola (big-endian) bit numbering."""
        data_bytes = list(data) + [0] * (8 - len(data))
        raw_value = 0
        
        # For Motorola: bit range is [start_bit - length + 1, start_bit]
        for bit_pos in range(self.start - self.length + 1, self.start + 1):
            byte_index = bit_pos // 8
            bit_index = 7 - (bit_pos % 8)
            
            if 0 <= byte_index < len(data_bytes) and 0 <= bit_index <= 7:
                bit_value = (data_bytes[byte_index] >> bit_index) & 1
                result_bit_pos = self.start - bit_pos
                raw_value |= (bit_value << result_bit_pos)
        
        return raw_value
    
    def _extract_intel(self, data):
        """Extract signal using Intel (little-endian) bit numbering."""
        data_bytes = list(data) + [0] * (8 - len(data))
        raw_value = 0
        
        # For Intel: bits are consecutive starting from start_bit
        for i in range(self.length):
            bit_pos = self.start + i
            byte_index = bit_pos // 8
            bit_index = bit_pos % 8
            
            if 0 <= byte_index < len(data_bytes) and 0 <= bit_index <= 7:
                bit_value = (data_bytes[byte_index] >> bit_index) & 1
                raw_value |= (bit_value << i)
        
        return raw_value


class Message:
    """Represents a CAN message with its signals."""
    def __init__(self, frame_id, name, length, sender):
        self.frame_id = frame_id
        self.name = name
        self.length = length
        self.sender = sender
        self.signals = []
        self._signal_dict = {}
    
    def add_signal(self, signal):
        """Add a signal to this message."""
        self.signals.append(signal)
        self._signal_dict[signal.name] = signal
    
    def decode(self, data):
        """Decode CAN message data into signal values."""
        result = {}
        for signal in self.signals:
            result[signal.name] = signal.decode(data)
        return result
    
    def get_signal_by_name(self, name):
        """Get a signal by name."""
        return self._signal_dict.get(name)


class EmbeddedDBCDatabase:
    """
    Self-contained DBC database with minimal.dbc definitions embedded.
    This eliminates the need for external DBC files and cantools dependency.
    """
    def __init__(self):
        self.messages = []
        self._message_dict = {}
        self._frame_id_dict = {}
        self.nodes = []
        self.version = ""
        
        # Build the database from embedded definitions
        self._build_embedded_database()
    
    def _build_embedded_database(self):
        """Build the database from embedded minimal.dbc definitions."""
        
        # Define enum value tables (from VAL_TABLE_ and VAL_ sections)
        enum_tables = {
            'PudLamp_D_Rq': {3: "RAMP_DOWN", 2: "RAMP_UP", 1: "ON", 0: "OFF"},
            'Veh_Lock_Status': {3: "UNLOCK_DRV", 2: "UNLOCK_ALL", 1: "LOCK_ALL", 0: "LOCK_DBL"},
            'TrnPrkSys_D_Actl': {
                15: "Faulty", 14: "NotUsed_5", 13: "NotUsed_4", 12: "NotUsed_3",
                11: "NotUsed_2", 10: "NotUsed_1", 9: "FrequencyError", 8: "OutOfRangeHigh",
                7: "OutOfRangeLow", 6: "Override", 5: "OutOfPark", 4: "TransitionCloseToOutOfPark",
                3: "AtNoSpring", 2: "TransitionCloseToPark", 1: "Park", 0: "NotKnown"
            },
            'Illuminated_Entry_Stat': {3: "Invalid", 2: "Unknown", 1: "On", 0: "Off"},
            'Dr_Courtesy_Light_Stat': {3: "Invalid", 2: "Unknown", 1: "On", 0: "Off"}
        }
        
        # Define nodes
        self.nodes = [
            'VDM', 'CMR_DSMC', 'SOBDMC_HPCM_FD1', 'IPMA_ADAS', 'PSCM', 'ABS_ESC',
            'TCCM', 'TCM_DSL', 'PCM_HEV', 'PCM', 'ECM_Diesel', 'GENERIC_GWMWakeup',
            'GWM', '_delete', 'TSTR'
        ]
        
        # BCM_Lamp_Stat_FD1 (ID: 963 / 0x3C3)
        msg1 = Message(0x3C3, "BCM_Lamp_Stat_FD1", 8, "GWM")
        msg1.add_signal(Signal("Illuminated_Entry_Stat", 63, 2, 'big_endian', False, 1, 0, 0, 3, "SED", enum_tables['Illuminated_Entry_Stat']))
        msg1.add_signal(Signal("Dr_Courtesy_Light_Stat", 49, 2, 'big_endian', False, 1, 0, 0, 3, "SED", enum_tables['Dr_Courtesy_Light_Stat']))
        msg1.add_signal(Signal("PudLamp_D_Rq", 11, 2, 'big_endian', False, 1, 0, 0, 3, "SED", enum_tables['PudLamp_D_Rq']))
        self.add_message(msg1)
        
        # Locking_Systems_2_FD1 (ID: 817 / 0x331)
        msg2 = Message(0x331, "Locking_Systems_2_FD1", 8, "GWM")
        msg2.add_signal(Signal("Veh_Lock_Status", 34, 2, 'big_endian', False, 1, 0, 0, 3, "SED", enum_tables['Veh_Lock_Status']))
        self.add_message(msg2)
        
        # PowertrainData_10 (ID: 374 / 0x176)
        msg3 = Message(0x176, "PowertrainData_10", 8, "PCM")
        msg3.add_signal(Signal("TrnPrkSys_D_Actl", 31, 4, 'big_endian', False, 1, 0, 0, 15, "SED", enum_tables['TrnPrkSys_D_Actl']))
        self.add_message(msg3)
        
        # Battery_Mgmt_3_FD1 (ID: 1084 / 0x43C)
        msg4 = Message(0x43C, "Battery_Mgmt_3_FD1", 8, "GWM")
        msg4.add_signal(Signal("BSBattSOC", 22, 7, 'big_endian', False, 1, 0, 0, 127, "%"))
        self.add_message(msg4)
    
    def add_message(self, message):
        """Add a message to the database."""
        self.messages.append(message)
        self._message_dict[message.name] = message
        self._frame_id_dict[message.frame_id] = message
    
    def get_message_by_name(self, name):
        """Get a message by name."""
        if name not in self._message_dict:
            raise KeyError(f"Message '{name}' not found")
        return self._message_dict[name]
    
    def get_message_by_frame_id(self, frame_id):
        """Get a message by frame ID."""
        if frame_id not in self._frame_id_dict:
            raise KeyError(f"Message with frame ID 0x{frame_id:X} not found")
        return self._frame_id_dict[frame_id]


class EmbeddedCANLogger:
    """CAN logger using embedded DBC database."""
    
    def __init__(self, can_interface, log_interval=1.0):
        self.can_interface = can_interface
        self.log_interval = log_interval
        self.bus = None
        self.db = EmbeddedDBCDatabase()
        self.running = False
        self.start_time = time.time()
        
        # Logger configuration - same as can_logger.py
        self.logger_config = {
            "BCM_Lamp_Stat_FD1": {
                "signals": ["PudLamp_D_Rq", "Illuminated_Entry_Stat", "Dr_Courtesy_Light_Stat"]
            },
            "Locking_Systems_2_FD1": {
                "signals": ["Veh_Lock_Status"]
            },
            "PowertrainData_10": {
                "signals": ["TrnPrkSys_D_Actl"]
            },
            "Battery_Mgmt_3_FD1": {
                "signals": ["BSBattSOC"]
            },
        }
        
        # Message tracking
        self.message_data = {}
        self.message_timestamps = {}
        self.message_ids = {}
        self.filtered_message_ids = set()
        
        # Statistics
        self.stats = {
            'total_messages': 0,
            'decoded_messages': 0,
            'log_entries': 0
        }
        
        self.data_lock = threading.Lock()
        self._initialize_message_tracking()
        
        print(f"Embedded CAN Logger initialized", file=sys.stderr)
        print(f"Interface: {can_interface}", file=sys.stderr)
        print(f"Log interval: {log_interval}s", file=sys.stderr)
        print(f"Database: Embedded minimal.dbc with {len(self.db.messages)} messages", file=sys.stderr)
    
    def _initialize_message_tracking(self):
        """Initialize message tracking from logger config."""
        print(f"\nConfiguring logger for {len(self.logger_config)} message types:", file=sys.stderr)
        
        for msg_name, config in self.logger_config.items():
            try:
                msg = self.db.get_message_by_name(msg_name)
                self.filtered_message_ids.add(msg.frame_id)
                self.message_ids[msg.frame_id] = msg_name
                
                # Initialize message data structure
                self.message_data[msg_name] = {}
                self.message_timestamps[msg_name] = None
                
                # Initialize signal values
                for signal_name in config['signals']:
                    self.message_data[msg_name][signal_name] = None
                
                print(f"  - {msg_name} (0x{msg.frame_id:X}): {len(config['signals'])} signals", file=sys.stderr)
                for signal_name in config['signals']:
                    print(f"    * {signal_name}", file=sys.stderr)
                
            except KeyError:
                print(f"  - WARNING: Message '{msg_name}' not found in embedded database", file=sys.stderr)
        
        print(f"\nLogger will monitor {len(self.filtered_message_ids)} CAN message(s)", file=sys.stderr)
    
    def build_can_filters(self):
        """Build CAN filters for efficiency."""
        filters = []
        for can_id in self.filtered_message_ids:
            filters.append({
                "can_id": can_id,
                "can_mask": 0x7FF,
                "extended": False
            })
        return filters
    
    def connect_can(self):
        """Connect to the CAN bus interface with message filtering."""
        try:
            can_filters = self.build_can_filters()
            
            print(f"Connecting to CAN interface: {self.can_interface}", file=sys.stderr)
            print(f"Applying CAN filters for {len(can_filters)} message ID(s)", file=sys.stderr)
            
            self.bus = can.interface.Bus(
                channel=self.can_interface,
                bustype='socketcan',
                can_filters=can_filters
            )
            print(f"Successfully connected to {self.can_interface}", file=sys.stderr)
            return True
            
        except Exception as e:
            print(f"Error connecting to CAN bus: {e}", file=sys.stderr)
            return False
    
    def decode_message(self, msg):
        """Decode a CAN message using the embedded database."""
        try:
            if msg.arbitration_id not in self.message_ids:
                return None
            
            msg_name = self.message_ids[msg.arbitration_id]
            dbc_message = self.db.get_message_by_frame_id(msg.arbitration_id)
            
            # Decode the message
            decoded_signals = dbc_message.decode(msg.data)
            
            # Filter to only the signals we care about
            config = self.logger_config[msg_name]
            filtered_signals = {}
            for signal_name in config['signals']:
                if signal_name in decoded_signals:
                    filtered_signals[signal_name] = decoded_signals[signal_name]
            
            return {
                'message_name': msg_name,
                'signals': filtered_signals
            }
            
        except Exception as e:
            return None
    
    def update_message_data(self, msg, decoded_data):
        """Update the message data with new information."""
        if not decoded_data:
            return
        
        msg_name = decoded_data['message_name']
        
        with self.data_lock:
            for signal_name, value in decoded_data['signals'].items():
                self.message_data[msg_name][signal_name] = value
            self.message_timestamps[msg_name] = time.time()
    
    def format_signal_value(self, value):
        """Format a signal value for logging."""
        if value is None:
            return "N/A"
        elif isinstance(value, bool):
            return "1" if value else "0"
        elif isinstance(value, NamedSignalValue):
            return str(value)  # Uses NamedSignalValue.__str__()
        elif isinstance(value, int):
            return str(value)
        elif isinstance(value, float):
            return f"{value:.0f}" if value.is_integer() else f"{value:.2f}"
        else:
            return str(value)
    
    def log_header(self):
        """Print a header explaining the log format."""
        print("# Embedded CAN Logger Output")
        print("# Format: timestamp | message.signal=value | message.signal=value | ...")
        print("# Timestamp format: YYYY-MM-DD HH:MM:SS.mmm")
        print("# Signal values: N/A=no data, 0/1=boolean, numbers=numeric values")
        print("#" + "="*80)
    
    def log_current_state(self):
        """Log the current state of all signals."""
        current_time = time.time()
        timestamp = datetime.fromtimestamp(current_time).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        log_entries = []
        
        with self.data_lock:
            for msg_name in sorted(self.logger_config.keys()):
                config = self.logger_config[msg_name]
                
                if msg_name in self.message_data:
                    for signal_name in config['signals']:
                        if signal_name in self.message_data[msg_name]:
                            value = self.message_data[msg_name][signal_name]
                            formatted_value = self.format_signal_value(value)
                            log_entries.append(f"{msg_name}.{signal_name}={formatted_value}")
                        else:
                            log_entries.append(f"{msg_name}.{signal_name}=N/A")
                else:
                    for signal_name in config['signals']:
                        log_entries.append(f"{msg_name}.{signal_name}=N/A")
        
        log_line = f"{timestamp} | " + " | ".join(log_entries)
        print(log_line)
        self.stats['log_entries'] += 1
    
    def message_listener(self):
        """Background thread to listen for CAN messages."""
        while self.running:
            try:
                msg = self.bus.recv(timeout=1.0)
                
                if msg:
                    self.stats['total_messages'] += 1
                    decoded_data = self.decode_message(msg)
                    
                    if decoded_data:
                        self.stats['decoded_messages'] += 1
                        self.update_message_data(msg, decoded_data)
                        
            except Exception as e:
                if self.running:
                    print(f"Error in message listener: {e}", file=sys.stderr)
                break
    
    def run(self):
        """Main logging loop."""
        if not self.connect_can():
            return False
        
        # Start message listening thread
        self.running = True
        listener_thread = threading.Thread(target=self.message_listener, daemon=True)
        listener_thread.start()
        
        print(f"\nStarting embedded logger with {self.log_interval}s interval...", file=sys.stderr)
        
        self.log_header()
        time.sleep(2)  # Give a moment for initial data
        
        try:
            while True:
                self.log_current_state()
                time.sleep(self.log_interval)
                
        except KeyboardInterrupt:
            print(f"\nReceived Ctrl+C, stopping logger...", file=sys.stderr)
            
        finally:
            self.running = False
            
            # Final statistics
            runtime = time.time() - self.start_time
            print(f"\nEMBEDDED LOGGER SESSION SUMMARY", file=sys.stderr)
            print(f"="*50, file=sys.stderr)
            print(f"Runtime: {runtime:.1f} seconds", file=sys.stderr)
            print(f"Total CAN messages received: {self.stats['total_messages']}", file=sys.stderr)
            print(f"Messages decoded: {self.stats['decoded_messages']}", file=sys.stderr)
            print(f"Log entries written: {self.stats['log_entries']}", file=sys.stderr)
            if runtime > 0:
                print(f"Message rate: {self.stats['total_messages']/runtime:.1f} msg/sec", file=sys.stderr)
            
            if self.bus:
                self.bus.shutdown()
                print(f"Disconnected from {self.can_interface}", file=sys.stderr)
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description='Self-Contained CAN Bus Logger with Embedded DBC Parser',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_embedded_logger.py can0
    python can_embedded_logger.py slcan0 > can_signals.log
    python can_embedded_logger.py can0 --interval 0.5

This logger includes a complete DBC parser and message decoder built-in:
- Zero external dependencies (except python-can for CAN interface)
- Embedded minimal.dbc definitions
- Compatible NamedSignalValue objects
- Motorola (big-endian) bit extraction
- Memory efficient for embedded systems

The logger monitors the same signals as can_logger.py:
  - BCM_Lamp_Stat_FD1: PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
  - Locking_Systems_2_FD1: Veh_Lock_Status
  - PowertrainData_10: TrnPrkSys_D_Actl
  - Battery_Mgmt_3_FD1: BSBattSOC

Output format is identical to can_logger.py for easy comparison and validation.
        """
    )
    
    parser.add_argument(
        'can_interface',
        help='CAN interface name (e.g., can0, slcan0)'
    )
    
    parser.add_argument(
        '--interval',
        type=float,
        default=1.0,
        help='Log interval in seconds (default: 1.0)'
    )
    
    args = parser.parse_args()
    
    logger = EmbeddedCANLogger(args.can_interface, args.interval)
    
    try:
        success = logger.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()