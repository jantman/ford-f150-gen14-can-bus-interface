# CAN Bus Message Monitoring Tools

A collection of Python tools for monitoring and decoding CAN bus messages on Ford vehicles using DBC (Database CAN) files. These tools capture CAN messages from a SocketCAN interface, decode them using Ford's DBC definitions, and provide both real-time display and comprehensive logging capabilities.

## Tools Overview

This directory contains two complementary CAN monitoring tools:

1. **`can_decoder.py`** - General-purpose CAN message logger and decoder
2. **`can_dashboard.py`** - Live dashboard display for specific signals

## Features

### can_decoder.py Features
- **Real-time CAN message monitoring** with live console output
- **DBC-based message decoding** using Ford/Lincoln DBC definitions
- **Message filtering** to focus on specific CAN messages of interest
- **Change detection** to only show messages when their content actually changes
- **Comprehensive logging** with timestamped JSON output
- **Signal-level change tracking** to identify which specific signals have changed
- **Flexible filtering** based on message names defined in the DBC file

### can_dashboard.py Features
- **Live terminal dashboard** with constantly updating signal displays
- **Easy configuration** for specific messages and signals of interest
- **Visual status indicators** showing data freshness (live, old, stale)
- **Threaded architecture** for responsive display updates
- **Session statistics** including message rates and decode counts
- **Minimal resource usage** with hardware-level CAN filtering

## Requirements

Install the required Python packages:

```bash
pip install -r requirements.txt
```

Required packages:
- `python-can` - CAN bus interface library
- `cantools` - DBC file parsing and message decoding

## DBC File

Both scripts use `ford_lincoln_base_pt.dbc` in the same directory for message decoding. This file contains the Ford/Lincoln CAN message definitions including:
- Message names and IDs
- Signal definitions and scaling
- Enumerated values for coded signals

# CAN Message Decoder (can_decoder.py)

A comprehensive CAN message logging and decoding tool for analysis and research.

## Usage

### Basic Usage

```bash
python can_decoder.py <can_interface>
```

Examples:
```bash
# Monitor on can0 interface
python can_decoder.py can0

# Monitor on slcan0 interface (serial CAN adapter)
python can_decoder.py slcan0

# Use custom DBC file
python can_decoder.py can0 --dbc custom_ford.dbc
```

### Changes-Only Mode

Monitor only when message content actually changes:

```bash
python can_decoder.py can0 --changes-only
```

This mode is particularly useful for:
- Identifying which messages change during specific vehicle actions
- Reducing noise from periodic/heartbeat messages
- Focusing on actionable CAN traffic

## Message Filtering

The script can be configured to monitor only specific messages by modifying the `FILTERED_MESSAGE_NAMES` constant in the code:

```python
FILTERED_MESSAGE_NAMES = [
    "BCM_Lamp_Stat_FD1",     # Body Control Module lamp status
    "Locking_Systems_2_FD1"   # Door lock system status
]
```

When filtering is enabled:
- Only the specified messages will be captured and displayed
- Hardware-level CAN filtering is applied for efficiency
- Unknown message IDs will be ignored

To monitor all messages, set `FILTERED_MESSAGE_NAMES = []`.

## Output Format

### Console Output

The script displays real-time CAN messages in a formatted table:

```
    Time      ID    DLC Data                 Decoded Message
--------------------------------------------------------------------------------
   1.234s  0x3B3  DLC:8 [02 00 FF 12 34 56 78 9A] -> BCM_Lamp_Stat_FD1 (8 signals) | HeadLamp_Lobeam_Left_Stat=0, HeadLamp_Lobeam_Right_Stat=0, HeadLamp_Hibeam_Left_Stat=1
```

Columns:
- **Time**: Relative timestamp since script start
- **ID**: CAN message ID in hexadecimal
- **DLC**: Data Length Code (number of data bytes)
- **Data**: Raw message data in hexadecimal
- **Decoded Message**: Message name, signal count, and key signal values

### Change Indicators

In `--changes-only` mode, messages are marked as:
- `[NEW]` - First time seeing this message ID
- `[CHANGED]` - Message content has changed since last seen

### Log Files

All messages are logged to timestamped JSON files:
- Filename format: `can_decoded_YYYYMMDD_HHMMSS_<interface>[_changes].log`
- Each line contains a complete JSON record with raw and decoded data
- Includes timestamps, signal values, and metadata

Example log entry:
```json
{
  "timestamp_abs": "2025-05-23T14:30:45.123456",
  "timestamp_rel": 1.234,
  "is_changed": true,
  "raw_message": {
    "arbitration_id": 947,
    "arbitration_id_hex": "0x3B3",
    "dlc": 8,
    "data": [2, 0, 255, 18, 52, 86, 120, 154],
    "data_hex": "02 00 FF 12 34 56 78 9A"
  },
  "decoded_message": {
    "message_name": "BCM_Lamp_Stat_FD1",
    "frame_id": 947,
    "signals": {
      "HeadLamp_Lobeam_Left_Stat": 0,
      "HeadLamp_Lobeam_Right_Stat": 0,
      "HeadLamp_Hibeam_Left_Stat": 1
    }
  }
}
```

## Command Line Options

- `can_interface` - Required. CAN interface name (e.g., can0, slcan0)
- `--dbc PATH` - Optional. Custom DBC file path (default: ford_lincoln_base_pt.dbc)
- `--changes-only` - Optional. Only show messages when content changes

# CAN Signal Dashboard (can_dashboard.py)

A live terminal dashboard for monitoring specific CAN signals in real-time.

## Usage

### Basic Usage

```bash
python can_dashboard.py <can_interface>
```

Examples:
```bash
# Monitor on can0 interface
python can_dashboard.py can0

# Monitor on slcan0 interface (serial CAN adapter)
python can_dashboard.py slcan0

# Use custom DBC file
python can_dashboard.py can0 --dbc custom_ford.dbc
```

## Dashboard Configuration

The dashboard is configured by modifying the `DASHBOARD_CONFIG` dictionary at the top of the script:

```python
DASHBOARD_CONFIG = {
    "BCM_Lamp_Stat_FD1": {
        "signals": ["PudLamp_D_Rq", "Illuminated_Entry_Stat", "Dr_Courtesy_Light_Stat"],
        "refresh_rate": 0.1  # seconds between updates
    },
    "Locking_Systems_2_FD1": {
        "signals": ["Veh_Lock_Status"],
        "refresh_rate": 0.1
    }
}
```

### Default Configuration

The dashboard comes pre-configured to monitor:

**BCM_Lamp_Stat_FD1** (Body Control Module Lamp Status):
- `PudLamp_D_Rq` - Puddle lamp request
- `Illuminated_Entry_Stat` - Illuminated entry status  
- `Dr_Courtesy_Light_Stat` - Door courtesy light status

**Locking_Systems_2_FD1** (Door Lock System):
- `Veh_Lock_Status` - Vehicle lock status

### Adding More Signals

To monitor additional messages and signals, simply add them to the configuration:

```python
DASHBOARD_CONFIG = {
    # ...existing config...
    "Your_Message_Name": {
        "signals": ["Signal1", "Signal2", "Signal3"],
        "refresh_rate": 0.1
    }
}
```

The script will automatically:
- Validate that the message exists in the DBC file
- Check that all configured signals are defined in the message
- Apply hardware-level CAN filtering for efficiency
- Handle signal decoding and value formatting

## Dashboard Display

The dashboard provides a clear, live view of your configured signals:

```
================================================================================
                              CAN SIGNAL DASHBOARD                              
================================================================================
Interface: can0             Runtime:     45.3s
Messages: 1247       Decoded: 186        Updates: 186

📊 BCM_Lamp_Stat_FD1
------------------------------------------------------------
   Status: ✅ LIVE
   PudLamp_D_Rq             : OFF (0)
   Illuminated_Entry_Stat   : ON (1)
   Dr_Courtesy_Light_Stat   : OFF (0)

📊 Locking_Systems_2_FD1
------------------------------------------------------------
   Status: ✅ LIVE
   Veh_Lock_Status          : LOCK_ALL (1)

================================================================================
Press Ctrl+C to stop
```

### Status Indicators

- ✅ **LIVE** - Data received within the last second
- ⏳ **OLD** - Data received 1-5 seconds ago
- ⚠️ **STALE** - No data received for more than 5 seconds
- **Waiting for data...** - No data received yet for this message

### Value Formatting

The dashboard automatically formats signal values based on their type:
- Boolean values: `ON`/`OFF`
- Numeric values: Raw numbers or formatted decimals
- Enumerated values: `NAME (value)` format when DBC defines named values
- Unknown values: `N/A`

## Command Line Options

- `can_interface` - Required. CAN interface name (e.g., can0, slcan0)
- `--dbc PATH` - Optional. Custom DBC file path (default: ford_lincoln_base_pt.dbc)

## Session Statistics

When you exit the dashboard (Ctrl+C), you'll see a session summary:

```
================================================================================
DASHBOARD SESSION SUMMARY
================================================================================
Runtime: 67.4 seconds
Total CAN messages received: 2156
Messages decoded for dashboard: 298
Dashboard updates: 298
Message rate: 32.0 msg/sec
Disconnected from can0
```

# Use Cases

## Vehicle Feature Analysis with can_decoder.py
```bash
# Monitor door lock messages while testing lock/unlock
python can_decoder.py can0 --changes-only
# Then perform lock/unlock actions and observe changes
```

## Real-time Signal Monitoring with can_dashboard.py
```bash
# Watch lighting and lock status in real-time
python can_dashboard.py can0
# Operate lights, locks, etc. and see immediate feedback
```

## Lighting System Investigation
```bash
# Configure filtering for lamp status messages
# Edit FILTERED_MESSAGE_NAMES = ["BCM_Lamp_Stat_FD1"]
python can_decoder.py can0 --changes-only
# Then operate headlights, turn signals, etc.
```

## General CAN Bus Exploration
```bash
# Monitor all messages to understand overall bus activity
python can_decoder.py can0
```

# Technical Notes

## CAN Interface Setup
Ensure your CAN interface is properly configured:
```bash
# For physical CAN interfaces
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# For USB CAN adapters, the interface may appear as slcan0
```

## Message Filtering
- Hardware-level filtering is applied at the CAN interface for efficiency
- Only exact CAN ID matches are captured when filtering is enabled
- Filtering reduces CPU load and focuses analysis on relevant messages

## Signal Decoding
- Signals are decoded according to DBC definitions (scaling, offsets, units)
- Enumerated values are properly decoded (e.g., 0="Off", 1="On")
- Complex signal types are handled and made JSON-serializable for logging

# Statistics and Monitoring

Both scripts provide real-time statistics:
- Total messages received
- Messages displayed/decoded
- Successfully decoded message count
- Decode rate percentage
- Session duration

Statistics are updated regularly and displayed in the final summary.

# Troubleshooting

**DBC file not found**: Ensure `ford_lincoln_base_pt.dbc` is in the same directory as the script

**CAN interface error**: Check that the interface exists and is properly configured:
```bash
ip link show | grep can
```

**Permission denied**: CAN interfaces may require elevated privileges:
```bash
sudo python can_decoder.py can0
sudo python can_dashboard.py can0
```

**No messages received**: Verify the CAN bus is active and the vehicle's CAN system is powered on

**Dashboard shows "Waiting for data"**: Check that the configured messages are actually present on the CAN bus. You can use `can_decoder.py` to see all available messages first.

**Signal not found in DBC**: The dashboard will show warnings during