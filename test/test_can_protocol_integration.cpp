#include <gtest/gtest.h>

// Include the ACTUAL production code (pure logic, no Arduino dependencies)
extern "C" {
    #include "../src/can_protocol.h"
    #include "../src/config.h"  // For CAN message ID constants
}

class CANProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed for pure logic tests
    }
    
    void TearDown() override {
        // No cleanup needed
    }
    
    // Helper to create test CAN frames
    CANFrame createFrame(uint32_t id, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, 
                        uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
        CANFrame frame;
        frame.id = id;
        frame.length = 8;
        frame.data[0] = b0; frame.data[1] = b1; frame.data[2] = b2; frame.data[3] = b3;
        frame.data[4] = b4; frame.data[5] = b5; frame.data[6] = b6; frame.data[7] = b7;
        return frame;
    }
};

// Test the ACTUAL extractBits function from production code
TEST_F(CANProtocolTest, BitExtractionProduction) {
    uint8_t testData[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    
    // Test basic bit extraction using ACTUAL PRODUCTION CODE
    EXPECT_EQ(extractBits(testData, 0, 4), 0x2);   // Lower 4 bits of 0x12
    EXPECT_EQ(extractBits(testData, 4, 4), 0x1);   // Upper 4 bits of 0x12  
    EXPECT_EQ(extractBits(testData, 8, 8), 0x34);  // Full second byte
    
    // Test edge cases
    EXPECT_EQ(extractBits(testData, 0, 1), 0x0);   // Single bit (LSB of 0x12)
    EXPECT_EQ(extractBits(testData, 1, 1), 0x1);   // Second bit
    EXPECT_EQ(extractBits(testData, 64, 1), 0x0);  // Out of bounds
    EXPECT_EQ(extractBits(testData, 0, 9), 0x0);   // Too many bits
}

// Test the ACTUAL BCM lamp parsing function
TEST_F(CANProtocolTest, BCMLampParsingProduction) {
    // Create test frame with PudLamp_D_Rq = 2 (RAMP_UP) at bits 11-12
    // Value 2 = binary 10, so bit 11=0, bit 12=1
    // Bit 12 is in byte 1, position 4: 0x10
    CANFrame frame = createFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    BCMLampData result = parseBCMLampFrame(&frame);
    
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.pudLampRequest, 2);  // RAMP_UP
}

// Test the ACTUAL locking systems parsing function  
TEST_F(CANProtocolTest, LockingSystemsParsingProduction) {
    // Create test frame with Veh_Lock_Status = 3 (UNLOCK_DRV) at bits 34-35
    CANFrame frame = createFrame(LOCKING_SYSTEMS_2_FD1_ID, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00);
    
    LockingSystemsData result = parseLockingSystemsFrame(&frame);
    
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.vehicleLockStatus, 3);  // UNLOCK_DRV
}

// Test the ACTUAL powertrain parsing function
TEST_F(CANProtocolTest, PowertrainParsingProduction) {
    // Create test frame with TrnPrkSys_D_Actl = 1 (Park) at bits 31-34
    CANFrame frame = createFrame(POWERTRAIN_DATA_10_ID, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00);
    
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
    CANFrame bcmFrame = createFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // PudLamp = 1 (ON)
    CANFrame lockFrame = createFrame(LOCKING_SYSTEMS_2_FD1_ID, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00); // Lock = 2 (UNLOCK_ALL)
    CANFrame parkFrame = createFrame(POWERTRAIN_DATA_10_ID, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00); // Park = 1 (Park)
    
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
    CANFrame wrongId = createFrame(0x999, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    BCMLampData bcmResult = parseBCMLampFrame(&wrongId);
    EXPECT_FALSE(bcmResult.valid);
    
    // Test null pointer
    BCMLampData nullResult = parseBCMLampFrame(nullptr);
    EXPECT_FALSE(nullResult.valid);
    
    // Test wrong length
    CANFrame shortFrame = createFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    shortFrame.length = 4;  // Wrong length
    BCMLampData shortResult = parseBCMLampFrame(&shortFrame);
    EXPECT_FALSE(shortResult.valid);
}

// Critical: Test that changes to production code affect tests appropriately
TEST_F(CANProtocolTest, DetectProductionCodeChanges) {
    // If someone changes bit positions in production code, these tests WILL FAIL
    // This ensures our tests actually reflect the production implementation
    
    CANFrame testFrame = createFrame(BCM_LAMP_STAT_FD1_ID, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    BCMLampData result = parseBCMLampFrame(&testFrame);
    
    // Based on current bit positions (11-12) in production code
    EXPECT_EQ(result.pudLampRequest, 2);  // RAMP_UP
    
    // If production logic changes, this will fail - which is GOOD!
    EXPECT_FALSE(shouldEnableBedlight(0));  // OFF should not enable bedlight  
    EXPECT_TRUE(shouldEnableBedlight(1));   // ON should enable bedlight
}
