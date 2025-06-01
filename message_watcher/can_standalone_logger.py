#!/usr/bin/env python3
"""
CAN Bus Standalone Signal Logger - Embedded Optimized

This script logs specific CAN signals to stdout at regular intervals.
It's optimized for embedded devices with limited resources by:
- Using hardcoded message decoding (no DBC file dependency)
- Minimal memory footprint
- Reduced dependencies (only python-can required)
- Efficient bit manipulation for signal extraction

Usage:
    python can_standalone_logger.py <can_interface>
    python can_standalone_logger.py <can_interface> > logfile.txt

Examples:
    python can_standalone_logger.py can0
    python can_standalone_logger.py slcan0 > can_signals.log

The script monitors the same signals as can_logger.py but uses embedded-friendly
hardcoded decoding methods instead of DBC file parsing.
"""

import argparse
import sys
import time
from datetime import datetime
import can
import threading
import struct

# Message configuration with CAN IDs and signal decoding info
# Format: message_name: {can_id, signals: {signal_name: (start_bit, length, scale, offset, signed, enum_values)}}
MESSAGE_CONFIG = {
    "BCM_Lamp_Stat_FD1": {
        "can_id": 0x3C3,  # 963 in decimal from DBC
        "signals": {
            "PudLamp_D_Rq": (11, 2, 1.0, 0.0, False, {0: "OFF", 1: "ON", 2: "RAMP_UP", 3: "RAMP_DOWN"}),
            "Dr_Courtesy_Light_Stat": (49, 2, 1.0, 0.0, False, {0: "Off", 1: "On", 2: "Unknown", 3: "Invalid"}),
            "Illuminated_Entry_Stat": (63, 2, 1.0, 0.0, False, {0: "Off", 1: "On", 2: "Unknown", 3: "Invalid"})
        }
    },
    "Locking_Systems_2_FD1": {
        "can_id": 0x331,  # 817 in decimal from DBC
        "signals": {
            "Veh_Lock_Status": (34, 2, 1.0, 0.0, False, {0: "LOCK_DBL", 1: "LOCK_ALL", 2: "UNLOCK_ALL", 3: "UNLOCK_DRV"})
        }
    },
    "PowertrainData_10": {
        "can_id": 0x176,  # 374 in decimal from DBC
        "signals": {
            "TrnPrkSys_D_Actl": (31, 4, 1.0, 0.0, False, {
                0: "NotKnown", 1: "Park", 2: "TransitionCloseToPark", 3: "AtNoSpring",
                4: "TransitionCloseToOutOfPark", 5: "OutOfPark", 6: "Override",
                7: "OutOfRangeLow", 8: "OutOfRangeHigh", 9: "FrequencyError",
                10: "NotUsed_1", 11: "NotUsed_2", 12: "NotUsed_3", 13: "NotUsed_4",
                14: "NotUsed_5", 15: "Faulty"
            })
        }
    },
    "Battery_Mgmt_3_FD1": {
        "can_id": 0x43C,  # 1084 in decimal from DBC
        "signals": {
            "BSBattSOC": (22, 7, 1.0, 0.0, False, None)  # Battery SOC percentage
        }
    }
}


class CANStandaloneLogger:
    def __init__(self, can_interface, log_interval=1.0):
        """
        Initialize the standalone CAN logger.

        Args:
            can_interface: CAN interface name (e.g., 'can0')
            log_interval: Interval in seconds between log entries
        """
        self.can_interface = can_interface
        self.log_interval = log_interval
        self.bus = None
        self.running = False
        self.start_time = time.time()
        
        # Message tracking - using CAN IDs as keys for efficiency
        self.message_data = {}  # {can_id: {signal_name: value, ...}}
        self.message_timestamps = {}  # {can_id: last_update_time}
        self.can_id_to_name = {}  # {can_id: message_name}
        self.monitored_ids = set()
        
        # Statistics
        self.stats = {
            'total_messages': 0,
            'decoded_messages': 0,
            'log_entries': 0
        }
        
        # Threading lock for data access
        self.data_lock = threading.Lock()
        
        # Initialize data structures
        self._initialize_message_tracking()
        
        print(f"CAN Standalone Logger initialized", file=sys.stderr)
        print(f"Interface: {can_interface}", file=sys.stderr)
        print(f"Log interval: {log_interval}s", file=sys.stderr)
        print(f"Monitoring {len(self.monitored_ids)} message types", file=sys.stderr)

    def _initialize_message_tracking(self):
        """Initialize message tracking data structures."""
        for msg_name, config in MESSAGE_CONFIG.items():
            can_id = config["can_id"]
            self.monitored_ids.add(can_id)
            self.can_id_to_name[can_id] = msg_name
            self.message_data[can_id] = {}
            self.message_timestamps[can_id] = None
            
            # Initialize signal values to None
            for signal_name in config["signals"]:
                self.message_data[can_id][signal_name] = None
            
            print(f"  - {msg_name} (0x{can_id:X}): {len(config['signals'])} signals", file=sys.stderr)

    def extract_signal(self, data, start_bit, length, scale, offset, signed, enum_values=None):
        """
        Extract a signal from CAN message data using proper DBC signal format.
        
        Ford DBC signals use Motorola (big-endian) bit numbering where:
        - Bit numbering: byte0[7:0], byte1[15:8], byte2[23:16], etc.
        - start_bit is the MSB (most significant bit) position of the signal
        - Signals are read from MSB to LSB in big-endian fashion
        
        Args:
            data: CAN message data bytes
            start_bit: Starting bit position (MSB of signal in DBC format)
            length: Signal length in bits
            scale: Scale factor
            offset: Offset value
            signed: Whether the signal is signed
            enum_values: Dictionary mapping values to names
            
        Returns:
            Tuple of (raw_value, scaled_value, enum_name) or None if extraction fails
        """
        try:
            # Convert data to bytes array and pad to 8 bytes if needed
            data_bytes = list(data) + [0] * (8 - len(data))
            
            # Convert bytes to a continuous bit stream (MSB first)
            # For Motorola byte order: bit 0 = byte0[7], bit 1 = byte0[6], ..., bit 7 = byte0[0], bit 8 = byte1[7], etc.
            raw_value = 0
            
            for i in range(length):
                # Calculate which bit position we need (counting from MSB of signal)
                bit_position = start_bit - i
                
                # Convert bit position to byte and bit index
                byte_index = bit_position // 8
                bit_index = 7 - (bit_position % 8)  # Bit index within byte (7=MSB, 0=LSB)
                
                # Extract the bit if within bounds
                if 0 <= byte_index < len(data_bytes) and 0 <= bit_index <= 7:
                    bit_value = (data_bytes[byte_index] >> bit_index) & 1
                    raw_value |= (bit_value << (length - 1 - i))
            
            # Handle signed values using two's complement
            if signed and length > 0 and (raw_value & (1 << (length - 1))):
                raw_value -= (1 << length)
            
            # Apply scale and offset
            scaled_value = raw_value * scale + offset
            
            # Get enum name if provided
            enum_name = None
            if enum_values and raw_value in enum_values:
                enum_name = enum_values[raw_value]
            
            # Return a custom object that mimics NamedSignalValue behavior
            class SignalValue:
                def __init__(self, value, name=None):
                    self.value = value
                    self.name = name
                    self._scaled = scaled_value
                
                def __str__(self):
                    if self.name:
                        return f"{self.name}({self.value})"
                    return str(self._scaled)
                
                def __int__(self):
                    return int(self._scaled)
                
                def __float__(self):
                    return float(self._scaled)
            
            return SignalValue(raw_value, enum_name)
            
        except Exception as e:
            return None

    def decode_message(self, msg):
        """
        Decode a CAN message using hardcoded signal definitions.

        Args:
            msg: python-can Message object

        Returns:
            dict: Decoded message data or None if not monitored
        """
        if msg.arbitration_id not in self.monitored_ids:
            return None
        
        msg_name = self.can_id_to_name[msg.arbitration_id]
        config = MESSAGE_CONFIG[msg_name]
        
        decoded_signals = {}
        
        for signal_name, signal_config in config["signals"].items():
            start_bit, length, scale, offset, signed, enum_values = signal_config
            
            value = self.extract_signal(
                msg.data, start_bit, length, scale, offset, signed, enum_values
            )
            
            if value is not None:
                decoded_signals[signal_name] = value
        
        return {
            'message_name': msg_name,
            'signals': decoded_signals
        }

    def update_message_data(self, msg, decoded_data):
        """Update the message data with new information."""
        if not decoded_data:
            return
        
        can_id = msg.arbitration_id
        
        with self.data_lock:
            # Update signal values
            for signal_name, value in decoded_data['signals'].items():
                self.message_data[can_id][signal_name] = value
            
            # Update timestamp
            self.message_timestamps[can_id] = time.time()

    def format_signal_value(self, value):
        """Format a signal value for logging."""
        if value is None:
            return "N/A"
        elif isinstance(value, bool):
            return "1" if value else "0"
        elif hasattr(value, 'name') and hasattr(value, 'value'):
            # Handle NamedSignalValue objects (both from cantools and our custom SignalValue)
            if value.name:
                return f"{value.name}({value.value})"
            else:
                # No enum name, just return the scaled value
                if isinstance(value._scaled, float):
                    return f"{value._scaled:.0f}" if value._scaled.is_integer() else f"{value._scaled:.2f}"
                else:
                    return str(value._scaled)
        elif isinstance(value, int):
            return str(value)
        elif isinstance(value, float):
            return f"{value:.0f}" if value.is_integer() else f"{value:.2f}"
        else:
            return str(value)

    def build_can_filters(self):
        """Build CAN filters for efficiency."""
        filters = []
        for can_id in self.monitored_ids:
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

    def log_header(self):
        """Print a header explaining the log format."""
        print("# CAN Standalone Signal Logger Output")
        print("# Format: timestamp | message.signal=value | message.signal=value | ...")
        print("# Timestamp format: YYYY-MM-DD HH:MM:SS.mmm")
        print("# Signal values: N/A=no data, 0/1=boolean, numbers=numeric values")
        print("#" + "="*80)

    def log_current_state(self):
        """Log the current state of all signals with names and values."""
        current_time = time.time()
        timestamp = datetime.fromtimestamp(current_time).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        log_entries = []
        
        with self.data_lock:
            # Process messages in consistent order
            for msg_name in sorted(MESSAGE_CONFIG.keys()):
                config = MESSAGE_CONFIG[msg_name]
                can_id = config["can_id"]
                
                # Check if we have data for this message
                if can_id in self.message_data:
                    for signal_name in config["signals"]:
                        if signal_name in self.message_data[can_id]:
                            value = self.message_data[can_id][signal_name]
                            formatted_value = self.format_signal_value(value)
                            log_entries.append(f"{msg_name}.{signal_name}={formatted_value}")
                        else:
                            log_entries.append(f"{msg_name}.{signal_name}=N/A")
                else:
                    # No data for this message yet
                    for signal_name in config["signals"]:
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
                    
                    # Decode the message
                    decoded_data = self.decode_message(msg)
                    
                    if decoded_data:
                        self.stats['decoded_messages'] += 1
                        self.update_message_data(msg, decoded_data)
                        
            except Exception as e:
                if self.running:  # Only print errors if we're still supposed to be running
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
        
        print(f"\nStarting standalone logger with {self.log_interval}s interval...", file=sys.stderr)
        
        # Print header
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
            print(f"\nSTANDALONE LOGGER SESSION SUMMARY", file=sys.stderr)
            print(f"="*50, file=sys.stderr)
            print(f"Runtime: {runtime:.1f} seconds", file=sys.stderr)
            print(f"Total CAN messages received: {self.stats['total_messages']}", file=sys.stderr)
            print(f"Messages decoded: {self.stats['decoded_messages']}", file=sys.stderr)
            print(f"Log entries written: {self.stats['log_entries']}", file=sys.stderr)
            if runtime > 0:
                print(f"Message rate: {self.stats['total_messages']/runtime:.1f} msg/sec", file=sys.stderr)
            
            # Close resources
            if self.bus:
                self.bus.shutdown()
                print(f"Disconnected from {self.can_interface}", file=sys.stderr)
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description='CAN Bus Standalone Signal Logger - Embedded Optimized',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_standalone_logger.py can0
    python can_standalone_logger.py slcan0 > can_signals.log
    python can_standalone_logger.py can0 --interval 0.5 > fast_log.txt

This logger is optimized for embedded devices with limited resources:
- No DBC file dependency (hardcoded message decoding)
- Minimal memory footprint
- Efficient bit manipulation for signal extraction
- Only requires python-can library

The logger monitors the same signals as can_logger.py:
  - BCM_Lamp_Stat_FD1: PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
  - Locking_Systems_2_FD1: Veh_Lock_Status
  - PowertrainData_10: TrnPrkSys_D_Actl
  - EngVehicleSpThrottle: EngAout_N_Actl
  - Body_Info_4_FD1: BattULo_U_Actl
  - Battery_Mgmt_3_FD1: BSBattSOC
  - Battery_Mgmt_2_FD1: Load shedding and battery status signals

Note: Some CAN IDs may need adjustment based on your specific vehicle configuration.
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
    
    # Create and run the logger
    logger = CANStandaloneLogger(args.can_interface, args.interval)
    
    try:
        success = logger.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()