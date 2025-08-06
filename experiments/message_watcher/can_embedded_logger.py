#!/usr/bin/env python3
"""
Embedded CAN Bus Signal Logger

This is an optimized version of can_logger.py designed for resource-constrained
embedded Linux systems. It implements native CAN decoding without external libraries
like cantools, focusing on minimal memory usage and CPU overhead.

The logger monitors 4 specific CAN messages and logs each matching message immediately
with both raw message data and decoded signal values:
- BCM_Lamp_Stat_FD1 (0x3C3): PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
- Locking_Systems_2_FD1 (0x331): Veh_Lock_Status  
- PowertrainData_10 (0x176): TrnPrkSys_D_Actl
- Battery_Mgmt_3_FD1 (0x43C): BSBattSOC

Usage:
    python can_embedded_logger.py <can_interface>
    python can_embedded_logger.py <can_interface> > logfile.csv
"""

import argparse
import sys
import time
import struct
from datetime import datetime
import socket
import threading

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

# Pre-compute CAN filter for SocketCAN
CAN_FILTER_IDS = list(CAN_MESSAGES.keys())


class EmbeddedCANLogger:
    """Minimal resource CAN logger for embedded systems."""
    
    def __init__(self, can_interface):
        self.can_interface = can_interface
        self.running = False
        self.start_time = time.time()
        
        # Signal state tracking - pre-allocated to avoid runtime allocation
        self.signal_values = {}
        self.message_timestamps = {}
        
        # Initialize signal storage
        for can_id, msg_def in CAN_MESSAGES.items():
            msg_name = msg_def['name']
            self.signal_values[msg_name] = {}
            self.message_timestamps[msg_name] = None
            for signal_name in msg_def['signals']:
                self.signal_values[msg_name][signal_name] = None
        
        # Statistics
        self.stats = {'total_messages': 0, 'decoded_messages': 0, 'log_entries': 0}
        
        # Threading
        self.data_lock = threading.Lock()
        
        print(f"Embedded CAN Logger initialized", file=sys.stderr)
        print(f"Interface: {can_interface}", file=sys.stderr)
        print(f"Mode: Per-message logging", file=sys.stderr)
        print(f"Monitoring {len(CAN_MESSAGES)} message types", file=sys.stderr)

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

    def connect_can_socket(self):
        """Connect to CAN interface using raw SocketCAN."""
        try:
            # Create raw CAN socket
            self.can_socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
            
            # Set CAN filters for efficiency
            can_filter = b""
            for can_id in CAN_FILTER_IDS:
                # Pack filter: can_id, can_mask (exact match)
                can_filter += struct.pack("=II", can_id, 0x7FF)
            
            self.can_socket.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, can_filter)
            
            # Bind to interface
            self.can_socket.bind((self.can_interface,))
            self.can_socket.settimeout(1.0)
            
            print(f"Connected to {self.can_interface} with filters for {len(CAN_FILTER_IDS)} message IDs", file=sys.stderr)
            return True
            
        except Exception as e:
            print(f"Error connecting to CAN: {e}", file=sys.stderr)
            return False

    def update_signal_data(self, decoded_data):
        """Update signal values with thread safety."""
        if not decoded_data:
            return
            
        msg_name = decoded_data['message_name']
        current_time = time.time()
        
        with self.data_lock:
            for signal_name, value in decoded_data['signals'].items():
                self.signal_values[msg_name][signal_name] = value
            self.message_timestamps[msg_name] = current_time

    def format_signal_value(self, value):
        """Format signal value for logging output."""
        if value is None:
            return "N/A"
        elif isinstance(value, int):
            return str(value)
        else:
            return str(value)

    def log_header(self):
        """Print log header."""
        print("# Embedded CAN Signal Logger Output - Per-Message Mode")
        print("# Format: timestamp | CAN_ID:0xXXX | data:XX XX XX XX XX XX XX XX | message_name | signal1=value1 signal2=value2 ...")
        print("# Timestamp: YYYY-MM-DD HH:MM:SS.mmm")
        print("#" + "="*80)

    def log_current_state(self):
        """Log current state of all monitored signals."""
        current_time = time.time()
        timestamp = datetime.fromtimestamp(current_time).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        log_entries = []
        
        with self.data_lock:
            # Process messages in deterministic order
            for can_id in sorted(CAN_MESSAGES.keys()):
                msg_def = CAN_MESSAGES[can_id]
                msg_name = msg_def['name']
                
                for signal_name in msg_def['signals']:
                    value = self.signal_values[msg_name][signal_name]
                    formatted_value = self.format_signal_value(value)
                    log_entries.append(f"{msg_name}.{signal_name}={formatted_value}")
        
        log_line = f"{timestamp} | " + " | ".join(log_entries)
        print(log_line)
        self.stats['log_entries'] += 1

    def log_can_message(self, can_id, data, decoded_data):
        """Log a single CAN message with both raw and decoded data."""
        current_time = time.time()
        timestamp = datetime.fromtimestamp(current_time).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        # Format raw data as hex bytes
        data_hex = " ".join(f"{byte:02X}" for byte in data)
        
        # Build log line with raw message info
        log_parts = [
            timestamp,
            f"CAN_ID:0x{can_id:03X}",
            f"data:{data_hex}"
        ]
        
        if decoded_data:
            # Add decoded message name
            log_parts.append(decoded_data['message_name'])
            
            # Add decoded signals
            signal_parts = []
            for signal_name, value in decoded_data['signals'].items():
                formatted_value = self.format_signal_value(value)
                signal_parts.append(f"{signal_name}={formatted_value}")
            
            if signal_parts:
                log_parts.append(" ".join(signal_parts))
        else:
            log_parts.append("UNKNOWN_MESSAGE")
        
        log_line = " | ".join(log_parts)
        print(log_line)
        self.stats['log_entries'] += 1

    def message_listener(self):
        """Background thread for CAN message reception."""
        while self.running:
            try:
                # Receive CAN frame: can_id(4) + dlc(1) + pad(3) + data(8)
                frame_data = self.can_socket.recv(16)
                if len(frame_data) >= 16:
                    # Unpack CAN frame
                    can_id, dlc = struct.unpack("=IB", frame_data[:5])
                    can_id &= 0x7FF  # Mask to standard 11-bit ID
                    data = frame_data[8:16]  # 8 bytes of data
                    
                    self.stats['total_messages'] += 1
                    
                    # Decode if this is a monitored message
                    decoded_data = self.decode_can_message(can_id, data)
                    if decoded_data:
                        self.stats['decoded_messages'] += 1
                        self.update_signal_data(decoded_data)
                        self.log_can_message(can_id, data, decoded_data)
                        
            except socket.timeout:
                continue  # Normal timeout, keep listening
            except Exception as e:
                if self.running:
                    print(f"Error in message listener: {e}", file=sys.stderr)
                break

    def run(self):
        """Main execution loop."""
        if not self.connect_can_socket():
            return False
        
        # Start message listener thread
        self.running = True
        listener_thread = threading.Thread(target=self.message_listener, daemon=True)
        listener_thread.start()
        
        print(f"Starting logger in per-message mode...", file=sys.stderr)
        self.log_header()
        
        try:
            while True:
                # Just wait for messages to be logged by the listener thread
                time.sleep(1.0)
                
        except KeyboardInterrupt:
            print(f"\nStopping logger...", file=sys.stderr)
            
        finally:
            self.running = False
            
            # Print final statistics
            runtime = time.time() - self.start_time
            print(f"\nSESSION SUMMARY", file=sys.stderr)
            print(f"Runtime: {runtime:.1f}s", file=sys.stderr)
            print(f"Total messages: {self.stats['total_messages']}", file=sys.stderr)
            print(f"Decoded messages: {self.stats['decoded_messages']}", file=sys.stderr)
            print(f"Log entries: {self.stats['log_entries']}", file=sys.stderr)
            if runtime > 0:
                print(f"Message rate: {self.stats['total_messages']/runtime:.1f} msg/sec", file=sys.stderr)
            
            if hasattr(self, 'can_socket'):
                self.can_socket.close()
                print(f"Disconnected from {self.can_interface}", file=sys.stderr)
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description='Embedded CAN Signal Logger - Optimized for resource-constrained systems',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_embedded_logger.py can0
    python can_embedded_logger.py vcan0 > can_log.csv

This embedded version:
- Uses no external dependencies beyond standard library
- Implements native CAN decoding without cantools
- Minimizes memory allocation and CPU overhead
- Logs each matching CAN message immediately with both raw and decoded data
- Monitors these specific signals:
  * BCM_Lamp_Stat_FD1: PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
  * Locking_Systems_2_FD1: Veh_Lock_Status
  * PowertrainData_10: TrnPrkSys_D_Actl
  * Battery_Mgmt_3_FD1: BSBattSOC
        """
    )
    
    parser.add_argument('can_interface', help='CAN interface name (e.g., can0, vcan0)')
    
    args = parser.parse_args()
    
    # Create and run the embedded logger (interval no longer used for periodic logging)
    logger = EmbeddedCANLogger(args.can_interface)
    
    try:
        success = logger.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
