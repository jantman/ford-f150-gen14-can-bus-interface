#include "can_manager.h"
#include <SPI.h>
#include <mcp2515.h>

// MCP2515 CAN controller instance
MCP2515 mcp2515(CAN_CS_PIN);

// Global CAN state
static bool canInitialized = false;
static bool canConnected = false;
static unsigned long lastCANActivity = 0;
static uint32_t messagesReceived = 0;
static uint32_t canErrors = 0;

bool initializeCAN() {
    LOG_INFO("Initializing CAN bus (MCP2515) in LISTEN-ONLY mode...");
    LOG_INFO("Using X2 header (CAN2H/CAN2L) with MCP2515 controller");
    
    // Check if already initialized
    if (canInitialized) {
        LOG_WARN("CAN driver already initialized");
        return isCANConnected();
    }
    
    // Initialize SPI for MCP2515
    SPI.begin(CAN_CLK_PIN, CAN_MISO_PIN, CAN_MOSI_PIN, CAN_CS_PIN);
    
    // Reset MCP2515
    mcp2515.reset();
    
    // Set CAN bit rate to 500kbps with 16MHz crystal
    MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
    if (result != MCP2515::ERROR_OK) {
        LOG_ERROR("Failed to set CAN bitrate: %d", (int)result);
        canErrors++;
        return false;
    }
    
    // Set to listen-only mode (no ACK, no error frames)
    result = mcp2515.setListenOnlyMode();
    if (result != MCP2515::ERROR_OK) {
        LOG_ERROR("Failed to set listen-only mode: %d", (int)result);
        canErrors++;
        return false;
    }
    
    canInitialized = true;
    canConnected = true;
    LOG_INFO("CAN bus initialized successfully using MCP2515");
    LOG_INFO("Ready to receive messages on X2 header (CAN2H/CAN2L)");
    
    return true;
}

bool receiveCANMessage(CANMessage& message) {
    if (!canInitialized) {
        return false;
    }
    
    struct can_frame frame;
    MCP2515::ERROR result = mcp2515.readMessage(&frame);
    
    if (result == MCP2515::ERROR_OK) {
        // Convert MCP2515 frame to our CANMessage structure
        message.id = frame.can_id;
        message.length = frame.can_dlc;
        memcpy(message.data, frame.data, frame.can_dlc);
        message.timestamp = millis();
        
        messagesReceived++;
        lastCANActivity = millis();
        canConnected = true;
        
        return true;
    } else if (result == MCP2515::ERROR_NOMSG) {
        // No message available - not an error
        return false;
    } else {
        // Actual error occurred
        canErrors++;
        LOG_DEBUG("CAN receive error: %d", (int)result);
        return false;
    }
}

// Debug function to receive and log any CAN message (not just target messages)
void debugReceiveAllMessages() {
    if (!canInitialized || !canConnected) {
        return;
    }
    
    struct can_frame frame;
    int messagesProcessed = 0;
    const int MAX_DEBUG_MESSAGES = 50; // Prevent spam
    
    while (messagesProcessed < MAX_DEBUG_MESSAGES) {
        MCP2515::ERROR result = mcp2515.readMessage(&frame);
        
        if (result == MCP2515::ERROR_OK) {
            messagesProcessed++;
            lastCANActivity = millis();
            
            // Log the message
            LOG_INFO("DEBUG CAN RX: ID=0x%03X (%d), Len=%d, Data=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                frame.can_id, frame.can_id, frame.can_dlc,
                frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
        } else if (result == MCP2515::ERROR_NOMSG) {
            // No more messages available
            break;
        } else {
            // Error occurred
            canErrors++;
            LOG_WARN("CAN debug receive error: %d", (int)result);
            break;
        }
    }
    
    if (messagesProcessed > 0) {
        LOG_INFO("Processed %d debug messages", messagesProcessed);
    }
}

void processPendingCANMessages() {
    if (!canInitialized || !canConnected) {
        return;
    }
    
    // Process up to 10 messages per call to avoid blocking too long
    int processedCount = 0;
    const int MAX_MESSAGES_PER_CALL = 10;
    
    while (processedCount < MAX_MESSAGES_PER_CALL) {
        struct can_frame frame;
        MCP2515::ERROR result = mcp2515.readMessage(&frame);
        
        if (result == MCP2515::ERROR_OK) {
            processedCount++;
            messagesReceived++;
            lastCANActivity = millis();
            
            // Create CANMessage structure
            CANMessage message;
            message.id = frame.can_id;
            message.length = frame.can_dlc;
            memcpy(message.data, frame.data, frame.can_dlc);
            message.timestamp = millis();
            
            // Log if it's a target message we care about
            if (isTargetCANMessage(message.id)) {
                LOG_DEBUG("Target CAN message: ID=0x%03X, Len=%d", message.id, message.length);
            }
        } else if (result == MCP2515::ERROR_NOMSG) {
            // No more messages available
            break;
        } else {
            // Error occurred
            canErrors++;
            LOG_DEBUG("CAN process error: %d", (int)result);
            break;
        }
    }
}

bool isCANConnected() {
    if (!canInitialized) {
        return false;
    }
    
    // For MCP2515, we consider it connected if:
    // 1. It's initialized
    // 2. We've received messages recently OR we haven't had many errors
    
    unsigned long timeSinceActivity = millis() - lastCANActivity;
    
    // If we've received messages recently, we're definitely connected
    if (timeSinceActivity < 5000) { // 5 seconds
        canConnected = true;
        return true;
    }
    
    // If we have too many errors, consider disconnected
    if (canErrors > 10) {
        canConnected = false;
        return false;
    }
    
    // Otherwise, maintain current connection state
    return canConnected;
}

void handleCANError() {
    LOG_ERROR("Handling CAN bus error - attempting recovery");
    
    if (!canInitialized) {
        return;
    }
    
    // For MCP2515, we can try to reset and reinitialize
    mcp2515.reset();
    delay(100);
    
    // Reinitialize
    MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
    if (result == MCP2515::ERROR_OK) {
        result = mcp2515.setListenOnlyMode();
        if (result == MCP2515::ERROR_OK) {
            LOG_INFO("CAN bus recovery successful");
            canConnected = true;
            canErrors = 0; // Reset error count on successful recovery
        } else {
            LOG_ERROR("CAN recovery failed - setListenOnlyMode error: %d", (int)result);
            canConnected = false;
        }
    } else {
        LOG_ERROR("CAN recovery failed - setBitrate error: %d", (int)result);
        canConnected = false;
    }
}

bool recoverCANSystem() {
    LOG_WARN("Performing full CAN system recovery...");
    
    // Reset MCP2515 completely
    mcp2515.reset();
    delay(200);
    
    // Reinitialize SPI
    SPI.end();
    delay(100);
    SPI.begin(CAN_CLK_PIN, CAN_MISO_PIN, CAN_MOSI_PIN, CAN_CS_PIN);
    delay(100);
    
    // Reinitialize MCP2515
    MCP2515::ERROR result = mcp2515.setBitrate(CAN_500KBPS, MCP_16MHZ);
    if (result != MCP2515::ERROR_OK) {
        LOG_ERROR("Failed to set bitrate during recovery: %d", (int)result);
        canInitialized = false;
        canConnected = false;
        return false;
    }
    
    result = mcp2515.setListenOnlyMode();
    if (result != MCP2515::ERROR_OK) {
        LOG_ERROR("Failed to set listen-only mode during recovery: %d", (int)result);
        canInitialized = false;
        canConnected = false;
        return false;
    }
    
    canInitialized = true;
    canConnected = true;
    lastCANActivity = millis();
    messagesReceived = 0;
    canErrors = 0;
    
    LOG_INFO("Full CAN system recovery successful");
    return true;
}

// Diagnostic function to check raw CAN bus activity
void checkRawCANActivity() {
    if (!canInitialized) {
        LOG_ERROR("CAN not initialized, cannot check raw activity");
        return;
    }
    
    LOG_INFO("=== RAW CAN DIAGNOSTICS ===");
    LOG_INFO("  Controller: MCP2515 (SPI-based)");
    LOG_INFO("  Interface: X2 header (CAN2H/CAN2L)");
    LOG_INFO("  SPI Pins: CS=%d, CLK=%d, MISO=%d, MOSI=%d", CAN_CS_PIN, CAN_CLK_PIN, CAN_MISO_PIN, CAN_MOSI_PIN);
    LOG_INFO("  Bitrate: 500kbps with 16MHz crystal");
    LOG_INFO("  Mode: Listen-only");
    LOG_INFO("  Messages Received: %lu", messagesReceived);
    LOG_INFO("  Errors: %lu", canErrors);
    LOG_INFO("  Last Activity: %lu ms ago", millis() - lastCANActivity);
    
    // Try to read a few messages immediately
    struct can_frame frame;
    int immediateMessages = 0;
    for (int i = 0; i < 5; i++) {
        if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
            immediateMessages++;
            LOG_INFO("  Immediate message %d: ID=0x%03X, Len=%d", i+1, frame.can_id, frame.can_dlc);
        } else {
            break;
        }
    }
    
    if (immediateMessages == 0) {
        LOG_INFO("  No immediate messages available");
    } else {
        LOG_INFO("  Found %d immediate messages", immediateMessages);
        lastCANActivity = millis();
    }
    
    LOG_INFO("=== END CAN DIAGNOSTICS ===");
}

// Enhanced statistics with more detailed information
void printCANStatistics() {
    LOG_INFO("CAN Bus Statistics (MCP2515 Listen-Only Mode):");
    LOG_INFO("  Controller: MCP2515 on X2 header");
    LOG_INFO("  Messages Received: %lu", messagesReceived);
    LOG_INFO("  Errors: %lu", canErrors);
    LOG_INFO("  Last Activity: %lu ms ago", millis() - lastCANActivity);
    LOG_INFO("  Connection Status: %s", canConnected ? "Connected" : "Disconnected");
    LOG_INFO("  Initialized: %s", canInitialized ? "Yes" : "No");
}

void resetCANStatistics() {
    messagesReceived = 0;
    canErrors = 0;
    lastCANActivity = millis();
    LOG_INFO("CAN statistics reset");
}

// Check if a specific message ID should be processed
bool isTargetCANMessage(uint32_t messageId) {
    return (messageId == BCM_LAMP_STAT_FD1_ID ||
            messageId == LOCKING_SYSTEMS_2_FD1_ID ||
            messageId == POWERTRAIN_DATA_10_ID ||
            messageId == BATTERY_MGMT_3_FD1_ID);
}
