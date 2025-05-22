# CAN Bus Action Analyzer

This tool helps identify CAN bus messages on a vehicle network that correspond to specific physical actions like locking/unlocking doors or operating lights. It now includes specialized support for binary state toggle detection.

## Setup Instructions

### Hardware Requirements
- Raspberry Pi
- Waveshare 2-channel isolated CAN FD HAT
- Connection to vehicle CAN bus (125Kbps)

### Software Setup (On Raspberry Pi)

1. Copy the files to your Raspberry Pi

2. Install the required Python packages:
```
pip install -r requirements.txt
```

3. Make sure SocketCAN is set up in listen-only mode for safety. If not already configured, run these commands:
```
# Set up CAN interface with appropriate bitrate (125kbps) in listen-only mode
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 125000 listen-only on restart-ms 1000
sudo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536

# Verify the interface is up and in listen-only mode
ip -details link show can0
```

The listen-only mode is critical for safely monitoring a vehicle's CAN bus as it ensures your device:
- Cannot transmit messages onto the vehicle bus
- Will not interfere with normal vehicle operations
- Cannot accidentally send potentially harmful messages

4. Make the script executable:
```
chmod +x can_action_analyzer.py
```

5. Run the analyzer:
```
python can_action_analyzer.py can0
```

## Usage Guide

The tool provides an interactive menu-driven interface with the following options:

1. **Establish baseline** - Capture normal CAN traffic when no actions are being performed
2. **Capture specific action** - Capture traffic while performing a specific action (e.g., locking doors)
   - You can perform the action multiple times during a single capture for better confidence
3. **Analyze differences** - Compare action captures with baseline to identify relevant messages
4. **Save/load captures** - Manage capture files for later analysis
5. **Visualize timeline** - Plot message timelines with action markers
6. **🆕 Capture binary toggle** - Specialized capture for on/off, lock/unlock type actions
7. **🆕 Analyze binary toggle** - Advanced analysis for binary state changes

## Binary Toggle Detection (New Feature)

This tool now includes specialized functionality for identifying CAN messages that control binary states (on/off, lock/unlock, open/close, etc.). This is particularly useful for:

- Door locks/unlocks
- Light switches (headlights, turn signals, etc.)
- Window controls (up/down)
- Any two-state vehicle function

### Binary Toggle Workflow

1. Select option 6 "Capture binary toggle action"
2. Describe the two states (e.g., "doors unlocked" and "doors locked")
3. The tool will prompt you to alternate between states multiple times
4. Analysis automatically identifies messages that:
   - Change between exactly 2 payloads
   - Correlate strongly with your state changes
   - Map specific payloads to specific states

### Binary Toggle Analysis Results

The analysis will show results like:

```
🎯 CONFIRMED BINARY TOGGLE MESSAGES (2 found):
============================================================

1. Message ID: 0x123 - Confidence: 95.0%
   ✓ BINARY TOGGLE DETECTED: Changes between exactly 2 payloads
   ✓ Correlation: 5/5 toggles matched
   ✓ STATE MAPPING:
     'doors unlocked': 00 00 00 00 00 00 00 00
     'doors locked': 01 00 00 00 00 00 00 00
```

## Standard Workflow Example

1. Start the program: `python can_action_analyzer.py can0`
2. Select option 1 to establish a baseline (10 seconds of normal traffic)
3. For binary toggle actions:
   - Select option 6 for binary toggle capture
   - Name it "door_lock_toggle"
   - Let it guide you through alternating states 5 times
   - Review the automatic analysis results
4. For other actions:
   - Select option 2 to capture an action
   - Choose to repeat it multiple times (e.g., 3 times with 5-second intervals)
   - Use option 3 to analyze differences with baseline
5. Use option 5 to visualize message timelines
6. Save important captures for later reference

## Tips for Better Results

- Keep the vehicle in the same state (ignition on/off) for all captures
- **For binary toggles**: Use the specialized binary toggle mode (option 6) for best results
- **For other actions**: Use the multi-action feature to repeat the same action several times
- Timeline visualizations show vertical lines marking when each action was performed
- Binary toggle analysis provides state-to-payload mapping when successful
- Longer capture durations may be needed for infrequent messages

## Saving and Loading Data

All captures are automatically saved to the `can_sessions` directory. Binary toggle captures include additional metadata about state changes and can be re-analyzed later using option 7.

## Output Examples

### Standard Analysis Output
```
ANALYSIS RESULTS - Found 3 potential messages of interest
========================================================================================

1. Message ID: 0x123 - Confidence: 95.0%
   ✓ NEW MESSAGE: Not present in baseline, appeared 5 times
   ✓ NEW DATA PATTERNS: 1 new patterns
     1. 01 00 FF 23 45 67 00 00
```

### Binary Toggle Analysis Output
```
🎯 CONFIRMED BINARY TOGGLE MESSAGES (1 found):
============================================================

1. Message ID: 0x456 - Confidence: 90.0%
   ✓ BINARY TOGGLE DETECTED: Changes between exactly 2 payloads
   ✓ Correlation: 4/5 toggles matched
   ✓ STATE MAPPING:
     'lights off': 00 A0 00 00 00 00 00 00
     'lights on': 01 A0 00 00 00 00 00 00

📊 OTHER CORRELATED MESSAGES (1 found):
============================================================

1. Message ID: 0x789 - Correlation: 40%
   • 3 unique payloads
   • 2 payload changes near toggle events
```

The binary toggle analysis specifically identifies messages that reliably change between exactly two states, making it much easier to identify the exact CAN messages responsible for binary vehicle functions.
