# CAN Bus Action Analyzer - Project Context

## Project Overview

This project creates a tool for analyzing and reverse-engineering CAN bus messages on automotive networks. The tool is designed to help identify specific CAN messages that correspond to physical actions performed on a vehicle (like locking/unlocking doors or light controls) without prior knowledge of the CAN message structure.

## Main Components

1. **`can_action_analyzer.py`** - The main Python script providing:
   - Interactive command-line interface
   - CAN bus message capture via SocketCAN
   - Comparison of baseline vs. action captures
   - Support for multiple action repetitions in a single capture
   - **NEW: Specialized binary toggle capture and analysis**
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
   - **NEW: Binary toggle analysis for two-state functions**

4. **Multi-Action Enhancement**:
   - Added support for repeating actions multiple times in a single capture
   - Implemented timestamp tracking for when actions are performed
   - Added correlation detection between message appearances and action times
   - Enhanced confidence scoring based on temporal correlation
   - Added visual markers on timeline plots for action timestamps

5. **Binary Toggle Optimization**:
   - **NEW: Specialized capture mode for binary state changes**
   - **NEW: Automatic detection of messages with exactly 2 payloads**
   - **NEW: State-to-payload mapping for confirmed toggles**
   - **NEW: Enhanced correlation analysis for toggle events**

6. **Visualization**:
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

4. **Binary Toggle Optimization (Latest)**:
   - **NEW: Added specialized binary toggle capture mode (Menu option 6)**
   - **NEW: Added binary toggle analysis (Menu option 7)**
   - **NEW: Implemented payload correlation with state changes**
   - **NEW: Added state mapping functionality**
   - **NEW: Enhanced metadata storage for toggle events**

## Technical Implementation Details

1. **Message Capture**:
   - Uses python-can library to connect to SocketCAN
   - Timestamps messages relative to capture start
   - Stores messages in memory with complete metadata
   - **NEW: Binary toggle metadata includes state descriptions and toggle events**

2. **Analysis Algorithm**:
   - Groups messages by ID
   - Calculates frequency changes between baseline and action captures
   - Identifies new data patterns
   - For multi-action captures, looks for temporal correlation with actions
   - **NEW: Binary toggle analysis specifically looks for exactly 2 unique payloads**
   - **NEW: Maps payloads to specific states based on toggle timing**
   - Calculates confidence score based on multiple factors

3. **Data Storage**:
   - Saves captures as JSON files for later analysis
   - Includes all message metadata and action timestamps
   - **NEW: Includes toggle events, state descriptions, and capture type**
   - Converts binary message data to/from lists for JSON serialization

4. **Visualization**:
   - Uses matplotlib to create timeline plots
   - Shows when messages occur relative to actions
   - Places vertical lines at action timestamps
   - **NEW: Enhanced for binary toggle visualization**

## Binary Toggle Feature Details

### New Capture Method (`capture_binary_toggle`)
- Guides user through alternating between two defined states
- Records precise timing of each state change
- Captures messages during each state period
- Stores rich metadata about the toggle process

### New Analysis Method (`analyze_binary_toggles`)
- Identifies messages with exactly 2 unique payloads
- Correlates payload changes with toggle events using time windows
- Calculates correlation ratios and confidence scores
- Maps specific payloads to specific states when possible
- Distinguishes confirmed binary toggles from other correlated messages

### Enhanced Output
- Clear separation of confirmed binary toggles vs. other correlations
- State-to-payload mapping when correlation is strong enough
- Visual indicators (🎯 for confirmed toggles, 📊 for other correlations)
- Confidence percentages based on correlation strength

## Intended Usage Workflow

### Standard Workflow:
1. Set up Raspberry Pi with Waveshare CAN HAT
2. Connect to vehicle CAN bus in listen-only mode
3. Run the analyzer to establish baseline traffic
4. Capture traffic while performing specific actions
5. Analyze differences to identify relevant messages

### Binary Toggle Workflow (NEW):
1. Set up hardware as above
2. **Use Menu option 6 for binary toggle actions (locks, lights, etc.)**
3. **Define the two states clearly (e.g., "doors locked" vs "doors unlocked")**
4. **Follow guided alternating state process**
5. **Review automatic analysis showing confirmed binary toggle messages**
6. **Use visualization to see toggle correlation**

## Future Enhancements Considered

1. Real-time message filtering based on identified patterns
2. Support for sequence detection (e.g., patterns of messages)
3. Deeper statistical analysis across multiple captures
4. Machine learning for automatic message identification
5. Support for additional CAN interfaces beyond SocketCAN
6. **Multi-state analysis (beyond binary) for actions with 3+ states**
7. **Pattern recognition for complex state machines**

## Known Limitations

1. Works best with actions that cause immediate, distinct CAN messages
2. May be less effective with actions that cause subtle changes
3. Requires manual action performance during capture
4. Limited to the capabilities of the Python-can library
5. Designed for 125kbps networks; may need adjustment for different speeds
6. **Binary toggle analysis requires exactly 2 distinct payloads for optimal results**

---

*This context document was last updated on May 22, 2025, to reflect the addition of specialized binary toggle detection functionality.*
