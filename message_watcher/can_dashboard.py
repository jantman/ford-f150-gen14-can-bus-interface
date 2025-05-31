#!/usr/bin/env python3
"""
CAN Bus Signal Dashboard

This script provides a dashboard-like view of specific CAN signals in the terminal.
It listens to specific CAN messages, decodes them using a DBC file, and displays
selected signals in a live, updating dashboard format.

Usage:
    python can_dashboard.py <can_interface>

Examples:
    python can_dashboard.py can0
    python can_dashboard.py slcan0

The script uses the ford_lincoln_base_pt.dbc file in the same directory
to decode messages and displays only the configured signals in a dashboard format.
"""

import argparse
import sys
import time
import os
from datetime import datetime
import can
import cantools
import threading
from collections import defaultdict

# Dashboard configuration - easily modify which messages and signals to display
DASHBOARD_CONFIG = {
    "BCM_Lamp_Stat_FD1": {
        "signals": ["PudLamp_D_Rq", "Illuminated_Entry_Stat", "Dr_Courtesy_Light_Stat"],
        "refresh_rate": 0.1  # seconds between updates for this message
    },
    "Locking_Systems_2_FD1": {
        "signals": ["Veh_Lock_Status"],
        "refresh_rate": 0.1
    },
    "PowertrainData_10": {
        "signals": [
            "TrnPrkSys_D_Actl",
        ],
        "refresh_rate": 1
    },
    "EngVehicleSpThrottle": {
        "signals": [
            "EngAout_N_Actl",
        ],
        "refresh_rate": 0.1
    },
    "Body_Info_4_FD1": {
        "signals": [
            "BattULo_U_Actl"
        ],
        "refresh_rate": 0.1
    },
    "Battery_Mgmt_3_FD1": {
        "signals": [
            "BSBattSOC"
        ],
        "refresh_rate": 0.1
    },
    "Battery_Mgmt_2_FD1": {
        "signals": [
            "Shed_T_Eng_Off_B", "Shed_Drain_Eng_Off_B",
            "Shed_Level_Req", "Batt_Lo_SoC_B", "Batt_Crit_SoC_B"
        ],
        "refresh_rate": 0.1
    }
}


class CANDashboard:
    def __init__(self, can_interface, dbc_file="ford_lincoln_base_pt.dbc", two_column_mode=False):
        """
        Initialize the CAN dashboard.

        Args:
            can_interface: CAN interface name (e.g., 'can0')
            dbc_file: Path to the DBC file for message decoding
        """
        self.can_interface = can_interface
        self.dbc_file = dbc_file
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
            'dashboard_updates': 0
        }
        
        # Threading lock for data access
        self.data_lock = threading.Lock()
        self.two_column_mode = two_column_mode
        
        print(f"CAN Dashboard initialized")
        print(f"Interface: {can_interface}")
        print(f"DBC file: {dbc_file}")

    def load_dbc(self):
        """Load the DBC file and build message filters."""
        try:
            if not os.path.exists(self.dbc_file):
                print(f"Error: DBC file '{self.dbc_file}' not found")
                return False
                
            print(f"Loading DBC file: {self.dbc_file}")
            self.db = cantools.database.load_file(self.dbc_file)
            print(f"Successfully loaded DBC with {len(self.db.messages)} message definitions")
            
            # Build message filters from dashboard config
            print(f"\nConfiguring dashboard for {len(DASHBOARD_CONFIG)} message types:")
            for msg_name, config in DASHBOARD_CONFIG.items():
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
                    
                    print(f"  - {msg_name} (0x{msg.frame_id:X}): {len(config['signals'])} signals")
                    for signal_name in config['signals']:
                        if signal_name in missing_signals:
                            print(f"    * {signal_name} [NOT FOUND IN DBC]")
                        else:
                            print(f"    * {signal_name}")
                    
                    if missing_signals:
                        print(f"    WARNING: {len(missing_signals)} signal(s) not found in DBC")
                    
                except KeyError:
                    print(f"  - WARNING: Message '{msg_name}' not found in DBC")
            
            if not self.filtered_message_ids:
                print(f"ERROR: No valid messages found for dashboard")
                return False
            
            print(f"\nDashboard will monitor {len(self.filtered_message_ids)} CAN message(s)")
            return True
            
        except Exception as e:
            print(f"Error loading DBC file: {e}")
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
            
            print(f"Connecting to CAN interface: {self.can_interface}")
            if can_filters:
                print(f"Applying CAN filters for {len(can_filters)} message ID(s)")
            
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

    def decode_message(self, msg):
        """
        Decode a CAN message using the loaded DBC.

        Args:
            msg: python-can Message object

        Returns:
            dict: Decoded message data or None if not in config
        """
        try:
            # Check if this message is in our dashboard config
            if msg.arbitration_id not in self.message_ids:
                return None
            
            msg_name = self.message_ids[msg.arbitration_id]
            
            # Find the message definition in the DBC
            dbc_message = self.db.get_message_by_frame_id(msg.arbitration_id)
            
            # Decode the message
            decoded_signals = dbc_message.decode(msg.data)
            
            # Filter to only the signals we care about
            config = DASHBOARD_CONFIG[msg_name]
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

    def update_dashboard_data(self, msg, decoded_data):
        """Update the dashboard data with new message information."""
        if not decoded_data:
            return
        
        msg_name = decoded_data['message_name']
        
        with self.data_lock:
            # Update signal values
            for signal_name, value in decoded_data['signals'].items():
                self.message_data[msg_name][signal_name] = value
            
            # Update timestamp
            self.message_timestamps[msg_name] = time.time()
            self.stats['dashboard_updates'] += 1

    def format_signal_value(self, value):
        """Format a signal value for display."""
        if value is None:
            return "N/A"
        elif isinstance(value, bool):
            return "ON" if value else "OFF"
        elif isinstance(value, int):
            return str(value)
        elif isinstance(value, float):
            return f"{value:.2f}"
        elif hasattr(value, 'name') and hasattr(value, 'value'):
            # Handle NamedSignalValue objects
            return f"{value.name} ({value.value})"
        else:
            return str(value)

    def clear_screen(self):
        """Clear the terminal screen."""
        os.system('clear' if os.name == 'posix' else 'cls')

    def display_dashboard(self):
        """Display the current dashboard state."""
        self.clear_screen()
        
        current_time = time.time()
        runtime = current_time - self.start_time
        
        # Header
        print("=" * 80 if not self.two_column_mode else "=" * 200)
        print(f"{'CAN SIGNAL DASHBOARD':^80}" if not self.two_column_mode else f"{'CAN SIGNAL DASHBOARD':^200}")
        print("=" * 80 if not self.two_column_mode else "=" * 200)
        print(f"Interface: {self.can_interface:<20} Runtime: {runtime:>8.1f}s")
        print(f"Messages: {self.stats['total_messages']:<12} Decoded: {self.stats['decoded_messages']:<12} Updates: {self.stats['dashboard_updates']}")
        print("=" * 80 if not self.two_column_mode else "=" * 200)
        
        # Dashboard data
        with self.data_lock:
            messages = list(DASHBOARD_CONFIG.keys())
            mid_point = len(messages) // 2 if self.two_column_mode else len(messages)
            left_column = messages[:mid_point]
            right_column = messages[mid_point:] if self.two_column_mode else []

            def display_column(column):
                for msg_name in column:
                    print(f"\n📊 {msg_name}")
                    print("-" * 60)
                    
                    # Check if we have recent data
                    last_update = self.message_timestamps.get(msg_name)
                    if last_update is None:
                        print("   Status: Waiting for data...")
                    else:
                        age = current_time - last_update
                        if age > 5.0:  # No data for 5 seconds
                            status = f"⚠️  STALE (last: {age:.1f}s ago)"
                        elif age > 1.0:  # No data for 1 second
                            status = f"⏳ OLD (last: {age:.1f}s ago)"
                        else:
                            status = "✅ LIVE"
                        
                        print(f"   Status: {status}")
                    
                    # Display signals
                    config = DASHBOARD_CONFIG[msg_name]
                    for signal_name in config['signals']:
                        value = self.message_data[msg_name].get(signal_name)
                        formatted_value = self.format_signal_value(value)
                        print(f"   {signal_name:<25}: {formatted_value}")

            if self.two_column_mode:
                left_output = []
                right_output = []
                for msg_name in left_column:
                    left_output.append(f"\n📊 {msg_name}\n" + "-" * 60)
                    
                    # Check if we have recent data
                    last_update = self.message_timestamps.get(msg_name)
                    if last_update is None:
                        left_output.append("   Status: Waiting for data...")
                    else:
                        age = current_time - last_update
                        if age > 5.0:  # No data for 5 seconds
                            status = f"⚠️  STALE (last: {age:.1f}s ago)"
                            left_output.append(f"   Status: {status}")
                        elif age > 1.0:  # No data for 1 second
                            status = f"⏳ OLD (last: {age:.1f}s ago)"
                            left_output.append(f"   Status: {status}")
                        else:
                            status = "✅ LIVE"
                            left_output.append(f"   Status: {status}")
                    
                    # Display signals
                    config = DASHBOARD_CONFIG[msg_name]
                    for signal_name in config['signals']:
                        value = self.message_data[msg_name].get(signal_name)
                        formatted_value = self.format_signal_value(value)
                        left_output.append(f"   {signal_name:<25}: {formatted_value}")
                
                for msg_name in right_column:
                    right_output.append(f"\n📊 {msg_name}\n" + "-" * 60)
                    
                    # Check if we have recent data
                    last_update = self.message_timestamps.get(msg_name)
                    if last_update is None:
                        right_output.append("   Status: Waiting for data...")
                    else:
                        age = current_time - last_update
                        if age > 5.0:  # No data for 5 seconds
                            status = f"⚠️  STALE (last: {age:.1f}s ago)"
                            right_output.append(f"   Status: {status}")
                        elif age > 1.0:  # No data for 1 second
                            status = f"⏳ OLD (last: {age:.1f}s ago)"
                            right_output.append(f"   Status: {status}")
                        else:
                            status = "✅ LIVE"
                            right_output.append(f"   Status: {status}")
                    
                    # Display signals
                    config = DASHBOARD_CONFIG[msg_name]
                    for signal_name in config['signals']:
                        value = self.message_data[msg_name].get(signal_name)
                        formatted_value = self.format_signal_value(value)
                        right_output.append(f"   {signal_name:<25}: {formatted_value}")
                
                # Print both columns
                for i in range(max(len(left_output), len(right_output))):
                    left_line = left_output[i] if i < len(left_output) else ""
                    right_line = right_output[i] if i < len(right_output) else ""
                    print(f"{left_line:<80}  {right_line}")
            else:
                display_column(messages)

        print("\n" + "=" * 80 if not self.two_column_mode else "=" * 200)
        print("Press Ctrl+C to stop")

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
                        self.update_dashboard_data(msg, decoded_data)
                        
            except Exception as e:
                if self.running:  # Only print errors if we're still supposed to be running
                    print(f"Error in message listener: {e}")
                break

    def run(self):
        """Main dashboard loop."""
        if not self.load_dbc():
            return False
            
        if not self.connect_can():
            return False
        
        # Start message listening thread
        self.running = True
        listener_thread = threading.Thread(target=self.message_listener, daemon=True)
        listener_thread.start()
        
        print("\nStarting dashboard...")
        time.sleep(2)  # Give a moment for initial data
        
        try:
            while True:
                self.display_dashboard()
                time.sleep(0.5)  # Update dashboard every 500ms
                
        except KeyboardInterrupt:
            print(f"\n\nReceived Ctrl+C, stopping dashboard...")
            
        finally:
            self.running = False
            
            # Final statistics
            runtime = time.time() - self.start_time
            print(f"\n" + "="*80)
            print(f"DASHBOARD SESSION SUMMARY")
            print(f"="*80)
            print(f"Runtime: {runtime:.1f} seconds")
            print(f"Total CAN messages received: {self.stats['total_messages']}")
            print(f"Messages decoded for dashboard: {self.stats['decoded_messages']}")
            print(f"Dashboard updates: {self.stats['dashboard_updates']}")
            if runtime > 0:
                print(f"Message rate: {self.stats['total_messages']/runtime:.1f} msg/sec")
            
            # Close resources
            if self.bus:
                self.bus.shutdown()
                print(f"Disconnected from {self.can_interface}")
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description='CAN Bus Signal Dashboard - Live display of specific signals',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python can_dashboard.py can0
    python can_dashboard.py slcan0

The dashboard displays real-time values for configured CAN signals in a
terminal-based interface. Signal configuration is done by modifying the
DASHBOARD_CONFIG dictionary at the top of this script.

Current configuration monitors:
  - BCM_Lamp_Stat_FD1: PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
  - Locking_Systems_2_FD1: Veh_Lock_Status
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
        '--two-column',
        action='store_true',
        help='Enable two-column mode for wide terminals (200 characters)'
    )
    
    args = parser.parse_args()
    
    # Create and run the dashboard
    dashboard = CANDashboard(args.can_interface, args.dbc, args.two_column)
    
    try:
        success = dashboard.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()