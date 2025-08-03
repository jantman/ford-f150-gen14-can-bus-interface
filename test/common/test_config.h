#pragma once

// Import the mock Arduino layer for native tests
#ifdef UNIT_TESTING
#include "../mocks/mock_arduino.h"
#endif

// Test-specific config that mirrors src/config.h but works in native environment
#ifdef NATIVE_ENV

// Version information
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// CAN Message IDs (from minimal.dbc)
#define BCM_LAMP_STAT_FD1_ID 0x3C3      // 963 decimal
#define LOCKING_SYSTEMS_2_FD1_ID 0x331  // 817 decimal
#define POWERTRAIN_DATA_10_ID 0x176     // 374 decimal
#define BATTERY_MGMT_3_FD1_ID 0x43C     // 1084 decimal

// Timing Configuration
#define TOOLBOX_OPENER_DURATION_MS 500  // Duration to keep toolbox opener active
#define BUTTON_DEBOUNCE_MS 50          // Button debounce time
#define BUTTON_HOLD_THRESHOLD_MS 1000  // Time to consider button "held"
#define BUTTON_DOUBLE_CLICK_MS 300     // Max time between clicks for double-click
#define CAN_TIMEOUT_MS 5000            // CAN message timeout (consider signal stale)
#define STATE_TIMEOUT_MS 10000         // State data timeout (consider system not ready)
#define SERIAL_BAUD_RATE 115200        // Serial monitor baud rate

// Signal Value Definitions (from minimal.dbc)
// PudLamp_D_Rq values
#define PUDLAMP_OFF 0
#define PUDLAMP_ON 1
#define PUDLAMP_RAMP_UP 2
#define PUDLAMP_RAMP_DOWN 3

// Veh_Lock_Status values
#define VEH_LOCK_DBL 0
#define VEH_LOCK_ALL 1
#define VEH_UNLOCK_ALL 2
#define VEH_UNLOCK_DRV 3
#define VEH_LOCK_UNKNOWN 255

// TrnPrkSys_D_Actl values
#define TRNPRKSTS_UNKNOWN 0
#define TRNPRKSTS_PARK 1
#define TRNPRKSTS_TRANSITION_CLOSE_TO_PARK 2
#define TRNPRKSTS_AT_NO_SPRING 3
#define TRNPRKSTS_TRANSITION_CLOSE_TO_OUT_OF_PARK 4
#define TRNPRKSTS_OUT_OF_PARK 5

// Debug Configuration
#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN 2
#define DEBUG_LEVEL_INFO 3
#define DEBUG_LEVEL_DEBUG 4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
#endif

// Logging macros for testing (captured by mock)
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
#define LOG_ERROR(format, ...) Serial.printf("[ERROR] " format "\n", ##__VA_ARGS__)
#else
#define LOG_ERROR(format, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
#define LOG_WARN(format, ...) Serial.printf("[WARN] " format "\n", ##__VA_ARGS__)
#else
#define LOG_WARN(format, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
#define LOG_INFO(format, ...) Serial.printf("[INFO] " format "\n", ##__VA_ARGS__)
#else
#define LOG_INFO(format, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
#define LOG_DEBUG(format, ...) Serial.printf("[DEBUG] " format "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

#else
// Include the original config for embedded builds
#include "../../src/config.h"
#endif
