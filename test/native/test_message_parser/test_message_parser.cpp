#include <gtest/gtest.h>
#include "../../common/test_helpers.h"
#include "../../common/can_test_utils.h"

// Import production code structures and functions - using production message_parser.h
// No need to import can_protocol.h since we removed the duplicate functions
#include "../src/state_manager.h"
// isTargetCANMessage is provided by mock_arduino.cpp
// Decision logic functions are declared in state_manager.h

/**
 * Message Parser Test Suite
 * 
 * These tests validate our C++ message parsing logic against the Python implementation
 * in can_embedded_logger.py. The Python code defines the exact bit positions and 
 * expected values for each CAN message we're interested in:
 * 
 * - BCM_Lamp_Stat_FD1 (0x3C3): PudLamp_D_Rq, Illuminated_Entry_Stat, Dr_Courtesy_Light_Stat
 * - Locking_Systems_2_FD1 (0x331): Veh_Lock_Status  
 * - PowertrainData_10 (0x176): TrnPrkSys_D_Actl
 * - Battery_Mgmt_3_FD1 (0x43C): BSBattSOC
 * 
 * The Python implementation uses Intel (little-endian) byte order with DBC bit positioning.
 */

class MessageParserTest : public ArduinoTest {
protected:
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
};

// ===============================================
// Test Suite: CAN Message ID Recognition
// ===============================================

TEST_F(MessageParserTest, CorrectMessageIDsFromPython) {
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

TEST_F(MessageParserTest, TargetMessageRecognition) {
    // Test isTargetCANMessage() function
    
    // Test target messages
    EXPECT_TRUE(isTargetCANMessage(BCM_LAMP_STAT_FD1_ID));
    EXPECT_TRUE(isTargetCANMessage(LOCKING_SYSTEMS_2_FD1_ID));
    EXPECT_TRUE(isTargetCANMessage(POWERTRAIN_DATA_10_ID));
    EXPECT_TRUE(isTargetCANMessage(BATTERY_MGMT_3_FD1_ID));
    
    // Test non-target messages
    EXPECT_FALSE(isTargetCANMessage(0x100));
    EXPECT_FALSE(isTargetCANMessage(0x200));
    EXPECT_FALSE(isTargetCANMessage(0x7FF));
    EXPECT_FALSE(isTargetCANMessage(0x000));
    
    // Test the old wrong IDs that were in the code before our fix
    EXPECT_FALSE(isTargetCANMessage(0x3B3)); // Old BCM_LAMP_STAT_FD1_ID
    EXPECT_FALSE(isTargetCANMessage(0x3B8)); // Old LOCKING_SYSTEMS_2_FD1_ID  
    EXPECT_FALSE(isTargetCANMessage(0x204)); // Old POWERTRAIN_DATA_10_ID
    EXPECT_FALSE(isTargetCANMessage(0x3D2)); // Old BATTERY_MGMT_3_FD1_ID
}

// ===============================================
// Test Suite: BCM_Lamp_Stat_FD1 Message Parsing
// ===============================================

TEST_F(MessageParserTest, BCMLampStatusBasicParsing) {
    // Test basic BCM_Lamp_Stat_FD1 message parsing
    // Python defines: PudLamp_D_Rq (bits 11-12, 2 bits)
    
    uint8_t testData[8] = {0};
    CANTestUtils::setSignalValue(testData, 11, 2, 1); // PudLamp_D_Rq = ON (1)
    
    CANFrame frame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, testData);
    CANMessage message = convertToCANMessage(frame);
    BCMLampStatus result;
    
    bool success = parseBCMLampStatus(message, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.pudLampRequest, 1); // PUDLAMP_ON
}

TEST_F(MessageParserTest, BCMLampStatusPythonValueMapping) {
    // Test Python value mappings for PudLamp_D_Rq:
    // {0: "OFF", 1: "ON", 2: "RAMP_UP", 3: "RAMP_DOWN"}
    
    struct TestCase {
        uint8_t rawValue;
        uint8_t expectedConstant;
        const char* description;
    };
    
    TestCase testCases[] = {
        {0, PUDLAMP_OFF, "OFF"},
        {1, PUDLAMP_ON, "ON"}, 
        {2, PUDLAMP_RAMP_UP, "RAMP_UP"},
        {3, PUDLAMP_RAMP_DOWN, "RAMP_DOWN"}
    };
    
    for (const auto& testCase : testCases) {
        uint8_t testData[8] = {0};
        CANTestUtils::setSignalValue(testData, 11, 2, testCase.rawValue);
        
        CANFrame frame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, testData);
        CANMessage message = convertToCANMessage(frame);
        BCMLampStatus result;
        
        bool success = parseBCMLampStatus(message, result);
        
        EXPECT_TRUE(success) << "Parsing failed for " << testCase.description;
        EXPECT_TRUE(result.valid) << "Failed for " << testCase.description;
        EXPECT_EQ(result.pudLampRequest, testCase.expectedConstant) << "Value mismatch for " << testCase.description;
        
        // Verify our bit extraction matches Python implementation
        uint32_t pythonValue = CANTestUtils::extractSignalValue(testData, 11, 2);
        EXPECT_EQ(pythonValue, testCase.rawValue) << "Python extraction mismatch for " << testCase.description;
    }
}

TEST_F(MessageParserTest, BCMLampStatusInvalidMessage) {
    // Test invalid message ID
    uint8_t testData[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    CANFrame frame = CANTestUtils::createCANFrame(0x999, testData); // Wrong ID
    CANMessage message = convertToCANMessage(frame);
    BCMLampStatus result;
    
    bool success = parseBCMLampStatus(message, result);
    
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.valid);
    
    // Test invalid length
    frame.id = BCM_LAMP_STAT_FD1_ID;
    frame.length = 4; // Wrong length
    message = convertToCANMessage(frame);
    
    success = parseBCMLampStatus(message, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.valid);
}

// ===============================================
// Test Suite: Locking_Systems_2_FD1 Message Parsing  
// ===============================================

TEST_F(MessageParserTest, LockingSystemsBasicParsing) {
    // Test Locking_Systems_2_FD1 message parsing
    // Python defines: Veh_Lock_Status (bits 34-35, 2 bits)
    
    uint8_t testData[8] = {0};
    CANTestUtils::setSignalValue(testData, 34, 2, 2); // Veh_Lock_Status = UNLOCK_ALL (2) - match production bit position
    
    CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, testData);
    CANMessage message = convertToCANMessage(frame);
    LockingSystemsStatus result;
    
    bool success = parseLockingSystemsStatus(message, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.vehicleLockStatus, 2); // VEH_UNLOCK_ALL
}

TEST_F(MessageParserTest, LockingSystemsRealCANData) {
    // Test with actual CAN data from can_logger_1754515370_locking.out
    // This validates against real Ford F-150 locking system messages
    
    struct TestCase {
        uint8_t data[8];
        uint8_t expectedStatus;
        const char* description;
    };
    
    TestCase testCases[] = {
        // LOCK_ALL cases from actual log
        {{0x00, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, VEH_LOCK_ALL, "LOCK_ALL - pattern 1"},
        {{0x04, 0x0F, 0x00, 0x00, 0x02, 0xC7, 0x44, 0x10}, VEH_LOCK_ALL, "LOCK_ALL - pattern 2"},
        
        // UNLOCK_ALL cases from actual log
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC2, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 1"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC3, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 2"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 3"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC4, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 4"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC5, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 5"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x44, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 6"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC6, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 7"},
        {{0x00, 0x0F, 0x00, 0x00, 0x05, 0xC8, 0x94, 0x10}, VEH_UNLOCK_ALL, "UNLOCK_ALL - pattern 8"}
    };
    
    for (const auto& testCase : testCases) {
        CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, testCase.data);
        CANMessage message = convertToCANMessage(frame);
        LockingSystemsStatus result;
        
        bool success = parseLockingSystemsStatus(message, result);
        
        EXPECT_TRUE(success) << "Parsing failed for " << testCase.description;
        EXPECT_TRUE(result.valid) << "Failed for " << testCase.description;
        EXPECT_EQ(result.vehicleLockStatus, testCase.expectedStatus) << "Value mismatch for " << testCase.description;
        
        // Verify decision logic
        bool isUnlocked = isVehicleUnlocked(result.vehicleLockStatus);
        bool expectedUnlocked = (testCase.expectedStatus == VEH_UNLOCK_ALL || testCase.expectedStatus == VEH_UNLOCK_DRV);
        EXPECT_EQ(isUnlocked, expectedUnlocked) << "Decision logic mismatch for " << testCase.description;
        
        printf("✓ %s: status=%u, unlocked=%s\n", 
               testCase.description, 
               result.vehicleLockStatus,
               isUnlocked ? "YES" : "NO");
    }
}

// ===============================================
// Test Suite: PowertrainData_10 Message Parsing
// ===============================================

TEST_F(MessageParserTest, PowertrainDataBasicParsing) {
    // Test PowertrainData_10 message parsing
    // Python defines: TrnPrkSys_D_Actl (bits 31-34, 4 bits)
    
    uint8_t testData[8] = {0};
    CANTestUtils::setSignalValue(testData, 31, 4, 1); // TrnPrkSys_D_Actl = PARK (1)
    
    CANFrame frame = CANTestUtils::createCANFrame(POWERTRAIN_DATA_10_ID, testData);
    CANMessage message = convertToCANMessage(frame);
    PowertrainData result;
    
    bool success = parsePowertrainData(message, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.transmissionParkStatus, 1); // TRNPRKSTS_PARK
}

TEST_F(MessageParserTest, PowertrainDataPythonValueMapping) {
    // Test Python value mappings for TrnPrkSys_D_Actl:
    // {0: "NotKnown", 1: "Park", 2: "TransitionCloseToPark", 3: "AtNoSpring", 
    //  4: "TransitionCloseToOutOfPark", 5: "OutOfPark", 6: "Override", 7: "OutOfRangeLow",
    //  8: "OutOfRangeHigh", 9: "FrequencyError", 15: "Faulty"}
    
    struct TestCase {
        uint8_t rawValue;
        uint8_t expectedConstant;
        const char* description;
    };
    
    TestCase testCases[] = {
        {0, TRNPRKSTS_UNKNOWN, "NotKnown"},
        {1, TRNPRKSTS_PARK, "Park"},
        {2, TRNPRKSTS_TRANSITION_CLOSE_TO_PARK, "TransitionCloseToPark"},
        {3, TRNPRKSTS_AT_NO_SPRING, "AtNoSpring"},
        {4, TRNPRKSTS_TRANSITION_CLOSE_TO_OUT_OF_PARK, "TransitionCloseToOutOfPark"},
        {5, TRNPRKSTS_OUT_OF_PARK, "OutOfPark"},
        // Test some higher values that should still parse correctly
        {6, 6, "Override"},
        {15, 15, "Faulty"}
    };
    
    for (const auto& testCase : testCases) {
        uint8_t testData[8] = {0};
        CANTestUtils::setSignalValue(testData, 31, 4, testCase.rawValue);
        
        CANFrame frame = CANTestUtils::createCANFrame(POWERTRAIN_DATA_10_ID, testData);
        CANMessage message = convertToCANMessage(frame);
        PowertrainData result;
        
        bool success = parsePowertrainData(message, result);
        
        EXPECT_TRUE(success) << "Parsing failed for " << testCase.description;
        EXPECT_TRUE(result.valid) << "Failed for " << testCase.description;
        EXPECT_EQ(result.transmissionParkStatus, testCase.expectedConstant) << "Value mismatch for " << testCase.description;
        
        // Verify our bit extraction matches Python implementation
        uint32_t pythonValue = CANTestUtils::extractSignalValue(testData, 31, 4);
        EXPECT_EQ(pythonValue, testCase.rawValue) << "Python extraction mismatch for " << testCase.description;
    }
}

// ===============================================
// Test Suite: Battery_Mgmt_3_FD1 Message Parsing
// ===============================================

TEST_F(MessageParserTest, BatteryManagementBasicParsing) {
    // Test Battery_Mgmt_3_FD1 message parsing
    // Python defines: BSBattSOC (bits 22-28, 7 bits) - Raw percentage value
    
    uint8_t testData[8] = {0};
    CANTestUtils::setSignalValue(testData, 22, 7, 85); // Battery SOC = 85%
    
    CANFrame frame = CANTestUtils::createCANFrame(BATTERY_MGMT_3_FD1_ID, testData);
    CANMessage message = convertToCANMessage(frame);
    BatteryManagement result;
    
    bool success = parseBatteryManagement(message, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.batterySOC, 85);
}

TEST_F(MessageParserTest, BatteryManagementRangeValues) {
    // Test various battery percentage values (0-127% as defined by 7-bit field)
    
    struct TestCase {
        uint8_t rawValue;
        const char* description;
    };
    
    TestCase testCases[] = {
        {0, "0% - Empty"},
        {50, "50% - Half"},
        {85, "85% - Typical"},
        {100, "100% - Full"},
        {127, "127% - Maximum (7-bit)"}
    };
    
    for (const auto& testCase : testCases) {
        uint8_t testData[8] = {0};
        CANTestUtils::setSignalValue(testData, 22, 7, testCase.rawValue);
        
        CANFrame frame = CANTestUtils::createCANFrame(BATTERY_MGMT_3_FD1_ID, testData);
        CANMessage message = convertToCANMessage(frame);
        BatteryManagement result;
        
        bool success = parseBatteryManagement(message, result);
        
        EXPECT_TRUE(success) << "Parsing failed for " << testCase.description;
        EXPECT_TRUE(result.valid) << "Failed for " << testCase.description;
        EXPECT_EQ(result.batterySOC, testCase.rawValue) << "Value mismatch for " << testCase.description;
        
        // Verify our bit extraction matches Python implementation
        uint32_t pythonValue = CANTestUtils::extractSignalValue(testData, 22, 7);
        EXPECT_EQ(pythonValue, testCase.rawValue) << "Python extraction mismatch for " << testCase.description;
    }
}

// ===============================================
// Test Suite: Decision Logic Functions
// ===============================================

TEST_F(MessageParserTest, VehicleStateDecisionLogic) {
    // Test shouldActivateToolboxWithParams logic
    EXPECT_TRUE(shouldActivateToolboxWithParams(true, true, true));   // All conditions met
    EXPECT_FALSE(shouldActivateToolboxWithParams(false, true, true)); // System not ready
    EXPECT_FALSE(shouldActivateToolboxWithParams(true, false, true)); // Not parked
    EXPECT_FALSE(shouldActivateToolboxWithParams(true, true, false)); // Not unlocked
    EXPECT_FALSE(shouldActivateToolboxWithParams(false, false, false)); // No conditions met
    
    // Test shouldEnableBedlight logic (ON or RAMP_UP)
    EXPECT_FALSE(shouldEnableBedlight(PUDLAMP_OFF));
    EXPECT_TRUE(shouldEnableBedlight(PUDLAMP_ON));
    EXPECT_TRUE(shouldEnableBedlight(PUDLAMP_RAMP_UP));
    EXPECT_FALSE(shouldEnableBedlight(PUDLAMP_RAMP_DOWN));
    
    // Test isVehicleUnlocked logic (UNLOCK_ALL or UNLOCK_DRV)
    EXPECT_FALSE(isVehicleUnlocked(VEH_LOCK_DBL));
    EXPECT_FALSE(isVehicleUnlocked(VEH_LOCK_ALL));
    EXPECT_TRUE(isVehicleUnlocked(VEH_UNLOCK_ALL));
    EXPECT_TRUE(isVehicleUnlocked(VEH_UNLOCK_DRV));
    
    // Test isVehicleParked logic (only PARK)
    EXPECT_FALSE(isVehicleParked(TRNPRKSTS_UNKNOWN));
    EXPECT_TRUE(isVehicleParked(TRNPRKSTS_PARK));
    EXPECT_FALSE(isVehicleParked(TRNPRKSTS_TRANSITION_CLOSE_TO_PARK));
    EXPECT_FALSE(isVehicleParked(TRNPRKSTS_OUT_OF_PARK));
}

// ===============================================
// Test Suite: Comprehensive Message Validation
// ===============================================

TEST_F(MessageParserTest, ComprehensiveMessageValidation) {
    // Test all message types with a single comprehensive test
    // This ensures the parsing pipeline works end-to-end
    
    // Create test messages for all types
    uint8_t bcmData[8] = {0};
    CANTestUtils::setSignalValue(bcmData, 11, 2, PUDLAMP_ON);
    CANFrame bcmFrame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, bcmData);
    
    uint8_t lockData[8] = {0};
    CANTestUtils::setSignalValue(lockData, 34, 2, VEH_UNLOCK_ALL); // Match production bit position
    CANFrame lockFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, lockData);
    
    uint8_t powerData[8] = {0};
    CANTestUtils::setSignalValue(powerData, 31, 4, TRNPRKSTS_PARK);
    CANFrame powerFrame = CANTestUtils::createCANFrame(POWERTRAIN_DATA_10_ID, powerData);
    
    uint8_t battData[8] = {0};
    CANTestUtils::setSignalValue(battData, 22, 7, 95);
    CANFrame battFrame = CANTestUtils::createCANFrame(BATTERY_MGMT_3_FD1_ID, battData);
    
    // Parse all messages
    CANMessage bcmMessage = convertToCANMessage(bcmFrame);
    CANMessage lockMessage = convertToCANMessage(lockFrame);
    CANMessage powerMessage = convertToCANMessage(powerFrame);
    CANMessage battMessage = convertToCANMessage(battFrame);
    
    BCMLampStatus bcmResult;
    LockingSystemsStatus lockResult;
    PowertrainData powerResult;
    BatteryManagement battResult;
    
    bool bcmSuccess = parseBCMLampStatus(bcmMessage, bcmResult);
    bool lockSuccess = parseLockingSystemsStatus(lockMessage, lockResult);
    bool powerSuccess = parsePowertrainData(powerMessage, powerResult);
    bool battSuccess = parseBatteryManagement(battMessage, battResult);
    
    // Validate all parsing succeeded
    EXPECT_TRUE(bcmSuccess);
    EXPECT_TRUE(lockSuccess);
    EXPECT_TRUE(powerSuccess);
    EXPECT_TRUE(battSuccess);
    
    // Validate all parsed values
    EXPECT_TRUE(bcmResult.valid);
    EXPECT_EQ(bcmResult.pudLampRequest, PUDLAMP_ON);
    
    EXPECT_TRUE(lockResult.valid);
    EXPECT_EQ(lockResult.vehicleLockStatus, VEH_UNLOCK_ALL);
    
    EXPECT_TRUE(powerResult.valid);
    EXPECT_EQ(powerResult.transmissionParkStatus, TRNPRKSTS_PARK);
    
    EXPECT_TRUE(battResult.valid);
    EXPECT_EQ(battResult.batterySOC, 95);
    
    // Test the decision logic with these parsed values
    bool bedlightShouldBeOn = shouldEnableBedlight(bcmResult.pudLampRequest);
    bool vehicleIsUnlocked = isVehicleUnlocked(lockResult.vehicleLockStatus);
    bool vehicleIsParked = isVehicleParked(powerResult.transmissionParkStatus);
    bool toolboxShouldOpen = shouldActivateToolboxWithParams(true, vehicleIsParked, vehicleIsUnlocked);
    
    EXPECT_TRUE(bedlightShouldBeOn);  // PUDLAMP_ON
    EXPECT_TRUE(vehicleIsUnlocked);   // VEH_UNLOCK_ALL
    EXPECT_TRUE(vehicleIsParked);     // TRNPRKSTS_PARK
    EXPECT_TRUE(toolboxShouldOpen);   // All conditions met
}

// Main function removed - using unified test runner in test_main.cpp
