# System Readiness Logic Changes

## Summary

This document summarizes the changes made to the Ford F150 Gen14 CAN Bus Interface system readiness handling. The system has been updated from requiring all critical messages to be fresh within 10 seconds, to considering the system ready if ANY monitored message has been received within the last 10 minutes.

## Changes Made

### 1. Configuration Updates

**Files Modified:**
- `src/config.h`
- `test/common/test_config.h`

**Changes:**
- Added new constant: `SYSTEM_READINESS_TIMEOUT_MS 600000` (10 minutes)
- Marked old constant as deprecated: `STATE_TIMEOUT_MS 10000 // DEPRECATED`

### 2. Core Logic Updates

**File Modified:** `src/state_manager.cpp`

**Function:** `checkForStateChanges()`

**Old Logic:**
```cpp
// System is ready if we have recent data from at least the critical systems
vehicleState.systemReady = hasBCMData && hasLockingData && hasPowertrainData;
```

**New Logic:**
```cpp
// System is ready if we have recent data from ANY of the monitored systems
vehicleState.systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
```

**Key Changes:**
- Changed from AND logic (`&&`) to OR logic (`||`)
- Now includes Battery data in readiness determination
- Uses new 10-minute timeout instead of 10-second timeout
- Modified timeout warnings to only log when ALL systems are timed out

### 3. Test Updates

**Files Modified:**
- `test/test_vehicle_state_management.cpp`
- `test/test_main.cpp`

**Changes:**
- Updated `updateSystemReady()` function to use OR logic with new timeout
- Modified `SystemReadyLogic` test to properly test the new behavior
- Updated `ToolboxActivationLogic` test to use realistic timeout values
- Fixed `EdgeCasesMessageTimeouts` test to work with 10-minute timeout

### 4. Documentation Updates

**Files Modified:**
- `ai-generated-docs/system_readiness.md`
- `ai-generated-docs/TECHNICAL_DOCUMENTATION.md`

**Changes:**
- Updated system overview to reflect simplified readiness detection
- Changed from "3 out of 4 required" to "ANY 1 out of 4 required"
- Updated timeout configuration documentation
- Modified log message examples to reflect new behavior
- Updated troubleshooting guide for new logic

## Behavioral Changes

### Before (Old System)
- **Required**: 3 out of 4 messages fresh within 10 seconds
- **Battery message**: Monitored but not required for readiness
- **Timeout**: 10 seconds for each required message
- **Ready when**: BCM AND Locking AND Powertrain all fresh
- **Not ready when**: Any of the 3 critical messages timed out

### After (New System)
- **Required**: ANY 1 out of 4 messages fresh within 10 minutes
- **Battery message**: Now included in readiness determination
- **Timeout**: 10 minutes for any message
- **Ready when**: BCM OR Locking OR Powertrain OR Battery fresh
- **Not ready when**: ALL 4 messages have timed out

## Benefits

1. **More Robust**: System stays ready with just one active message type
2. **Extended Timeout**: 10-minute timeout accommodates vehicle sleep modes
3. **Battery Inclusion**: Battery messages now contribute to system readiness
4. **Less Chatty Logs**: Warnings only when completely disconnected
5. **Better User Experience**: System remains functional with partial CAN connectivity

## Impact on System Behavior

### GPIO Outputs
- **No change**: Still disabled when system not ready
- **Benefit**: Outputs remain active with partial CAN connectivity

### Toolbox Activation
- **No change**: Still requires system ready + parked + unlocked
- **Benefit**: Works with partial CAN connectivity

### Watchdog Monitoring
- **Reduced alerts**: 60-second watchdog timeout rarely triggered with 10-minute message timeout
- **Better stability**: System enters recovery mode less frequently

### Logging
- **Reduced noise**: Individual timeout warnings only when system completely not ready
- **Better diagnostics**: Clear indication when ANY vs ALL messages are available

## Compatibility

- **Backward compatible**: No breaking changes to external interfaces
- **Configuration**: Old `STATE_TIMEOUT_MS` marked deprecated but not removed
- **Test coverage**: All existing functionality tested with new logic

## Validation

The changes have been validated through:

1. **Unit tests**: Updated existing tests to work with new logic
2. **Integration tests**: Verified GPIO and toolbox behavior unchanged
3. **Timeout tests**: Confirmed new 10-minute timeout behavior
4. **Edge cases**: Tested with single message types active

## Future Considerations

- Consider removing deprecated `STATE_TIMEOUT_MS` constant in future version
- Monitor real-world performance to validate 10-minute timeout is appropriate
- May want to add configuration option for timeout duration
- Could add metrics for message availability patterns
