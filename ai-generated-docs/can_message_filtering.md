# CAN Message Filtering in Ford F-150 Gen14 CAN Bus Interface

**Document Version:** 1.0  
**Date:** August 4, 2025  
**Application:** Ford F-150 Gen14 CAN Bus Interface  
**Hardware:** ESP32 with MCP2515 CAN Controller  

## Executive Summary

The Ford F-150 Gen14 CAN Bus Interface application implements **software-based CAN message filtering** that is **always active during normal operation**. The system receives all CAN messages from the bus but only processes 4 specific target message types, filtering out all other traffic for efficiency and focus.

## Overview

### What is CAN Message Filtering?

CAN message filtering is a technique used to process only relevant CAN messages from the vehicle's CAN bus, which typically carries hundreds of different message types. This application uses filtering to focus on the specific messages needed for its functionality while ignoring irrelevant traffic.

### Filtering Approach Used

- **Type:** Software-based filtering (not hardware filtering)
- **Scope:** Always active during normal operation
- **Method:** Runtime ID comparison using `isTargetCANMessage()` function
- **Coverage:** 4 specific message IDs out of 2048 possible CAN IDs (11-bit)

## Hardware vs Software Filtering

### Hardware Filtering (NOT Used)

The MCP2515 CAN controller supports hardware-level message filtering through acceptance filters, but this capability is **intentionally disabled** in this application:

```cpp
// From can_interface_test/src/main.cpp
static const twai_filter_config_t filter_config = {
    .acceptance_code = 0x00000000,  // Accept all
    .acceptance_mask = 0x00000000,  // No masking
    .single_filter = true
};
```

**Why hardware filtering is disabled:**
- Maximum flexibility for debugging
- Ability to monitor all CAN traffic when needed
- Simplified hardware configuration
- Software filtering provides sufficient efficiency

### Software Filtering (ALWAYS Used)

The application implements efficient software filtering that:
- Receives all CAN messages from the bus
- Applies filtering logic in the main processing loop
- Only processes messages that match target criteria
- Discards non-target messages immediately

## Target Messages

The application filters for exactly **4 specific CAN message IDs**:

| Message ID (Hex) | Message ID (Decimal) | Message Name | Purpose |
|------------------|---------------------|--------------|---------|
| `0x3C3` | 963 | BCM_Lamp_Stat_FD1 | Body Control Module lamp status (puddle lamps) |
| `0x331` | 817 | Locking_Systems_2_FD1 | Vehicle door lock status |
| `0x176` | 374 | PowertrainData_10 | Transmission park status |
| `0x43C` | 1084 | Battery_Mgmt_3_FD1 | Battery state of charge |

### Message ID Constants

These are defined in `src/config.h`:

```cpp
#define BCM_LAMP_STAT_FD1_ID 0x3C3      // 963 decimal
#define LOCKING_SYSTEMS_2_FD1_ID 0x331  // 817 decimal
#define POWERTRAIN_DATA_10_ID 0x176     // 374 decimal
#define BATTERY_MGMT_3_FD1_ID 0x43C     // 1084 decimal
```

## Implementation Details

### Core Filtering Function

The filtering logic is implemented in `src/can_manager.cpp`:

```cpp
bool isTargetCANMessage(uint32_t messageId) {
    return (messageId == BCM_LAMP_STAT_FD1_ID ||
            messageId == LOCKING_SYSTEMS_2_FD1_ID ||
            messageId == POWERTRAIN_DATA_10_ID ||
            messageId == BATTERY_MGMT_3_FD1_ID);
}
```

**Design characteristics:**
- Simple boolean logic (very fast execution)
- No lookup tables or hash maps (minimal memory)
- Compile-time constants (optimal performance)
- Easy to modify and maintain

### Main Loop Integration

The filtering is applied in the main processing loop (`src/main.cpp`):

```cpp
while (receiveCANMessage(message) && messagesProcessed < MAX_MESSAGES_PER_LOOP) {
    messagesProcessed++;
    systemHealth.lastCanActivity = currentTime;
    
    if (isTargetCANMessage(message.id)) {
        // Parse and process the target message
        switch (message.id) {
            case BCM_LAMP_STAT_FD1_ID:
                // Handle BCM lamp status
                break;
            case LOCKING_SYSTEMS_2_FD1_ID:
                // Handle locking systems status
                break;
            case POWERTRAIN_DATA_10_ID:
                // Handle powertrain data
                break;
            case BATTERY_MGMT_3_FD1_ID:
                // Handle battery management data
                break;
        }
    }
    // Non-target messages are silently discarded here
}
```

### Performance Characteristics

- **Execution Time:** <1ms for message recognition
- **Memory Usage:** Minimal (no lookup tables)
- **CPU Load:** Very low (simple boolean comparisons)
- **Filter Efficiency:** Typically >50% of messages filtered out

## When Filtering is Active

### Normal Operation (Filtering ALWAYS Active)

During normal operation, filtering is continuously applied:
- Every received CAN message is tested against the filter
- Only target messages proceed to parsing and processing
- Non-target messages are discarded immediately
- No exceptions or bypasses during normal operation

### Debug Mode (Filtering Bypassed)

When CAN connection issues are detected, debug mode may be activated:

```cpp
if (!isCANConnected()) {
    LOG_WARN("CAN bus status: Disconnected");
    // Enable debug message monitoring when having connection issues
    LOG_INFO("=== DEBUG: Attempting to receive ANY CAN messages ===");
    debugReceiveAllMessages();  // Bypasses filtering
}
```

**Debug mode characteristics:**
- Temporarily receives and logs ALL CAN messages
- Used only for troubleshooting connectivity issues
- Automatically returns to normal filtering when connection restored
- Limited to prevent message spam (max 50 messages per debug session)

### Recovery Mode

During error recovery, the system may temporarily use debug mode to:
- Verify CAN bus connectivity
- Diagnose communication problems
- Test message reception capabilities

## Filtering Efficiency

### Expected Performance

Based on testing and Ford F-150 CAN bus characteristics:

- **Total CAN Messages:** Hundreds of different message types
- **Target Messages:** 4 specific message types
- **Filter Rate:** >50% of messages typically filtered out
- **Processing Load:** Significant reduction in CPU and memory usage

### Measurement Example

From test results simulating realistic CAN bus load:

```
CAN Bus Load Simulation Results:
  Total messages: 16
  Target messages: 4 (25.0%)
  Filtered messages: 12 (75.0%)
  Filter efficiency: 75.0%
```

## Python Implementation Compatibility

The C++ filtering implementation is designed to match the Python reference implementation used in the experimental tools:

### Python Reference

```python
# From experiments/message_watcher/can_embedded_logger.py
CAN_MESSAGES = {
    0x3C3: 'BCM_Lamp_Stat_FD1',
    0x331: 'Locking_Systems_2_FD1',
    0x176: 'PowertrainData_10',
    0x43C: 'Battery_Mgmt_3_FD1'
}

CAN_FILTER_IDS = list(CAN_MESSAGES.keys())
```

### Hardware-level Filtering in Python Tools

The Python monitoring tools use hardware-level filtering at the SocketCAN interface:

```python
can_filters = []
for can_id in self.filtered_message_ids:
    can_filters.append({
        "can_id": can_id,
        "can_mask": 0x7FF,
        "extended": False
    })

self.bus = can.interface.Bus(
    channel=self.can_interface,
    bustype='socketcan',
    can_filters=can_filters
)
```

This ensures consistency between the embedded application and monitoring tools.

## Testing and Validation

### Test Coverage

The filtering implementation is thoroughly tested in the test suite:

- **Message ID Recognition:** Validates all target IDs are recognized
- **Non-target Filtering:** Ensures non-target messages are rejected
- **Performance Testing:** Measures execution time and memory usage
- **Pipeline Integration:** Tests complete message processing flow
- **Python Compatibility:** Validates matching behavior with reference implementation

### Test Files

- `test/test_can_message_recognition.cpp` - Primary filtering tests
- `test/test_message_parser.cpp` - Integration with message parsing
- `test/test_can_protocol_integration.cpp` - End-to-end validation

## Configuration and Modification

### Adding New Target Messages

To add a new target message:

1. **Define the message ID** in `src/config.h`:
   ```cpp
   #define NEW_MESSAGE_ID 0x123  // Example ID
   ```

2. **Update the filtering function** in `src/can_manager.cpp`:
   ```cpp
   bool isTargetCANMessage(uint32_t messageId) {
       return (messageId == BCM_LAMP_STAT_FD1_ID ||
               messageId == LOCKING_SYSTEMS_2_FD1_ID ||
               messageId == POWERTRAIN_DATA_10_ID ||
               messageId == BATTERY_MGMT_3_FD1_ID ||
               messageId == NEW_MESSAGE_ID);  // Add new ID
   }
   ```

3. **Add message parsing** in the main loop switch statement
4. **Update test cases** to include the new message ID

### Removing Target Messages

Simply remove the message ID from the `isTargetCANMessage()` function and corresponding parsing logic.

## Troubleshooting

### Common Issues

**No CAN messages being processed:**
- Check if `isTargetCANMessage()` recognizes your expected message IDs
- Verify message IDs match the actual Ford F-150 CAN specification
- Use debug mode to see all received messages

**Unexpected messages being processed:**
- Verify message ID constants are correct
- Check for duplicate or conflicting message IDs

**Performance issues:**
- Monitor filter efficiency - should be >50%
- Check for excessive non-target message volume
- Consider hardware filtering if software filtering becomes insufficient

### Debug Commands

The application provides serial commands for filtering diagnostics:
- View CAN statistics and filter performance
- Enable/disable debug message monitoring
- Monitor real-time filtering decisions

## Future Considerations

### Potential Improvements

1. **Dynamic Filtering:** Runtime configuration of target message IDs
2. **Hardware Filtering:** Enable MCP2515 hardware filters for maximum efficiency
3. **Adaptive Filtering:** Automatically adjust filtering based on bus load
4. **Message Prioritization:** Different processing priorities for different message types

### Scalability

The current implementation can easily handle:
- Additional target message IDs (simply extend the boolean logic)
- Higher CAN bus loads (software filtering is very efficient)
- Different vehicle models (modify message IDs as needed)

## Conclusion

The Ford F-150 Gen14 CAN Bus Interface implements a robust, efficient software-based CAN message filtering system that:

- **Always operates** during normal system function
- **Focuses processing** on 4 specific target message types
- **Filters out** irrelevant CAN bus traffic efficiently
- **Maintains compatibility** with Python reference implementations
- **Provides flexibility** for debugging and troubleshooting
- **Scales well** for future enhancements

This filtering approach provides an optimal balance between performance, flexibility, and maintainability for the embedded automotive application.
