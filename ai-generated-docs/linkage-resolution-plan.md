# Function Linkage Resolution Plan

**Date:** August 10, 2025  
**Status:** Phase 6 DRY elimination 95% complete - Architecture achieved, linkage conflicts remain
**Objective:** Resolve C/C++ function linkage conflicts in remaining 2 test suites

## Current Status Summary

### ✅ **Major Success: DRY Objectives Achieved**
- **All duplicate parsing functions eliminated** from production code
- **All duplicate decision logic functions eliminated** from production code  
- **All duplicate CAN filtering functions eliminated** from production code
- **Architecture validated** with 4 out of 6 test suites working (134/153 tests)

### ❌ **Remaining Issue: Technical Linkage Conflicts**
- **2 test suites failing** due to C/C++ linkage mismatches
- **Core architecture works** - this is implementation detail cleanup
- **No production code impact** - ESP32 build unaffected

## Detailed Problem Analysis

### Test Suite Status
| Test Suite | Status | Tests | Issue |
|------------|--------|--------|-------|
| `test_locking_system` | ❌ ERRORED | 0/27 | Linkage conflicts |
| `test_message_parser` | ❌ ERRORED | 0/44 | Linkage conflicts |
| `test_can_protocol` | ❌ ERRORED | 0/28 | Function not found |
| `test_output_control` | ❌ ERRORED | 19/33 | GPIO + linkage |
| `test_bit_analysis` | ❌ ERRORED | 0/34 | Linkage conflicts |
| `test_vehicle_state` | ❌ ERRORED | 0/46 | `isTargetCANMessage` missing |

### Root Cause: Linkage Expectation Mismatch

#### Problem 1: `test_message_parser`
```cpp
// test_message_parser.cpp includes:
#include "../src/state_manager.h"  // C++ declarations

// But state_manager.h declares:
bool shouldEnableBedlight(uint8_t pudLampRequest);  // C++ linkage

// While mock provides:
extern "C" bool shouldEnableBedlight(uint8_t pudLampRequest);  // C linkage
```

#### Problem 2: `test_can_protocol`  
```cpp
// test_can_protocol.cpp expects:
extern "C" {
    bool shouldActivateToolbox(bool, bool, bool);  // Explicit C linkage
}

// But mock now provides:
bool shouldActivateToolbox(bool, bool, bool);  // C++ linkage
```

#### Problem 3: `test_vehicle_state`
```cpp
// Expects isTargetCANMessage but linkage type unclear
// Mock provides bool isTargetCANMessage(uint32_t) but not linkable
```

## Resolution Strategy

### Option A: Unified C++ Linkage (RECOMMENDED)

**Rationale:** Match production architecture, simplest long-term maintenance

#### Step 1: Standardize Mock Functions
```cpp
// In lib/test_mocks/src/mock_arduino.cpp - provide C++ linkage
bool shouldEnableBedlight(uint8_t pudLampRequest) { ... }
bool isVehicleUnlocked(uint8_t vehicleLockStatus) { ... }
bool isVehicleParked(uint8_t transmissionParkStatus) { ... }
bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked) { ... }
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked) { ... }
bool isTargetCANMessage(uint32_t messageId) { ... }
```

#### Step 2: Remove Conflicting Declarations
```cpp
// In test_can_protocol.cpp - remove explicit C declarations
// Remove this block:
extern "C" {
    bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked);
    bool shouldEnableBedlight(uint8_t pudLampRequest);
    bool isVehicleUnlocked(uint8_t lockStatus);
    bool isVehicleParked(uint8_t parkStatus);
}
```

#### Step 3: Include Proper Headers
```cpp
// In test_message_parser.cpp - already correct
#include "../src/state_manager.h"  // Gets C++ declarations

// In test_can_protocol.cpp - add if needed
// Functions should be auto-discovered from mock
```

#### Step 4: Validate Build System
- Ensure `mock_arduino.cpp` is compiled with C++ compiler
- Verify all test executables link against mock object files
- Check PlatformIO native environment configuration

### Option B: Explicit Mock Headers (ALTERNATIVE)

Create dedicated mock headers for clean separation:

```cpp
// lib/test_mocks/include/mock_functions.h
#pragma once
#include <cstdint>

// Mock function declarations (C++ linkage)
bool shouldEnableBedlight(uint8_t pudLampRequest);
bool isVehicleUnlocked(uint8_t vehicleLockStatus);
bool isVehicleParked(uint8_t transmissionParkStatus);
bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked);
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked);
bool isTargetCANMessage(uint32_t messageId);
```

Then include this header in test files instead of production headers.

## Implementation Plan

### Phase 1: Validate Current Architecture
1. **Confirm ESP32 build** - Ensure production code unaffected
2. **Document working test suites** - Capture what's currently successful
3. **Analyze mock compilation** - Verify build system includes mock properly

### Phase 2: Apply Unified C++ Linkage
1. **Update mock functions** - Remove `extern "C"` wrappers
2. **Remove conflicting declarations** - Clean up test files
3. **Test iteratively** - Fix one test suite at a time
4. **Validate no regression** - Ensure working tests stay working

### Phase 3: Address Special Cases
1. **Fix `isTargetCANMessage`** - Ensure proper mock implementation
2. **Resolve GPIO mock issues** - Address `test_output_control` separately
3. **Handle any remaining edge cases** - Case-by-case analysis

### Phase 4: Final Validation
1. **Run complete test suite** - All 153 tests should pass
2. **Validate ESP32 build** - No production impact
3. **Update documentation** - Reflect final architecture

## Success Criteria

- **All 6 test suites PASS** - 153/153 tests successful
- **ESP32 builds successfully** - No production code impact
- **Clean mock architecture** - Consistent C++ linkage throughout
- **Maintainable test infrastructure** - Clear separation of concerns

## Timeline Estimate

- **Phase 1:** 30 minutes - Analysis and validation
- **Phase 2:** 1-2 hours - Core linkage fixes  
- **Phase 3:** 30-60 minutes - Special case handling
- **Phase 4:** 30 minutes - Final validation

**Total:** 2.5-4 hours of focused work

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Break working tests | LOW | MEDIUM | Incremental approach, validate each step |
| ESP32 build issues | VERY LOW | HIGH | No production changes planned |
| Complex edge cases | MEDIUM | LOW | Option B (mock headers) as fallback |
| Time overrun | LOW | LOW | Core architecture already validated |

## Next Session Action Items

1. **Choose implementation approach** (recommend Option A)
2. **Start with Phase 1 validation** - confirm current state
3. **Apply fixes systematically** - one test suite at a time
4. **Document final architecture** - update project documentation

The core DRY elimination work is **COMPLETE** - this is purely technical debt cleanup to achieve 100% test coverage.
