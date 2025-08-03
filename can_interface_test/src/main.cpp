// CAN Interface Test - Test both CAN1 (built-in ESP32) and CAN2 (MCP2515)
// This tool helps identify which physical header (X1 or X2) is connected to the vehicle

#include <Arduino.h>
#include "driver/twai.h"
#include <SPI.h>
#include <mcp2515.h>

// CAN1 (Built-in ESP32 TWAI) - connects to X1 header
#define CAN1_TX_PIN 7
#define CAN1_RX_PIN 6

// CAN2 (MCP2515) - connects to X2 header  
#define CAN2_CS_PIN 10
#define CAN2_CLK_PIN 12
#define CAN2_MISO_PIN 13
#define CAN2_MOSI_PIN 11
#define CAN2_IRQ_PIN 3

// MCP2515 object for CAN2
MCP2515 CAN2(CAN2_CS_PIN);

// Test results structure
struct CANTestResult {
    bool initialized;
    bool receiving_messages;
    uint32_t message_count;
    uint32_t last_message_time;
    String status;
};

CANTestResult can1_result = {false, false, 0, 0, "Not tested"};
CANTestResult can2_result = {false, false, 0, 0, "Not tested"};

// CAN1 (Built-in ESP32) configuration
static const twai_general_config_t can1_config = {
    .mode = TWAI_MODE_LISTEN_ONLY,
    .tx_io = (gpio_num_t)CAN1_TX_PIN,
    .rx_io = (gpio_num_t)CAN1_RX_PIN,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 0,
    .rx_queue_len = 20,
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1
};

static const twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t filter_config = {
    .acceptance_code = 0x00000000,
    .acceptance_mask = 0x00000000,
    .single_filter = true
};

void testCAN1() {
    Serial.println("Testing CAN1 (Built-in ESP32 - X1 header)...");
    
    // Install and start TWAI driver
    esp_err_t result = twai_driver_install(&can1_config, &timing_config, &filter_config);
    if (result != ESP_OK) {
        can1_result.status = "Driver install failed: " + String(esp_err_to_name(result));
        Serial.println("  ✗ " + can1_result.status);
        return;
    }
    
    result = twai_start();
    if (result != ESP_OK) {
        can1_result.status = "Driver start failed: " + String(esp_err_to_name(result));
        Serial.println("  ✗ " + can1_result.status);
        twai_driver_uninstall();
        return;
    }
    
    can1_result.initialized = true;
    can1_result.status = "Initialized successfully";
    Serial.println("  ✓ CAN1 initialized successfully");
    
    // Quick test for immediate messages
    twai_message_t message;
    result = twai_receive(&message, pdMS_TO_TICKS(100));
    if (result == ESP_OK) {
        can1_result.receiving_messages = true;
        can1_result.message_count = 1;
        can1_result.last_message_time = millis();
        Serial.printf("  ✓ CAN1 immediately received message ID: 0x%X\n", message.identifier);
    } else {
        Serial.println("  ? CAN1 no immediate messages (will monitor during test)");
    }
}

void testCAN2() {
    Serial.println("Testing CAN2 (MCP2515 - X2 header)...");
    
    // Initialize SPI for MCP2515
    SPI.begin(CAN2_CLK_PIN, CAN2_MISO_PIN, CAN2_MOSI_PIN, CAN2_CS_PIN);
    
    // Reset MCP2515
    CAN2.reset();
    
    // Initialize MCP2515 with 500kbps
    if (CAN2.setBitrate(CAN_500KBPS, MCP_16MHZ) != MCP2515::ERROR_OK) {
        can2_result.status = "MCP2515 setBitrate failed";
        Serial.println("  ✗ " + can2_result.status);
        return;
    }
    
    // Set to listen-only mode
    if (CAN2.setListenOnlyMode() != MCP2515::ERROR_OK) {
        can2_result.status = "MCP2515 setListenOnlyMode failed";
        Serial.println("  ✗ " + can2_result.status);
        return;
    }
    
    can2_result.initialized = true;
    can2_result.status = "Initialized successfully";
    Serial.println("  ✓ CAN2 (MCP2515) initialized successfully");
    
    // Quick test for immediate messages
    struct can_frame frame;
    
    if (CAN2.readMessage(&frame) == MCP2515::ERROR_OK) {
        can2_result.receiving_messages = true;
        can2_result.message_count = 1;
        can2_result.last_message_time = millis();
        Serial.printf("  ✓ CAN2 immediately received message ID: 0x%X\n", frame.can_id);
    } else {
        Serial.println("  ? CAN2 no immediate messages (will monitor during test)");
    }
}

void printResults() {
    Serial.println("CAN1 (X1 header - Built-in ESP32):");
    Serial.println("  Initialized: " + String(can1_result.initialized ? "✓" : "✗"));
    Serial.println("  Status: " + can1_result.status);
    Serial.println("  Receiving: " + String(can1_result.receiving_messages ? "✓" : "✗"));
    Serial.println("  Messages: " + String(can1_result.message_count));
    if (can1_result.last_message_time > 0) {
        Serial.println("  Last msg: " + String((millis() - can1_result.last_message_time) / 1000) + " sec ago");
    }
    
    Serial.println();
    
    Serial.println("CAN2 (X2 header - MCP2515):");
    Serial.println("  Initialized: " + String(can2_result.initialized ? "✓" : "✗"));
    Serial.println("  Status: " + can2_result.status);
    Serial.println("  Receiving: " + String(can2_result.receiving_messages ? "✓" : "✗"));
    Serial.println("  Messages: " + String(can2_result.message_count));
    if (can2_result.last_message_time > 0) {
        Serial.println("  Last msg: " + String((millis() - can2_result.last_message_time) / 1000) + " sec ago");
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== ESP32-CAN-X2 Interface Test ===");
    Serial.println("Testing both CAN1 (X1 header) and CAN2 (X2 header)");
    Serial.println("Connect your vehicle CAN bus to ONE of these headers:");
    Serial.println("  X1: CAN1H/CAN1L (built-in ESP32 controller)");
    Serial.println("  X2: CAN2H/CAN2L (MCP2515 controller)");
    Serial.println("========================================\n");
    
    // Test CAN1 (Built-in ESP32)
    testCAN1();
    delay(1000);
    
    // Test CAN2 (MCP2515)  
    testCAN2();
    delay(1000);
    
    Serial.println("\n=== Initial Test Results ===");
    printResults();
    Serial.println("==============================\n");
    
    Serial.println("Now monitoring both interfaces for 30 seconds...");
    Serial.println("Any received messages will be displayed below:\n");
}

void loop() {
    static unsigned long test_start = millis();
    static unsigned long last_status = 0;
    
    // Monitor for 30 seconds
    if (millis() - test_start > 30000) {
        Serial.println("\n=== Final Test Results ===");
        printResults();
        Serial.println("===========================");
        
        // Provide recommendations
        Serial.println("\n=== RECOMMENDATIONS ===");
        if (can1_result.receiving_messages && can2_result.receiving_messages) {
            Serial.println("⚠️  Both interfaces receiving messages!");
            Serial.println("   This is unusual - check your wiring");
        } else if (can1_result.receiving_messages) {
            Serial.println("✅ Use CAN1 (X1 header) - receiving messages");
            Serial.println("   Your vehicle CAN bus is connected to X1");
            Serial.println("   GPIO pins: RX=6, TX=7 (built-in ESP32 controller)");
        } else if (can2_result.receiving_messages) {
            Serial.println("✅ Use CAN2 (X2 header) - receiving messages");
            Serial.println("   Your vehicle CAN bus is connected to X2");
            Serial.println("   You need to modify your code to use MCP2515 library");
        } else {
            Serial.println("❌ No messages received on either interface");
            Serial.println("   Check:");
            Serial.println("   1. Vehicle is running or CAN bus is active");
            Serial.println("   2. Wiring: CAN_H and CAN_L properly connected");
            Serial.println("   3. CAN bus speed (currently testing 500kbps)");
            Serial.println("   4. CAN termination (may need to disable on ESP32-CAN-X2)");
        }
        Serial.println("========================\n");
        
        // Stop here
        while (true) {
            delay(1000);
        }
    }
    
    // Check CAN1 for new messages
    if (can1_result.initialized) {
        twai_message_t message;
        while (twai_receive(&message, 0) == ESP_OK) {
            can1_result.receiving_messages = true;
            can1_result.message_count++;
            can1_result.last_message_time = millis();
            
            Serial.printf("CAN1 RX: ID=0x%X, Len=%d, Data=", message.identifier, message.data_length_code);
            for (int i = 0; i < message.data_length_code; i++) {
                Serial.printf("%02X ", message.data[i]);
            }
            Serial.println();
        }
    }
    
    // Check CAN2 for new messages
    if (can2_result.initialized) {
        struct can_frame frame;
        
        while (CAN2.readMessage(&frame) == MCP2515::ERROR_OK) {
            can2_result.receiving_messages = true;
            can2_result.message_count++;
            can2_result.last_message_time = millis();
            
            Serial.printf("CAN2 RX: ID=0x%X, Len=%d, Data=", frame.can_id, frame.can_dlc);
            for (int i = 0; i < frame.can_dlc; i++) {
                Serial.printf("%02X ", frame.data[i]);
            }
            Serial.println();
        }
    }
    
    // Print status every 5 seconds
    if (millis() - last_status > 5000) {
        last_status = millis();
        Serial.printf("[%d sec] CAN1: %d msgs, CAN2: %d msgs\n", 
                     (millis() - test_start) / 1000,
                     can1_result.message_count, 
                     can2_result.message_count);
    }
    
    delay(10);
}
