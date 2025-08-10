# Ford F150 Gen14 CAN Bus Technical Documentation

## CAN Message Specifications

This document details the specific CAN messages and## System Health Monitoring

### Timeout Detection
- **Timeout Period**: 600000ms (10 minutes)
- **Monitored Messages**: All four CAN messages
- **Action**: System marked as "not ready" only when ALL messages have timed out
- **Recovery**: Automatic when ANY fresh message is received

### Error Handling
- **CAN Bus Errors**: Automatic reinitialization
- **GPIO Errors**: Automatic reinitialization  
- **Critical Errors**: Safe shutdown mode (all outputs disabled)
- **Watchdog**: System health monitoring every loop cycle by the Ford F150 Gen14 CAN Bus Interface.

## CAN Bus Configuration

- **Speed**: 500 kbps
- **Frame Format**: Standard CAN 2.0A (11-bit identifiers)
- **Bus**: High-Speed CAN (HS-CAN)
- **Termination**: 120Ω (verify termination requirements for your specific installation)
- **Filtering**: Hybrid hardware + software filtering
  - **Hardware Filtering**: Configurable via `ENABLE_HARDWARE_CAN_FILTERING` (default: enabled)
  - **Software Filtering**: Always active as backup using `isTargetCANMessage()`
  - **Debugging**: Disable hardware filtering to receive all messages for troubleshooting

### Hardware Filtering Configuration

The MCP2515 controller supports hardware-level message filtering which is enabled by default for optimal performance:

```cpp
// In src/config.h - enabled by default
#define ENABLE_HARDWARE_CAN_FILTERING 1
```

**When enabled (recommended for production):**
- Only target message IDs are received by hardware
- Improved system efficiency and reduced CPU load
- Messages filtered before reaching software processing

**When disabled (for debugging CAN issues):**
- All CAN messages are received by hardware
- Software filtering still applies as backup
- Required when troubleshooting message reception problems
- Use `can_debug` serial command to see all traffic

## Monitored CAN Messages

### BCM_Lamp_Stat_FD1 (0x3B3)
**Body Control Module Lamp Status**

| Field | Start Bit | Length | Values | Description |
|-------|-----------|--------|---------|-------------|
| PudLamp_D_Rq | 11 | 2 bits | 0=OFF, 1=ON, 2=RAMP_UP, 3=RAMP_DOWN | Puddle lamp request status |
| IlluminatedEntry_D_Stat | 13 | 2 bits | Status values | Illuminated entry status |
| DrCourtesyLight_D_Stat | 15 | 2 bits | Status values | Driver courtesy light status |

**Usage**: Controls bedlight based on PudLamp_D_Rq signal
- Bedlight ON when PudLamp_D_Rq = 1 (ON) or 2 (RAMP_UP)
- Bedlight OFF when PudLamp_D_Rq = 0 (OFF) or 3 (RAMP_DOWN)

### Locking_Systems_2_FD1 (0x3B8)
**Vehicle Locking Systems Status**

| Field | Start Bit | Length | Values | Description |
|-------|-----------|--------|---------|-------------|
| Veh_Lock_Status | 34 | 2 bits | 0=DBL_LOCK, 1=LOCK_ALL, 2=UNLOCK_ALL, 3=UNLOCK_DRV | Vehicle lock status |

**Usage**: Controls toolbox activation logic
- Required for toolbox activation (must be unlocked)

### PowertrainData_10 (0x204)
**Powertrain Data Message**

| Field | Start Bit | Length | Values | Description |
|-------|-----------|--------|---------|-------------|
| TrnPrkSys_D_Actl | 31 | 4 bits | 0=UNKNOWN, 1=PARK, others=NOT_PARK | Transmission park system status |

**Usage**: Controls toolbox activation logic
- Required for toolbox activation (must be parked)
- **Default Value**: When the `POWERTRAIN_DATA_10` message is not received (e.g., vehicle not running), the system defaults to `TRNPRKSTS_PARK` (1), meaning the vehicle is considered parked until the actual transmission status is received.

### Battery_Mgmt_3_FD1 (0x3D2)
**Battery Management Status**

| Field | Start Bit | Length | Values | Description |
|-------|-----------|--------|---------|-------------|
| Battery_SOC | Various | 8 bits | 0-100 | Battery state of charge percentage |

**Usage**: System health monitoring (timeout detection)

## Signal Processing Logic

### Bit Extraction Algorithm
The application uses a robust bit extraction function that handles:
- Cross-byte boundary extraction
- Proper bit ordering (LSB first)
- Bounds checking and validation
- Support for 1-8 bit fields

### Message Validation
Each CAN message is validated for:
- Correct message ID
- Proper message length (8 bytes)
- Signal value ranges
- Timestamp freshness (5-second timeout)

### State Management
The system maintains comprehensive state tracking:

```c
typedef struct {
    // Current signal values
    uint8_t pudLampRequest;
    uint8_t vehicleLockStatus;
    uint8_t transmissionParkStatus;
    uint8_t batterySOC;
    
    // Previous values for change detection
    uint8_t prevPudLampRequest;
    uint8_t prevVehicleLockStatus;
    uint8_t prevTransmissionParkStatus;
    uint8_t prevBatterySOC;
    
    // Timestamps for timeout detection
    unsigned long lastBCMLampUpdate;
    unsigned long lastLockingSystemsUpdate;
    unsigned long lastPowertrainUpdate;
    unsigned long lastBatteryUpdate;
    
    // Derived states
    bool isUnlocked;
    bool isParked;
    bool bedlightShouldBeOn;
    bool systemReady;
    
    // Manual bed light control
    bool bedlightManualOverride;    // True when bed lights are manually controlled
    bool bedlightManualState;       // Manual override state (ON/OFF)
} VehicleState;
```

**Important State Initialization**: The transmission park status (`transmissionParkStatus`) defaults to `TRNPRKSTS_PARK` when the system initializes, which means the vehicle is considered parked by default until the `POWERTRAIN_DATA_10` message is received and processed. This ensures safe operation when the vehicle is not running and the powertrain message is not being transmitted on the CAN bus.

## Decision Logic Functions

### Bedlight Control
```c
bool shouldEnableBedlight(uint8_t pudLampRequest) {
    return (pudLampRequest == 1 || pudLampRequest == 2);  // ON or RAMP_UP
}
```

### Vehicle Status Detection
```c
bool isVehicleUnlocked(uint8_t vehicleLockStatus) {
    return (vehicleLockStatus == 2 || vehicleLockStatus == 3);  // UNLOCK_ALL or UNLOCK_DRV
}

bool isVehicleParked(uint8_t transmissionParkStatus) {
    return (transmissionParkStatus == 1);  // PARK
}
```

### Toolbox Activation Logic
```c
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked) {
    return systemReady && isParked && isUnlocked;
}
```

### Manual Bed Light Override Logic
The system supports manual bed light control independent of CAN bus signals:

```c
// Manual override decision logic
bool getBedlightState(VehicleState* state) {
    if (state->bedlightManualOverride) {
        // Use manual state when override is active
        return state->bedlightManualState;
    } else {
        // Use automatic CAN-based control
        return state->bedlightShouldBeOn;
    }
}

// Toggle manual override state
void toggleBedlightManualOverride(VehicleState* state) {
    if (state->bedlightManualOverride) {
        // Already in manual mode - toggle manual state
        state->bedlightManualState = !state->bedlightManualState;
    } else {
        // Enter manual mode with opposite of current automatic state
        state->bedlightManualOverride = true;
        state->bedlightManualState = !state->bedlightShouldBeOn;
    }
}

// Automatic clearing when vehicle is locked
void clearManualOverrideIfNeeded(VehicleState* state, uint8_t vehicleLockStatus) {
    bool isUnlocked = (vehicleLockStatus == VEH_UNLOCK_ALL || vehicleLockStatus == VEH_UNLOCK_DRV);
    if (!isUnlocked && state->bedlightManualOverride) {
        state->bedlightManualOverride = false;
        state->bedlightManualState = false;
    }
}
```

## Button Input Processing

### Button Hardware Configuration
- **Pin**: GPIO_17 (TOOLBOX_BUTTON_PIN)
- **Type**: INPUT_PULLUP (active low)
- **Debouncing**: Software debouncing with 50ms threshold
- **Detection**: Supports both press/release events and hold detection

### Button Event Types

#### Hold Detection (Toolbox Activation)
- **Threshold**: 1000ms continuous press
- **Purpose**: Activate toolbox opener when vehicle conditions are met
- **Safety**: Only activates when vehicle is parked, unlocked, and system ready

#### Double-Click Detection (Bed Light Toggle)
- **Timing**: Two presses within 300ms window
- **Purpose**: Toggle bed light manual override mode
- **Security**: Only processes button input when vehicle is unlocked
- **Logic**: 
  - First double-click: Enter manual mode with opposite of current automatic state
  - Subsequent double-clicks: Toggle manual state
  - Automatic clearing: When CAN requests OFF/RAMP_DOWN
  - Ignored when locked: Double-clicks are ignored for security when vehicle is locked

#### Implementation Details
```c
// Double-click detection logic
if (buttonState.pressCount > 0 && 
    timeSinceLastPress <= BUTTON_DOUBLE_CLICK_MS && 
    timeSinceLastPress > BUTTON_DEBOUNCE_MS) {
    buttonState.doubleClickDetected = true;
}

// Security check for button input processing
if (shouldProcessButtonInput() && isButtonDoubleClicked()) {
    // Process bed light toggle only when vehicle is unlocked
    toggleBedlightManualOverride();
} else if (!shouldProcessButtonInput() && isButtonDoubleClicked()) {
    // Ignore button input when vehicle is locked
    LOG_WARN("Button double-click ignored - vehicle must be unlocked");
}

// Manual bed light control
if (vehicleState.bedlightManualOverride) {
    // Use manual state instead of CAN-derived state
    bedlightState = vehicleState.bedlightManualState;
} else {
    // Use automatic CAN-based control
    bedlightState = vehicleState.bedlightShouldBeOn;
}
```

### Timing Constants

The system uses several critical timing constants for reliable operation:

| Constant | Value | Purpose |
|----------|-------|---------|
| `BUTTON_DEBOUNCE_MS` | 50ms | Minimum stable time for button state changes |
| `BUTTON_DOUBLE_CLICK_MS` | 300ms | Maximum time between clicks for double-click detection |
| `BUTTON_HOLD_THRESHOLD_MS` | 1000ms | Minimum hold time for toolbox activation |
| `TOOLBOX_OPENER_DURATION_MS` | 500ms | Duration to keep toolbox opener relay active |
| `SYSTEM_READINESS_TIMEOUT_MS` | 600000ms (10 min) | Timeout for system readiness based on CAN activity |

**Double-Click Detection Logic**: A double-click is detected when two button presses occur within 300ms of each other, but with proper debouncing (must be > 50ms apart to avoid switch bounce). This timing ensures reliable detection while preventing interference with the 1000ms hold threshold for toolbox activation.

## System Health Monitoring

### Timeout Detection
- **Timeout Period**: 600000ms (10 minutes)
- **Monitored Messages**: All four CAN messages
- **Action**: System marked as "not ready" only if ALL messages time out
- **Recovery**: Automatic when ANY fresh message is received

### Error Handling
- **CAN Bus Errors**: Automatic reinitialization
- **GPIO Errors**: Automatic reinitialization  
- **Critical Errors**: Safe shutdown mode (all outputs disabled)
- **Watchdog**: System health monitoring every loop cycle

## Performance Characteristics

### Memory Usage
- **RAM**: 20,316 bytes (6.2% of 327,680 bytes available)
- **Flash**: 346,624 bytes (26.4% of 1,310,720 bytes available)
- **Stack**: Optimized for embedded operation

### Timing Performance
- **Loop Cycle**: Non-blocking operation
- **Message Processing**: Up to 10 messages per cycle
- **Button Debouncing**: 50ms debounce time
- **Toolbox Pulse**: 500ms with automatic shutoff
- **Status Updates**: Throttled to 100ms intervals

### CAN Bus Statistics
The system tracks comprehensive CAN statistics:
- Messages received per message type
- Total bytes processed
- Error counts and types
- Bus health status
- Queue utilization

## Testing and Validation

### Test Coverage
The application includes 49 comprehensive tests:

#### GPIO/Output Control Tests (22 tests)
- GPIO initialization and pin configuration
- Output control (bedlight, LEDs, toolbox opener)
- Button input handling and debouncing
- Timing control and rollover handling
- Integration scenarios

#### State Management Tests (19 tests)
- CAN message parsing and validation
- State transition logic
- Decision logic validation
- System health monitoring
- Timeout detection

#### Production Code Integration Tests (8 tests)
- Direct testing of actual production bit extraction
- CAN parsing function validation
- Decision logic function testing
- End-to-end scenario validation
- Change detection verification

### Test Execution
```bash
# Run all tests
pio test -e native

# Expected output:
# 49 test cases: 49 succeeded
```

## Development Notes

### Architecture Principles
- **Modular Design**: Clear separation of concerns
- **Error Resilience**: Comprehensive error handling and recovery
- **Resource Efficiency**: Minimal memory and CPU usage
- **Testability**: Pure logic functions for comprehensive testing

### Code Organization
```
src/
├── main.cpp              # Main application entry point
├── can_manager.cpp/h     # CAN bus interface and statistics
├── can_protocol.c/h      # Pure CAN message parsing logic
├── gpio_controller.cpp/h # GPIO control and button handling
├── message_parser.cpp/h  # Legacy message parsing (being phased out)
├── state_manager.cpp/h   # Vehicle state tracking and logic
└── config.h             # Pin definitions and constants
```

### Build Configuration
- **Platform**: ESP32-S3 (Espressif 32)
- **Framework**: Arduino
- **Build System**: PlatformIO
- **Compiler**: GCC with optimizations
- **Libraries**: ArduinoJson (minimal usage)

## Troubleshooting Guide

### CAN Bus Issues
1. **No Messages Received**
   - Check CAN H/L wiring
   - Verify 500kbps bus speed
   - Check termination resistors
   - Monitor serial output for CAN errors
   - **⚠️ CRITICAL**: Disable hardware filtering for debugging:
     ```cpp
     // In src/config.h, change:
     #define ENABLE_HARDWARE_CAN_FILTERING 0
     ```
   - Use `can_debug` serial command to monitor ALL CAN traffic
   - Re-enable hardware filtering once CAN reception is confirmed

2. **Target Messages Not Processed (but CAN activity detected)**
   - Temporarily disable hardware filtering to verify message IDs
   - Check that vehicle sends the expected message IDs (0x3C3, 0x331, 0x176, 0x43C)
   - Use `can_debug` to see all received messages and verify target IDs are present
   - Verify hardware filter configuration in initialization logs
   - Re-enable hardware filtering once message IDs are confirmed

3. **Invalid Message Data**
   - Verify message IDs match specification
   - Check bit positions in signal extraction
   - Validate message length (8 bytes)

3. **Timeout Errors**
   - Check for intermittent connections
   - Verify vehicle is generating expected messages
   - Check for bus overload conditions

### GPIO Issues
1. **Outputs Not Working**
   - Verify GPIO pin assignments
   - Check output current requirements
   - Test with multimeter/oscilloscope
   - Review serial debug output

2. **Button Not Responding**
   - Check pullup resistor (internal pullup enabled)
   - Verify button wiring (active low)
   - Check debouncing in serial output

### System Issues
1. **Frequent Resets**
   - Check power supply stability
   - Monitor for watchdog triggers
   - Review error recovery logs

2. **Memory Issues**
   - Monitor heap usage
   - Check for memory leaks
   - Verify stack usage

## Future Enhancements

### Potential Improvements
- Additional CAN message support
- Configurable timeout values
- Remote monitoring capabilities
- Enhanced error logging
- Additional safety features

### Hardware Considerations
- External CAN transceiver support
- Power management improvements
- Additional I/O expansion
- Wireless connectivity options

---

**Document Version**: 1.0.0  
**Last Updated**: August 3, 2025  
**Application Version**: 1.0.0
