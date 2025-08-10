#ifndef CONFIG_H
#define CONFIG_H

// Version information
// FIRMWARE_VERSION is automatically generated at build time from git commit hash
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// GPIO Pin Definitions
#define BEDLIGHT_PIN 5          // Output: Controls bed light relay
#define TOOLBOX_OPENER_PIN 4    // Output: Controls toolbox opener relay
#define TOOLBOX_BUTTON_PIN 17   // Input: Toolbox unlock button (with pullup)
#define SYSTEM_READY_PIN 18     // Output: System ready indicator (relay VCC)

// CAN Bus Configuration (Listen-Only Mode)
// Using MCP2515 controller on X2 header (CAN2) - ESP32-CAN-X2 board
// Pin assignments for MCP2515 SPI interface
#define CAN_CS_PIN 10           // SPI Chip Select for MCP2515
#define CAN_CLK_PIN 12          // SPI Clock 
#define CAN_MISO_PIN 13         // SPI MISO
#define CAN_MOSI_PIN 11         // SPI MOSI
#define CAN_IRQ_PIN 3           // MCP2515 Interrupt pin
#define CAN_BAUDRATE 500000     // 500kbps - standard automotive rate

// CAN Message IDs (from minimal.dbc and validated with Python can_embedded_logger.py)
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
#define SYSTEM_READINESS_TIMEOUT_MS 600000  // System ready if ANY monitored message received within 10 minutes
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
// Additional values 6-15 exist but we only care about Park(1)

// Debug Configuration
#define DEBUG_LEVEL_NONE 0
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARN 2
#define DEBUG_LEVEL_INFO 3
#define DEBUG_LEVEL_DEBUG 4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_DEBUG
#endif

// Logging macros
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

// Hardware CAN Filtering Configuration
// Enable hardware filtering on MCP2515 to only receive target messages
// This improves efficiency by filtering at hardware level before software processing
// Can be disabled for debugging to receive all messages
#define ENABLE_HARDWARE_CAN_FILTERING 1

// System Health Tracking Structure
struct SystemHealth {
    unsigned long canErrors;
    unsigned long parseErrors;
    unsigned long criticalErrors;
    unsigned long lastCanActivity;
    unsigned long lastSystemOK;
    bool watchdogTriggered;
    bool recoveryMode;
};

#endif // CONFIG_H
