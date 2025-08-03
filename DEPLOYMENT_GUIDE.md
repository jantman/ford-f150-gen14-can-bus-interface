# Deployment Guide - Ford F150 Gen14 CAN Bus Interface

## Pre-Deployment Checklist

### Software Requirements
- [ ] PlatformIO installed and configured
- [ ] ESP32-S3 toolchain installed
- [ ] Repository cloned and accessible
- [ ] All tests passing (49/49)
- [ ] Serial monitor access configured

### Hardware Requirements
- [ ] ESP32-S3 development board available
- [ ] CAN transceiver connected and tested
- [ ] All GPIO connections verified
- [ ] Power supply stable and tested
- [ ] Vehicle CAN bus access confirmed

### Documentation Review
- [ ] User Manual reviewed
- [ ] Technical Documentation understood
- [ ] Hardware Guide followed
- [ ] Safety considerations acknowledged

## Deployment Process

### Phase 1: Bench Testing

#### Step 1: Software Build and Upload
```bash
# Navigate to project directory
cd ford-f150-gen14-can-bus-interface

# Clean previous builds
pio run --target clean

# Build for ESP32-S3
pio run -e esp32-s3-devkitc-1

# Upload to device
pio run -e esp32-s3-devkitc-1 --target upload

# Monitor serial output
pio device monitor
```

**Expected Output:**
```
[INFO] Ford F150 CAN Interface Starting...
[INFO] Config: Bedlight=GPIO_2, Parked=GPIO_4, Unlocked=GPIO_5
[INFO] Config: Toolbox=GPIO_16, Button=GPIO_17, CAN TX/RX=21/22
[INFO] GPIO initialization completed successfully
[INFO] CAN bus initialized at 500kbps
[INFO] System ready - monitoring CAN messages
```

#### Step 2: Basic Function Test
1. **Power-On Test**: Verify clean startup with no errors
2. **GPIO Test**: Test LED outputs manually if possible
3. **Button Test**: Verify button input detection
4. **Memory Check**: Confirm RAM usage ≤ 6.2%, Flash ≤ 26.4%

#### Step 3: CAN Interface Test (if CAN simulator available)
1. **Message Reception**: Send test CAN messages
2. **Parsing Validation**: Verify correct signal extraction
3. **State Logic**: Test decision logic with known inputs

### Phase 2: Vehicle Integration

#### Step 4: Initial Vehicle Connection
1. **Safety First**: Disconnect vehicle battery
2. **Connect CAN Bus**: Make initial CAN H/L connections
3. **Connect Power**: Establish stable 3.3V power supply
4. **Reconnect Battery**: Restore vehicle power

#### Step 5: CAN Communication Verification
```bash
# Monitor for CAN messages
pio device monitor

# Expected CAN messages:
[INFO] CAN: Received BCM_Lamp_Stat_FD1 (0x3B3) - 8 bytes
[INFO] CAN: Received Locking_Systems_2_FD1 (0x3B8) - 8 bytes  
[INFO] CAN: Received PowertrainData_10 (0x204) - 8 bytes
[INFO] CAN: Received Battery_Mgmt_3_FD1 (0x3D2) - 8 bytes
```

**Troubleshooting No CAN Messages:**
- Verify CAN H/L connections
- Check for proper 120Ω termination
- Confirm vehicle is powered on
- Check ground connections

#### Step 6: Signal Validation
1. **Lock/Unlock Test**: Change vehicle lock state, verify LED response
2. **Park Test**: Shift to/from Park, verify LED response
3. **Puddle Lamp Test**: Activate puddle lamps, verify bedlight response

### Phase 3: Output Integration

#### Step 7: LED Installation and Testing
1. **Mount LEDs**: Install Parked (Green) and Unlocked (Blue) LEDs
2. **Test Operation**: Verify LEDs respond to vehicle state changes
3. **Adjust Brightness**: Modify resistor values if needed

#### Step 8: Bedlight Integration
1. **Connect Bedlight Control**: Wire GPIO_2 to bedlight relay/MOSFET
2. **Test Automatic Control**: Verify bedlight follows puddle lamp state
3. **Safety Check**: Ensure bedlight turns off when expected

#### Step 9: Toolbox Opener Integration
1. **Connect Toolbox Relay**: Wire GPIO_16 to toolbox opener relay
2. **Install Manual Button**: Wire button to GPIO_17
3. **Test Conditions**: Verify toolbox only operates when parked and unlocked
4. **Test Manual Override**: Verify button operation when conditions met

### Phase 4: System Validation

#### Step 10: Comprehensive Testing
1. **Normal Operation**: Test all functions in typical use scenarios
2. **Edge Cases**: Test timeout conditions, error recovery
3. **Extended Runtime**: Run system for several hours
4. **Performance Monitoring**: Check memory usage, error counts

#### Step 11: Documentation Update
1. **Record Configuration**: Document actual pin assignments and connections
2. **Note Customizations**: Record any modifications made during installation
3. **Create Maintenance Log**: Establish ongoing maintenance schedule

## Production Configuration

### Final Build Settings
```ini
# platformio.ini - Production Configuration
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
build_flags = 
    -DPRODUCTION_BUILD=1
    -DLOG_LEVEL=LOG_LEVEL_INFO
    -Wall
    -Wextra
```

### Performance Optimization
- **Disable Debug Logging**: Set LOG_LEVEL to reduce serial output
- **Enable Compiler Optimization**: Use -O2 or -Os for size optimization
- **Monitor Resource Usage**: Verify RAM/Flash usage within limits

### Security Considerations
- **Code Protection**: Consider enabling flash encryption if supported
- **OTA Updates**: Plan for secure over-the-air update mechanism
- **Access Control**: Limit physical access to device and serial interface

## Troubleshooting Guide

### Common Issues and Solutions

#### Issue: Device Won't Start
**Symptoms**: No serial output, LEDs don't light
**Diagnosis**:
```bash
# Check device enumeration
pio device list

# Try manual upload
pio run -e esp32-s3-devkitc-1 --target upload --verbose
```
**Solutions**:
- Check power supply (stable 3.3V)
- Verify USB connection
- Try different USB cable/port
- Hold BOOT button during upload

#### Issue: No CAN Messages Received
**Symptoms**: System starts but no CAN data in serial output
**Diagnosis**:
```
[INFO] System ready - monitoring CAN messages
[WARNING] No CAN messages received in 5000ms
[WARNING] System not ready due to stale data
```
**Solutions**:
- Verify CAN H/L connections with multimeter
- Check CAN voltage levels (H: ~3.5V, L: ~1.5V idle)
- Confirm vehicle is powered on and generating CAN traffic
- Add 120Ω termination resistors if needed
- Check ground connection between device and vehicle

#### Issue: Incorrect Vehicle State Detection
**Symptoms**: LEDs don't match actual vehicle state
**Diagnosis**:
```
[INFO] BCM Lamp: PudLamp=0 (OFF) -> Bedlight OFF
[INFO] Locking: Status=1 (LOCK_ALL) -> Unlocked LED OFF  
[INFO] Powertrain: Park=0 (UNKNOWN) -> Parked LED OFF
```
**Solutions**:
- Verify CAN message IDs match your vehicle year/model
- Check bit positions for signal extraction
- Validate signal value mappings
- Test with known vehicle states

#### Issue: Bedlight Not Responding
**Symptoms**: Bedlight doesn't follow puddle lamp state
**Diagnosis**:
```
[INFO] BCM Lamp: PudLamp=1 (ON) -> Bedlight ON
[DEBUG] GPIO_2 set HIGH for bedlight control
```
**Solutions**:
- Check bedlight relay/MOSFET wiring
- Verify GPIO_2 connection with multimeter
- Test relay operation manually
- Check bedlight power supply and fusing

#### Issue: Toolbox Won't Activate
**Symptoms**: Toolbox doesn't respond to button or automatic activation
**Diagnosis**:
```
[INFO] Toolbox conditions: parked=true, unlocked=true, ready=true
[INFO] Button pressed - toolbox activation requested
[WARNING] Toolbox activation failed - conditions not met
```
**Solutions**:
- Verify both parked AND unlocked conditions are met
- Check button wiring and pullup resistor
- Test toolbox relay independently
- Verify 500ms pulse timing requirements

#### Issue: System Resets or Crashes
**Symptoms**: Device restarts unexpectedly, watchdog triggers
**Diagnosis**:
```
[ERROR] Watchdog timeout - system restart
[ERROR] Critical error count exceeded: 10
[INFO] Entering safe shutdown mode
```
**Solutions**:
- Check power supply stability (no voltage drops)
- Monitor for EMI interference
- Verify adequate current capacity
- Check for memory leaks or stack overflow
- Review error logs for specific failure patterns

#### Issue: Poor Performance or High Resource Usage
**Symptoms**: Slow response, memory warnings
**Diagnosis**:
```
[WARNING] High memory usage: 85% RAM utilization
[WARNING] CAN queue full - dropping messages
```
**Solutions**:
- Monitor heap usage over time
- Check for memory leaks
- Reduce logging verbosity
- Optimize message processing rate
- Verify no infinite loops or blocking operations

### Advanced Diagnostics

#### CAN Bus Analysis
```bash
# Enable verbose CAN debugging
# Modify config.h: #define LOG_LEVEL LOG_LEVEL_DEBUG

# Expected detailed output:
[DEBUG] CAN: Frame ID=0x3B3, DLC=8, Data: 12 34 56 78 9A BC DE F0
[DEBUG] BCM: Extracted PudLamp=1 from bits 11-12
[DEBUG] BCM: Validation passed, updating state
```

#### Memory Monitoring
```cpp
// Add to main loop for memory monitoring
void printMemoryStatus() {
    Serial.printf("[DEBUG] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[DEBUG] Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("[DEBUG] Min free heap: %d bytes\n", ESP.getMinFreeHeap());
}
```

#### GPIO State Monitoring
```cpp
// Add GPIO state debug output
void printGPIOStatus() {
    Serial.printf("[DEBUG] GPIO States - Bedlight:%d, Parked:%d, Unlocked:%d, Toolbox:%d\n",
                  digitalRead(BEDLIGHT_PIN),
                  digitalRead(PARKED_LED_PIN), 
                  digitalRead(UNLOCKED_LED_PIN),
                  digitalRead(TOOLBOX_OPENER_PIN));
}
```

## Maintenance Procedures

### Regular Maintenance (Monthly)
1. **Visual Inspection**: Check all connections for corrosion or looseness
2. **Functional Test**: Verify all outputs respond correctly
3. **Performance Check**: Monitor serial output for errors or warnings
4. **Physical Security**: Ensure enclosure is secure and weatherproof

### Periodic Maintenance (Quarterly)
1. **Extended Test**: Run comprehensive test scenarios
2. **Memory Analysis**: Check for memory leaks or degradation
3. **Error Log Review**: Analyze accumulated error statistics
4. **Documentation Update**: Update maintenance logs

### Annual Maintenance
1. **Firmware Update**: Check for and apply firmware updates
2. **Component Inspection**: Detailed inspection of all hardware
3. **Performance Baseline**: Establish new performance benchmarks
4. **Backup Configuration**: Document current configuration settings

## Support and Updates

### Getting Help
1. **Documentation**: Review User Manual and Technical Documentation
2. **Serial Logs**: Collect detailed serial output when reporting issues
3. **Hardware Details**: Provide exact hardware configuration details
4. **Vehicle Information**: Include year, model, and trim level

### Reporting Issues
When reporting problems, include:
- Complete serial output log
- Hardware configuration details
- Vehicle year/model/trim
- Steps to reproduce the issue
- Environmental conditions

### Firmware Updates
1. **Check Repository**: Monitor for updates and bug fixes
2. **Test Updates**: Always test on bench before vehicle deployment
3. **Backup Configuration**: Save current working configuration
4. **Rollback Plan**: Keep previous working firmware available

---

**Document Version**: 1.0.0  
**Last Updated**: August 3, 2025  
**Next Review**: February 3, 2026
