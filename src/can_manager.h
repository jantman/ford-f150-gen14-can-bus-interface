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

// Function declarations (to be implemented in Step 3)
bool initializeCAN();
bool sendCANMessage(const CANMessage& message);
bool receiveCANMessage(CANMessage& message);
void processPendingCANMessages();
bool isCANConnected();
void handleCANError();

#endif // CAN_MANAGER_H
