#include <gtest/gtest.h>
#include "mocks/mock_arduino.h"
#include "common/test_config.h"

// Import production code structures and functions
extern "C" {
    #include "../src/bit_utils.h"
    #include "../src/can_protocol.h"
}

/**
 * Bit Position Analysis Test Suite
 * 
 * This test analyzes the real CAN data to determine the correct bit positions
 * for each signal by looking at the pattern differences between messages.
 */

class BitPositionAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    // Helper to convert hex string to bytes
    void hexStringToBytes(const char* hexStr, uint8_t data[8]) {
        const char* hex = hexStr;
        for (int i = 0; i < 8; i++) {
            while (*hex == ' ') hex++;
            if (*hex && *(hex+1)) {
                char byteStr[3] = {hex[0], hex[1], '\0'};
                data[i] = (uint8_t)strtol(byteStr, NULL, 16);
                hex += 2;
            } else {
                data[i] = 0;
            }
        }
    }
    
    // Helper to print binary representation of data
    void printBinary(const uint8_t data[8], const char* label) {
        printf("%s:\n", label);
        for (int i = 0; i < 8; i++) {
            printf("  Byte %d: ", i);
            for (int bit = 7; bit >= 0; bit--) {
                printf("%c", (data[i] & (1 << bit)) ? '1' : '0');
            }
            printf(" (0x%02X)\n", data[i]);
        }
        printf("\n");
    }
    
    // Helper to try extracting at various bit positions
    void analyzeSignalPosition(const char* signalName, 
                              const char* data1Hex, const char* data2Hex,
                              const char* expectedValue1, const char* expectedValue2) {
        uint8_t data1[8], data2[8];
        hexStringToBytes(data1Hex, data1);
        hexStringToBytes(data2Hex, data2);
        
        printf("=== Analyzing %s ===\n", signalName);
        printf("Data 1: %s (expected: %s)\n", data1Hex, expectedValue1);
        printf("Data 2: %s (expected: %s)\n", data2Hex, expectedValue2);
        
        printBinary(data1, "Data 1 Binary");
        printBinary(data2, "Data 2 Binary");
        
        // Try different bit positions and lengths
        printf("Trying different bit positions:\n");
        for (uint8_t startBit = 0; startBit < 64; startBit++) {
            for (uint8_t length = 1; length <= 8; length++) {
                if (startBit + length > 64) continue;
                
                uint8_t val1 = extractBits(data1, startBit, length);
                uint8_t val2 = extractBits(data2, startBit, length);
                
                // Only print if values are different (indicates this might be our signal)
                if (val1 != val2 && val1 > 0 && val2 > 0) {
                    printf("  Bit %2d (len %d): val1=%u, val2=%u\n", startBit, length, val1, val2);
                }
            }
        }
        printf("\n");
    }
};

// Analyze BCM Lamp Status - PudLamp_D_Rq signal
TEST_F(BitPositionAnalysisTest, AnalyzePudLampSignal) {
    // Compare PudLamp=ON vs PudLamp=RAMP_UP patterns
    analyzeSignalPosition(
        "PudLamp_D_Rq (ON vs RAMP_UP)",
        "40 C4 00 00 00 00 81 00", // PudLamp=ON (expected: 1)
        "40 C8 00 00 00 00 80 40", // PudLamp=RAMP_UP (expected: 2)
        "1", "2"
    );
    
    // Compare PudLamp=RAMP_UP vs PudLamp=RAMP_DOWN patterns
    analyzeSignalPosition(
        "PudLamp_D_Rq (RAMP_UP vs RAMP_DOWN)",
        "40 C8 00 00 00 00 81 00", // PudLamp=RAMP_UP (expected: 2)
        "40 CC 00 00 00 00 80 00", // PudLamp=RAMP_DOWN (expected: 3)
        "2", "3"
    );
}

// Analyze Locking Systems - Veh_Lock_Status signal  
TEST_F(BitPositionAnalysisTest, AnalyzeLockStatusSignal) {
    // Using actual data from can_logger_1754515370_locking.out
    
    // Compare LOCK_ALL vs UNLOCK_ALL patterns from the actual log
    analyzeSignalPosition(
        "Veh_Lock_Status (LOCK_ALL vs UNLOCK_ALL from actual log)",
        "00 0F 00 00 02 C7 44 10", // Veh_Lock_Status=LOCK_ALL from log sequence 1
        "00 0F 00 00 05 C2 44 10", // Veh_Lock_Status=UNLOCK_ALL from log sequence 2
        "LOCK_ALL", "UNLOCK_ALL"
    );
    
    // Additional analysis with more patterns from the log
    printf("=== Additional Lock Status Pattern Analysis ===\n");
    
    struct LockPattern {
        const char* data;
        const char* description;
        uint8_t expectedValue; // VEH_LOCK_ALL=1 or VEH_UNLOCK_ALL=2
    };
    
    LockPattern patterns[] = {
        // From actual log file - LOCK_ALL cases  
        {"00 0F 00 00 02 C7 44 10", "LOCK_ALL sequence 1", VEH_LOCK_ALL},
        {"04 0F 00 00 02 C7 44 10", "LOCK_ALL sequence 10", VEH_LOCK_ALL},
        
        // From actual log file - UNLOCK_ALL cases
        {"00 0F 00 00 05 C2 44 10", "UNLOCK_ALL sequence 2", VEH_UNLOCK_ALL},
        {"00 0F 00 00 05 C3 44 10", "UNLOCK_ALL sequence 3", VEH_UNLOCK_ALL},
        {"00 0F 00 00 05 C4 44 10", "UNLOCK_ALL sequence 4", VEH_UNLOCK_ALL},
        {"00 0F 00 00 05 C8 94 10", "UNLOCK_ALL sequence 9", VEH_UNLOCK_ALL}
    };
    
    printf("Analyzing %d patterns from actual CAN log:\n", (int)(sizeof(patterns)/sizeof(patterns[0])));
    
    // Key observation: byte 4 differs (0x02 for LOCK_ALL, 0x05 for UNLOCK_ALL)
    printf("\nKey pattern: byte 4 values\n");
    printf("LOCK_ALL patterns: byte 4 = 0x02\n");
    printf("UNLOCK_ALL patterns: byte 4 = 0x05\n");
    
    // Test bit positions in byte 4 (bits 32-39) to find the encoding
    printf("\nTesting bit positions in byte 4 for lock status:\n");
    
    for (int startBit = 32; startBit <= 39; startBit++) {
        for (int length = 1; length <= 4; length++) {
            bool allCorrect = true;
            printf("Bit %d (len %d): ", startBit, length);
            
            for (const auto& pattern : patterns) {
                uint8_t data[8];
                hexStringToBytes(pattern.data, data);
                uint32_t extracted = extractBits(data, startBit, length);
                
                printf("%u ", extracted);
                
                if (extracted != pattern.expectedValue) {
                    allCorrect = false;
                }
            }
            
            if (allCorrect) {
                printf("✓ PERFECT MATCH FOR ALL PATTERNS!");
            }
            printf("\n");
        }
    }
}

// Analyze Powertrain Data - TrnPrkSys_D_Actl signal
TEST_F(BitPositionAnalysisTest, AnalyzeParkStatusSignal) {
    // Compare different park status patterns
    analyzeSignalPosition(
        "TrnPrkSys_D_Actl (Park status patterns)",
        "00 00 00 10 00 00 00 00", // TrnPrkSys_D_Actl=Park (expected: 1)
        "00 01 FD 10 00 00 00 00", // TrnPrkSys_D_Actl=Park (expected: 1)
        "1", "1"
    );
    
    // Let's look at what's changing in the data
    uint8_t data1[8], data2[8];
    hexStringToBytes("00 00 00 10 00 00 00 00", data1);
    hexStringToBytes("00 01 FD 10 00 00 00 00", data2);
    
    printf("Looking for constant park signal (value should be 1 in both):\n");
    for (uint8_t startBit = 0; startBit < 64; startBit++) {
        for (uint8_t length = 1; length <= 8; length++) {
            if (startBit + length > 64) continue;
            
            uint8_t val1 = extractBits(data1, startBit, length);
            uint8_t val2 = extractBits(data2, startBit, length);
            
            // Look for positions where both values are 1 (indicating park status)
            if (val1 == 1 && val2 == 1) {
                printf("  Bit %2d (len %d): both=1 ✓\n", startBit, length);
            }
        }
    }
}

// Analyze Battery Management - BSBattSOC signal
TEST_F(BitPositionAnalysisTest, AnalyzeBatterySOCSignal) {
    // Compare 65% vs 66% battery levels
    analyzeSignalPosition(
        "BSBattSOC (65% vs 66%)",
        "32 00 41 57 40 D9 88 C8", // BSBattSOC=65 (expected: 65)
        "32 00 42 57 40 D9 88 C8", // BSBattSOC=66 (expected: 66)
        "65", "66"
    );
}

// Comprehensive bit position mapping
TEST_F(BitPositionAnalysisTest, ComprehensiveBitMapping) {
    printf("=== COMPREHENSIVE BIT MAPPING ANALYSIS ===\n");
    
    // Test all known signal patterns
    struct TestCase {
        const char* name;
        const char* data;
        uint8_t expectedValue;
    };
    
    TestCase pudLampCases[] = {
        {"PudLamp_ON_1", "40 C4 00 00 00 00 81 00", 1},
        {"PudLamp_ON_2", "40 C4 00 00 00 00 81 40", 1},
        {"PudLamp_RAMP_UP_1", "40 C8 00 00 00 00 80 40", 2},
        {"PudLamp_RAMP_UP_2", "40 C8 00 00 00 00 81 00", 2},
        {"PudLamp_RAMP_DOWN_1", "40 CC 00 00 00 00 80 00", 3},
        {"PudLamp_RAMP_DOWN_2", "40 CC 01 00 00 00 80 00", 3}
    };
    
    printf("\nAnalyzing PudLamp_D_Rq patterns:\n");
    
    // For each bit position, check if it consistently extracts the right values
    for (uint8_t startBit = 0; startBit < 64; startBit++) {
        for (uint8_t length = 1; length <= 4; length++) {
            if (startBit + length > 64) continue;
            
            bool consistent = true;
            printf("Testing bit %2d (len %d): ", startBit, length);
            
            for (const auto& testCase : pudLampCases) {
                uint8_t data[8];
                hexStringToBytes(testCase.data, data);
                uint8_t extracted = extractBits(data, startBit, length);
                
                printf("%u ", extracted);
                
                if (extracted != testCase.expectedValue) {
                    consistent = false;
                }
            }
            
            if (consistent) {
                printf(" ✓ PERFECT MATCH!");
            }
            printf("\n");
        }
    }
}

// Test the bit ordering differences between DBC MSB and LSB conventions
TEST_F(BitPositionAnalysisTest, BitOrderingConventions) {
    printf("=== BIT ORDERING CONVENTIONS TEST ===\n");
    
    uint8_t testData[8] = {0x40, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00}; // PudLamp=ON
    
    printf("Test data: 40 C4 00 00 00 00 81 00\n");
    printf("Expected PudLamp_D_Rq = 1 (ON)\n\n");
    
    // Show different bit numbering conventions
    printf("Bit numbering conventions:\n");
    printf("MSB (DBC): 63 62 61 ... 2 1 0\n");
    printf("LSB:        0  1  2 ... 61 62 63\n\n");
    
    // Convert to 64-bit and show bit positions
    uint64_t data_int = 0;
    for (int i = 0; i < 8; i++) {
        data_int |= ((uint64_t)testData[i]) << (i * 8);
    }
    
    printf("64-bit value: 0x%016llX\n", data_int);
    printf("Binary (LSB bit 0 on right):\n");
    for (int bit = 63; bit >= 0; bit--) {
        if (bit % 8 == 7) printf(" ");
        printf("%c", (data_int & (1ULL << bit)) ? '1' : '0');
    }
    printf("\n\n");
    
    // Test different extraction approaches
    printf("Testing different bit extraction approaches:\n");
    
    // Approach 1: DBC MSB style (current implementation)
    printf("DBC MSB style (startBit=12, len=2): %u\n", extractBits(testData, 12, 2));
    
    // Approach 2: Try Intel/Motorola byte order differences
    printf("Different byte orderings:\n");
    
    // Try extracting from different bytes directly
    for (int byte = 0; byte < 8; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            for (int len = 1; len <= 4; len++) {
                if (bit + len > 8) continue;
                
                uint8_t mask = ((1 << len) - 1) << bit;
                uint8_t value = (testData[byte] & mask) >> bit;
                
                if (value == 1) { // Looking for the value 1 for PudLamp=ON
                    printf("  Byte %d, bit %d, len %d: value=%u ✓\n", byte, bit, len, value);
                }
            }
        }
    }
}
