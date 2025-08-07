# Ford F150 Gen14 CAN Bus Interface - System Readiness Documentation

## Overview

The Ford F150 Gen14 CAN Bus Interface system uses a simplified readiness detection mechanism to ensure safe and reliable operation. The system is considered "ready" when it has received ANY valid CAN message from the monitored vehicle systems within the configured timeout window.

## System Ready Requirements

### Message Types Monitored

The system monitors **4 CAN message types** for system readiness:

#### Monitored Messages (ANY 1 Must Be Fresh):
1. **BCM_Lamp_Stat_FD1** (ID: 0x3C3 / 963 decimal)
   - Contains puddle lamp status that controls the bedlight
   - Signal: `PudLamp_D_Rq` (bits 11-12)

2. **Locking_Systems_2_FD1** (ID: 0x331 / 817 decimal)
   - Contains vehicle lock status that controls the unlocked LED
   - Signal: `Veh_Lock_Status` (bits 34-35)

3. **PowertrainData_10** (ID: 0x176 / 374 decimal)
   - Contains transmission park status that controls the parked LED
   - Signal: `TrnPrkSys_D_Actl` (bits 31-34)

4. **Battery_Mgmt_3_FD1** (ID: 0x43C / 1084 decimal)
   - Contains battery state of charge for monitoring
   - Signal: `BSBattSOC` (bits 22-28)

**Note**: All monitored messages are now treated equally for system readiness determination.

### Timeout Configuration

- **SYSTEM_READINESS_TIMEOUT_MS**: `600000` milliseconds (10 minutes)
- The system is ready if ANY monitored message has been received within the last 10 minutes
- The timeout is checked against the last update timestamp for each message type

### System Ready Logic

```cpp
bool hasBCMData = (currentTime - vehicleState.lastBCMLampUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
bool hasLockingData = (currentTime - vehicleState.lastLockingSystemsUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
bool hasPowertrainData = (currentTime - vehicleState.lastPowertrainUpdate) < SYSTEM_READINESS_TIMEOUT_MS;
bool hasBatteryData = (currentTime - vehicleState.lastBatteryUpdate) < SYSTEM_READINESS_TIMEOUT_MS;

// System is ready if we have recent data from ANY of the monitored systems
vehicleState.systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
```

## Log Messages Related to System Readiness

### System Readiness Changes

When the system readiness state changes, an INFO level log message is generated:

```
[INFO] System readiness changed: READY (BCM:OK, Lock:TIMEOUT, PT:TIMEOUT, Batt:TIMEOUT)
[INFO] System readiness changed: NOT_READY (BCM:TIMEOUT, Lock:TIMEOUT, PT:TIMEOUT, Batt:TIMEOUT)
```

The message shows the status of each monitored system:
- `OK`: Message received within timeout window
- `TIMEOUT`: Message not received within timeout window

**Note**: The system is ready as long as ANY message shows `OK` status.

### Timeout Warnings

When ALL message types have timed out (system not ready), warning messages are logged every 30 seconds:

```
[WARN] BCM lamp data timeout (last update 650000 ms ago)
[WARN] Locking systems data timeout (last update 620000 ms ago)
[WARN] Powertrain data timeout (last update 680000 ms ago)
[WARN] Battery data timeout (last update 615000 ms ago)
```

**Note**: Individual timeout warnings are only logged when the entire system is not ready (all messages timed out).

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

**Note**: With the new 10-minute timeout, watchdog alerts for system readiness are much less likely to occur during normal operation.

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

1. **Complete CAN Bus Disconnection**
   - **Symptom**: All monitored messages stop arriving
   - **Cause**: Physical disconnection from vehicle CAN bus
   - **Detection**: No target messages visible in `can_debug` for over 10 minutes

2. **CAN Bus Connection Issues**
   - Check physical CAN bus connections
   - Verify MCP2515 controller is functioning
   - Check for CAN bus errors in statistics

3. **Missing Target Messages**
   - Vehicle may not be transmitting expected messages
   - CAN message IDs may be different for your vehicle variant
   - Use `can_debug` command to see what messages are being received

4. **Extended Vehicle Inactivity**
   - Vehicle has been inactive for more than 10 minutes
   - All monitored systems have gone to sleep
   - Normal behavior when vehicle is parked and locked for extended periods

### Debug Steps

1. **Check CAN Statistics**: Use `can_status` command
2. **Monitor Message Reception**: Use `can_debug` command
3. **Check System Uptime**: Look for extended periods without ANY monitored messages
4. **Review Error Logs**: Look for parsing errors or CAN errors
5. **Verify Message IDs**: Confirm your vehicle sends the expected message IDs
6. **Check Vehicle Activity**: Ensure vehicle has been active within the last 10 minutes

### Expected Log Sequence for Working System

When a target message is received and processed, you should see:

```
[DEBUG] Target CAN message: ID=0x331, Len=8
[DEBUG] Parsed Locking_Systems_2_FD1: VehLockStatus=2
[DEBUG] Lock Status updated: VehLock=2
[INFO] Vehicle lock state changed: LOCK_ALL -> UNLOCK_ALL (unlocked: YES)
[INFO] Unlocked LED ON (vehicle lock status: 2)
```

With the new system readiness logic, the system will remain ready as long as ANY of the monitored messages continue to arrive within the 10-minute window.
