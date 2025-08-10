# CAN Message Parsing Test Suite

This test suite validates our C++ embedded application's CAN message parsing logic against the working Python implementation in `can_embedded_logger.py`.

## Test Files Overview

### Core Message Processing Tests

1. **`test_can_bit_extraction.cpp`**
   - Validates bit extraction logic against Python's `extract_signal_value()` function
   - Tests all signal bit positions used in our target CAN messages
   - Ensures C++ and Python implementations produce identical results
   - Validates Intel (little-endian) byte order with DBC bit positioning

2. **`test_message_parser.cpp`** *(Note: May have compilation issues due to Arduino.h dependencies)*
   - Tests complete message parsing functions for all target message types
   - Validates parsed signal values against known test patterns
   - Tests error handling for invalid messages
   - Comprehensive end-to-end message parsing validation

3. **`test_vehicle_state_management.cpp`**
   - Tests vehicle state logic based on parsed CAN signals
   - Validates state change detection and decision making
   - Tests toolbox activation logic (parked AND unlocked AND system ready)
   - Simulates real-world driving scenarios (approaching vehicle, driving away)

4. **`test_can_message_recognition.cpp`**
   - Validates CAN message ID recognition and filtering
   - Tests `isTargetCANMessage()` function against Python's `CAN_FILTER_IDS`
   - Simulates CAN bus load with mixed target/non-target messages
   - Ensures message processing pipeline efficiency

### Target CAN Messages (from Python implementation)

Our tests validate processing of these specific Ford F-150 CAN messages:

| Message ID | Decimal | Name | Signals |
|------------|---------|------|---------|
| `0x3C3` | 963 | BCM_Lamp_Stat_FD1 | PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat |
| `0x331` | 817 | Locking_Systems_2_FD1 | Veh_Lock_Status |
| `0x176` | 374 | PowertrainData_10 | TrnPrkSys_D_Actl |
| `0x43C` | 1084 | Battery_Mgmt_3_FD1 | BSBattSOC |

### Signal Value Mappings (from Python)

The tests validate these signal interpretations:

- **PudLamp_D_Rq**: `{0: "OFF", 1: "ON", 2: "RAMP_UP", 3: "RAMP_DOWN"}`
- **Veh_Lock_Status**: `{0: "LOCK_DBL", 1: "LOCK_ALL", 2: "UNLOCK_ALL", 3: "UNLOCK_DRV"}`
- **TrnPrkSys_D_Actl**: `{0: "NotKnown", 1: "Park", 2-5: transitions, 6+: errors}`
- **BSBattSOC**: Raw percentage value (0-127)

## Key Test Scenarios

### Bit Extraction Validation
- Ensures C++ `extractBits()` matches Python `extract_signal_value()`
- Tests all bit positions: 12, 35, 34, 28, 50, 63
- Validates little-endian byte order handling
- Tests boundary conditions and edge cases

### Message Processing Pipeline
- Validates message ID filtering (4 target IDs out of 2048 possible)
- Tests complete parse-to-state-decision pipeline
- Simulates realistic CAN bus loads
- Ensures memory and CPU efficiency

### Vehicle State Logic
- Tests bedlight activation (PudLamp ON or RAMP_UP)
- Tests toolbox activation (parked AND unlocked AND system ready)
- Tests toolbox activation logic (requires unlocked and parked conditions)
- Validates 5-second message timeout handling

### Real-world Scenarios
- **Approaching Vehicle**: Puddle lamps activate → unlock → toolbox opens
- **Driving Away**: Out of park → toolbox closes → lights off
- **System Timeouts**: Stale CAN data → system not ready → no activation

## Running the Tests

```bash
# Build and run all tests
cd test
make test

# Run specific test suite
./test_can_bit_extraction
./test_vehicle_state_management  
./test_can_message_recognition
```

## Python Reference Implementation

The tests are based on the working Python implementation in:
- `experiments/message_watcher/can_embedded_logger.py`

This Python code successfully decodes the same CAN messages from a real Ford F-150, providing a proven reference for our C++ implementation validation.

## Test Philosophy

These tests follow the principle of **validating against working code**. Rather than testing against specifications or assumptions, we validate our C++ implementation produces identical results to the proven Python implementation. This ensures:

1. **Compatibility**: Our C++ app will recognize the same messages as the Python logger
2. **Correctness**: Bit extraction and signal interpretation match the working implementation  
3. **Completeness**: All target messages and signal values are properly handled
4. **Robustness**: Edge cases and error conditions are handled consistently

The goal is to ensure that when our C++ application reports "receiving CAN messages but not recognizing them as target messages," we can confidently debug the issue knowing our parsing logic is validated against working code.
