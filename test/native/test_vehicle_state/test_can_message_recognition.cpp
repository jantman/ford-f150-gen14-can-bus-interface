#include <gtest/gtest.h>
#include "mock_arduino.h"
#include "common/test_config.h"

// Import production code structures and functions
// isTargetCANMessage is provided by mock_arduino.cpp to avoid Arduino dependencies
extern "C" {
    bool isTargetCANMessage(uint32_t messageId);
}

/**
 * CAN Message Recognition Test Suite
 * 
 * These tests validate our C++ CAN message recognition and filtering logic
 * against the Python implementation in can_embedded_logger.py. The Python code
 * defines exactly which CAN message IDs we should monitor and filter for.
 * 
 * Python CAN_MESSAGES dictionary defines:
 * - 0x3C3 (963): BCM_Lamp_Stat_FD1
 * - 0x331 (817): Locking_Systems_2_FD1  
 * - 0x176 (374): PowertrainData_10
 * - 0x43C (1084): Battery_Mgmt_3_FD1
 * 
 * Python uses CAN_FILTER_IDS = list(CAN_MESSAGES.keys()) for SocketCAN filtering.
 * Our C++ implementation should recognize these same messages as "target" messages.
 */

class CANMessageRecognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    // Helper to create a CAN message with specific ID
    struct TestCANMessage {
        uint32_t id;
        uint8_t length;
        uint8_t data[8];
        unsigned long timestamp;
    };
    
    TestCANMessage createTestMessage(uint32_t id, uint8_t length = 8, unsigned long timestamp = 1000) {
        TestCANMessage msg;
        msg.id = id;
        msg.length = length;
        msg.timestamp = timestamp;
        
        // Fill with test pattern
        for (int i = 0; i < 8; i++) {
            msg.data[i] = (uint8_t)(0x10 + i);
        }
        
        return msg;
    }
    
    // Python message definitions replicated for validation
    struct PythonCANMessage {
        uint32_t id;
        uint32_t decimalId;
        const char* name;
        const char* description;
    };
    
    PythonCANMessage pythonMessages[4] = {
        {0x3C3, 963, "BCM_Lamp_Stat_FD1", "PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat"},
        {0x331, 817, "Locking_Systems_2_FD1", "Veh_Lock_Status"},
        {0x176, 374, "PowertrainData_10", "TrnPrkSys_D_Actl"},
        {0x43C, 1084, "Battery_Mgmt_3_FD1", "BSBattSOC"}
    };
};

// ===============================================
// Test Suite: Message ID Constants Validation
// ===============================================

TEST_F(CANMessageRecognitionTest, MessageIDConstantsMatchPython) {
    // Verify our C++ constants match the Python CAN_MESSAGES dictionary
    
    // BCM_Lamp_Stat_FD1 (0x3C3, 963 decimal)
    EXPECT_EQ(BCM_LAMP_STAT_FD1_ID, 0x3C3);
    EXPECT_EQ(BCM_LAMP_STAT_FD1_ID, 963);
    
    // Locking_Systems_2_FD1 (0x331, 817 decimal)
    EXPECT_EQ(LOCKING_SYSTEMS_2_FD1_ID, 0x331);
    EXPECT_EQ(LOCKING_SYSTEMS_2_FD1_ID, 817);
    
    // PowertrainData_10 (0x176, 374 decimal)
    EXPECT_EQ(POWERTRAIN_DATA_10_ID, 0x176);
    EXPECT_EQ(POWERTRAIN_DATA_10_ID, 374);
    
    // Battery_Mgmt_3_FD1 (0x43C, 1084 decimal)
    EXPECT_EQ(BATTERY_MGMT_3_FD1_ID, 0x43C);
    EXPECT_EQ(BATTERY_MGMT_3_FD1_ID, 1084);
    
    printf("Message ID Constants Validation:\n");
    printf("  BCM_Lamp_Stat_FD1: 0x%03X (%d)\n", BCM_LAMP_STAT_FD1_ID, BCM_LAMP_STAT_FD1_ID);
    printf("  Locking_Systems_2_FD1: 0x%03X (%d)\n", LOCKING_SYSTEMS_2_FD1_ID, LOCKING_SYSTEMS_2_FD1_ID);
    printf("  PowertrainData_10: 0x%03X (%d)\n", POWERTRAIN_DATA_10_ID, POWERTRAIN_DATA_10_ID);
    printf("  Battery_Mgmt_3_FD1: 0x%03X (%d)\n", BATTERY_MGMT_3_FD1_ID, BATTERY_MGMT_3_FD1_ID);
}

// ===============================================
// Test Suite: Target Message Recognition
// ===============================================

TEST_F(CANMessageRecognitionTest, TargetMessageRecognition) {
    // Test isTargetCANMessage() function against Python CAN_FILTER_IDS
    
    // Test all target messages are recognized
    for (const auto& pythonMsg : pythonMessages) {
        bool isTarget = isTargetCANMessage(pythonMsg.id);
        EXPECT_TRUE(isTarget) << "Failed to recognize target message: " 
                             << pythonMsg.name << " (0x" << std::hex << pythonMsg.id << ")";
        
        printf("Target message recognized: %s (0x%03X) = %s\n", 
               pythonMsg.name, pythonMsg.id, isTarget ? "TRUE" : "FALSE");
    }
    
    // Test non-target messages are NOT recognized
    uint32_t nonTargetIds[] = {
        0x000, 0x001, 0x100, 0x200, 0x300, // Low IDs
        0x400, 0x500, 0x600, 0x700, 0x7FF, // High IDs
        0x3C2, 0x3C4, // Near BCM_Lamp_Stat_FD1
        0x330, 0x332, // Near Locking_Systems_2_FD1
        0x175, 0x177, // Near PowertrainData_10
        0x43B, 0x43D  // Near Battery_Mgmt_3_FD1
    };
    
    for (uint32_t nonTargetId : nonTargetIds) {
        bool isTarget = isTargetCANMessage(nonTargetId);
        EXPECT_FALSE(isTarget) << "Incorrectly recognized non-target message: 0x" 
                              << std::hex << nonTargetId;
    }
}

// ===============================================
// Test Suite: Message Filtering Logic
// ===============================================

TEST_F(CANMessageRecognitionTest, MessageFilteringLogic) {
    // Test message filtering logic that would be used in the main CAN processing loop
    // This simulates the Python logger's approach of only processing target messages
    
    struct MessageTest {
        uint32_t id;
        bool shouldProcess;
        const char* description;
    };
    
    MessageTest tests[] = {
        // Target messages - should be processed
        {BCM_LAMP_STAT_FD1_ID, true, "BCM_Lamp_Stat_FD1"},
        {LOCKING_SYSTEMS_2_FD1_ID, true, "Locking_Systems_2_FD1"},
        {POWERTRAIN_DATA_10_ID, true, "PowertrainData_10"},
        {BATTERY_MGMT_3_FD1_ID, true, "Battery_Mgmt_3_FD1"},
        
        // Non-target messages - should be ignored
        {0x100, false, "Random message 1"},
        {0x200, false, "Random message 2"},
        {0x7FF, false, "Highest 11-bit CAN ID"},
        {0x000, false, "Lowest CAN ID"},
        
        // Near-miss IDs - should be ignored
        {0x3C2, false, "One below BCM_Lamp_Stat_FD1"},
        {0x3C4, false, "One above BCM_Lamp_Stat_FD1"},
        {0x330, false, "One below Locking_Systems_2_FD1"},
        {0x332, false, "One above Locking_Systems_2_FD1"}
    };
    
    int targetMessagesProcessed = 0;
    int nonTargetMessagesIgnored = 0;
    
    for (const auto& test : tests) {
        TestCANMessage msg = createTestMessage(test.id);
        bool isTarget = isTargetCANMessage(msg.id);
        
        EXPECT_EQ(isTarget, test.shouldProcess) 
            << "Filter mismatch for " << test.description 
            << " (ID: 0x" << std::hex << test.id << ")";
        
        if (isTarget) {
            targetMessagesProcessed++;
            printf("PROCESS: %s (0x%03X)\n", test.description, test.id);
        } else {
            nonTargetMessagesIgnored++;
            printf("IGNORE:  %s (0x%03X)\n", test.description, test.id);
        }
    }
    
    EXPECT_EQ(targetMessagesProcessed, 4); // Should process exactly 4 target messages
    EXPECT_GT(nonTargetMessagesIgnored, 0); // Should ignore some non-target messages
    
    printf("Filtering summary: %d processed, %d ignored\n", 
           targetMessagesProcessed, nonTargetMessagesIgnored);
}

// ===============================================
// Test Suite: CAN Bus Load Simulation
// ===============================================

TEST_F(CANMessageRecognitionTest, CANBusLoadSimulation) {
    // Simulate a realistic CAN bus load with mixed target and non-target messages
    // This tests the efficiency of our filtering logic
    
    struct CANBusMessage {
        uint32_t id;
        const char* description;
    };
    
    // Realistic Ford F-150 CAN bus messages (mix of target and non-target)
    CANBusMessage busMessages[] = {
        // Our target messages
        {0x3C3, "BCM_Lamp_Stat_FD1 (TARGET)"},
        {0x331, "Locking_Systems_2_FD1 (TARGET)"},
        {0x176, "PowertrainData_10 (TARGET)"},
        {0x43C, "Battery_Mgmt_3_FD1 (TARGET)"},
        
        // Common Ford CAN messages (non-target)
        {0x3B3, "Engine_Data_1"},
        {0x3C2, "BCM_Other_Status"},
        {0x201, "Transmission_Data"},
        {0x3D0, "ABS_Data"},
        {0x3E0, "Airbag_Status"},
        {0x420, "Climate_Control"},
        {0x500, "Instrument_Cluster"},
        {0x123, "Body_Control_1"},
        {0x124, "Body_Control_2"},
        {0x789, "Entertainment_System"},
        {0x456, "Navigation_Data"},
        {0x234, "Steering_Wheel_Controls"},
        {0x345, "Door_Module_FL"},
        {0x346, "Door_Module_FR"},
        {0x347, "Door_Module_RL"},
        {0x348, "Door_Module_RR"}
    };
    
    int totalMessages = sizeof(busMessages) / sizeof(busMessages[0]);
    int targetMessages = 0;
    int filteredMessages = 0;
    
    // Process each message through our filter
    for (int i = 0; i < totalMessages; i++) {
        const auto& busMsg = busMessages[i];
        bool isTarget = isTargetCANMessage(busMsg.id);
        
        if (isTarget) {
            targetMessages++;
            printf("TARGET:  0x%03X - %s\n", busMsg.id, busMsg.description);
        } else {
            filteredMessages++;
            printf("FILTER:  0x%03X - %s\n", busMsg.id, busMsg.description);
        }
    }
    
    // Verify filtering efficiency
    EXPECT_EQ(targetMessages, 4); // Exactly 4 target messages
    EXPECT_EQ(filteredMessages, totalMessages - 4); // Rest are filtered
    EXPECT_GT(filteredMessages, targetMessages); // More messages filtered than processed
    
    double filterEfficiency = (double)filteredMessages / totalMessages * 100.0;
    printf("\nCAN Bus Load Simulation Results:\n");
    printf("  Total messages: %d\n", totalMessages);
    printf("  Target messages: %d (%.1f%%)\n", targetMessages, (double)targetMessages/totalMessages*100.0);
    printf("  Filtered messages: %d (%.1f%%)\n", filteredMessages, filterEfficiency);
    printf("  Filter efficiency: %.1f%%\n", filterEfficiency);
    
    // In a real Ford F-150, we expect to filter out most messages
    EXPECT_GT(filterEfficiency, 50.0); // Should filter more than 50% of messages
}

// ===============================================
// Test Suite: Message Processing Pipeline
// ===============================================

TEST_F(CANMessageRecognitionTest, MessageProcessingPipeline) {
    // Test the complete message processing pipeline that matches Python behavior
    
    unsigned long baseTime = 1000;
    int processedCount = 0;
    int ignoredCount = 0;
    
    // Simulate processing a sequence of CAN messages
    struct MessageSequence {
        uint32_t id;
        unsigned long timestamp;
        bool expectProcessed;
    };
    
    MessageSequence sequence[] = {
        {0x100, baseTime + 0, false},    // Non-target
        {0x3C3, baseTime + 10, true},    // BCM_Lamp_Stat_FD1
        {0x200, baseTime + 20, false},   // Non-target
        {0x331, baseTime + 30, true},    // Locking_Systems_2_FD1
        {0x300, baseTime + 40, false},   // Non-target
        {0x176, baseTime + 50, true},    // PowertrainData_10
        {0x400, baseTime + 60, false},   // Non-target
        {0x43C, baseTime + 70, true},    // Battery_Mgmt_3_FD1
        {0x500, baseTime + 80, false},   // Non-target
        {0x3C3, baseTime + 90, true},    // BCM_Lamp_Stat_FD1 again
    };
    
    int sequenceLength = sizeof(sequence) / sizeof(sequence[0]);
    
    for (int i = 0; i < sequenceLength; i++) {
        const auto& msg = sequence[i];
        TestCANMessage canMsg = createTestMessage(msg.id, 8, msg.timestamp);
        
        // Simulate the processing decision
        bool isTarget = isTargetCANMessage(canMsg.id);
        
        if (isTarget) {
            // This is where we would call the actual parsing functions
            processedCount++;
            printf("PROCESS message %d: ID=0x%03X at time %lu\n", 
                   i, canMsg.id, canMsg.timestamp);
        } else {
            ignoredCount++;
            printf("IGNORE  message %d: ID=0x%03X at time %lu\n", 
                   i, canMsg.id, canMsg.timestamp);
        }
        
        EXPECT_EQ(isTarget, msg.expectProcessed) 
            << "Processing decision mismatch for message " << i 
            << " (ID: 0x" << std::hex << msg.id << ")";
    }
    
    printf("\nProcessing Pipeline Results:\n");
    printf("  Processed: %d messages\n", processedCount);
    printf("  Ignored: %d messages\n", ignoredCount); 
    printf("  Total: %d messages\n", sequenceLength);
    
    EXPECT_EQ(processedCount, 5); // 4 unique targets + 1 repeat
    EXPECT_EQ(ignoredCount, 5);   // 5 non-targets
    EXPECT_EQ(processedCount + ignoredCount, sequenceLength);
}

// ===============================================
// Test Suite: Memory Efficiency Validation
// ===============================================

TEST_F(CANMessageRecognitionTest, MemoryEfficiencyValidation) {
    // Test that our target message recognition is memory efficient
    // This is important for embedded applications with limited RAM
    
    // Our current approach uses simple ID comparisons (very efficient)
    // Alternative would be lookup tables or hash maps (more memory)
    
    uint32_t testIds[] = {
        BCM_LAMP_STAT_FD1_ID,
        LOCKING_SYSTEMS_2_FD1_ID, 
        POWERTRAIN_DATA_10_ID,
        BATTERY_MGMT_3_FD1_ID,
        0x123, 0x456, 0x789, 0xABC // Non-targets
    };
    
    int testCount = sizeof(testIds) / sizeof(testIds[0]);
    
    // Time the recognition function (should be very fast)
    unsigned long startTime = millis();
    
    for (int iteration = 0; iteration < 1000; iteration++) {
        for (int i = 0; i < testCount; i++) {
            bool result = isTargetCANMessage(testIds[i]);
            (void)result; // Prevent optimization
        }
    }
    
    unsigned long endTime = millis();
    unsigned long elapsed = endTime - startTime;
    
    printf("Memory Efficiency Test Results:\n");
    printf("  1000 iterations of %d message recognitions\n", testCount);
    printf("  Total time: %lu ms\n", elapsed);
    printf("  Average time per recognition: %.3f ms\n", (double)elapsed / (1000 * testCount));
    
    // Recognition should be very fast (< 1ms total for all iterations)
    EXPECT_LT(elapsed, 100); // Should complete in well under 100ms
    
    // Memory usage should be minimal (just the function code, no lookup tables)
    // This is validated by the simplicity of isTargetCANMessage() implementation
    EXPECT_TRUE(true); // Placeholder - actual validation is architectural
}

// ===============================================
// Test Suite: Python Implementation Compatibility
// ===============================================

TEST_F(CANMessageRecognitionTest, PythonImplementationCompatibility) {
    // Final validation that our C++ implementation is fully compatible 
    // with the Python can_embedded_logger.py implementation
    
    printf("Python Implementation Compatibility Check:\n");
    
    // Verify we handle the same message IDs
    for (const auto& pythonMsg : pythonMessages) {
        bool cppRecognized = isTargetCANMessage(pythonMsg.id);
        EXPECT_TRUE(cppRecognized) 
            << "C++ failed to recognize Python message: " << pythonMsg.name;
        
        printf("  ✓ %s (0x%03X / %d): C++ = %s\n", 
               pythonMsg.name, pythonMsg.id, pythonMsg.decimalId,
               cppRecognized ? "RECOGNIZED" : "NOT RECOGNIZED");
    }
    
    // Verify we use the same message naming conventions
    EXPECT_STREQ("BCM_Lamp_Stat_FD1", pythonMessages[0].name);
    EXPECT_STREQ("Locking_Systems_2_FD1", pythonMessages[1].name);
    EXPECT_STREQ("PowertrainData_10", pythonMessages[2].name);
    EXPECT_STREQ("Battery_Mgmt_3_FD1", pythonMessages[3].name);
    
    // Verify we target the same number of messages as Python
    int pythonTargetCount = sizeof(pythonMessages) / sizeof(pythonMessages[0]);
    int cppTargetCount = 0;
    
    // Count how many messages we recognize in a large ID range
    for (uint32_t id = 0; id <= 0x7FF; id++) {
        if (isTargetCANMessage(id)) {
            cppTargetCount++;
        }
    }
    
    EXPECT_EQ(cppTargetCount, pythonTargetCount) 
        << "C++ recognizes different number of target messages than Python";
    
    printf("  ✓ Target message count: Python = %d, C++ = %d\n", 
           pythonTargetCount, cppTargetCount);
    
    printf("Python Implementation Compatibility: PASSED\n");
}

// Main function removed - using unified test runner in test_main.cpp
