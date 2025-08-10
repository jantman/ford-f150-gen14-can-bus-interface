#include <gtest/gtest.h>
#include "common/can_test_utils.h"

// Include the ACTUAL production code (pure logic, no Arduino dependencies)
extern "C" {
    #include "../src/can_protocol.h"
    #include "../src/config.h"  // For CAN message ID constants
    #include "../src/bit_utils.h"  // For setBits function
}

class CANProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed for pure logic tests
    }
    
    void TearDown() override {
        // No cleanup needed
    }
};

// Test the ACTUAL extractBits function from production code
TEST_F(CANProtocolTest, BitExtractionProduction) {
    uint8_t testData[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    
    // Test basic bit extraction using ACTUAL PRODUCTION CODE with DBC-style positioning
    // testData as little-endian 64-bit: 0xF0DEBC9A78563412
    EXPECT_EQ(extractBits(testData, 3, 4), 0x2);   // DBC bits 0-3: lower 4 bits of 0x12
    EXPECT_EQ(extractBits(testData, 7, 4), 0x1);   // DBC bits 4-7: upper 4 bits of 0x12  
    EXPECT_EQ(extractBits(testData, 15, 8), 0x34); // DBC bits 8-15: full second byte
    
    // Test edge cases
    EXPECT_EQ(extractBits(testData, 0, 1), 0x0);   // DBC bit 0: LSB of 0x12
    EXPECT_EQ(extractBits(testData, 1, 1), 0x1);   // DBC bit 1: second bit
    EXPECT_EQ(extractBits(testData, 64, 1), 0x0);  // Out of bounds
    EXPECT_EQ(extractBits(testData, 0, 9), 0x0);   // Too many bits
}

// Test the ACTUAL BCM lamp parsing function
TEST_F(CANProtocolTest, BCMLampParsingProduction) {
    // Create test frame with PudLamp_D_Rq = 2 (RAMP_UP) at start_bit=11, length=2
    // Value 2 = binary 10, bit_pos = 11-2+1 = 10, so bits 10=0, 11=1
    // Bit 11 is in byte 1, position 3: 0x08
    CANFrame frame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    BCMLampData result = parseBCMLampFrame(&frame);
    
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.pudLampRequest, 2);  // RAMP_UP
}

// Test the ACTUAL locking systems parsing function  
TEST_F(CANProtocolTest, LockingSystemsParsingProduction) {
    // Create test frame with Veh_Lock_Status = 3 (UNLOCK_DRV) at bits 33-34
    uint8_t testData[8];
    CANTestUtils::setSignalValue(testData, 34, 2, 3);  // Use corrected bit position  
    CANFrame frame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID, 
                                testData[0], testData[1], testData[2], testData[3],
                                testData[4], testData[5], testData[6], testData[7]);
    
    LockingSystemsData result = parseLockingSystemsFrame(&frame);
    
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.vehicleLockStatus, 3);  // UNLOCK_DRV
}

// Test the ACTUAL powertrain parsing function
TEST_F(CANProtocolTest, PowertrainParsingProduction) {
    // Create test frame with TrnPrkSys_D_Actl = 1 (Park) at bits 28-31 (MSB at 31)
    uint8_t testData[8];
    CANTestUtils::setSignalValue(testData, 31, 4, 1);  // Use setSignalValue to get correct bit pattern
    CANFrame frame = CANTestUtils::createCANFrame(POWERTRAIN_DATA_10_ID, 
                                testData[0], testData[1], testData[2], testData[3],
                                testData[4], testData[5], testData[6], testData[7]);
    
    PowertrainData result = parsePowertrainFrame(&frame);
    
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.transmissionParkStatus, 1);  // Park
}

// Test the ACTUAL decision logic functions
TEST_F(CANProtocolTest, DecisionLogicProduction) {
    // Test toolbox activation logic
    EXPECT_FALSE(shouldActivateToolbox(false, false, false));  // Nothing ready
    EXPECT_FALSE(shouldActivateToolbox(true, false, false));   // System ready only
    EXPECT_FALSE(shouldActivateToolbox(true, true, false));    // System ready + parked
    EXPECT_FALSE(shouldActivateToolbox(true, false, true));    // System ready + unlocked
    EXPECT_TRUE(shouldActivateToolbox(true, true, true));      // All conditions met
    
    // Test bedlight logic
    EXPECT_FALSE(shouldEnableBedlight(0));  // OFF
    EXPECT_TRUE(shouldEnableBedlight(1));   // ON
    EXPECT_TRUE(shouldEnableBedlight(2));   // RAMP_UP  
    EXPECT_FALSE(shouldEnableBedlight(3));  // RAMP_DOWN
    
    // Test vehicle state logic
    EXPECT_FALSE(isVehicleUnlocked(0));  // LOCK_DBL
    EXPECT_FALSE(isVehicleUnlocked(1));  // LOCK_ALL
    EXPECT_TRUE(isVehicleUnlocked(2));   // UNLOCK_ALL
    EXPECT_TRUE(isVehicleUnlocked(3));   // UNLOCK_DRV
    
    EXPECT_FALSE(isVehicleParked(0));  // NotKnown
    EXPECT_TRUE(isVehicleParked(1));   // Park
    EXPECT_FALSE(isVehicleParked(2));  // Other gear
}

// Test realistic end-to-end scenario with ACTUAL production code
TEST_F(CANProtocolTest, EndToEndScenarioProduction) {
    // Scenario: Vehicle unlocked and parked, PUD lamp should be on
    uint8_t bcmData[8], lockData[8], parkData[8];
    
    // Create proper test data using signal setting
    CANTestUtils::setSignalValue(bcmData, 11, 2, 1);   // PudLamp = 1 (ON) - corrected bit position
    CANTestUtils::setSignalValue(lockData, 34, 2, 2);  // Lock = 2 (UNLOCK_ALL) - match production bit position
    CANTestUtils::setSignalValue(parkData, 31, 4, 1);  // Park = 1 (Park) - corrected bit position
    
    CANFrame bcmFrame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, 
                                   bcmData[0], bcmData[1], bcmData[2], bcmData[3],
                                   bcmData[4], bcmData[5], bcmData[6], bcmData[7]);
    CANFrame lockFrame = CANTestUtils::createCANFrame(LOCKING_SYSTEMS_2_FD1_ID,
                                    lockData[0], lockData[1], lockData[2], lockData[3],
                                    lockData[4], lockData[5], lockData[6], lockData[7]);
    CANFrame parkFrame = CANTestUtils::createCANFrame(POWERTRAIN_DATA_10_ID,
                                    parkData[0], parkData[1], parkData[2], parkData[3],
                                    parkData[4], parkData[5], parkData[6], parkData[7]);
    
    // Parse using ACTUAL production functions
    BCMLampData bcm = parseBCMLampFrame(&bcmFrame);
    LockingSystemsData lock = parseLockingSystemsFrame(&lockFrame);
    PowertrainData park = parsePowertrainFrame(&parkFrame);
    
    // Verify parsing worked
    EXPECT_TRUE(bcm.valid);
    EXPECT_TRUE(lock.valid);
    EXPECT_TRUE(park.valid);
    
    EXPECT_EQ(bcm.pudLampRequest, 1);      // ON
    EXPECT_EQ(lock.vehicleLockStatus, 2);  // UNLOCK_ALL  
    EXPECT_EQ(park.transmissionParkStatus, 1); // Park
    
    // Test ACTUAL decision logic
    EXPECT_TRUE(shouldEnableBedlight(bcm.pudLampRequest));
    EXPECT_TRUE(isVehicleUnlocked(lock.vehicleLockStatus));
    EXPECT_TRUE(isVehicleParked(park.transmissionParkStatus));
    EXPECT_TRUE(shouldActivateToolbox(true, isVehicleParked(park.transmissionParkStatus), isVehicleUnlocked(lock.vehicleLockStatus)));
}

// Test invalid frames are rejected properly
TEST_F(CANProtocolTest, InvalidFrameHandling) {
    // Test wrong CAN ID
    CANFrame wrongId = CANTestUtils::createCANFrame(0x999, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    BCMLampData bcmResult = parseBCMLampFrame(&wrongId);
    EXPECT_FALSE(bcmResult.valid);
    
    // Test null pointer
    BCMLampData nullResult = parseBCMLampFrame(nullptr);
    EXPECT_FALSE(nullResult.valid);
    
    // Test wrong length
    CANFrame shortFrame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    shortFrame.length = 4;  // Wrong length
    BCMLampData shortResult = parseBCMLampFrame(&shortFrame);
    EXPECT_FALSE(shortResult.valid);
}

// Critical: Test that changes to production code affect tests appropriately
TEST_F(CANProtocolTest, DetectProductionCodeChanges) {
    // If someone changes bit positions in production code, these tests WILL FAIL
    // This ensures our tests actually reflect the production implementation
    
    uint8_t testData[8];
    CANTestUtils::setSignalValue(testData, 11, 2, 2);  // PudLamp = 2 (RAMP_UP) at correct bit position
    CANFrame testFrame = CANTestUtils::createCANFrame(BCM_LAMP_STAT_FD1_ID, 
                                    testData[0], testData[1], testData[2], testData[3],
                                    testData[4], testData[5], testData[6], testData[7]);
    BCMLampData result = parseBCMLampFrame(&testFrame);
    
    // Based on current bit positions (11-12) in production code
    EXPECT_EQ(result.pudLampRequest, 2);  // RAMP_UP
    
    // If production logic changes, this will fail - which is GOOD!
    EXPECT_FALSE(shouldEnableBedlight(0));  // OFF should not enable bedlight  
    EXPECT_TRUE(shouldEnableBedlight(1));   // ON should enable bedlight
}
