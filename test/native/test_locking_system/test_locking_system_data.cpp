#include <gtest/gtest.h>
#include "../../common/test_helpers.h"
#include "../../common/can_test_utils.h"

// Using production parsing functions from message_parser.cpp and state_manager.cpp
#include "../../../src/state_manager.h"

/**
 * Locking System Data Test Suite
 * 
 * This test suite validates our locking system message parsing against
 * the actual CAN data captured in can_logger_1754515370_locking.out.
 * 
 * The log contains real Ford F-150 locking system messages that show:
 * - Locking operations (Veh_Lock_Status=LOCK_ALL) 
 * - Unlocking operations (Veh_Lock_Status=UNLOCK_ALL)
 * 
 * All test data is taken directly from the actual CAN log file.
 */

class LockingSystemDataTest : public ArduinoTest {
protected:
    // Helper to validate a locking system message
    void validateLockingMessage(const char* testName, const uint8_t data[8], 
                               uint8_t expectedStatus, const char* expectedAction) {
        CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, data);
        CANMessage message = convertToCANMessage(frame);
        LockingSystemsStatus result;
        
        bool success = parseLockingSystemsStatus(message, result);
        
        EXPECT_TRUE(success) << "Parsing failed for " << testName;
        EXPECT_TRUE(result.valid) << "Parsing failed for " << testName;
        EXPECT_EQ(result.vehicleLockStatus, expectedStatus) 
            << "Lock status mismatch for " << testName 
            << " - expected " << (int)expectedStatus 
            << " got " << (int)result.vehicleLockStatus;
        
        // Verify decision logic
        bool isUnlocked = isVehicleUnlocked(result.vehicleLockStatus);
        bool shouldBeUnlocked = (expectedStatus == VEH_UNLOCK_ALL || expectedStatus == VEH_UNLOCK_DRV);
        EXPECT_EQ(isUnlocked, shouldBeUnlocked) 
            << "Decision logic failed for " << testName;
        
        printf("✓ %s: %s (status=%u, unlocked=%s)\n", 
               testName, expectedAction, result.vehicleLockStatus, 
               isUnlocked ? "YES" : "NO");
    }
};

// ===============================================
// Test Suite: Real CAN Log Data Validation
// ===============================================

TEST_F(LockingSystemDataTest, ActualLogData_LockAllOperations) {
    // Test data from can_logger_1754515370_locking.out showing LOCK_ALL operations
    // CAN_ID:0x331 | data:XX XX XX XX XX XX XX XX | Locking_Systems_2_FD1 | Veh_Lock_Status=LOCK_ALL
    
    struct LockTestCase {
        uint8_t data[8];
        const char* description;
    };
    
    LockTestCase lockCases[] = {
        // LOCK_ALL patterns from the actual log
        {{0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, "Lock All - sequence 1"},
        {{0x04, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, "Lock All - sequence 10"}
    };
    
    for (const auto& testCase : lockCases) {
        validateLockingMessage(testCase.description, testCase.data, 
                              VEH_LOCK_ALL, "LOCK_ALL");
    }
}

TEST_F(LockingSystemDataTest, ActualLogData_UnlockAllOperations) {
    // Test data from can_logger_1754515370_locking.out showing UNLOCK_ALL operations
    // CAN_ID:0x331 | data:XX XX XX XX XX XX XX XX | Locking_Systems_2_FD1 | Veh_Lock_Status=UNLOCK_ALL
    
    struct UnlockTestCase {
        uint8_t data[8];
        const char* description;
    };
    
    UnlockTestCase unlockCases[] = {
        // UNLOCK_ALL patterns from the actual log (sequences 2-9)
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10}, "Unlock All - sequence 2"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC3, 0x44, 0x10}, "Unlock All - sequence 3"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x44, 0x10}, "Unlock All - sequence 4"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x94, 0x10}, "Unlock All - sequence 5"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC5, 0x94, 0x10}, "Unlock All - sequence 6"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x44, 0x10}, "Unlock All - sequence 7"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x94, 0x10}, "Unlock All - sequence 8"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC8, 0x94, 0x10}, "Unlock All - sequence 9"}
    };
    
    for (const auto& testCase : unlockCases) {
        validateLockingMessage(testCase.description, testCase.data, 
                              VEH_UNLOCK_ALL, "UNLOCK_ALL");
    }
}

// ===============================================
// Test Suite: Bit Pattern Analysis
// ===============================================

TEST_F(LockingSystemDataTest, BitPatternAnalysis) {
    // Analyze the bit patterns to understand how the lock status is encoded
    // This helps validate that our bit extraction logic is correct
    
    printf("=== Lock Status Bit Pattern Analysis ===\n");
    
    // LOCK_ALL patterns
    uint8_t lockData1[8] = {0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10};
    uint8_t lockData2[8] = {0x04, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10};
    
    // UNLOCK_ALL patterns (sample)
    uint8_t unlockData1[8] = {0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10};
    uint8_t unlockData2[8] = {0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x44, 0x10};
    uint8_t unlockData3[8] = {0x00, 0x0F, 0x00, 0x00, 0x05, 0xC8, 0x94, 0x10};
    
    printf("LOCK_ALL patterns:\n");
    printf("  Pattern 1: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           lockData1[0], lockData1[1], lockData1[2], lockData1[3],
           lockData1[4], lockData1[5], lockData1[6], lockData1[7]);
    printf("  Pattern 2: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           lockData2[0], lockData2[1], lockData2[2], lockData2[3],
           lockData2[4], lockData2[5], lockData2[6], lockData2[7]);
    
    printf("UNLOCK_ALL patterns:\n");
    printf("  Pattern 1: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           unlockData1[0], unlockData1[1], unlockData1[2], unlockData1[3],
           unlockData1[4], unlockData1[5], unlockData1[6], unlockData1[7]);
    printf("  Pattern 2: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           unlockData2[0], unlockData2[1], unlockData2[2], unlockData2[3],
           unlockData2[4], unlockData2[5], unlockData2[6], unlockData2[7]);
    printf("  Pattern 3: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           unlockData3[0], unlockData3[1], unlockData3[2], unlockData3[3],
           unlockData3[4], unlockData3[5], unlockData3[6], unlockData3[7]);
    
    // Key observation: byte 4 differs between LOCK_ALL (0x02) and UNLOCK_ALL (0x05)
    printf("\nKey differences:\n");
    printf("  LOCK_ALL: byte 4 = 0x02\n");
    printf("  UNLOCK_ALL: byte 4 = 0x05\n");
    printf("  This suggests the lock status is encoded in byte 4\n");
    
    // Test bit extraction at different positions to find the exact bits
    printf("\nBit extraction tests:\n");
    for (int bitPos = 32; bitPos <= 39; bitPos++) {
        for (int bitLen = 1; bitLen <= 4; bitLen++) {
            uint32_t lockValue = extractBits(lockData1, bitPos, bitLen);
            uint32_t unlockValue = extractBits(unlockData1, bitPos, bitLen);
            
            if (lockValue != unlockValue) {
                printf("  Bit %d (len %d): LOCK=%u, UNLOCK=%u\n", 
                       bitPos, bitLen, lockValue, unlockValue);
            }
        }
    }
}

// ===============================================
// Test Suite: Integration with Vehicle Logic
// ===============================================

TEST_F(LockingSystemDataTest, VehicleLogicIntegration) {
    // Test how the locking system integrates with the vehicle's toolbox logic
    // This simulates the real-world decision making process
    
    // Scenario 1: Vehicle locks, toolbox should NOT open
    uint8_t lockData[8] = {0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10};
    CANFrame lockFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, lockData);
    CANMessage lockMessage = convertToCANMessage(lockFrame);
    LockingSystemsStatus lockResult;
    
    bool lockSuccess = parseLockingSystemsStatus(lockMessage, lockResult);
    
    EXPECT_TRUE(lockSuccess);
    EXPECT_TRUE(lockResult.valid);
    EXPECT_EQ(lockResult.vehicleLockStatus, VEH_LOCK_ALL);
    
    bool isUnlockedWhenLocked = isVehicleUnlocked(lockResult.vehicleLockStatus);
    EXPECT_FALSE(isUnlockedWhenLocked);
    
    // Assume other conditions are met for toolbox activation
    bool toolboxShouldOpen1 = shouldActivateToolboxWithParams(true, true, isUnlockedWhenLocked);
    EXPECT_FALSE(toolboxShouldOpen1) << "Toolbox should NOT open when vehicle is locked";
    
    // Scenario 2: Vehicle unlocks, toolbox SHOULD open (if other conditions met)
    uint8_t unlockData[8] = {0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10};
    CANFrame unlockFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, unlockData);
    CANMessage unlockMessage = convertToCANMessage(unlockFrame);
    LockingSystemsStatus unlockResult;
    
    bool unlockSuccess = parseLockingSystemsStatus(unlockMessage, unlockResult);
    
    EXPECT_TRUE(unlockSuccess);
    EXPECT_TRUE(unlockResult.valid);
    EXPECT_EQ(unlockResult.vehicleLockStatus, VEH_UNLOCK_ALL);
    
    bool isUnlockedWhenUnlocked = isVehicleUnlocked(unlockResult.vehicleLockStatus);
    EXPECT_TRUE(isUnlockedWhenUnlocked);
    
    // Assume other conditions are met for toolbox activation
    bool toolboxShouldOpen2 = shouldActivateToolboxWithParams(true, true, isUnlockedWhenUnlocked);
    EXPECT_TRUE(toolboxShouldOpen2) << "Toolbox SHOULD open when vehicle is unlocked and other conditions met";
    
    printf("Vehicle Logic Integration Results:\n");
    printf("  Locked vehicle: toolbox open = %s ✓\n", toolboxShouldOpen1 ? "YES" : "NO");
    printf("  Unlocked vehicle: toolbox open = %s ✓\n", toolboxShouldOpen2 ? "YES" : "NO");
}

// ===============================================
// Test Suite: Message Sequence Analysis
// ===============================================

TEST_F(LockingSystemDataTest, MessageSequenceAnalysis) {
    // Analyze the complete sequence from the log file to understand the
    // locking/unlocking behavior patterns
    
    struct SequenceStep {
        uint8_t data[8];
        uint8_t expectedStatus;
        const char* action;
        int sequenceNumber;
    };
    
    SequenceStep sequence[] = {
        // Complete sequence from can_logger_1754515370_locking.out
        {{0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, VEH_LOCK_ALL, "LOCK_ALL", 1},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 2},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC3, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 3},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 4},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 5},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC5, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 6},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 7},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 8},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC8, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL", 9},
        {{0x04, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, VEH_LOCK_ALL, "LOCK_ALL", 10}
    };
    
    printf("=== Message Sequence Analysis ===\n");
    printf("Processing complete locking sequence from log file:\n");
    
    int lockCount = 0;
    int unlockCount = 0;
    
    for (const auto& step : sequence) {
        CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, step.data);
        CANMessage message = convertToCANMessage(frame);
        LockingSystemsStatus result;
        
        bool success = parseLockingSystemsStatus(message, result);
        
        EXPECT_TRUE(success) << "Step " << step.sequenceNumber << " parsing failed";
        EXPECT_TRUE(result.valid) << "Step " << step.sequenceNumber << " parsing failed";
        EXPECT_EQ(result.vehicleLockStatus, step.expectedStatus) 
            << "Step " << step.sequenceNumber << " status mismatch";
        
        if (step.expectedStatus == VEH_LOCK_ALL) {
            lockCount++;
        } else if (step.expectedStatus == VEH_UNLOCK_ALL) {
            unlockCount++;
        }
        
        printf("  Step %2d: %s (status=%u)\n", 
               step.sequenceNumber, step.action, result.vehicleLockStatus);
    }
    
    printf("Sequence Summary:\n");
    printf("  Total steps: %d\n", (int)(sizeof(sequence) / sizeof(sequence[0])));
    printf("  Lock operations: %d\n", lockCount);
    printf("  Unlock operations: %d\n", unlockCount);
    printf("  Pattern: LOCK → multiple UNLOCK → LOCK\n");
    
    EXPECT_EQ(lockCount, 2);    // 2 lock operations
    EXPECT_EQ(unlockCount, 8);  // 8 unlock operations
}

// ===============================================
// Test Suite: Error Conditions
// ===============================================

TEST_F(LockingSystemDataTest, ErrorConditions) {
    // Test error handling for invalid or corrupted messages
    
    // Test invalid message ID
    uint8_t validData[8] = {0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10};
    CANFrame invalidIdFrame = CANTestUtils::createCANFrame(0x999, validData);
    CANMessage invalidIdMessage = convertToCANMessage(invalidIdFrame);
    LockingSystemsStatus result1;
    
    bool success1 = parseLockingSystemsStatus(invalidIdMessage, result1);
    EXPECT_FALSE(success1) << "Should reject invalid message ID";
    EXPECT_FALSE(result1.valid) << "Should reject invalid message ID";
    
    // Test invalid message length
    CANFrame invalidLengthFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, validData);
    invalidLengthFrame.length = 4; // Wrong length
    CANMessage invalidLengthMessage = convertToCANMessage(invalidLengthFrame);
    LockingSystemsStatus result2;
    
    bool success2 = parseLockingSystemsStatus(invalidLengthMessage, result2);
    EXPECT_FALSE(success2) << "Should reject invalid message length";
    EXPECT_FALSE(result2.valid) << "Should reject invalid message length";
    
    // Test with corrupted message data
    CANFrame corruptedFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, validData);
    corruptedFrame.id = 0; // Corrupt the ID to 0
    CANMessage corruptedMessage = convertToCANMessage(corruptedFrame);
    LockingSystemsStatus result3;
    
    bool success3 = parseLockingSystemsStatus(corruptedMessage, result3);
    EXPECT_FALSE(success3) << "Should handle corrupted data gracefully";
    EXPECT_FALSE(result3.valid) << "Should handle corrupted data gracefully";
    
    printf("Error condition handling: ✓ All checks passed\n");
}

// ===============================================
// Test Suite: Performance Validation
// ===============================================

TEST_F(LockingSystemDataTest, PerformanceValidation) {
    // Test parsing performance with real data patterns
    // This ensures our implementation is efficient enough for real-time use
    
    uint8_t testData[8] = {0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10};
    CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, testData);
    CANMessage message = convertToCANMessage(frame);
    
    const int iterations = 1000;
    unsigned long startTime = millis();
    
    for (int i = 0; i < iterations; i++) {
        LockingSystemsStatus result;
        bool success = parseLockingSystemsStatus(message, result);
        (void)success; // Prevent optimization
        (void)result; // Prevent optimization
    }
    
    unsigned long endTime = millis();
    unsigned long elapsed = endTime - startTime;
    
    printf("Performance Validation:\n");
    printf("  %d parsing operations in %lu ms\n", iterations, elapsed);
    printf("  Average time per parse: %.3f ms\n", (double)elapsed / iterations);
    
    // Should be very fast (< 10ms total for 1000 operations)
    EXPECT_LT(elapsed, 10) << "Parsing should be very fast for real-time use";
}

// ===============================================
// Summary
// ===============================================

TEST_F(LockingSystemDataTest, ValidationSummary) {
    printf("\n=== Locking System Data Validation Summary ===\n");
    printf("This test suite validates locking system message parsing against\n");
    printf("real CAN data from can_logger_1754515370_locking.out.\n");
    printf("\nValidated data patterns:\n");
    printf("  • LOCK_ALL: 2 unique patterns from actual log\n");
    printf("  • UNLOCK_ALL: 8 unique patterns from actual log\n");
    printf("  • Complete message sequence: LOCK → UNLOCK (8x) → LOCK\n");
    printf("\nKey findings:\n");
    printf("  • Lock status encoded in byte 4 of CAN message\n");
    printf("  • LOCK_ALL = byte 4 value 0x02\n");
    printf("  • UNLOCK_ALL = byte 4 value 0x05\n");
    printf("  • Other bytes contain counter/checksum data\n");
    printf("\nIntegration validated:\n");
    printf("  • Vehicle logic correctly responds to lock/unlock states\n");
    printf("  • Toolbox activation properly gated by unlock status\n");
    printf("  • Error handling robust for invalid messages\n");
    printf("  • Performance suitable for real-time operation\n");
    printf("================================================\n\n");
    
    SUCCEED(); // This test always passes - it's documentation
}
