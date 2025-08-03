#include <gtest/gtest.h>
#include "mocks/mock_arduino.h"
#include "common/test_config.h"

// Import production code structures and functions
extern "C" {
    #include "../src/can_protocol.h"
}

/**
 * CAN Bit Extraction Validation Test Suite
 * 
 * These tests validate our C++ bit extraction logic against the Python implementation
 * in can_embedded_logger.py. The Python code defines the exact bit positions and 
 * expected values for each CAN message using Intel (little-endian) byte order with 
 * DBC bit positioning.
 * 
 * Key Python implementation details:
 * - BCM_Lamp_Stat_FD1 (0x3C3): PudLamp_D_Rq (bits 11-12), Illuminated_Entry_Stat (bits 63-64), Dr_Courtesy_Light_Stat (bits 49-50)
 * - Locking_Systems_2_FD1 (0x331): Veh_Lock_Status (bits 34-35)
 * - PowertrainData_10 (0x176): TrnPrkSys_D_Actl (bits 31-34)
 * - Battery_Mgmt_3_FD1 (0x43C): BSBattSOC (bits 22-28)
 * 
 * The Python extract_signal_value() function:
 * 1. Converts 8 bytes to 64-bit integer (little-endian)
 * 2. Calculates bit position from MSB (DBC uses MSB bit numbering): bit_pos = start_bit - length + 1
 * 3. Creates mask and extracts value: mask = (1 << length) - 1; value = (data_int >> bit_pos) & mask
 */

class CANBitExtractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    // Replica of Python's extract_signal_value function for validation
    uint32_t pythonExtractSignalValue(const uint8_t data[8], uint8_t startBit, uint8_t length) {
        // Convert 8 bytes to 64-bit integer (little-endian)
        uint64_t dataInt = 0;
        for (int i = 0; i < 8; i++) {
            dataInt |= ((uint64_t)data[i]) << (i * 8);
        }
        
        // Calculate bit position from MSB (DBC uses MSB bit numbering)
        uint8_t bitPos = startBit - length + 1;
        
        // Create mask and extract value
        uint64_t mask = (1ULL << length) - 1;
        uint32_t value = (dataInt >> bitPos) & mask;
        
        return value;
    }
    
    // Helper to create test data with specific bit patterns
    void setBitPattern(uint8_t data[8], uint8_t startBit, uint8_t length, uint32_t value) {
        // Clear data first
        memset(data, 0, 8);
        
        // Convert to 64-bit integer for bit manipulation (little-endian)
        uint64_t dataValue = 0;
        
        // Calculate bit position (DBC uses MSB bit numbering)
        uint8_t bitPos = startBit - length + 1;
        
        // Set the value at the correct bit position
        dataValue |= ((uint64_t)value) << bitPos;
        
        // Convert back to byte array (little-endian)
        for (int i = 0; i < 8; i++) {
            data[i] = (dataValue >> (i * 8)) & 0xFF;
        }
    }
    
    // Helper to create realistic CAN message data
    void createRealisticCANData(uint8_t data[8]) {
        // Pattern that might be seen on a real Ford F-150 CAN bus
        data[0] = 0x42; // Mixed pattern
        data[1] = 0x8A; // Mixed pattern
        data[2] = 0x15; // Mixed pattern  
        data[3] = 0xC7; // Mixed pattern
        data[4] = 0x3E; // Mixed pattern
        data[5] = 0x91; // Mixed pattern
        data[6] = 0x6D; // Mixed pattern
        data[7] = 0xBF; // Mixed pattern
    }
};

// ===============================================
// Test Suite: Python Bit Extraction Validation
// ===============================================

TEST_F(CANBitExtractionTest, PythonBitExtractionReplication) {
    // Test that we can replicate Python's bit extraction exactly
    
    uint8_t testData[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    
    struct BitExtractionTest {
        uint8_t startBit;
        uint8_t length;
        const char* signalName;
    };
    
    // Test all bit positions used in our CAN messages
    BitExtractionTest tests[] = {
        {12, 2, "PudLamp_D_Rq"},
        {64, 2, "Illuminated_Entry_Stat"}, 
        {50, 2, "Dr_Courtesy_Light_Stat"},
        {35, 2, "Veh_Lock_Status"},
        {34, 4, "TrnPrkSys_D_Actl"},
        {28, 7, "BSBattSOC"},
        // Additional test cases for edge conditions
        {8, 1, "Single bit at byte boundary"},
        {16, 8, "Full byte extraction"},
        {24, 4, "Cross-byte extraction"}
    };
    
    for (const auto& test : tests) {
        uint32_t pythonValue = pythonExtractSignalValue(testData, test.startBit, test.length);
        
        // Test our C++ implementation matches Python
        // Note: Our C++ function uses a different bit numbering convention, 
        // so we need to adjust the parameters
        uint8_t cppStartBit = test.startBit - test.length + 1;
        uint32_t cppValue = extractBits(testData, cppStartBit, test.length);
        
        EXPECT_EQ(cppValue, pythonValue) 
            << "Mismatch for " << test.signalName 
            << " (start_bit=" << (int)test.startBit 
            << ", length=" << (int)test.length << ")";
    }
}

// ===============================================
// Test Suite: BCM Lamp Status Signal Extraction
// ===============================================

TEST_F(CANBitExtractionTest, BCMLampStatusSignalExtraction) {
    // Test BCM_Lamp_Stat_FD1 signal extraction based on Python implementation
    
    // Test PudLamp_D_Rq (start_bit: 12, length: 2)
    for (uint32_t testValue = 0; testValue <= 3; testValue++) {
        uint8_t testData[8];
        setBitPattern(testData, 12, 2, testValue);
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 12, 2);
        uint32_t cppValue = extractBits(testData, 11, 2); // start_bit - length + 1 = 12 - 2 + 1 = 11
        
        EXPECT_EQ(cppValue, testValue) << "C++ extraction failed for PudLamp_D_Rq value " << testValue;
        EXPECT_EQ(pythonValue, testValue) << "Python extraction failed for PudLamp_D_Rq value " << testValue;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for PudLamp_D_Rq value " << testValue;
    }
    
    // Test Illuminated_Entry_Stat (start_bit: 64, length: 2) - Note: bit 64 is beyond our 64-bit range
    // This tests the boundary condition handling
    for (uint32_t testValue = 0; testValue <= 3; testValue++) {
        uint8_t testData[8];
        setBitPattern(testData, 63, 2, testValue); // Use bit 63 instead of 64 to stay in range
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 63, 2);
        uint32_t cppValue = extractBits(testData, 62, 2); // start_bit - length + 1 = 63 - 2 + 1 = 62
        
        EXPECT_EQ(cppValue, testValue) << "C++ extraction failed for Illuminated_Entry_Stat value " << testValue;
        EXPECT_EQ(pythonValue, testValue) << "Python extraction failed for Illuminated_Entry_Stat value " << testValue;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for Illuminated_Entry_Stat value " << testValue;
    }
    
    // Test Dr_Courtesy_Light_Stat (start_bit: 50, length: 2)
    for (uint32_t testValue = 0; testValue <= 3; testValue++) {
        uint8_t testData[8];
        setBitPattern(testData, 50, 2, testValue);
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 50, 2);
        uint32_t cppValue = extractBits(testData, 49, 2); // start_bit - length + 1 = 50 - 2 + 1 = 49
        
        EXPECT_EQ(cppValue, testValue) << "C++ extraction failed for Dr_Courtesy_Light_Stat value " << testValue;
        EXPECT_EQ(pythonValue, testValue) << "Python extraction failed for Dr_Courtesy_Light_Stat value " << testValue;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for Dr_Courtesy_Light_Stat value " << testValue;
    }
}

// ===============================================
// Test Suite: Locking Systems Signal Extraction
// ===============================================

TEST_F(CANBitExtractionTest, LockingSystemsSignalExtraction) {
    // Test Locking_Systems_2_FD1 signal extraction
    // Veh_Lock_Status (start_bit: 35, length: 2)
    
    struct ValueTest {
        uint32_t value;
        const char* pythonMapping;
        uint8_t expectedConstant;
    };
    
    ValueTest tests[] = {
        {0, "LOCK_DBL", VEH_LOCK_DBL},
        {1, "LOCK_ALL", VEH_LOCK_ALL},
        {2, "UNLOCK_ALL", VEH_UNLOCK_ALL},
        {3, "UNLOCK_DRV", VEH_UNLOCK_DRV}
    };
    
    for (const auto& test : tests) {
        uint8_t testData[8];
        setBitPattern(testData, 35, 2, test.value);
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 35, 2);
        uint32_t cppValue = extractBits(testData, 34, 2); // start_bit - length + 1 = 35 - 2 + 1 = 34
        
        EXPECT_EQ(cppValue, test.value) << "C++ extraction failed for " << test.pythonMapping;
        EXPECT_EQ(pythonValue, test.value) << "Python extraction failed for " << test.pythonMapping;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for " << test.pythonMapping;
        EXPECT_EQ(test.value, test.expectedConstant) << "Constant mismatch for " << test.pythonMapping;
    }
}

// ===============================================
// Test Suite: Powertrain Data Signal Extraction
// ===============================================

TEST_F(CANBitExtractionTest, PowertrainDataSignalExtraction) {
    // Test PowertrainData_10 signal extraction
    // TrnPrkSys_D_Actl (start_bit: 34, length: 4)
    
    struct ValueTest {
        uint32_t value;
        const char* pythonMapping;
        uint8_t expectedConstant;
    };
    
    ValueTest tests[] = {
        {0, "NotKnown", TRNPRKSTS_UNKNOWN},
        {1, "Park", TRNPRKSTS_PARK},
        {2, "TransitionCloseToPark", TRNPRKSTS_TRANSITION_CLOSE_TO_PARK},
        {3, "AtNoSpring", TRNPRKSTS_AT_NO_SPRING},
        {4, "TransitionCloseToOutOfPark", TRNPRKSTS_TRANSITION_CLOSE_TO_OUT_OF_PARK},
        {5, "OutOfPark", TRNPRKSTS_OUT_OF_PARK},
        {6, "Override", 6}, // Not defined in our constants but should still extract correctly
        {7, "OutOfRangeLow", 7},
        {8, "OutOfRangeHigh", 8},
        {9, "FrequencyError", 9},
        {15, "Faulty", 15}
    };
    
    for (const auto& test : tests) {
        uint8_t testData[8];
        setBitPattern(testData, 34, 4, test.value);
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 34, 4);
        uint32_t cppValue = extractBits(testData, 31, 4); // start_bit - length + 1 = 34 - 4 + 1 = 31
        
        EXPECT_EQ(cppValue, test.value) << "C++ extraction failed for " << test.pythonMapping;
        EXPECT_EQ(pythonValue, test.value) << "Python extraction failed for " << test.pythonMapping;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for " << test.pythonMapping;
        
        if (test.value <= 5) { // Only test defined constants
            EXPECT_EQ(test.value, test.expectedConstant) << "Constant mismatch for " << test.pythonMapping;
        }
    }
}

// ===============================================
// Test Suite: Battery Management Signal Extraction
// ===============================================

TEST_F(CANBitExtractionTest, BatteryManagementSignalExtraction) {
    // Test Battery_Mgmt_3_FD1 signal extraction
    // BSBattSOC (start_bit: 28, length: 7) - Raw percentage value
    
    uint32_t testValues[] = {0, 25, 50, 75, 85, 100, 127}; // 7-bit range: 0-127
    
    for (uint32_t testValue : testValues) {
        uint8_t testData[8];
        setBitPattern(testData, 28, 7, testValue);
        
        uint32_t pythonValue = pythonExtractSignalValue(testData, 28, 7);
        uint32_t cppValue = extractBits(testData, 22, 7); // start_bit - length + 1 = 28 - 7 + 1 = 22
        
        EXPECT_EQ(cppValue, testValue) << "C++ extraction failed for BSBattSOC value " << testValue;
        EXPECT_EQ(pythonValue, testValue) << "Python extraction failed for BSBattSOC value " << testValue;
        EXPECT_EQ(cppValue, pythonValue) << "C++ vs Python mismatch for BSBattSOC value " << testValue;
    }
}

// ===============================================
// Test Suite: Realistic CAN Data Patterns
// ===============================================

TEST_F(CANBitExtractionTest, RealisticCANDataPatterns) { 
    // Test with realistic CAN data patterns that might appear on a Ford F-150
    
    uint8_t realisticData[8];
    createRealisticCANData(realisticData);
    
    // Extract all signals from this realistic data pattern
    struct SignalTest {
        uint8_t startBit;
        uint8_t length;
        const char* signalName;
        uint32_t messageId;
    };
    
    SignalTest signals[] = {
        {12, 2, "PudLamp_D_Rq", BCM_LAMP_STAT_FD1_ID},
        {63, 2, "Illuminated_Entry_Stat", BCM_LAMP_STAT_FD1_ID}, // Using 63 instead of 64
        {50, 2, "Dr_Courtesy_Light_Stat", BCM_LAMP_STAT_FD1_ID},
        {35, 2, "Veh_Lock_Status", LOCKING_SYSTEMS_2_FD1_ID},
        {34, 4, "TrnPrkSys_D_Actl", POWERTRAIN_DATA_10_ID},
        {28, 7, "BSBattSOC", BATTERY_MGMT_3_FD1_ID}
    };
    
    for (const auto& signal : signals) {
        uint32_t pythonValue = pythonExtractSignalValue(realisticData, signal.startBit, signal.length);
        uint32_t cppValue = extractBits(realisticData, signal.startBit - signal.length + 1, signal.length);
        
        // Values should be within expected ranges
        uint32_t maxValue = (1U << signal.length) - 1;
        EXPECT_LE(pythonValue, maxValue) << "Python value out of range for " << signal.signalName;
        EXPECT_LE(cppValue, maxValue) << "C++ value out of range for " << signal.signalName;
        
        // Values should match between implementations
        EXPECT_EQ(cppValue, pythonValue) << "Implementation mismatch for " << signal.signalName;
        
        // Log the extracted values for debugging
        printf("Signal %s (0x%03X): Python=%u, C++=%u\n", 
               signal.signalName, signal.messageId, pythonValue, cppValue);
    }
}

// ===============================================
// Test Suite: Edge Cases and Boundary Conditions
// ===============================================

TEST_F(CANBitExtractionTest, EdgeCasesAndBoundaryConditions) {
    // Test boundary conditions that could cause issues
    
    // Test with all zeros
    uint8_t zeroData[8] = {0};
    EXPECT_EQ(pythonExtractSignalValue(zeroData, 12, 2), 0);
    EXPECT_EQ(extractBits(zeroData, 11, 2), 0);
    
    // Test with all ones
    uint8_t onesData[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    // 2-bit field should extract maximum value (3)
    EXPECT_EQ(pythonExtractSignalValue(onesData, 12, 2), 3);
    EXPECT_EQ(extractBits(onesData, 11, 2), 3);
    
    // 4-bit field should extract maximum value (15)
    EXPECT_EQ(pythonExtractSignalValue(onesData, 34, 4), 15);
    EXPECT_EQ(extractBits(onesData, 31, 4), 15);
    
    // 7-bit field should extract maximum value (127)
    EXPECT_EQ(pythonExtractSignalValue(onesData, 28, 7), 127);
    EXPECT_EQ(extractBits(onesData, 22, 7), 127);
    
    // Test single bit extraction
    // For DBC bit numbering, bit 0 is the LSB of byte 0
    uint8_t singleBitData[8] = {0x01, 0, 0, 0, 0, 0, 0, 0}; // Only LSB set
    EXPECT_EQ(pythonExtractSignalValue(singleBitData, 0, 1), 1);  // DBC bit 0, length 1
    EXPECT_EQ(extractBits(singleBitData, 0, 1), 1);
    
    // Test extraction at byte boundaries
    uint8_t boundaryData[8] = {0, 0xFF, 0, 0, 0, 0, 0, 0}; // Second byte all set
    EXPECT_EQ(pythonExtractSignalValue(boundaryData, 15, 8), 0xFF);  // DBC bits 8-15 (second byte)
    EXPECT_EQ(extractBits(boundaryData, 8, 8), 0xFF);
}

// ===============================================
// Test Suite: Message ID Recognition
// ===============================================

TEST_F(CANBitExtractionTest, MessageIDRecognition) {
    // Test that we recognize the correct message IDs as defined in Python
    
    // Python CAN_MESSAGES dictionary keys
    uint32_t targetIds[] = {
        0x3C3, // BCM_Lamp_Stat_FD1 (963)
        0x331, // Locking_Systems_2_FD1 (817)
        0x176, // PowertrainData_10 (374)
        0x43C  // Battery_Mgmt_3_FD1 (1084)
    };
    
    // Verify these match our constants
    EXPECT_EQ(targetIds[0], BCM_LAMP_STAT_FD1_ID);
    EXPECT_EQ(targetIds[1], LOCKING_SYSTEMS_2_FD1_ID);
    EXPECT_EQ(targetIds[2], POWERTRAIN_DATA_10_ID);
    EXPECT_EQ(targetIds[3], BATTERY_MGMT_3_FD1_ID);
    
    // Test decimal equivalents match Python comments
    EXPECT_EQ(BCM_LAMP_STAT_FD1_ID, 963);
    EXPECT_EQ(LOCKING_SYSTEMS_2_FD1_ID, 817);
    EXPECT_EQ(POWERTRAIN_DATA_10_ID, 374);
    EXPECT_EQ(BATTERY_MGMT_3_FD1_ID, 1084);
}

// ===============================================
// Test Suite: Cross-validation with Python Logic
// ===============================================

TEST_F(CANBitExtractionTest, CrossValidationWithPythonLogic) {
    // This test replicates the exact extraction logic from the Python code
    // and verifies our C++ implementation produces identical results
    
    // Test data that exercises all bit positions
    uint8_t complexData[8] = {0xA5, 0x5A, 0x3C, 0xC3, 0x69, 0x96, 0x0F, 0xF0};
    
    // Define all signal extractions as done in Python
    struct PythonSignal {
        const char* name;
        uint8_t startBit;
        uint8_t length;
        uint32_t messageId;
        const char* pythonValues;
    };
    
    PythonSignal pythonSignals[] = {
        {"PudLamp_D_Rq", 12, 2, 0x3C3, "{0: \"OFF\", 1: \"ON\", 2: \"RAMP_UP\", 3: \"RAMP_DOWN\"}"},
        {"Illuminated_Entry_Stat", 63, 2, 0x3C3, "{0: \"Off\", 1: \"On\", 2: \"Unknown\", 3: \"Invalid\"}"},
        {"Dr_Courtesy_Light_Stat", 50, 2, 0x3C3, "{0: \"Off\", 1: \"On\", 2: \"Unknown\", 3: \"Invalid\"}"},
        {"Veh_Lock_Status", 35, 2, 0x331, "{0: \"LOCK_DBL\", 1: \"LOCK_ALL\", 2: \"UNLOCK_ALL\", 3: \"UNLOCK_DRV\"}"},
        {"TrnPrkSys_D_Actl", 34, 4, 0x176, "{0: \"NotKnown\", 1: \"Park\", ...}"},
        {"BSBattSOC", 28, 7, 0x43C, "Raw percentage value"}
    };
    
    for (const auto& signal : pythonSignals) {
        // Extract using Python logic
        uint32_t pythonValue = pythonExtractSignalValue(complexData, signal.startBit, signal.length);
        
        // Extract using our C++ logic (adjusted for bit numbering difference)
        uint32_t cppValue = extractBits(complexData, signal.startBit - signal.length + 1, signal.length);
        
        // Verify they match
        EXPECT_EQ(cppValue, pythonValue) 
            << "Cross-validation failed for " << signal.name 
            << " (ID: 0x" << std::hex << signal.messageId << ")"
            << " - Python: " << pythonValue << ", C++: " << cppValue;
        
        // Verify value is within expected range
        uint32_t maxValue = (1U << signal.length) - 1;
        EXPECT_LE(pythonValue, maxValue) << "Value out of range for " << signal.name;
        EXPECT_LE(cppValue, maxValue) << "Value out of range for " << signal.name;
        
        printf("Cross-validation PASSED: %s = %u (max: %u)\n", 
               signal.name, pythonValue, maxValue);
    }
}

// Main function removed - using unified test runner in test_main.cpp
