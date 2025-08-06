# CAN Message Parsing Documentation

This document details the CAN message parsing implementation used in the Ford F-150 Gen 14 CAN Bus Interface project, specifically as implemented in the `can_embedded_logger.py` module.

## Overview

The embedded CAN logger monitors 4 specific CAN messages from the Ford F-150's CAN bus and extracts signal values using native Python bit manipulation without external dependencies like cantools. This approach is optimized for resource-constrained embedded Linux systems.

## Monitored CAN Messages

### 1. BCM_Lamp_Stat_FD1 (0x3C3 / 963)
**Body Control Module Lamp Status**

- **CAN ID**: `0x3C3` (963 decimal)
- **Purpose**: Controls and reports status of various vehicle lighting systems
- **Signals**:
  - `PudLamp_D_Rq` - Puddle lamp request status
  - `Illuminated_Entry_Stat` - Illuminated entry system status  
  - `Dr_Courtesy_Light_Stat` - Door courtesy light status

### 2. Locking_Systems_2_FD1 (0x331 / 817)
**Vehicle Locking System Status**

- **CAN ID**: `0x331` (817 decimal)
- **Purpose**: Reports vehicle door lock/unlock status
- **Signals**:
  - `Veh_Lock_Status` - Overall vehicle lock status

### 3. PowertrainData_10 (0x176 / 374)
**Powertrain Data**

- **CAN ID**: `0x176` (374 decimal)
- **Purpose**: Transmission and parking system data
- **Signals**:
  - `TrnPrkSys_D_Actl` - Transmission park system actual status

### 4. Battery_Mgmt_3_FD1 (0x43C / 1084)
**Battery Management System**

- **CAN ID**: `0x43C` (1084 decimal)
- **Purpose**: Battery state of charge monitoring
- **Signals**:
  - `BSBattSOC` - Battery state of charge percentage

## Signal Definition Structure

Each signal is defined with the following parameters:

```python
'signal_name': {
    'start_bit': int,    # Starting bit position (DBC format)
    'length': int,       # Number of bits for the signal
    'values': dict       # Value mapping (None for raw values)
}
```

## Bit Ordering and Byte Layout

### Byte Order Convention
The implementation uses **Intel (little-endian)** byte order as specified by the DBC format (@0+). This means:
- Least significant byte comes first
- Multi-byte values are stored with the LSB at the lowest memory address

### Bit Numbering Convention
The system uses **DBC (MSB) bit numbering**:
- Bit 0 is the LSB (least significant bit) of byte 0
- Bit 7 is the MSB (most significant bit) of byte 0
- Bit numbering continues sequentially across bytes
- For an 8-byte CAN frame: bits 0-63 (bit 63 is MSB of byte 7)

### Signal Extraction Algorithm

The `extract_signal_value()` method implements the following algorithm:

1. **Convert to 64-bit integer**: Convert the 8-byte CAN data payload to a 64-bit integer using little-endian byte order
2. **Calculate bit position**: Convert DBC bit position to extraction position using: `bit_pos = start_bit - length + 1`
3. **Create bit mask**: Generate a mask with the appropriate number of bits: `mask = (1 << length) - 1`
4. **Extract value**: Shift and mask to extract the signal: `value = (data_int >> bit_pos) & mask`

```python
def extract_signal_value(self, data, start_bit, length):
    # Convert 8 bytes to 64-bit integer (little-endian)
    data_int = int.from_bytes(data, byteorder='little')
    
    # Calculate bit position from MSB (DBC uses MSB bit numbering)
    bit_pos = start_bit - length + 1
    
    # Create mask and extract value
    mask = (1 << length) - 1
    value = (data_int >> bit_pos) & mask
    
    return value
```

## Detailed Signal Specifications

### BCM_Lamp_Stat_FD1 (0x3C3) Signals

#### PudLamp_D_Rq
- **Start Bit**: 11
- **Length**: 2 bits
- **Values**:
  - `0`: "OFF"
  - `1`: "ON" 
  - `2`: "RAMP_UP"
  - `3`: "RAMP_DOWN"

#### Illuminated_Entry_Stat
- **Start Bit**: 63
- **Length**: 2 bits
- **Values**:
  - `0`: "Off"
  - `1`: "On"
  - `2`: "Unknown"
  - `3`: "Invalid"

#### Dr_Courtesy_Light_Stat
- **Start Bit**: 49
- **Length**: 2 bits
- **Values**:
  - `0`: "Off"
  - `1`: "On"
  - `2`: "Unknown"
  - `3`: "Invalid"

### Locking_Systems_2_FD1 (0x331) Signals

#### Veh_Lock_Status
- **Start Bit**: 34
- **Length**: 2 bits
- **Values**:
  - `0`: "LOCK_DBL" (Double lock)
  - `1`: "LOCK_ALL" (All doors locked)
  - `2`: "UNLOCK_ALL" (All doors unlocked)
  - `3`: "UNLOCK_DRV" (Driver door unlocked)

### PowertrainData_10 (0x176) Signals

#### TrnPrkSys_D_Actl
- **Start Bit**: 31
- **Length**: 4 bits
- **Values**:
  - `0`: "NotKnown"
  - `1`: "Park"
  - `2`: "TransitionCloseToPark"
  - `3`: "AtNoSpring"
  - `4`: "TransitionCloseToOutOfPark"
  - `5`: "OutOfPark"
  - `6`: "Override"
  - `7`: "OutOfRangeLow"
  - `8`: "OutOfRangeHigh"
  - `9`: "FrequencyError"
  - `15`: "Faulty"

### Battery_Mgmt_3_FD1 (0x43C) Signals

#### BSBattSOC
- **Start Bit**: 22
- **Length**: 7 bits
- **Values**: None (raw percentage value 0-100)

## Message Filtering and Reception

### CAN Socket Configuration
The implementation uses raw SocketCAN with hardware-level filtering for efficiency:

```python
# Set CAN filters for monitored message IDs only
can_filter = b""
for can_id in CAN_FILTER_IDS:
    # Pack filter: can_id, can_mask (exact match)
    can_filter += struct.pack("=II", can_id, 0x7FF)

self.can_socket.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, can_filter)
```

### Frame Structure
Standard CAN frames received have the following structure:
- **CAN ID**: 4 bytes (masked to 11-bit standard ID)
- **DLC**: 1 byte (data length code)
- **Padding**: 3 bytes
- **Data**: 8 bytes payload

```python
# Receive CAN frame: can_id(4) + dlc(1) + pad(3) + data(8)
frame_data = self.can_socket.recv(16)
can_id, dlc = struct.unpack("=IB", frame_data[:5])
can_id &= 0x7FF  # Mask to standard 11-bit ID
data = frame_data[8:16]  # 8 bytes of data
```

## Message Decoding Process

1. **Filter Check**: Verify the CAN ID is in the monitored message list
2. **Signal Extraction**: For each defined signal in the message:
   - Extract raw bit value using `extract_signal_value()`
   - Apply value mapping if defined (enum values)
   - Store decoded value
3. **State Update**: Update internal signal state with thread safety
4. **Logging**: Output formatted signal values at specified intervals

## Performance Considerations

### Optimization Features
- **Pre-computed filters**: CAN hardware filtering reduces CPU load
- **Pre-allocated storage**: Signal value storage allocated at startup
- **Minimal allocations**: Avoids runtime memory allocation
- **Direct bit manipulation**: No external library dependencies
- **Threading**: Separate message reception and logging threads

### Resource Usage
- **Memory**: Minimal footprint with pre-allocated data structures
- **CPU**: Efficient bit operations and hardware-filtered reception
- **Dependencies**: Standard library only (no cantools/python-can)

## Error Handling

### Signal Value Handling
- **Unknown values**: When a raw value doesn't match defined mappings, format as "Unknown(raw_value)"
- **Missing signals**: Display "N/A" when signal hasn't been received
- **Invalid frames**: Skip frames with incorrect length or format

### Connection Management
- **Socket timeouts**: 1-second timeout prevents blocking
- **Interface errors**: Graceful handling of CAN interface issues
- **Thread safety**: Data locks prevent race conditions during updates

## Usage Example

```python
# Initialize logger for can0 interface with 1-second intervals
logger = EmbeddedCANLogger('can0', 1.0)

# Run the logger (blocks until Ctrl+C)
logger.run()
```

Output format:
```
2025-08-05 14:30:15.123 | BCM_Lamp_Stat_FD1.PudLamp_D_Rq=OFF | BCM_Lamp_Stat_FD1.Illuminated_Entry_Stat=Off | ...
```

This implementation provides a lightweight, efficient solution for monitoring specific Ford F-150 CAN bus signals without requiring external dependencies, making it suitable for embedded Linux deployments.
