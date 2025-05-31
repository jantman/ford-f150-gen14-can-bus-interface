#!/usr/bin/env python3
"""
CAN Bus Signal Logger

This script logs specific CAN signals to stdout (or a file) at regular intervals.
It listens to the same CAN messages as can_dashboard.py, decodes them using a DBC file,
and outputs timestamped log entries with the current state of all configured signals.

Usage:
    python can_logger.py <can_interface>
    python can_logger.py <can_interface> > logfile.csv

Examples:
    python can_logger.py can0
    python can_logger.py slcan0 > can_signals.log

The script uses the ford_lincoln_base_pt.dbc file in the same directory
to decode messages and logs all configured signals every second.
"""

import argparse
import sys
import time
import os
from datetime import datetime
import can
import cantools
import threading

# Use the same configuration as the dashboard
LOGGER_CONFIG = {
    "BCM_Lamp_Stat_FD1": {
        "signals": ["PudLamp_D_Rq", "Illuminated_Entry_Stat", "Dr_Courtesy_Light_Stat"]
    },
    "Locking_Systems_2_FD1": {
        "signals": ["Veh_Lock_Status"]
    },
    "PowertrainData_10": {
        "signals": ["TrnPrkSys_D_Actl"]
    },
    "EngVehicleSpThrottle": {
        "signals": ["EngAout_N_Actl"]
    },
    "Body_Info_4_FD1": {
        "signals": ["BattULo_U_Actl"]
    },
    "Battery_Mgmt_3_FD1": {
        "signals": ["BSBattSOC"]
    },
    "Battery_Mgmt_2_FD1": {
        "signals": ["Shed_T_Eng_Off_B", "Shed_Drain_Eng_Off_B",
                   "Shed_Level_Req", "Batt_Lo_SoC_B", "Batt_Crit_SoC_B"]
    }
}


class CANLogger:
    def __init__(self, can_interface, dbc_file="ford_lincoln_base_pt.dbc", log_interval=1.0):
        """
        Initialize the CAN logger.

        Args:
            can_interface: CAN interface name (e.g., 'can0')
            dbc_file: Path to the DBC file for message decoding
            log_interval: Interval in seconds between log entries
        """
        self.can_interface = can_interface
        self.dbc_file = dbc_file
        self.log_interval = log_interval
        self.bus = None
        self.db = None
        self.running = False
        self.start_time = time.time()
        
        # Message tracking
        self.message_data = {}  # {msg_name: {signal_name: value, ...}}
        self.message_timestamps = {}  # {msg_name: last_update_time}
        self.message_ids = {}  # {frame_id: msg_name}
        self.filtered_message_ids = set()
        
        # Statistics
        self.stats = {
            'total_messages': 0,
            'decoded_messages': 0,
            'log_entries': 0
        }
        
        # Threading lock for data access
        self.data_lock = threading.Lock()
        
        print(f"CAN Logger initialized", file=sys.stderr)
        print(f"Interface: {can_interface}", file=sys.stderr)
        print(f"DBC file: {dbc_file}", file=sys.stderr)
        print(f"Log interval: {log_interval}s", file=sys.stderr)

    def load_dbc(self):
        """Load the DBC file and build message filters."""
        try:
            if not os.path.exists(self.dbc_file):
                print(f"Error: DBC file '{self.dbc_file}' not found", file=sys.stderr)
                return False
                
            print(f"Loading DBC file: {self.dbc_file}", file=sys.stderr)
            self.db = cantools.database.load_file(self.dbc_file)
            print(f"Successfully loaded DBC with {len(self.db.messages)} message definitions", file=sys.stderr)
            
            # Build message filters from logger config
            print(f"\nConfiguring logger for {len(LOGGER_CONFIG)} message types:", file=sys.stderr)
            for msg_name, config in LOGGER_CONFIG.items():
                try:
                    msg = self.db.get_message_by_name(msg_name)
                    self.filtered_message_ids.add(msg.frame_id)
                    self.message_ids[msg.frame_id] = msg_name
                    
                    # Initialize message data structure
                    self.message_data[msg_name] = {}
                    self.message_timestamps[msg_name] = None
                    
                    # Verify all configured signals exist in the message
                    available_signals = [s.name for s in msg.signals]
                    missing_signals = []
                    for signal_name in config['signals']:
                        if signal_name not in available_signals:
                            missing_signals.append(signal_name)
                        else:
                            self.message_data[msg_name][signal_name] = None
                    
                    print(f"  - {msg_name} (0x{msg.frame_id:X}): {len(config['signals'])} signals", file=sys.stderr)
                    for signal_name in config['signals']:
                        if signal_name in missing_signals:
                            print(f"    * {signal_name} [NOT FOUND IN DBC]", file=sys.stderr)
                        else:
                            print(f"    * {signal_name}", file=sys.stderr)
                    
                    if missing_signals:
                        print(f"    WARNING: {len(missing_signals)} signal(s) not found in DBC", file=sys.stderr)
                    
                except KeyError:
                    print(f"  - WARNING: Message '{msg_name}' not found in DBC", file=sys.stderr)
            
            if not self.filtered_message_ids:
                print(f"ERROR: No valid messages found for logging", file=sys.stderr)
                return False
            
            print(f"\nLogger will monitor {len(self.filtered_message_ids)} CAN message(s)", file=sys.stderr)
            return True
            
        except Exception as e:
            print(f"Error loading DBC file: {e}", file=sys.stderr)
            return False

    def build_can_filters(self):
        """Build CAN filters for the python-can Bus."""
        if not self.filtered_message_ids:
            return None
        
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
            if can_filters:
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
        """
        Decode a CAN message using the loaded DBC.

        Args:
            msg: python-can Message object

        Returns:
            dict: Decoded message data or None if not in config
        """
        try:
            # Check if this message is in our logger config
            if msg.arbitration_id not in self.message_ids:
                return None
            
            msg_name = self.message_ids[msg.arbitration_id]
            
            # Find the message definition in the DBC
            dbc_message = self.db.get_message_by_frame_id(msg.arbitration_id)
            
            # Decode the message
            decoded_signals = dbc_message.decode(msg.data)
            
            # Filter to only the signals we care about
            config = LOGGER_CONFIG[msg_name]
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
        """Update the message data with new message information."""
        if not decoded_data:
            return
        
        msg_name = decoded_data['message_name']
        
        with self.data_lock:
            # Update signal values
            for signal_name, value in decoded_data['signals'].items():
                self.message_data[msg_name][signal_name] = value
            
            # Update timestamp
            self.message_timestamps[msg_name] = time.time()

    def format_signal_value(self, value):
        """Format a signal value for logging."""
        if value is None:
            return "N/A"
        elif isinstance(value, bool):
            return "1" if value else "0"
        elif isinstance(value, int):
            return str(value)
        elif isinstance(value, float):
            return f"{value:.2f}"
        elif hasattr(value, 'name') and hasattr(value, 'value'):
            # Handle NamedSignalValue objects
            return f"{value.name}({value.value})"
        else:
            return str(value)

    def get_all_signal_names(self):
        """Get a sorted list of all signal names for consistent column ordering."""
        all_signals = []
        for msg_name in sorted(LOGGER_CONFIG.keys()):
            config = LOGGER_CONFIG[msg_name]
            for signal_name in config['signals']:
                all_signals.append(f"{msg_name}.{signal_name}")
        return all_signals

    def log_header(self):
        """Print the CSV header."""
        signal_names = self.get_all_signal_names()
        header = "timestamp," + ",".join(signal_names)
        print(header)

    def log_current_state(self):
        """Log the current state of all signals."""
        current_time = time.time()
        timestamp = datetime.fromtimestamp(current_time).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        signal_names = self.get_all_signal_names()
        values = []
        
        with self.data_lock:
            for signal_name in signal_names:
                msg_name, sig_name = signal_name.split('.', 1)
                
                # Check if we have data for this message
                if msg_name in self.message_data and sig_name in self.message_data[msg_name]:
                    value = self.message_data[msg_name][sig_name]
                    formatted_value = self.format_signal_value(value)
                else:
                    formatted_value = "N/A"
                
                values.append(formatted_value)
        
        log_line = timestamp + "," + ",".join(values)
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
        if not self.load_dbc():
            return False
            
        if not self.connect_can():
            return False
        
        # Start message listening thread
        self.running = True
        listener_thread = threading.Thread(target=self.message_listener, daemon=True)
        listener_thread.start()
        
        print(f"\nStarting logger with {self.log_interval}s interval...", file=sys.stderr)
        
        # Print CSV header
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
            print(f"\nLOGGER SESSION SUMMARY", file=sys.stderr)
            print(f"="*50, file=sys.stderr)
            print(f"Runtime: {runtime:.1f} seconds", file=sys.stderr)
            print(f"Total CAN messages received: {self.stats['total_messages']}", file=sys.stderr)
            print(f"Messages decoded for logging: {self.stats['decoded_messages']}", file=sys.stderr)
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
        description='CAN Bus Signal Logger - Log specific signals to CSV format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_logger.py can0
    python can_logger.py slcan0 > can_signals.log
    python can_logger.py can0 --interval 0.5 > fast_log.csv

The logger outputs CSV data to stdout with timestamps and signal values.
All status messages are sent to stderr so they don't interfere with the CSV output.

Current configuration logs the same signals as can_dashboard.py:
  - BCM_Lamp_Stat_FD1: PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
  - Locking_Systems_2_FD1: Veh_Lock_Status
  - PowertrainData_10: TrnPrkSys_D_Actl
  - EngVehicleSpThrottle: EngAout_N_Actl
  - Body_Info_4_FD1: BattULo_U_Actl
  - Battery_Mgmt_3_FD1: BSBattSOC
  - Battery_Mgmt_2_FD1: Shed_T_Eng_Off_B, Shed_Drain_Eng_Off_B, Shed_Level_Req, Batt_Lo_SoC_B, Batt_Crit_SoC_B
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
        '--interval',
        type=float,
        default=1.0,
        help='Log interval in seconds (default: 1.0)'
    )
    
    args = parser.parse_args()
    
    # Create and run the logger
    logger = CANLogger(args.can_interface, args.dbc, args.interval)
    
    try:
        success = logger.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()