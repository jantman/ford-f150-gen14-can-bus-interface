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
            "GearEngag_D_Actl", "TrnRng_D_Rq", "TrnPrkSys_D_Actl",
            "GearLvr_D_ActlDrv", "GearPos_D_Actl"
        ],
        "refresh_rate": 1
    },
    "Gear_Shift_by_Wire_FD1": {
        "signals": [
            "TrnRng_D_RqGsm", "PrkBrkActv_D_RqGsmGear", "BrkSwtchPos_B_ActlGsm"
        ],
        "refresh_rate": 1
    },
    "Gear_Shift_by_Wire_3": {
        "signals": [
            "TrnGear_D_RqPt"
        ],
        "refresh_rate": 1
    },
    "EngVehicleSpThrottle": {
        "signals": [
            "EngAout3_N_Actl", "EngAout_N_Actl",
        ],
        "refresh_rate": 0.1
    },
    "APIMGPS_Data_Nav_3_FD1": {
        "signals": [
            "GPS_Speed"
        ],
        "refresh_rate": 0.1
    }
}

"""
BO_ 374 PowertrainData_10: 8 PCM
 SG_ GearEngag_D_Actl : 47|3@0+ (1,0) [0|7] "SED"  GWM
 SG_ TrnRng_D_Rq : 27|4@0+ (1,0) [0|15] "SED"  SOBDMC_HPCM_FD1,ABS_ESC,IPMA_ADAS,PSCM,ECM_Diesel,GWM
 SG_ TrnPrkSys_D_Actl : 31|4@0+ (1,0) [0|15] "SED"  SOBDMC_HPCM_FD1,ABS_ESC,IPMA_ADAS,ECM_Diesel,GWM
 SG_ GearLvr_D_ActlDrv : 7|4@0+ (1,0) [0|15] "SED"  SOBDMC_HPCM_FD1,ABS_ESC,IPMA_ADAS,TCCM,ECM_Diesel,GWM
 SG_ GearPos_No_Cs : 23|8@0+ (1,0) [0|255] "unitless"  SOBDMC_HPCM_FD1,IPMA_ADAS,ABS_ESC,ECM_Diesel,GWM
 SG_ GearPos_D_Trg : 15|4@0+ (1,0) [0|15] "SED"  SOBDMC_HPCM_FD1,IPMA_ADAS,ABS_ESC,TCCM,ECM_Diesel,GWM
 SG_ GearPos_No_Cnt : 11|4@0+ (1,0) [0|15] "unitless"  SOBDMC_HPCM_FD1,IPMA_ADAS,ABS_ESC,ECM_Diesel,GWM
 SG_ TrnIgnOffDly_T_Rq : 39|8@0+ (4,0) [0|1020] "ms"  GWM
 SG_ GearPos_D_Actl : 3|4@0+ (1,0) [0|15] "SED"  SOBDMC_HPCM_FD1,PSCM,IPMA_ADAS,ABS_ESC,ECM_Diesel,GWM

VAL_TABLE_ GearLvr_D_ActlDrv 15 "Fault" 14 "Unknown_Position" 13 "NotUsed_2" 12 "NotUsed_1" 11 "Range6" 10 "Range5" 9 "Range4" 8 "Range3_M3_L3" 7 "Range2_M2_L2" 6 "Range1_M1_L1" 5 "Low" 4 "Sport_DriveSport_Mposition" 3 "Drive" 2 "Neutral" 1 "Reverse" 0 "Park";
VAL_TABLE_ TrnPrkSys_D_Actl 15 "Faulty" 14 "NotUsed_5" 13 "NotUsed_4" 12 "NotUsed_3" 11 "NotUsed_2" 10 "NotUsed_1" 9 "FrequencyError" 8 "OutOfRangeHigh" 7 "OutOfRangeLow" 6 "Override" 5 "OutOfPark" 4 "TransitionCloseToOutOfPark" 3 "AtNoSpring" 2 "TransitionCloseToPark" 1 "Park" 0 "NotKnown";
VAL_ 374 TrnRng_D_Rq 15 "Fault" 14 "Unknown_Position" 13 "NotUsed_2" 12 "NotUsed_1" 11 "Range6" 10 "Range5" 9 "Range4" 8 "Range3_M3_L3" 7 "Range2_M2_L2" 6 "Range1_M1_L1" 5 "Low" 4 "Sport_DriveSport_Mposition" 3 "Drive" 2 "Neutral" 1 "Reverse" 0 "Park";
VAL_ 374 GearEngag_D_Actl 7 "Undefined" 6 "Fwd_Clutch_Fully_Engaged" 5 "Neutral_Idle" 4 "Disengaged_to_Neutral_Idle" 3 "Disengaged_to_Neutral_Park" 2 "Engagement_in_Progress" 1 "InitializeFwdClutchEngagmt" 0 "Park_Neutral";

BO_ 90 Gear_Shift_by_Wire_FD1: 8 GWM
 SG_ TrnGsmNtmState_D_Actl : 55|2@0+ (1,0) [0|3] "SED"  ABS_ESC,PCM,PCM_HEV,TCM_DSL
 SG_ DrQltyDrv_D_StatGsm : 42|3@0+ (1,0) [0|7] "SED"  ABS_ESC,PCM,PCM_HEV,TCM_DSL,IPMA_ADAS
 SG_ TrnBtsiOvrrd_B_Stat : 43|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ GsmGearMsgTxt_D_Rq : 63|2@0+ (1,0) [0|3] "SED" Vector__XXX
 SG_ TrnRng_D_RqGsm : 51|4@0+ (1,0) [0|15] "SED"  PCM,TCM_DSL
 SG_ PrkBrkActv_D_RqGsmGear : 53|2@0+ (1,0) [0|3] "SED"  ABS_ESC
 SG_ TrnValidGearRq_D_Stat : 25|2@0+ (1,0) [0|3] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearRqCnt_B_Actl : 26|1@0+ (1,0) [0|1] "unitless"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_U_Actl : 23|8@0+ (0.05,0) [0|12.7] "VOLT"  TCM_DSL
 SG_ TrnGearButtn_B_ActlR2 : 8|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlR1 : 9|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlR0 : 10|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlP2 : 11|1@0+ (1,0) [0|1] "SED"  TCM_DSL,PCM,PCM_HEV
 SG_ TrnGearButtn_B_ActlP1 : 12|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlP0 : 13|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlN2 : 14|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlN1 : 15|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlN0 : 0|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlM2 : 1|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlM1 : 2|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlM0 : 3|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlD2 : 4|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlD1 : 5|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGearButtn_B_ActlD0 : 6|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL
 SG_ TrnGear_No_Cs : 39|8@0+ (1,0) [0|255] "Unitless"  SOBDMC_HPCM_FD1,PCM,PCM_HEV,TCM_DSL
 SG_ TrnGear_No_Cnt : 47|4@0+ (1,0) [0|15] "Unitless"  SOBDMC_HPCM_FD1,PCM,PCM_HEV,TCM_DSL
 SG_ TrnGear_D_RqDrv : 31|5@0+ (1,0) [0|31] "SED"  SOBDMC_HPCM_FD1,PCM,PCM_HEV,TCM_DSL
 SG_ BrkSwtchPos_B_ActlGsm : 7|1@0+ (1,0) [0|1] "SED"  PCM,PCM_HEV,TCM_DSL

VAL_TABLE_ TrnRng_D_RqGsm 15 "Fault" 14 "UnknownPosition" 13 "Undefined_2" 12 "Undefined_1" 11 "_6" 10 "_5" 9 "_4" 8 "_3" 7 "_2" 6 "_1" 5 "Low" 4 "Sport" 3 "Drive" 2 "Neutral" 1 "Reverse" 0 "Park";
VAL_TABLE_ PrkBrkActv_D_RqGsmGear 3 "NotUsed" 2 "RequestParkBrakeEngage" 1 "NoRequest" 0 "Null";

BO_ 92 Gear_Shift_by_Wire_3: 8 PCM_HEV
 SG_ TrnLvrV_D_Rq : 43|2@0+ (1,0) [0|3] "SED"  GWM
 SG_ TrnSbwSysHlth_D_Actl : 1|2@0+ (1,0) [0|3] "SED"  GWM
 SG_ TrnGearNtmAllow_B_Stat : 47|1@0+ (1,0) [0|1] "SED"  GWM
 SG_ TrnDtpCmd_D_Actl : 46|3@0+ (1,0) [0|7] "SED"  GWM
 SG_ GearSelLck_D_Rq : 41|2@0+ (1,0) [0|3] "SED"  GWM
 SG_ TrnGearCmd_No_Cs : 31|8@0+ (1,0) [0|255] "Unitless"  GWM
 SG_ TrnValidGear_D_Cnfm : 11|2@0+ (1,0) [0|3] "SED"  GWM
 SG_ TrnNtrlTowCmd_D_Actl : 6|2@0+ (1,0) [0|3] "SED"  IPMA_ADAS,GWM,ABS_ESC
 SG_ TrnGearCmd_Pc_ActlPt : 9|10@0+ (0.1,0) [0|102.2] "percent duty cycle"  GWM
 SG_ TrnGear_D_RqPt : 4|3@0+ (1,0) [0|7] "SED"  GWM
 SG_ TrnCmdState_B_Actl : 39|1@0+ (1,0) [0|1] "SED"  GWM
 SG_ TrnCmdCnt_B_Actl : 7|1@0+ (1,0) [0|1] "unitless"  GWM
 SG_ PrkBrkActv_D_RqTrnGear : 38|2@0+ (1,0) [0|3] "SED"  GWM,ABS_ESC
 SG_ TrnGearMsgTxt_D_Rq : 36|5@0+ (1,0) [0|31] "SED"  GWM
 SG_ TrnGearCmd_No_Cnt : 15|4@0+ (1,0) [0|15] "Unitless"  GWM

VAL_TABLE_ TrnGear_D_RqPt 7 "Fault" 6 "NotUsed" 5 "Manual" 4 "Drive" 3 "Neutral" 2 "Reverse" 1 "Park" 0 "No_Gear";

BO_ 516 EngVehicleSpThrottle: 8 PCM_HEV
 SG_ EngAoutNActl_D_QF : 31|2@0+ (1,0) [0|3] "SED"  TCM_DSL,GWM
 SG_ EngAout3_N_Actl : 55|16@0+ (0.25,0) [0|16383.5] "RPM"  SOBDMC_HPCM_FD1,GWM
 SG_ ApedPos_PcRate_ActlArb : 23|8@0+ (0.04,-5) [-5|5.12] "%/ms"  TCM_DSL,GWM
 SG_ ApedPos_Pc_ActlArb : 1|10@0+ (0.1,0) [0|102.3] "%"  VDM,CMR_DSMC,SOBDMC_HPCM_FD1,IPMA_ADAS,TCCM,ABS_ESC,PSCM,TCM_DSL,GWM
 SG_ ApedPosPcActl_D_Qf : 7|2@0+ (1,0) [0|3] "SED"  VDM,CMR_DSMC,IPMA_ADAS,SOBDMC_HPCM_FD1,ABS_ESC,PSCM,TCM_DSL,GWM
 SG_ EngAout_N_Actl : 28|13@0+ (2,0) [0|16382] "rpm"  VDM,ABS_ESC,PSCM,TCCM,TCM_DSL,GWM
 SG_ ApedPosPcActl_No_Cnt : 5|4@0+ (1,0) [0|15] "Unitless"  ABS_ESC,SOBDMC_HPCM_FD1,GWM
 SG_ ApedPosPcActl_No_Cs : 47|8@0+ (1,0) [0|255] "Unitless"  ABS_ESC,SOBDMC_HPCM_FD1,GWM

BO_ 1124 APIMGPS_Data_Nav_3_FD1: 8 GWM
 SG_ GPS_Vdop : 63|5@0+ (0.2,0) [0|5.8] "unitless"  IPMA_ADAS
 SG_ GPS_Speed : 47|8@0+ (1,0) [0|253] "MPH" Vector__XXX
 SG_ GPS_Sat_num_in_view : 7|5@0+ (1,0) [0|29] "unitless"  SOBDMC_HPCM_FD1
 SG_ GPS_MSL_altitude : 15|12@0+ (10,-20460) [-20460|20470] "feet" Vector__XXX
 SG_ GPS_Heading : 31|16@0+ (0.01,0) [0|655.33] "Degrees"  IPMA_ADAS
 SG_ GPS_Hdop : 55|5@0+ (0.2,0) [0|5.8] "unitless"  IPMA_ADAS
 SG_ GPS_dimension : 2|3@0+ (1,0) [0|7] "SED"  SOBDMC_HPCM_FD1
"""

class CANDashboard:
    def __init__(self, can_interface, dbc_file="ford_lincoln_base_pt.dbc"):
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
        print("=" * 80)
        print(f"{'CAN SIGNAL DASHBOARD':^80}")
        print("=" * 80)
        print(f"Interface: {self.can_interface:<20} Runtime: {runtime:>8.1f}s")
        print(f"Messages: {self.stats['total_messages']:<12} Decoded: {self.stats['decoded_messages']:<12} Updates: {self.stats['dashboard_updates']}")
        print("=" * 80)
        
        # Dashboard data
        with self.data_lock:
            for msg_name in DASHBOARD_CONFIG.keys():
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
        
        print("\n" + "=" * 80)
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
    
    args = parser.parse_args()
    
    # Create and run the dashboard
    dashboard = CANDashboard(args.can_interface, args.dbc)
    
    try:
        success = dashboard.run()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Fatal error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()