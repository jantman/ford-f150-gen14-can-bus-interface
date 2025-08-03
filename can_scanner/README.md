# CAN Scanner

A simple, standalone diagnostic tool to scan for ANY CAN bus activity on the ESP32-CAN-X2 board.

## Purpose

This tool helps diagnose CAN bus connectivity issues by:
- Dumping ALL CAN messages received (not filtered to specific IDs)
- Showing real-time statistics
- Displaying detailed driver status information
- Monitoring for CAN bus errors and alerts

## Usage

1. **Build and flash:**
   ```bash
   cd can_scanner
   pio run --target upload
   ```

2. **Monitor output:**
   ```bash
   pio device monitor
   ```

3. **Test with vehicle:**
   - Connect the ESP32 to your vehicle's CAN bus
   - Turn on ignition (engine doesn't need to be running)
   - Lock/unlock doors, turn on lights, etc.
   - Watch for ANY CAN activity in the output

## Expected Output

If the CAN connection is working, you should see:
```
[00:05.123] ID=0x123 DLC=8 DATA=[01 02 03 04 05 06 07 08]
[00:05.145] ID=0x456 DLC=4 DATA=[AA BB CC DD -- -- -- --]
```

If there are no messages but the system is healthy:
- TWAI State should be "1" (RUNNING)
- Error counters should be low
- No bus errors

## Troubleshooting

- **No messages + Bus errors**: Timing/wiring issue
- **No messages + No errors**: Wrong bus or vehicle not active
- **Messages but wrong IDs**: Different CAN bus than expected
- **Driver fails to start**: Hardware problem

## Hardware Configuration

- **TX Pin**: 7 (connected but not used in listen-only mode)
- **RX Pin**: 6
- **Baud Rate**: 500kbps
- **Mode**: Listen-only
- **Filter**: Accept all messages (0x000-0x7FF)
