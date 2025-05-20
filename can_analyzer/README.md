# CAN Bus Action Analyzer

This tool helps identify CAN bus messages on a vehicle network that correspond to specific physical actions like locking/unlocking doors or operating lights.

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
python can_action_analyzer.py
```

## Usage Guide

The tool provides an interactive menu-driven interface:

1. First, establish a baseline by capturing normal CAN traffic when no actions are being performed
2. Next, capture traffic while performing a specific action (e.g., locking doors)
   - You can now perform the action multiple times during a single capture for better confidence
3. Use the analysis feature to compare the action capture with the baseline
4. The tool will identify messages that are:
   - New (not present in baseline)
   - Have changed frequency
   - Contain new data patterns

## Workflow Example

1. Start the program
2. Select option 1 to establish a baseline (10 seconds of normal traffic)
3. Select option 2 to capture an action:
   - Name it "lock_doors"
   - Choose to repeat it multiple times (e.g., 3 times with 5-second intervals)
   - Perform the action each time you're prompted
4. Select option 3 to analyze differences
5. Note the message IDs and data patterns that change
6. Repeat steps 2-5 for other actions like "unlock_doors", "lights_on", etc.
7. Use option 5 to visualize message timelines, which now show markers for when each action was performed

## Tips for Better Results

- Keep the vehicle in the same state (ignition on/off) for both baseline and action captures
- Use the multi-action feature to repeat the same action several times in one capture
- Timeline visualizations now show vertical lines marking when each action was performed
- Longer capture durations may be needed for infrequent messages
- Use the visualization to help understand the timing of messages

## Saving and Loading Data

All captures are automatically saved to the `can_sessions` directory. You can load previous captures using option 4 in the main menu.

## Output Examples

The analysis will show differences similar to:

```
ANALYSIS RESULTS - Found 3 potential messages of interest
========================================================================================

1. Message ID: 0x123 - Confidence: 95.0%
   ✓ NEW MESSAGE: Not present in baseline, appeared 5 times
   ✓ NEW DATA PATTERNS: 1 new patterns
     1. 01 00 FF 23 45 67 00 00

2. Message ID: 0x456 - Confidence: 85.0%
   ✓ FREQUENCY INCREASED: +120.5%
   ✓ NEW DATA PATTERNS: 2 new patterns
     1. 10 A0 05 00 00 00 00 00
     2. 10 A0 06 00 00 00 00 00
```

The timeline visualization helps you see exactly when messages appeared relative to when you performed the action, with vertical lines marking each time you performed the action.
