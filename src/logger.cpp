#include "logger.h"

// Log CAN message with direction, ID, and raw data
void logCANMessage(const char* direction, uint32_t id, const uint8_t* data, uint8_t length) {
    // Create hex string representation of the data
    char dataHex[25]; // 8 bytes * 3 chars per byte + null terminator
    char* ptr = dataHex;
    
    for (uint8_t i = 0; i < length && i < 8; i++) {
        if (i > 0) {
            *ptr++ = ' ';
        }
        sprintf(ptr, "%02X", data[i]);
        ptr += 2;
    }
    *ptr = '\0';
    
    LOG_DEBUG("CAN %s: ID=0x%03X (%d), Len=%d, Data=[%s]", 
              direction, id, id, length, dataHex);
}

// Utility functions for converting values to strings
const char* pudLampToString(uint8_t value) {
    switch (value) {
        case PUDLAMP_OFF: return "OFF";
        case PUDLAMP_ON: return "ON";
        case PUDLAMP_RAMP_UP: return "RAMP_UP";
        case PUDLAMP_RAMP_DOWN: return "RAMP_DOWN";
        default: return "UNKNOWN";
    }
}

const char* lockStatusToString(uint8_t value) {
    switch (value) {
        case VEH_LOCK_DBL: return "DOUBLE_LOCK";
        case VEH_LOCK_ALL: return "LOCK_ALL";
        case VEH_UNLOCK_ALL: return "UNLOCK_ALL";
        case VEH_UNLOCK_DRV: return "UNLOCK_DRIVER";
        case VEH_LOCK_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* parkStatusToString(uint8_t value) {
    switch (value) {
        case TRNPRKSTS_UNKNOWN: return "UNKNOWN";
        case TRNPRKSTS_PARK: return "PARK";
        case TRNPRKSTS_TRANSITION_CLOSE_TO_PARK: return "TRANS_TO_PARK";
        case TRNPRKSTS_AT_NO_SPRING: return "AT_NO_SPRING";
        case TRNPRKSTS_TRANSITION_CLOSE_TO_OUT_OF_PARK: return "TRANS_FROM_PARK";
        case TRNPRKSTS_OUT_OF_PARK: return "OUT_OF_PARK";
        default: return "INVALID";
    }
}

const char* boolToString(bool value) {
    return value ? "TRUE" : "FALSE";
}

// System startup logging
void logSystemStartup() {
    LOG_INFO("=== SYSTEM STARTUP ===");
    LOG_INFO("Build Date: %s %s", BUILD_DATE, BUILD_TIME);
    #ifdef FIRMWARE_VERSION
    LOG_INFO("Firmware Version: %s", FIRMWARE_VERSION);
    #endif
    LOG_INFO("Target CAN Messages:");
    LOG_INFO("  BCM_Lamp_Stat_FD1: 0x%03X (%d)", BCM_LAMP_STAT_FD1_ID, BCM_LAMP_STAT_FD1_ID);
    LOG_INFO("  Locking_Systems_2_FD1: 0x%03X (%d)", LOCKING_SYSTEMS_2_FD1_ID, LOCKING_SYSTEMS_2_FD1_ID);
    LOG_INFO("  PowertrainData_10: 0x%03X (%d)", POWERTRAIN_DATA_10_ID, POWERTRAIN_DATA_10_ID);
    LOG_INFO("  Battery_Mgmt_3_FD1: 0x%03X (%d)", BATTERY_MGMT_3_FD1_ID, BATTERY_MGMT_3_FD1_ID);
    LOG_INFO("=== INITIALIZATION COMPLETE ===");
}

// Log state changes
void logStateChange(const char* signal, const char* oldValue, const char* newValue) {
    LOG_INFO("STATE CHANGE: %s: %s -> %s", signal, oldValue, newValue);
}

// Log GPIO changes
void logGPIOChange(const char* pin, bool state) {
    LOG_DEBUG("GPIO %s: %s", pin, state ? "HIGH" : "LOW");
}

// Log button press
void logButtonPress() {
    LOG_INFO("BUTTON: Toolbox button pressed");
}

// Log errors
void logError(const char* component, const char* error) {
    LOG_ERROR("%s ERROR: %s", component, error);
}

// Performance monitoring
void logMemoryUsage() {
    #ifdef ESP32
    LOG_DEBUG("Memory - Free: %d bytes, Largest block: %d bytes", 
              ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    #endif
}

void logSystemPerformance() {
    LOG_DEBUG("System uptime: %lu ms", millis());
    logMemoryUsage();
}
