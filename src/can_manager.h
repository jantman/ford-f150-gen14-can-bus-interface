#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include "config.h"

// CAN message structure
struct CANMessage {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
    unsigned long timestamp;
};

// Function declarations
bool initializeCAN();
bool receiveCANMessage(CANMessage& message);
void processPendingCANMessages();
bool isCANConnected();
void handleCANError();

// Utility functions for monitoring and debugging
void printCANStatistics();
void resetCANStatistics();
bool isTargetCANMessage(uint32_t messageId);

#endif // CAN_MANAGER_H
