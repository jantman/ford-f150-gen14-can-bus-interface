# Ford F150 Gen14 CAN Bus Interface - System Readiness Documentation

## Overview

The Ford F150 Gen14 CAN Bus Interface system uses a sophisticated readiness detection mechanism to ensure safe and reliable operation. The system is considered "ready" only when it has current, reliable data from critical vehicle systems.

## System Ready Requirements

### Critical Message Types Required

The system requires **recent data from 3 out of 4 monitored CAN message types** to be considered ready:

#### Required Messages (All 3 Must Be Fresh):
1. **BCM_Lamp_Stat_FD1** (ID: 0x3C3 / 963 decimal)
   - Contains puddle lamp status that controls the bedlight
   - Signal: `PudLamp_D_Rq` (bits 11-12)

2. **Locking_Systems_2_FD1** (ID: 0x331 / 817 decimal)
   - Contains vehicle lock status that controls the unlocked LED
   - Signal: `Veh_Lock_Status` (bits 34-35)

3. **PowertrainData_10** (ID: 0x176 / 374 decimal)
   - Contains transmission park status that controls the parked LED
   - Signal: `TrnPrkSys_D_Actl` (bits 31-34)

#### Optional Message (Not Required for Readiness):
4. **Battery_Mgmt_3_FD1** (ID: 0x43C / 1084 decimal)
   - Contains battery state of charge for monitoring
   - Signal: `BSBattSOC` (bits 22-28)
   - **Note**: Battery data is monitored and logged but NOT required for system readiness

### Timeout Configuration

- **STATE_TIMEOUT_MS**: `10000` milliseconds (10 seconds)
- Each required message must have been received within the last 10 seconds
- The timeout is checked against the last update timestamp for each message type

### System Ready Logic

```cpp
bool hasBCMData = (currentTime - vehicleState.lastBCMLampUpdate) < STATE_TIMEOUT_MS;
bool hasLockingData = (currentTime - vehicleState.lastLockingSystemsUpdate) < STATE_TIMEOUT_MS;
bool hasPowertrainData = (currentTime - vehicleState.lastPowertrainUpdate) < STATE_TIMEOUT_MS;
bool hasBatteryData = (currentTime - vehicleState.lastBatteryUpdate) < STATE_TIMEOUT_MS;

// System is ready if we have recent data from the critical systems
vehicleState.systemReady = hasBCMData && hasLockingData && hasPowertrainData;
```

## Log Messages Related to System Readiness

### System Readiness Changes

When the system readiness state changes, an INFO level log message is generated:

```
[INFO] System readiness changed: READY (BCM:OK, Lock:OK, PT:OK, Batt:OK)
[INFO] System readiness changed: NOT_READY (BCM:TIMEOUT, Lock:OK, PT:OK, Batt:OK)
```

The message shows the status of each monitored system:
- `OK`: Message received within timeout window
- `TIMEOUT`: Message not received within timeout window

### Timeout Warnings

When individual message types timeout, warning messages are logged every 30 seconds:

```
[WARN] BCM lamp data timeout (last update 15000 ms ago)
[WARN] Locking systems data timeout (last update 12000 ms ago)
[WARN] Powertrain data timeout (last update 18000 ms ago)
```

## Effects of System Readiness

### When System is READY (`systemReady = true`)

1. **GPIO Outputs Enabled**: All outputs operate based on vehicle state
   - Bedlight controlled by PudLamp signal
   - Parked LED controlled by transmission park status
   - Unlocked LED controlled by vehicle lock status

2. **Toolbox Activation Possible**: Button can activate toolbox opener (if also parked and unlocked)

3. **Normal Operation**: All system functions are available

### When System is NOT READY (`systemReady = false`)

1. **All GPIO Outputs Disabled for Safety**:
   ```cpp
   // If system not ready, turn off all outputs for safety
   outputState.bedlightActive = false;
   outputState.parkedLEDActive = false;
   outputState.unlockedLEDActive = false;
   ```

2. **Toolbox Activation Disabled**: Even if vehicle is parked and unlocked, toolbox won't activate without system ready

3. **System Health Impact**: Extended periods without readiness trigger watchdog alerts

## System Health Monitoring

### Watchdog Monitoring

The system watchdog monitors readiness and triggers alerts:

- **60-second timeout**: If system not ready for more than 60 seconds:
  ```
  [ERROR] Watchdog: System not ready for 65000 ms
  ```

- **Recovery mode**: System enters recovery mode if readiness issues persist

### Health Status Logging

Regular health status is logged at DEBUG level when system is healthy:
```
[DEBUG] Watchdog: System healthy - CAN:0 Parse:0 Critical:0 Heap:45632
```

## Diagnostic Commands

### Check System Status

Use the `can_status` or `cs` command via serial interface:

```
[INFO] === VEHICLE STATE ===
[INFO] System Ready: YES
[INFO] Bed Light Should Be On: NO
[INFO] Is Parked: YES
[INFO] Is Unlocked: NO
```

### Debug CAN Reception

Use the `can_debug` or `cd` command to monitor CAN message reception for 10 seconds and help diagnose why the system might not be ready.

## Troubleshooting System Readiness Issues

### Common Causes of "Not Ready" Status

1. **Message Processing Pipeline Issue (Fixed in Latest Version)**
   - **Symptom**: Messages visible in `can_debug` but no processing logs
   - **Cause**: Redundant message consumption in main loop
   - **Fix**: Updated main.cpp to remove `processPendingCANMessages()` call
   - **Detection**: Run `can_debug` - if you see target messages but no parsing logs, this was the issue

2. **CAN Bus Connection Issues**
   - Check physical CAN bus connections
   - Verify MCP2515 controller is functioning
   - Check for CAN bus errors in statistics

3. **Missing Target Messages**
   - Vehicle may not be transmitting expected messages
   - CAN message IDs may be different for your vehicle variant
   - Use `can_debug` command to see what messages are being received

4. **Intermittent Reception**
   - Poor CAN bus signal quality
   - Electrical interference
   - Check error counters and CAN statistics

### Debug Steps

1. **Check CAN Statistics**: Use `can_status` command
2. **Monitor Message Reception**: Use `can_debug` command
3. **Compare Debug vs Normal Logs**: 
   - If `can_debug` shows target messages but normal operation doesn't process them, there's a pipeline issue
   - If `can_debug` shows no target messages, it's a CAN bus/vehicle issue
4. **Review Error Logs**: Look for parsing errors or CAN errors
5. **Verify Message IDs**: Confirm your vehicle sends the expected message IDs

### Expected Log Sequence for Working System

When a target message is received and processed, you should see:

```
[DEBUG] Target CAN message: ID=0x331, Len=8
[DEBUG] Parsed Locking_Systems_2_FD1: VehLockStatus=2
[DEBUG] Lock Status updated: VehLock=2
[INFO] Vehicle lock state changed: LOCK_ALL -> UNLOCK_ALL (unlocked: YES)
[INFO] Unlocked LED ON (vehicle lock status: 2)
```

If you only see the first line or none at all, there's a processing issue.
