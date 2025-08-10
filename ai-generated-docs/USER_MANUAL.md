# Ford F150 Gen14 CAN Bus Interface - User Manual

## Overview

This ESP32-S3 based device interfaces with your Ford F150 Gen14 CAN bus to provide automated bedlight control and toolbox opener functionality based on vehicle state.

## Features

- **Automated Bedlight Control**: Automatically turns on/off bedlight based on vehicle PUD lamp status
- **Smart Toolbox Opener**: Opens toolbox when vehicle is parked and unlocked (with manual button that requires 1000ms hold)
- **Visual Status Indicators**: LEDs show vehicle locked/unlocked and parked status
- **Safety Systems**: Built-in watchdog, error recovery, and timeout protections
- **CAN Bus Monitoring**: Real-time monitoring of Ford F150 CAN messages
- **Button Control**: Manual toolbox opener with debouncing and hold detection

## Hardware Requirements

### ESP32-S3 Development Board
- **Recommended**: AutoSport Labs ESP32-CAN-X2 (includes built-in CAN transceiver)  
- **Alternative**: Any ESP32-S3 board + external CAN transceiver (MCP2515 or SN65HVD230)
- **Minimum RAM**: 320KB (application uses only 6.2%)
- **Minimum Flash**: 1.3MB (application uses only 26.4%)

### Additional Components
- **CAN Transceiver**: If not using ESP32-CAN-X2 board
- **Connectors**: Appropriate connectors for your F150's CAN bus
- **Power Supply**: 12V to 3.3V converter (if not powered via USB)
- **Enclosure**: Weather-resistant if mounting externally

## Pin Configuration

The following GPIO pins are used by the application:

| Function | GPIO Pin | Type | Description |
|----------|----------|------|-------------|
| Bedlight Control | GPIO_2 | Output | Controls bedlight relay/MOSFET |
| Parked LED | GPIO_4 | Output | Green LED indicating vehicle is parked |
| Unlocked LED | GPIO_5 | Output | Blue LED indicating vehicle is unlocked |
| Toolbox Opener | GPIO_16 | Output | Controls toolbox opener relay (500ms pulse) |
| Toolbox Button | GPIO_17 | Input (Pullup) | Manual toolbox opener button |
| CAN TX | GPIO_21 | CAN | CAN bus transmit line |
| CAN RX | GPIO_22 | CAN | CAN bus receive line |

## Installation

### Step 1: Hardware Assembly

1. **Mount ESP32-S3 Board**: Secure the ESP32-S3 in a suitable enclosure
2. **Connect CAN Interface**: Wire CAN TX/RX to your F150's CAN bus
3. **Wire Outputs**: Connect bedlight, LEDs, and toolbox opener to appropriate GPIO pins
4. **Add Button**: Install manual toolbox button with pullup resistor
5. **Power Connection**: Provide stable 3.3V power supply

### Step 2: Software Installation

1. **Install PlatformIO**: 
   ```bash
   pip install platformio
   ```

2. **Clone Repository**:
   ```bash
   git clone <repository-url>
   cd ford-f150-gen14-can-bus-interface
   ```

3. **Build and Upload**:
   ```bash
   pio run -e esp32-s3-devkitc-1 --target upload
   ```

4. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

### Step 3: CAN Bus Connection

⚠️ **WARNING**: Connecting to your vehicle's CAN bus can potentially affect vehicle operation. Proceed with caution and at your own risk.

1. **Locate CAN Bus**: Find High-Speed CAN (HS-CAN) wires in your F150
2. **Tap Connection**: Use appropriate splicing method (DO NOT cut wires)
3. **Add Termination**: Ensure proper 120Ω termination if required
4. **Test Connection**: Verify CAN messages are received before final installation

## Operation

### Normal Operation

Once installed and powered on, the device will:

1. **Initialize**: All systems start up, LEDs may briefly flash
2. **CAN Bus Connect**: Device connects to F150 CAN bus and begins monitoring
3. **Status Display**: LEDs show current vehicle state
4. **Automatic Control**: Bedlight responds to vehicle PUD lamp status
5. **Toolbox Control**: Available when vehicle is parked and unlocked

### LED Status Indicators

| LED | State | Meaning |
|-----|-------|---------|
| Parked LED (Green) | ON | Vehicle transmission is in Park |
| Parked LED (Green) | OFF | Vehicle not in Park or status unknown |
| Unlocked LED (Blue) | ON | Vehicle is unlocked (all doors or driver door) |
| Unlocked LED (Blue) | OFF | Vehicle is locked or status unknown |

**Note**: When the vehicle is not running and the system has not received transmission status data, the Parked LED will be ON by default, as the system assumes the vehicle is parked in this scenario.

### Bedlight Control

- **Automatic ON**: When F150 PUD lamps are ON or RAMPING UP
- **Automatic OFF**: When F150 PUD lamps are OFF or RAMPING DOWN
- **Manual Override**: Double-click toolbox button to toggle bed lights manually

#### Manual Bed Light Override

The toolbox button can be used to manually control the bed lights independently of the vehicle's PUD lamp system:

1. **Security Requirement**: Vehicle must be unlocked for button input to be processed
2. **Double-Click to Toggle**: Press and release the toolbox button twice within 300ms
3. **First Double-Click**: Enters manual override mode with opposite state of current automatic setting
4. **Subsequent Double-Clicks**: Toggle bed lights ON/OFF while in manual mode
5. **Automatic Clearing**: Manual override is automatically cleared when vehicle requests lights OFF or RAMP_DOWN

**Examples**:
- If automatic bed lights are currently OFF, double-click turns them ON in manual mode
- If automatic bed lights are currently ON, double-click turns them OFF in manual mode
- While in manual mode, double-click toggles between manual ON and manual OFF
- When vehicle turns PUD lamps to OFF/RAMP_DOWN, manual override is cleared and returns to automatic mode

**Security Note**: Double-clicks are ignored when the vehicle is locked for security reasons. The Unlocked LED (Blue) must be ON for button input to be processed.

**Note**: Double-clicks are only detected on quick press/release cycles. Holding the button for 1000ms will activate the toolbox opener instead.

### Toolbox Opener

- **Automatic Enable**: When vehicle is parked AND unlocked
- **Manual Button**: Hold button for 1000ms when conditions are met
- **Safety Lockout**: Disabled when vehicle is moving or locked
- **Pulse Duration**: 500ms activation pulse to opener relay

#### Proper Button Usage

1. **Check Vehicle State**: Ensure truck is parked and unlocked (LEDs will indicate)
2. **Press and Hold**: Press the toolbox button and hold it down
3. **Wait for Threshold**: Keep holding for at least 1000ms (1 second)
4. **Toolbox Activation**: Device will activate toolbox opener for 500ms
5. **Release Button**: You can release the button after toolbox starts opening

**Important**: Brief button presses (less than 1000ms) are ignored for safety. The button must be held for the full threshold time to activate the toolbox opener.

## Troubleshooting

### No CAN Communication
- **Check Connections**: Verify CAN H/L wires are correct
- **Check Termination**: Ensure proper 120Ω termination  
- **Check Power**: Verify stable 3.3V supply to ESP32-S3
- **Check Serial**: Monitor serial output for CAN error messages
- **⚠️ IMPORTANT**: If no CAN messages are received, disable hardware filtering for debugging:
  ```cpp
  // In src/config.h, change:
  #define ENABLE_HARDWARE_CAN_FILTERING 0
  ```
  Then recompile and flash. This allows reception of ALL CAN messages for troubleshooting.
- **Debug Commands**: Use `can_debug` command via serial to monitor all CAN traffic
- **Re-enable Filtering**: Once CAN reception is confirmed working, re-enable hardware filtering

### CAN Messages Received But Not Processed
- **Hardware Filtering**: If you see CAN activity but target messages aren't processed:
  1. Temporarily disable hardware filtering: `#define ENABLE_HARDWARE_CAN_FILTERING 0`
  2. Use `can_debug` serial command to verify target message IDs are present
  3. Check that target message IDs match your vehicle's actual IDs
  4. Re-enable hardware filtering once confirmed: `#define ENABLE_HARDWARE_CAN_FILTERING 1`

### LEDs Not Working
- **Check Wiring**: Verify GPIO connections to LEDs
- **Check Power**: Ensure adequate current for LED operation
- **Check Serial**: Look for GPIO initialization messages

### Bedlight Not Responding
- **Check Relay**: Verify bedlight relay/MOSFET operation
- **Check CAN Data**: Monitor serial for BCM lamp messages
- **Check Wiring**: Verify GPIO_2 connection to bedlight control

### Toolbox Not Opening
- **Check Vehicle State**: Both parked AND unlocked required
- **Check Button**: Verify button wiring and pullup resistor
- **Check Relay**: Test toolbox opener relay operation
- **Check Serial**: Look for toolbox activation messages

### System Errors
- **Watchdog Resets**: May indicate CAN bus issues or power problems
- **Error Recovery**: System will attempt automatic recovery
- **Serial Monitoring**: Check serial output for error codes and recovery attempts

## Serial Monitor Output

The device provides detailed serial output for debugging:

```
[INFO] Ford F150 CAN Interface Starting...
[INFO] GPIO initialization completed
[INFO] CAN bus initialized at 500kbps
[INFO] System ready - monitoring CAN messages
[INFO] BCM Lamp: PudLamp=1 (ON) -> Bedlight ON
[INFO] Locking: Status=2 (UNLOCK_ALL) -> Unlocked LED ON
[INFO] Powertrain: Park=1 (PARK) -> Parked LED ON
[INFO] Toolbox conditions met - ready for activation
```

## Technical Specifications

- **CAN Bus Speed**: 500 kbps
- **Message Processing**: Up to 10 messages per loop cycle
- **Timeout Detection**: 5-second timeout for stale CAN data
- **Button Debouncing**: 50ms debounce time, 1000ms hold detection
- **Toolbox Pulse**: 500ms activation pulse with automatic shutoff
- **Memory Usage**: 6.2% RAM (20,316 bytes), 26.4% Flash (346,624 bytes)
- **Error Recovery**: Automatic CAN and GPIO reinitialization on errors

## Safety Notes

⚠️ **Important Safety Information**:

- This device interfaces with your vehicle's electronic systems
- Improper installation could affect vehicle operation
- Always test thoroughly before permanent installation
- Consider professional installation for critical applications
- Author assumes no responsibility for vehicle damage or malfunction
- Use at your own risk

## Support and Development

### Testing
The application includes 49 comprehensive tests validating all functionality:
```bash
pio test -e native
```

### Development
- **Build System**: PlatformIO with ESP32-S3 support
- **Testing**: GoogleTest with Arduino mocking
- **Architecture**: Modular design with comprehensive error handling

### Contributing
1. Fork the repository
2. Create feature branch
3. Add tests for new functionality  
4. Ensure all tests pass
5. Submit pull request

## License

[Include your license information here]

## Version History

- **v1.0.0**: Initial release with full F150 Gen14 support
- Complete CAN bus interface
- Automated bedlight and toolbox control
- Comprehensive testing and validation
