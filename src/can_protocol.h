#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pure CAN message structure (no Arduino dependencies)
typedef struct {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} CANFrame;

#include "bit_utils.h"

// Pure decision logic functions (no Arduino dependencies)
// These functions have been moved to state_manager.h to eliminate duplication

#ifdef __cplusplus
}
#endif

#endif // CAN_PROTOCOL_H
