# CAN Bus Action Analyzer - Project Context

## Project Overview

This project creates a tool for analyzing and reverse-engineering CAN bus messages on automotive networks. The tool is designed to help identify specific CAN messages that correspond to physical actions performed on a vehicle (like locking/unlocking doors or light controls) without prior knowledge of the CAN message structure.

## Main Components

1. **`can_action_analyzer.py`** - The main Python script providing:
   - Interactive command-line interface
   - CAN bus message capture via SocketCAN
   - Comparison of baseline vs. action captures
   - Support for multiple action repetitions in a single capture
   - Message analysis and confidence scoring
   - Timeline visualization with action markers

2. **`requirements.txt`** - Dependencies:
   - python-can (for SocketCAN interface)
   - numpy (for data analysis)
   - matplotlib (for visualization)

3. **`README.md`** - Documentation for setup and usage

## Key Design Decisions

1. **Interface**:
   - Used SocketCAN as the interface to the CAN bus
   - Decided on a menu-driven interactive CLI for ease of use
   - Set the default CAN bus interface to `can0` with 125kbps bitrate

2. **Safety**:
   - Implemented listen-only mode as a critical safety measure
   - Added explicit warnings about not transmitting on the vehicle bus

3. **Analysis Approach**:
   - Capture baseline traffic with no actions performed
   - Capture traffic during specific physical actions
   - Compare to identify differences in:
     - New messages (not in baseline)
     - Changes in message frequency
     - New data patterns

4. **Multi-Action Enhancement**:
   - Added support for repeating actions multiple times in a single capture
   - Implemented timestamp tracking for when actions are performed
   - Added correlation detection between message appearances and action times
   - Enhanced confidence scoring based on temporal correlation
   - Added visual markers on timeline plots for action timestamps

5. **Visualization**:
   - Timeline plots showing message occurrences over time
   - Vertical markers showing when actions were performed
   - Enhanced to visualize correlation between actions and messages

## Project Evolution

1. **Initial Version**:
   - Basic capture and comparison functionality
   - Single action capture

2. **Enhanced Version**:
   - Added multi-action support for better statistical confidence
   - Improved visualization with action markers
   - Enhanced analysis to detect correlation with action timing
   - Updated UI to guide through multi-action process
   - Added special handling for metadata about action timestamps

3. **Safety Enhancement**:
   - Updated documentation to emphasize listen-only mode
   - Added explicit commands for setting up CAN in listen-only mode

## Technical Implementation Details

1. **Message Capture**:
   - Uses python-can library to connect to SocketCAN
   - Timestamps messages relative to capture start
   - Stores messages in memory with complete metadata

2. **Analysis Algorithm**:
   - Groups messages by ID
   - Calculates frequency changes between baseline and action captures
   - Identifies new data patterns
   - For multi-action captures, looks for temporal correlation with actions
   - Calculates confidence score based on multiple factors

3. **Data Storage**:
   - Saves captures as JSON files for later analysis
   - Includes all message metadata and action timestamps
   - Converts binary message data to/from lists for JSON serialization

4. **Visualization**:
   - Uses matplotlib to create timeline plots
   - Shows when messages occur relative to actions
   - Places vertical lines at action timestamps

## Intended Usage Workflow

1. Set up Raspberry Pi with Waveshare CAN HAT
2. Connect to vehicle CAN bus in listen-only mode
3. Run the analyzer to establish baseline traffic
4. Capture traffic while performing specific actions (e.g., lock doors)
5. Analyze differences to identify relevant messages
6. Repeat with different actions to build a comprehensive understanding

## Future Enhancements Considered

1. Real-time message filtering based on identified patterns
2. Support for sequence detection (e.g., patterns of messages)
3. Deeper statistical analysis across multiple captures
4. Machine learning for automatic message identification
5. Support for additional CAN interfaces beyond SocketCAN

## Known Limitations

1. Works best with actions that cause immediate, distinct CAN messages
2. May be less effective with actions that cause subtle changes
3. Requires manual action performance during capture
4. Limited to the capabilities of the Python-can library
5. Designed for 125kbps networks; may need adjustment for different speeds

---

*This context document was generated on May 20, 2025, based on our collaborative development of the CAN Bus Action Analyzer tool.*
