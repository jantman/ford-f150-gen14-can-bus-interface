#include "can_manager.h"
#include "driver/twai.h"
#include "esp_err.h"

// Global CAN state
static bool canInitialized = false;
static bool canConnected = false;
static unsigned long lastCANActivity = 0;
static uint32_t messagesReceived = 0;
static uint32_t canErrors = 0;

// TWAI configuration with more robust settings
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_LISTEN_ONLY,  // Listen-only mode - cannot transmit
    .tx_io = (gpio_num_t)CAN_TX_PIN, // Still needed even in listen-only mode
    .rx_io = (gpio_num_t)CAN_RX_PIN,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 0,              // No TX queue needed
    .rx_queue_len = 50,             // Increased from 20 to handle burst traffic
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | 
                      TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_ERR_ACTIVE,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1
};

// More specific timing configuration for 500kbps
static const twai_timing_config_t t_config = {
    .brp = 8,           // Baud rate prescaler
    .tseg_1 = 15,       // Time segment 1
    .tseg_2 = 4,        // Time segment 2  
    .sjw = 3,           // Synchronization jump width
    .triple_sampling = false
};

static const twai_filter_config_t f_config = {
    .acceptance_code = 0x00000000,
    .acceptance_mask = 0x00000000,  // Accept all messages
    .single_filter = true
};

bool initializeCAN() {
    LOG_INFO("Initializing CAN bus (TWAI) in LISTEN-ONLY mode...");
    LOG_INFO("Note: TX/RX pins connect ESP32 to CAN transceiver, not directly to CAN_H/CAN_L");
    
    // Check if driver is already installed
    if (canInitialized) {
        LOG_WARN("CAN driver already initialized, skipping installation");
        return isCANConnected();
    }
    
    // Install TWAI driver
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result == ESP_ERR_INVALID_STATE) {
        LOG_WARN("TWAI driver already installed, attempting to start...");
        // Driver already installed, try to start it
        result = twai_start();
        if (result == ESP_OK) {
            canInitialized = true;
            canConnected = true;
            lastCANActivity = millis();
            LOG_INFO("CAN bus started successfully (driver was already installed)");
            return true;
        } else {
            LOG_ERROR("Failed to start existing TWAI driver: %s", esp_err_to_name(result));
            return false;
        }
    } else if (result != ESP_OK) {
        LOG_ERROR("Failed to install TWAI driver: %s", esp_err_to_name(result));
        return false;
    }
    
    // Start TWAI driver
    result = twai_start();
    if (result != ESP_OK) {
        LOG_ERROR("Failed to start TWAI driver: %s", esp_err_to_name(result));
        twai_driver_uninstall();
        return false;
    }
    
    canInitialized = true;
    canConnected = true;
    lastCANActivity = millis();
    messagesReceived = 0;
    canErrors = 0;
    
    LOG_INFO("CAN bus initialized successfully (LISTEN-ONLY MODE)");
    LOG_INFO("  TX Pin: %d (connected but won't transmit)", CAN_TX_PIN);
    LOG_INFO("  RX Pin: %d", CAN_RX_PIN);
    LOG_INFO("  Baud Rate: %d bps", CAN_BAUDRATE);
    LOG_INFO("  Mode: Listen-only (No transmission capability)");
    LOG_INFO("  Filter: Accept all messages");
    LOG_INFO("  RX Queue Size: %d messages", g_config.rx_queue_len);
    
    // Give some time for the CAN controller to settle and start receiving
    delay(100);
    
    // Check if we can immediately see any bus activity
    LOG_INFO("Checking for immediate bus activity...");
    checkRawCANActivity();
    
    return true;
}

bool receiveCANMessage(CANMessage& message) {
    if (!canInitialized || !canConnected) {
        return false;
    }
    
    twai_message_t twai_msg;
    esp_err_t result = twai_receive(&twai_msg, 0);  // Non-blocking receive
    
    if (result == ESP_OK) {
        // Convert TWAI message to our CANMessage
        message.id = twai_msg.identifier;
        message.length = twai_msg.data_length_code;
        message.timestamp = millis();
        
        // Copy data
        for (int i = 0; i < twai_msg.data_length_code && i < 8; i++) {
            message.data[i] = twai_msg.data[i];
        }
        
        messagesReceived++;
        lastCANActivity = millis();
        
        LOG_DEBUG("CAN message received: ID=0x%03X, Length=%d", message.id, message.length);
        return true;
    } else if (result == ESP_ERR_TIMEOUT) {
        // No message available, this is normal
        return false;
    } else if (result == ESP_ERR_INVALID_STATE) {
        // Driver not in correct state - mark as disconnected
        LOG_ERROR("CAN receive failed - driver invalid state");
        canConnected = false;
        canErrors++;
        return false;
    } else {
        canErrors++;
        LOG_WARN("CAN receive error: %s", esp_err_to_name(result));
        return false;
    }
}

// Debug function to receive and log any CAN message (not just target messages)
void debugReceiveAllMessages() {
    if (!canInitialized || !canConnected) {
        return;
    }
    
    twai_message_t twai_msg;
    int messagesProcessed = 0;
    const int MAX_DEBUG_MESSAGES = 50; // Prevent spam
    
    while (messagesProcessed < MAX_DEBUG_MESSAGES) {
        esp_err_t result = twai_receive(&twai_msg, 0);  // Non-blocking receive
        
        if (result == ESP_OK) {
            lastCANActivity = millis(); // Update activity timestamp
            
            // Log the message details
            LOG_INFO("CAN MSG: ID=0x%03X (%d), DLC=%d, Data=[%02X %02X %02X %02X %02X %02X %02X %02X]%s",
                     twai_msg.identifier, twai_msg.identifier, twai_msg.data_length_code,
                     twai_msg.data[0], twai_msg.data[1], twai_msg.data[2], twai_msg.data[3],
                     twai_msg.data[4], twai_msg.data[5], twai_msg.data[6], twai_msg.data[7],
                     isTargetCANMessage(twai_msg.identifier) ? " [TARGET]" : "");
            
            messagesProcessed++;
        } else if (result == ESP_ERR_TIMEOUT) {
            // No more messages available
            break;
        } else {
            LOG_WARN("Debug receive error: %s", esp_err_to_name(result));
            break;
        }
    }
    
    if (messagesProcessed > 0) {
        LOG_INFO("Debug: Processed %d CAN messages", messagesProcessed);
    }
}

void processPendingCANMessages() {
    if (!canInitialized || !canConnected) {
        return;
    }
    
    // Check for alerts
    uint32_t alerts;
    esp_err_t result = twai_read_alerts(&alerts, 0);  // Non-blocking
    
    if (result == ESP_OK && alerts != 0) {
        if (alerts & TWAI_ALERT_ERR_PASS) {
            LOG_WARN("CAN: Error passive state");
        }
        if (alerts & TWAI_ALERT_BUS_ERROR) {
            LOG_ERROR("CAN: Bus error detected");
            canErrors++;
        }
        if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
            LOG_WARN("CAN: RX queue full, messages may be lost");
        }
        // Note: No bus-off alert in listen-only mode
    }
    
    // Process all pending messages
    CANMessage message;
    int messagesProcessed = 0;
    const int MAX_MESSAGES_PER_CYCLE = 10;  // Prevent infinite loop
    
    while (messagesProcessed < MAX_MESSAGES_PER_CYCLE && receiveCANMessage(message)) {
        // Check if this is one of our target messages
        if (message.id == BCM_LAMP_STAT_FD1_ID ||
            message.id == LOCKING_SYSTEMS_2_FD1_ID ||
            message.id == POWERTRAIN_DATA_10_ID ||
            message.id == BATTERY_MGMT_3_FD1_ID) {
            
            LOG_DEBUG("Target CAN message received: ID=0x%03X", message.id);
            // Message will be processed by message parser in the main loop
        }
        
        messagesProcessed++;
    }
}

bool isCANConnected() {
    if (!canInitialized) {
        return false;
    }
    
    // Check TWAI driver state first
    twai_status_info_t status;
    esp_err_t result = twai_get_status_info(&status);
    if (result != ESP_OK) {
        LOG_WARN("Failed to get TWAI status: %s", esp_err_to_name(result));
        canConnected = false;
        return false;
    }
    
    // Check if driver is in a healthy state
    if (status.state != TWAI_STATE_RUNNING) {
        LOG_WARN("TWAI driver not running (state: %d)", status.state);
        canConnected = false;
        return false;
    }
    
    // Check for any sign of bus activity:
    // 1. Recent successful message receipt
    // 2. Messages pending in queue (even if we haven't processed them)
    // 3. Low error counters indicate healthy communication
    unsigned long timeSinceActivity = millis() - lastCANActivity;
    bool hasRecentActivity = (timeSinceActivity <= CAN_TIMEOUT_MS);
    bool hasQueuedMessages = (status.msgs_to_rx > 0);
    bool hasHealthyErrorCounters = (status.rx_error_counter < 128 && status.tx_error_counter < 128);
    
    // Update last activity if there are queued messages
    if (hasQueuedMessages) {
        lastCANActivity = millis();
        hasRecentActivity = true;
        LOG_DEBUG("Updated CAN activity timestamp due to queued messages");
    }
    
    // Consider connected if we have recent activity OR healthy error counters
    // (healthy counters indicate the bus is working even if we're not receiving target messages)
    canConnected = hasRecentActivity || hasHealthyErrorCounters;
    
    if (!canConnected) {
        LOG_WARN("CAN bus timeout - no activity for %lu ms, queued: %lu, RX errors: %lu", 
                 timeSinceActivity, status.msgs_to_rx, status.rx_error_counter);
    }
    
    return canConnected;
}

void handleCANError() {
    LOG_ERROR("Handling CAN bus error - attempting recovery");
    
    if (!canInitialized) {
        return;
    }
    
    // Stop the driver
    esp_err_t result = twai_stop();
    if (result != ESP_OK) {
        LOG_ERROR("Failed to stop TWAI driver: %s", esp_err_to_name(result));
    }
    
    // Wait a bit
    delay(100);
    
    // Restart the driver
    result = twai_start();
    if (result == ESP_OK) {
        LOG_INFO("CAN bus recovery successful");
        canConnected = true;
        lastCANActivity = millis();
    } else {
        LOG_ERROR("CAN bus recovery failed: %s", esp_err_to_name(result));
        canConnected = false;
    }
}

// Full CAN system recovery - uninstalls and reinstalls the driver
bool recoverCANSystem() {
    LOG_INFO("Attempting full CAN system recovery...");
    
    // First try to stop if running
    if (canInitialized) {
        esp_err_t result = twai_stop();
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
            LOG_WARN("Failed to stop TWAI driver during recovery: %s", esp_err_to_name(result));
        }
        
        // Uninstall the driver
        result = twai_driver_uninstall();
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
            LOG_ERROR("Failed to uninstall TWAI driver during recovery: %s", esp_err_to_name(result));
            return false;
        }
        
        canInitialized = false;
        canConnected = false;
        LOG_INFO("TWAI driver uninstalled for recovery");
    }
    
    // Wait a bit for hardware to settle
    delay(200);
    
    // Reinstall and start the driver
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        LOG_ERROR("Failed to reinstall TWAI driver during recovery: %s", esp_err_to_name(result));
        return false;
    }
    
    result = twai_start();
    if (result != ESP_OK) {
        LOG_ERROR("Failed to start TWAI driver during recovery: %s", esp_err_to_name(result));
        twai_driver_uninstall();
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
    
    twai_status_info_t status;
    esp_err_t result = twai_get_status_info(&status);
    if (result == ESP_OK) {
        LOG_INFO("=== RAW CAN DIAGNOSTICS ===");
        LOG_INFO("  TWAI State: %d (0=STOPPED, 1=RUNNING, 2=BUS_OFF, 3=RECOVERING)", status.state);
        LOG_INFO("  Messages in RX Queue: %lu", status.msgs_to_rx);
        LOG_INFO("  Messages in TX Queue: %lu", status.msgs_to_tx);
        LOG_INFO("  TX Error Counter: %lu", status.tx_error_counter);
        LOG_INFO("  RX Error Counter: %lu", status.rx_error_counter);
        LOG_INFO("  TX Failed Count: %lu", status.tx_failed_count);
        LOG_INFO("  RX Missed Count: %lu", status.rx_missed_count);
        LOG_INFO("  RX Overrun Count: %lu", status.rx_overrun_count);
        LOG_INFO("  Arbitration Lost Count: %lu", status.arb_lost_count);
        LOG_INFO("  Bus Error Count: %lu", status.bus_error_count);
        
        // Check for any messages in the queue without removing them
        if (status.msgs_to_rx > 0) {
            LOG_INFO("  *** MESSAGES ARE AVAILABLE IN QUEUE ***");
            lastCANActivity = millis(); // Update activity if messages are queued
        }
    } else {
        LOG_ERROR("Failed to get TWAI status for diagnostics: %s", esp_err_to_name(result));
    }
}

// Enhanced statistics with more detailed information
void printCANStatistics() {
    LOG_INFO("CAN Bus Statistics (Listen-Only Mode):");
    LOG_INFO("  Messages Received: %lu", messagesReceived);
    LOG_INFO("  Errors: %lu", canErrors);
    LOG_INFO("  Last Activity: %lu ms ago", millis() - lastCANActivity);
    LOG_INFO("  Connected: %s", canConnected ? "Yes" : "No");
    LOG_INFO("  Initialized: %s", canInitialized ? "Yes" : "No");
    
    checkRawCANActivity();
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
