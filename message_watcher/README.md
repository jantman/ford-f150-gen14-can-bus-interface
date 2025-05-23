# CAN Bus Message Decoder

A Python script for monitoring and decoding CAN bus messages on Ford vehicles using DBC (Database CAN) files. This tool captures CAN messages from a SocketCAN interface, decodes them using Ford's DBC definitions, and provides both real-time display and comprehensive logging capabilities.

## Features

- **Real-time CAN message monitoring** with live console output
- **DBC-based message decoding** using Ford/Lincoln DBC definitions
- **Message filtering** to focus on specific CAN messages of interest
- **Change detection** to only show messages when their content actually changes
- **Comprehensive logging** with timestamped JSON output
- **Signal-level change tracking** to identify which specific signals have changed
- **Flexible filtering** based on message names defined in the DBC file

## Requirements

Install the required Python packages:

```bash
pip install -r requirements.txt
```

Required packages:
- `python-can` - CAN bus interface library
- `cantools` - DBC file parsing and message decoding

## DBC File

The script uses `ford_lincoln_base_pt.dbc` in the same directory for message decoding. This file contains the Ford/Lincoln CAN message definitions including:
- Message names and IDs
- Signal definitions and scaling
- Enumerated values for coded signals

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

## Use Cases

### Vehicle Feature Analysis
```bash
# Monitor door lock messages while testing lock/unlock
python can_decoder.py can0 --changes-only
# Then perform lock/unlock actions and observe changes
```

### Lighting System Investigation
```bash
# Configure filtering for lamp status messages
# Edit FILTERED_MESSAGE_NAMES = ["BCM_Lamp_Stat_FD1"]
python can_decoder.py can0 --changes-only
# Then operate headlights, turn signals, etc.
```

### General CAN Bus Exploration
```bash
# Monitor all messages to understand overall bus activity
python can_decoder.py can0
```

## Command Line Options

- `can_interface` - Required. CAN interface name (e.g., can0, slcan0)
- `--dbc PATH` - Optional. Custom DBC file path (default: ford_lincoln_base_pt.dbc)
- `--changes-only` - Optional. Only show messages when content changes

## Statistics and Monitoring

The script provides real-time statistics:
- Total messages received
- Messages displayed (in changes-only mode)
- Successfully decoded message count
- Decode rate percentage
- Session duration

Statistics are updated every 100 messages and displayed in the final summary.

## Technical Notes

### CAN Interface Setup
Ensure your CAN interface is properly configured:
```bash
# For physical CAN interfaces
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# For USB CAN adapters, the interface may appear as slcan0
```

### Message Filtering
- Hardware-level filtering is applied at the CAN interface for efficiency
- Only exact CAN ID matches are captured when filtering is enabled
- Filtering reduces CPU load and focuses analysis on relevant messages

### Signal Decoding
- Signals are decoded according to DBC definitions (scaling, offsets, units)
- Enumerated values are properly decoded (e.g., 0="Off", 1="On")
- Complex signal types are handled and made JSON-serializable for logging

## Troubleshooting

**DBC file not found**: Ensure `ford_lincoln_base_pt.dbc` is in the same directory as the script

**CAN interface error**: Check that the interface exists and is properly configured:
```bash
ip link show | grep can
```

**Permission denied**: CAN interfaces may require elevated privileges:
```bash
sudo python can_decoder.py can0
```

**No messages received**: Verify the CAN bus is active and the vehicle's CAN system is powered on

## License

This tool is designed for legitimate automotive research and diagnostics. Always comply with local laws and vehicle warranty terms when performing CAN bus analysis.