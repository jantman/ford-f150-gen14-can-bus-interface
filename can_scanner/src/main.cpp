// CAN Scanner - Simple diagnostic tool to dump all CAN bus activity
// This is a minimal standalone program to help diagnose CAN bus connectivity issues

#include <Arduino.h>
#include "driver/twai.h"
#include "esp_err.h"

// Pin definitions for ESP32-CAN-X2 board
#define CAN_TX_PIN 7
#define CAN_RX_PIN 6

// TWAI configuration - matches main project
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_LISTEN_ONLY,
    .tx_io = (gpio_num_t)CAN_TX_PIN,
    .rx_io = (gpio_num_t)CAN_RX_PIN,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 0,
    .rx_queue_len = 50,
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | 
                      TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_ERR_ACTIVE,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1
};

// Standard ESP-IDF timing configuration for 500kbps
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

// Accept all messages filter
static const twai_filter_config_t f_config = {
    .acceptance_code = 0x00000000,
    .acceptance_mask = 0x00000000,
    .single_filter = true
};

// Statistics
unsigned long totalMessages = 0;
unsigned long lastStatsTime = 0;
unsigned long startTime = 0;

void setup() {
    Serial.begin(115200);
    delay(2000); // Give time for serial to connect
    
    Serial.println("========================================");
    Serial.println("ESP32 CAN Bus Scanner v1.0");
    Serial.println("========================================");
    Serial.println("Purpose: Scan for ANY CAN bus activity");
    Serial.println("Board: ESP32-CAN-X2");
    Serial.println("Baud Rate: 500kbps");
    Serial.println("Mode: Listen-Only");
    Serial.println("Filter: Accept ALL messages");
    Serial.printf("TX Pin: %d (not used in listen-only)\n", CAN_TX_PIN);
    Serial.printf("RX Pin: %d\n", CAN_RX_PIN);
    Serial.println("========================================");
    Serial.println();
    
    // Install TWAI driver
    Serial.print("Installing TWAI driver... ");
    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        Serial.printf("FAILED: %s\n", esp_err_to_name(result));
        Serial.println("Cannot continue without TWAI driver!");
        while(1) {
            delay(1000);
            Serial.println("FATAL: TWAI driver installation failed");
        }
    }
    Serial.println("SUCCESS");
    
    // Start TWAI driver
    Serial.print("Starting TWAI driver... ");
    result = twai_start();
    if (result != ESP_OK) {
        Serial.printf("FAILED: %s\n", esp_err_to_name(result));
        twai_driver_uninstall();
        while(1) {
            delay(1000);
            Serial.println("FATAL: TWAI driver start failed");
        }
    }
    Serial.println("SUCCESS");
    
    Serial.println();
    Serial.println("CAN Scanner ready - listening for ANY messages...");
    Serial.println("Output format: [TIME] ID=0xXXX DLC=X DATA=[XX XX XX XX XX XX XX XX]");
    Serial.println("Press Ctrl+C to stop");
    Serial.println();
    
    startTime = millis();
    lastStatsTime = startTime;
}

void printDriverStatus() {
    twai_status_info_t status;
    esp_err_t result = twai_get_status_info(&status);
    
    Serial.println("--- DRIVER STATUS ---");
    if (result == ESP_OK) {
        Serial.printf("TWAI State: %d (0=STOPPED, 1=RUNNING, 2=BUS_OFF, 3=RECOVERING)\n", status.state);
        Serial.printf("Messages in RX Queue: %lu\n", status.msgs_to_rx);
        Serial.printf("TX Error Counter: %lu\n", status.tx_error_counter);
        Serial.printf("RX Error Counter: %lu\n", status.rx_error_counter);
        Serial.printf("Bus Error Count: %lu\n", status.bus_error_count);
        Serial.printf("RX Missed Count: %lu\n", status.rx_missed_count);
        Serial.printf("RX Overrun Count: %lu\n", status.rx_overrun_count);
    } else {
        Serial.printf("Failed to get status: %s\n", esp_err_to_name(result));
    }
    Serial.println("--------------------");
}

void checkAlerts() {
    uint32_t alerts;
    esp_err_t result = twai_read_alerts(&alerts, 0);
    
    if (result == ESP_OK && alerts != 0) {
        Serial.print("[ALERT] ");
        if (alerts & TWAI_ALERT_ERR_PASS) {
            Serial.print("ERROR_PASSIVE ");
        }
        if (alerts & TWAI_ALERT_BUS_ERROR) {
            Serial.print("BUS_ERROR ");
        }
        if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
            Serial.print("RX_QUEUE_FULL ");
        }
        if (alerts & TWAI_ALERT_ERR_ACTIVE) {
            Serial.print("ERROR_ACTIVE ");
        }
        Serial.println();
    }
}

void loop() {
    unsigned long currentTime = millis();
    
    // Check for alerts
    checkAlerts();
    
    // Try to receive messages
    twai_message_t message;
    esp_err_t result = twai_receive(&message, 0); // Non-blocking
    
    if (result == ESP_OK) {
        totalMessages++;
        
        // Calculate elapsed time
        unsigned long elapsed = currentTime - startTime;
        unsigned long seconds = elapsed / 1000;
        unsigned long ms = elapsed % 1000;
        
        // Print message in readable format
        Serial.printf("[%02lu:%02lu.%03lu] ID=0x%03X DLC=%d DATA=[", 
                     (seconds / 60), (seconds % 60), ms,
                     message.identifier, message.data_length_code);
        
        for (int i = 0; i < 8; i++) {
            if (i < message.data_length_code) {
                Serial.printf("%02X", message.data[i]);
            } else {
                Serial.print("--");
            }
            if (i < 7) Serial.print(" ");
        }
        Serial.println("]");
        
    } else if (result == ESP_ERR_TIMEOUT) {
        // No message available - this is normal
    } else {
        Serial.printf("[ERROR] CAN receive failed: %s\n", esp_err_to_name(result));
    }
    
    // Print periodic statistics
    if (currentTime - lastStatsTime >= 30000) { // Every 30 seconds
        unsigned long elapsed = currentTime - startTime;
        Serial.println();
        Serial.printf("=== STATISTICS (after %lu seconds) ===\n", elapsed / 1000);
        Serial.printf("Total messages received: %lu\n", totalMessages);
        if (elapsed > 0) {
            Serial.printf("Messages per second: %.2f\n", (float)totalMessages * 1000.0 / elapsed);
        }
        Serial.printf("Free heap: %lu bytes\n", ESP.getFreeHeap());
        printDriverStatus();
        Serial.println("======================================");
        Serial.println();
        
        lastStatsTime = currentTime;
    }
    
    // Small delay to prevent overwhelming the CPU
    delay(1);
}
