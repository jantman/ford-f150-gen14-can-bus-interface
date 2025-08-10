#include "mock_arduino.h"
#include "../../src/config.h"

// Define the mock Serial instance
MockSerial Serial;

// Stub implementation of isTargetCANMessage from production code
// This avoids the need to include can_manager.cpp with its SPI dependencies
bool isTargetCANMessage(uint32_t messageId) {
    return (messageId == BCM_LAMP_STAT_FD1_ID ||
            messageId == LOCKING_SYSTEMS_2_FD1_ID ||
            messageId == POWERTRAIN_DATA_10_ID ||
            messageId == BATTERY_MGMT_3_FD1_ID);
}
