# Hardware Connection Guide - Ford F150 Gen14 CAN Bus Interface

## Overview

This guide provides detailed instructions for connecting the ESP32-S3 CAN bus interface to your Ford F150 Gen14 vehicle.

⚠️ **SAFETY WARNING**: Working with vehicle electrical systems can be dangerous and may void your warranty. Proceed at your own risk and consider professional installation.

## Required Components

### ESP32-S3 Development Board Options

#### Option 1: AutoSport Labs ESP32-CAN-X2 (Recommended)
- **Advantages**: Built-in CAN transceiver, ready to connect
- **Disadvantages**: Higher cost
- **CAN Pins**: GPIO_21 (TX), GPIO_22 (RX)
- **Purchase**: AutoSport Labs website

#### Option 2: Generic ESP32-S3 + External CAN Transceiver
- **Board**: Any ESP32-S3 development board
- **Transceiver**: SN65HVD230 or MCP2515 CAN transceiver
- **Advantages**: Lower cost, more flexible
- **Disadvantages**: Requires additional wiring

### Additional Components

| Component | Quantity | Description | Notes |
|-----------|----------|-------------|-------|
| ESP32-S3 Board | 1 | Development board with USB programming | See options above |
| CAN Transceiver | 1 | SN65HVD230 or similar | Only if not using ESP32-CAN-X2 |
| Breadboard/PCB | 1 | For prototyping connections | Optional for permanent install |
| Jumper Wires | Various | Male-to-male, male-to-female | As needed |
| Resistors | 2 | 120Ω termination resistors | Check if needed |
| LEDs | 2 | Green (parked) and Blue (unlocked) | 5mm standard LEDs |
| LED Resistors | 2 | 220Ω - 470Ω current limiting | Based on LED specifications |
| Relay/MOSFET | 1 | For bedlight control | Rated for bedlight current |
| Relay | 1 | For toolbox opener | 12V coil, appropriate contacts |
| Button | 1 | Momentary push button | For manual toolbox control |
| Pullup Resistor | 1 | 10kΩ | For button (internal pullup also available) |
| Connectors | Various | For vehicle connection | Weather-resistant recommended |
| Enclosure | 1 | Protective housing | Weather-resistant if external |
| Wire | Various | 18-22 AWG stranded | Automotive grade recommended |

## Wiring Diagrams

### ESP32-S3 Pin Assignments

```
ESP32-S3 Pin Assignments:
┌─────────────────────────────────────┐
│  GPIO_2  → Bedlight Control        │
│  GPIO_4  → Parked LED (Green)      │
│  GPIO_5  → Unlocked LED (Blue)     │
│  GPIO_16 → Toolbox Opener Relay    │
│  GPIO_17 → Toolbox Button (Input)  │
│  GPIO_21 → CAN TX                  │
│  GPIO_22 → CAN RX                  │
│  GND     → Common Ground           │
│  3.3V    → Power Supply            │
└─────────────────────────────────────┘
```

### CAN Bus Connection (Option 1: ESP32-CAN-X2)

```
ESP32-CAN-X2 Board:
┌─────────────────┐    ┌─────────────────┐
│   ESP32-CAN-X2  │    │   F150 CAN Bus  │
│                 │    │                 │
│  CAN_H ●────────┼────┼─● CAN_H         │
│  CAN_L ●────────┼────┼─● CAN_L         │
│  GND   ●────────┼────┼─● Ground        │
└─────────────────┘    └─────────────────┘
```

### CAN Bus Connection (Option 2: External Transceiver)

```
ESP32-S3 + SN65HVD230:
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│    ESP32-S3     │    │   SN65HVD230    │    │   F150 CAN Bus  │
│                 │    │                 │    │                 │
│  GPIO_21 ●──────┼────┼─● TX            │    │                 │
│  GPIO_22 ●──────┼────┼─● RX            │    │                 │
│  3.3V    ●──────┼────┼─● VCC           │    │                 │
│  GND     ●──────┼────┼─● GND           │    │                 │
│                 │    │  CAN_H ●────────┼────┼─● CAN_H         │
│                 │    │  CAN_L ●────────┼────┼─● CAN_L         │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Complete System Wiring

```
Complete System Diagram:
┌─────────────────────────────────────────────────────────────────┐
│                           ESP32-S3                             │
├─────────────────────────────────────────────────────────────────┤
│  GPIO_2  ●─── Relay/MOSFET ──● Bedlight Power                  │
│  GPIO_4  ●─── 330Ω Resistor ─● Green LED (Parked)             │
│  GPIO_5  ●─── 330Ω Resistor ─● Blue LED (Unlocked)            │
│  GPIO_16 ●─── Relay Coil ────● Toolbox Opener                 │
│  GPIO_17 ●─── Button ─── 10kΩ ── 3.3V (Pullup)               │
│  GPIO_21 ●─── CAN TX                                           │
│  GPIO_22 ●─── CAN RX                                           │
│  GND     ●─── Common Ground                                    │
│  3.3V    ●─── Power Supply                                     │
└─────────────────────────────────────────────────────────────────┘
```

## F150 CAN Bus Location

### Accessing the CAN Bus

#### Method 1: OBD-II Port (Easiest)
- **Location**: Under dashboard, driver's side
- **CAN_H**: Pin 6 (typically yellow/green wire)
- **CAN_L**: Pin 14 (typically yellow/brown wire)
- **Ground**: Pin 4 or 5
- **Advantages**: Easy access, standardized
- **Disadvantages**: May interfere with diagnostic tools

#### Method 2: BCM Connector (Advanced)
- **Location**: Body Control Module connector
- **Access**: Requires dashboard panel removal
- **Advantages**: Dedicated tap, no OBD interference
- **Disadvantages**: More complex installation

#### Method 3: Wire Harness Tap (Professional)
- **Location**: Suitable harness point
- **Method**: Non-invasive tap connectors
- **Advantages**: Clean installation, no cutting
- **Disadvantages**: Requires professional installation

### CAN Bus Wire Identification

**Typical Ford CAN Wire Colors:**
- **CAN_H (High)**: Yellow with Green stripe
- **CAN_L (Low)**: Yellow with Brown stripe
- **Ground**: Black

⚠️ **Verify wire colors with multimeter - colors may vary by model year and trim level**

## Installation Procedure

### Step 1: Preparation
1. **Disconnect Battery**: Remove negative terminal for safety
2. **Gather Tools**: Multimeter, wire strippers, crimping tools
3. **Plan Layout**: Determine mounting locations for components
4. **Test Components**: Verify all components before installation

### Step 2: ESP32-S3 Setup
1. **Flash Firmware**: Upload application using PlatformIO
2. **Test Basic Function**: Verify serial output and GPIO operation
3. **Prepare Enclosure**: Mount ESP32-S3 in protective enclosure

### Step 3: CAN Bus Connection
1. **Locate CAN Wires**: Use multimeter to verify CAN_H and CAN_L
2. **Test Voltages**: 
   - CAN_H: ~3.5V idle, 2.5-4.0V active
   - CAN_L: ~1.5V idle, 1.0-2.0V active
3. **Make Connection**: Use appropriate tap method
4. **Verify Signal**: Check for CAN activity with oscilloscope if available

### Step 4: Output Connections

#### Bedlight Control
```
Bedlight Relay Wiring:
12V+ ●──[Fuse]──● Relay 87
     │
     └──[Switch]──● Relay 30
                  
ESP32 GPIO_2 ●──● Relay 86
Ground       ●──● Relay 85

Bedlight+ ●────● Relay 87a
Bedlight- ●────● Ground
```

#### LED Indicators
```
LED Wiring (each LED):
ESP32 GPIO ●──[330Ω Resistor]──● LED Anode
Ground     ●────────────────────● LED Cathode
```

#### Toolbox Opener
```
Toolbox Relay Wiring:
12V+ ●──[Fuse]──● Relay 30
     
ESP32 GPIO_16 ●──● Relay 86
Ground        ●──● Relay 85

Toolbox+ ●────● Relay 87
Toolbox- ●────● Relay 87a
```

### Step 5: Button Installation
1. **Mount Button**: Install in convenient location
2. **Wire Connection**: Connect to GPIO_17 with pullup
3. **Test Operation**: Verify button press detection

### Step 6: Power Supply
1. **12V Source**: Tap into appropriate 12V circuit (accessory or ignition)
2. **Voltage Regulation**: Convert 12V to stable 3.3V
3. **Current Rating**: Ensure adequate current capacity (500mA minimum)
4. **Protection**: Add fuse protection on 12V input

### Step 7: Testing and Commissioning

#### Initial Power-Up
1. **Connect Serial Monitor**: Monitor startup messages
2. **Check Initialization**: Verify GPIO and CAN initialization
3. **Verify CAN Reception**: Look for incoming CAN messages

#### Functional Testing
1. **Vehicle State Changes**: Test with vehicle lock/unlock, park/drive
2. **LED Operation**: Verify parked and unlocked LEDs respond
3. **Bedlight Control**: Test automatic bedlight operation
4. **Toolbox Function**: Test manual and automatic toolbox operation

#### System Validation
1. **Extended Operation**: Run for several hours
2. **Error Monitoring**: Check for any error conditions
3. **Performance Check**: Verify memory usage and system health

## Troubleshooting

### No CAN Messages
- **Check Connections**: Verify CAN_H and CAN_L connections
- **Check Termination**: May need 120Ω termination resistors
- **Check Ground**: Ensure common ground with vehicle
- **Check Voltage**: Verify CAN voltage levels

### Intermittent Operation
- **Check Connections**: Look for loose connections
- **Check Power Supply**: Verify stable 3.3V supply
- **Check EMI**: Consider shielding for noisy environments

### GPIO Not Working
- **Check Wiring**: Verify correct GPIO pin connections
- **Check Load**: Ensure adequate drive capability
- **Check Logic Levels**: Verify 3.3V logic compatibility

## Safety Considerations

### Electrical Safety
- Always disconnect battery before working
- Use appropriate fuses on all power connections
- Ensure proper grounding
- Use automotive-grade components

### Vehicle Safety
- Do not interfere with safety-critical systems
- Test thoroughly before permanent installation
- Consider impact on vehicle warranty
- Document all modifications

### Installation Safety
- Use proper tools and techniques
- Secure all wiring to prevent chafing
- Protect connections from moisture
- Follow local electrical codes

## Maintenance

### Regular Checks
- **Visual Inspection**: Check for loose connections monthly
- **Functional Test**: Verify operation quarterly
- **Software Updates**: Check for firmware updates periodically

### Troubleshooting Tools
- **Serial Monitor**: Primary diagnostic tool
- **Multimeter**: For voltage and continuity testing
- **Oscilloscope**: For CAN signal analysis (advanced)

---

**Document Version**: 1.0.0  
**Last Updated**: August 3, 2025  
**Compatibility**: Ford F150 Gen14 (2021+)
