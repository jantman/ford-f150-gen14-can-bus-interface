# Implementation Plan: Ford F150 Gen14 CAN Bus Interface

## Project Overview

We are implementing a Ford F150 Gen14 CAN bus interface application using Arduino Framework with PlatformIO on the AutoSport Labs ESP32-CAN-X2 development board. The application will monitor CAN bus messages and control various outputs based on vehicle state.

## Hardware Requirements

- **MCU**: ESP32-S3-WROOM-1-N8R8
- **CAN Controller**: Built-in TWAI (Two-Wire Automotive Interface) controller
- **GPIO Assignments**:
  - `BEDLIGHT_PIN` - GPIO5
  - `TOOLBOX_OPENER_PIN` - GPIO4
  - `TOOLBOX_BUTTON_PIN` - GPIO17 (input with pullup)
  - `SYSTEM_READY_PIN` - GPIO18

- **Available for Future Use** (previously LED indicators removed):
  - GPIO15 (was `UNLOCKED_LED_PIN`)
  - GPIO16 (was `PARKED_LED_PIN`)

## CAN Messages to Monitor

Based on the minimal.dbc file, we need to monitor:

1. **BCM_Lamp_Stat_FD1** (ID: 963, 8 bytes)
   - `PudLamp_D_Rq` - Bits 11-12, values: OFF(0), ON(1), RAMP_UP(2), RAMP_DOWN(3)

2. **Locking_Systems_2_FD1** (ID: 817, 8 bytes)
   - `Veh_Lock_Status` - Bits 34-35, values: LOCK_DBL(0), LOCK_ALL(1), UNLOCK_ALL(2), UNLOCK_DRV(3)

3. **PowertrainData_10** (ID: 374, 8 bytes)
   - `TrnPrkSys_D_Actl` - Bits 31-34, values: NotKnown(0), Park(1), etc.

4. **Battery_Mgmt_3_FD1** (ID: 1084, 8 bytes)
   - `BSBattSOC` - Bits 22-28, values: 0-127%

## Implementation Steps

### Step 1: Project Setup
- Create PlatformIO project structure
- Configure platformio.ini for ESP32-S3
- Set up required libraries (ESP32CAN, logging)
- Create basic project structure with modular design

### Step 2: GPIO Configuration
- Define GPIO pin constants
- Initialize output pins (LEDs, relays)
- Configure input pin with pullup and debouncing
- Create GPIO management functions

### Step 3: CAN Bus Setup
- Initialize TWAI/CAN controller
- Configure CAN bus parameters (500kbps, standard format)
- Implement CAN message reception
- Create message filtering for required IDs

### Step 4: Message Parsing
- Implement DBC-style message parsing functions
- Create structures for each monitored message
- Extract signal values from CAN frames
- Handle bit manipulation and value decoding

### Step 5: State Management
- Create variables to track current state of all signals
- Implement state change detection
- Add logging for all state changes
- Create getter functions for current states

### Step 6: Button Debouncing
- Implement software debouncing for toolbox button
- Create non-blocking button state detection
- Add logging for button press events

### Step 7: Output Control Logic
- Implement bed light control based on PudLamp_D_Rq
- Implement toolbox opener logic with timing and conditions
- Implement system ready indicator control

### Step 8: Main Loop Integration
- Integrate all components into main loop
- Ensure non-blocking operation
- Add error handling and recovery
- Implement watchdog functionality

### Step 9: Testing and Validation
- **Set up PlatformIO Native Testing** - Configure native test environment with GoogleTest
- **Create comprehensive host-based tests** - Test all critical logic on development machine
- **Extract testable logic** - Separate pure logic from hardware dependencies
- **Message Parsing Tests** - Comprehensive bit extraction and CAN parsing validation
- **State Management Tests** - Vehicle state transitions and condition logic  
- **Output Control Tests** - Output decision logic separated from hardware calls
- **Error Recovery Tests** - Watchdog and recovery decision logic
- **Hardware-in-Loop Testing** - Test on actual ESP32-S3 hardware
- **Integration Testing** - Test with real CAN bus interface

### Step 10: Documentation and Deployment
- Update README.md with build and flash instructions
- Add troubleshooting guide
- Create configuration constants for easy customization
- Final code review and optimization

## Key Design Principles

1. **Modular Design**: Separate concerns into distinct modules (CAN handling, GPIO control, message parsing)
2. **Non-blocking Operation**: Use state machines and timing functions instead of delays
3. **Robust Error Handling**: Handle CAN bus errors, message parsing failures, etc.
4. **Extensive Logging**: Log all state changes and significant events for debugging
5. **Configurable Constants**: Make timing, pins, and other parameters easily configurable
6. **Memory Efficiency**: Optimize for ESP32 memory constraints
7. **Testable Architecture**: Separate pure logic from hardware dependencies for comprehensive host-based testing
8. **Future-Proof Design**: Structure code to easily accommodate new CAN messages, outputs, and hardware changes

## Libraries Required

- **ESP32CAN**: For TWAI/CAN bus communication
- **Arduino Framework**: Core framework
- **FreeRTOS**: For timing and task management (built-in)

## File Structure

```
src/
├── main.cpp              # Main application entry point
├── config.h              # Pin definitions and constants
├── can_manager.h/cpp     # CAN bus initialization and message handling
├── message_parser.h/cpp  # DBC message parsing functions
├── gpio_controller.h/cpp # GPIO initialization and control
├── state_manager.h/cpp   # Vehicle state tracking and change detection
└── logger.h/cpp          # Logging utilities

test/
├── test_main.cpp         # Comprehensive native test suite with GoogleTest
├── common/               # Shared test utilities
│   ├── test_config.h     # Test configuration constants
│   └── test_helpers.h    # Helper functions for tests
└── mocks/
    ├── mock_arduino.h/cpp    # Arduino function mocks
    └── mock_arduino.h        # Arduino hardware abstraction
```
```

## Success Criteria

- [ ] CAN bus messages are correctly received and parsed
- [ ] All GPIO outputs respond correctly to vehicle state changes
- [ ] Toolbox button works only under correct conditions (unlocked + parked)
- [ ] All state changes are properly logged
- [ ] System operates reliably without crashes or hangs
- [ ] Code is well-documented and maintainable

This implementation plan provides a structured approach to building the Ford F150 CAN bus interface while maintaining code quality and reliability standards suitable for automotive applications.
