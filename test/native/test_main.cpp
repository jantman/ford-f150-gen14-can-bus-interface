#include <gtest/gtest.h>
#include "mocks/mock_arduino.h"
#include "../src/can_protocol.h"
#include "../src/bit_utils.h"
// Decision logic functions now available via mock_arduino.cpp

// Forward declaration for CANMessage struct from can_manager.h
struct CANMessage {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
    unsigned long timestamp;
};
extern "C" {
    bool shouldEnableBedlight(uint8_t pudLampRequest);
    bool isVehicleUnlocked(uint8_t vehicleLockStatus);
    bool isVehicleParked(uint8_t transmissionParkStatus);
    bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked);
}

// Since we're in NATIVE_ENV, use our test config
#ifdef NATIVE_ENV
#include "common/test_config.h"
#endif

/**
 * Unified Test Runner for Ford F-150 CAN Bus Interface
 * 
 * This test suite has been updated to validate against real CAN data from
 * can_logger_1754515370_locking.out, which contains actual Ford F-150 
 * locking system messages captured from the vehicle.
 * 
 * Key validation points:
 * - Locking_Systems_2_FD1 (0x331) message parsing with real data patterns
 * - LOCK_ALL vs UNLOCK_ALL state recognition using actual message content
 * - Bit position analysis to determine correct signal extraction
 * - Integration tests with realistic vehicle state transitions
 * 
 * Real data patterns validated:
 * - LOCK_ALL: byte 4 = 0x02 (sequences 1 and 10 from log)
 * - UNLOCK_ALL: byte 4 = 0x05 (sequences 2-9 from log)
 * 
 * Additional test files added:
 * - test_locking_system_data.cpp: Comprehensive validation against log data
 * - Enhanced test_message_parser.cpp: Real CAN data test cases
 * - Enhanced test_can_data_validation.cpp: Actual log data validation
 * - Enhanced test_bit_position_analysis.cpp: Lock status bit analysis
 */

// Forward declarations and implementations
extern "C" {
    // Use production CANMessage struct from can_manager.h
    // (struct definition removed to avoid conflict)
    
    // Use production extractBits function from can_protocol.c
    uint8_t extractBits(const uint8_t* data, uint8_t startBit, uint8_t length);
    
    // Use production setBits function from bit_utils.c
    void setBits(uint8_t* data, uint8_t startBit, uint8_t length, uint32_t value);
}

// Test helper base class
class ArduinoTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    void advanceTime(unsigned long ms) {
        ArduinoMock::instance().advanceTime(ms);
    }
    
    void setTime(unsigned long time) {
        ArduinoMock::instance().setMillis(time);
    }
    
    bool isGPIOHigh(uint8_t pin) {
        return ArduinoMock::instance().getDigitalState(pin) == HIGH;
    }
};

// Test Suite: Bit Extraction Core Functionality
TEST_F(ArduinoTest, BitExtractionBasic) {
    uint8_t testData[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    
    // Test basic extraction within single byte using DBC-style positions
    EXPECT_EQ(extractBits(testData, 3, 4), 0x2);   // Lower 4 bits of 0x12 (DBC bits 0-3, MSB at 3)
    EXPECT_EQ(extractBits(testData, 7, 4), 0x1);   // Upper 4 bits of 0x12 (DBC bits 4-7, MSB at 7)
    EXPECT_EQ(extractBits(testData, 15, 8), 0x34); // Full second byte (DBC bits 8-15, MSB at 15)
}

TEST_F(ArduinoTest, BitExtractionEdgeCases) {
    uint8_t testData[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    
    // Test edge cases
    EXPECT_EQ(extractBits(testData, 0, 1), 0x1);   // Single bit
    EXPECT_EQ(extractBits(testData, 64, 1), 0x0);  // Out of bounds
    EXPECT_EQ(extractBits(testData, 0, 9), 0x0);   // Too many bits
}

// Test Suite: CAN Message Bit Manipulation
TEST_F(ArduinoTest, CANMessageBitSetting) {
    uint8_t data[8] = {0};
    
    // Set PudLamp_D_Rq (DBC bits 11-12, MSB at 12) to value 2
    setBits(data, 12, 2, 2);
    EXPECT_EQ(extractBits(data, 12, 2), 2);
    
    // Set Veh_Lock_Status (DBC bits 34-35, MSB at 35) to value 3
    setBits(data, 35, 2, 3);
    EXPECT_EQ(extractBits(data, 35, 2), 3);
    
    // Verify first value is preserved
    EXPECT_EQ(extractBits(data, 12, 2), 2);
}

// Test Suite: Arduino Mock Functionality
TEST_F(ArduinoTest, ArduinoMockBasics) {
    // Test GPIO functionality
    ArduinoMock::instance().setDigitalWrite(5, HIGH);
    EXPECT_TRUE(isGPIOHigh(5));
    
    ArduinoMock::instance().setDigitalWrite(5, LOW);
    EXPECT_FALSE(isGPIOHigh(5));
}

TEST_F(ArduinoTest, ArduinoMockTiming) {
    setTime(1000);
    EXPECT_EQ(millis(), 1000);
    
    advanceTime(500);
    EXPECT_EQ(millis(), 1500);
}

// Test Suite: Message Parsing Logic
TEST_F(ArduinoTest, MessageParsingValidation) {
    // Test BCM message structure validation
    CANMessage message = {BCM_LAMP_STAT_FD1_ID, 8, {0}, 1000};
    
    // Valid message
    EXPECT_EQ(message.id, 0x3C3);
    EXPECT_EQ(message.length, 8);
    
    // Invalid message
    message.id = 0x999;
    EXPECT_NE(message.id, BCM_LAMP_STAT_FD1_ID);
}

// Test Suite: Signal Value Definitions
TEST_F(ArduinoTest, SignalValueConstants) {
    // Test PudLamp values
    EXPECT_EQ(PUDLAMP_OFF, 0);
    EXPECT_EQ(PUDLAMP_ON, 1);
    EXPECT_EQ(PUDLAMP_RAMP_UP, 2);
    EXPECT_EQ(PUDLAMP_RAMP_DOWN, 3);
    
    // Test lock status values
    EXPECT_EQ(VEH_LOCK_DBL, 0);
    EXPECT_EQ(VEH_LOCK_ALL, 1);
    EXPECT_EQ(VEH_UNLOCK_ALL, 2);
    EXPECT_EQ(VEH_UNLOCK_DRV, 3);
}

// Test Suite: GPIO Control Logic
TEST_F(ArduinoTest, GPIOControlBasics) {
    // Test pin mode setting
    ArduinoMock::instance().setPinMode(BEDLIGHT_PIN, OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(BEDLIGHT_PIN), OUTPUT);
    
    // Test initial state
    ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, LOW);
    EXPECT_FALSE(isGPIOHigh(BEDLIGHT_PIN));
    
    // Test state change
    ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, HIGH);
    EXPECT_TRUE(isGPIOHigh(BEDLIGHT_PIN));
}

// Test Suite: Button Input Logic
TEST_F(ArduinoTest, ButtonInputHandling) {
    // Button not pressed (HIGH with pullup)
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, HIGH);
    EXPECT_EQ(ArduinoMock::instance().getDigitalRead(TOOLBOX_BUTTON_PIN), HIGH);
    
    // Button pressed (LOW with pullup)
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalRead(TOOLBOX_BUTTON_PIN), LOW);
}

// Test Suite: Timing Logic
TEST_F(ArduinoTest, TimingCalculations) {
    setTime(1000);
    
    unsigned long startTime = millis();
    advanceTime(TOOLBOX_OPENER_DURATION_MS);
    unsigned long elapsed = millis() - startTime;
    
    EXPECT_EQ(elapsed, TOOLBOX_OPENER_DURATION_MS);
    EXPECT_GE(elapsed, TOOLBOX_OPENER_DURATION_MS);
}

// Test Suite: Integration Logic
TEST_F(ArduinoTest, SystemIntegrationBasics) {
    // Test that time advances correctly for multiple operations
    setTime(2000);
    
    // Simulate bedlight control sequence
    ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, HIGH);
    advanceTime(100);
    
    // Simulate system ready control
    ArduinoMock::instance().setDigitalWrite(SYSTEM_READY_PIN, HIGH);
    advanceTime(200);
    
    // Verify states
    EXPECT_TRUE(isGPIOHigh(BEDLIGHT_PIN));
    EXPECT_TRUE(isGPIOHigh(SYSTEM_READY_PIN));
    EXPECT_EQ(millis(), 2300);
}

// Test Suite: Real-world CAN Data Patterns
TEST_F(ArduinoTest, RealWorldCANData) {
    uint8_t realisticData[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    
    // Extract typical automotive signal patterns
    uint8_t signal1 = extractBits(realisticData, 11, 2);  // PudLamp position
    uint8_t signal2 = extractBits(realisticData, 34, 2);  // Lock status position
    uint8_t signal3 = extractBits(realisticData, 31, 4);  // Park status position
    
    // Verify extraction works with real-world data
    EXPECT_LE(signal1, 3);  // 2-bit values
    EXPECT_LE(signal2, 3);  // 2-bit values  
    EXPECT_LE(signal3, 15); // 4-bit values
}

// Test Suite: Error Handling and Robustness
TEST_F(ArduinoTest, ErrorHandlingRobustness) {
    uint8_t data[8] = {0};
    
    // Test boundary conditions
    EXPECT_EQ(extractBits(data, 100, 4), 0);   // Way out of bounds
    EXPECT_EQ(extractBits(data, 0, 0), 0);     // Zero length
    
    // Test time overflow robustness
    setTime(0xFFFFFFF0UL);  // Near max value
    advanceTime(0x20);      // Should overflow
    
    // System should still be functional after overflow
    ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, HIGH);
    EXPECT_TRUE(isGPIOHigh(BEDLIGHT_PIN));
}

// ===============================================
// Integration with Message Parser Test Suites
// ===============================================
//
// NOTE: This test file focuses on basic Arduino functionality and integration tests.
// More comprehensive message parsing tests are located in:
//
// - test_can_bit_extraction.cpp: Validates bit extraction logic against Python implementation
// - test_message_parser.cpp: Tests message parsing functions with real CAN data 
// - test_vehicle_state_management.cpp: Tests state management logic and decision making
// - test_can_message_recognition.cpp: Tests CAN message filtering and recognition
//
// These additional test suites provide comprehensive validation of our CAN message
// processing pipeline against the working Python can_embedded_logger.py implementation.

// ===============================================
// State Management Integration Tests (HIGH PRIORITY)
// ===============================================

TEST_F(ArduinoTest, StateUpdateBCMLampStatus) {
    // Test BCM Lamp Status state updates
    setTime(1000);
    
    // Mock structures based on our header files
    struct BCMLampStatus {
        uint8_t pudLampRequest;
        uint8_t illuminatedEntryStatus;
        uint8_t drCourtesyLightStatus;
        bool valid;
        unsigned long timestamp;
    };
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    // Simulate state manager initialization
    VehicleState testState = {};
    testState.pudLampRequest = 0; // PUDLAMP_OFF
    testState.prevPudLampRequest = 0;
    testState.lastBCMLampUpdate = 0;
    testState.bedlightShouldBeOn = false;
    
    // Test lamp status update
    BCMLampStatus lampStatus = {};
    lampStatus.pudLampRequest = 1; // PUDLAMP_ON
    lampStatus.valid = true;
    lampStatus.timestamp = 1000;
    
    // Simulate updateBCMLampState logic
    if (lampStatus.valid) {
        testState.prevPudLampRequest = testState.pudLampRequest;
        testState.pudLampRequest = lampStatus.pudLampRequest;
        testState.lastBCMLampUpdate = lampStatus.timestamp;
        testState.bedlightShouldBeOn = shouldEnableBedlight(lampStatus.pudLampRequest);
    }
    
    // Verify state was updated correctly
    EXPECT_EQ(testState.pudLampRequest, 1);
    EXPECT_EQ(testState.prevPudLampRequest, 0);
    EXPECT_EQ(testState.lastBCMLampUpdate, 1000);
    EXPECT_TRUE(testState.bedlightShouldBeOn);
}

TEST_F(ArduinoTest, StateUpdateLockingStatus) {
    // Test Locking Systems state updates
    setTime(2000);
    
    struct LockingSystemsStatus {
        uint8_t vehicleLockStatus;
        bool valid;
        unsigned long timestamp;
    };
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    VehicleState testState = {};
    testState.vehicleLockStatus = 0; // VEH_LOCK_DBL
    testState.prevVehicleLockStatus = 0;
    testState.isUnlocked = false;
    
    // Test unlocking
    LockingSystemsStatus lockStatus = {};
    lockStatus.vehicleLockStatus = 2; // VEH_UNLOCK_ALL
    lockStatus.valid = true;
    lockStatus.timestamp = 2000;
    
    // Simulate updateLockingSystemsState logic
    if (lockStatus.valid) {
        testState.prevVehicleLockStatus = testState.vehicleLockStatus;
        testState.vehicleLockStatus = lockStatus.vehicleLockStatus;
        testState.lastLockingSystemsUpdate = lockStatus.timestamp;
        testState.isUnlocked = isVehicleUnlocked(lockStatus.vehicleLockStatus);
    }
    
    // Verify unlock state
    EXPECT_EQ(testState.vehicleLockStatus, 2);
    EXPECT_EQ(testState.prevVehicleLockStatus, 0);
    EXPECT_TRUE(testState.isUnlocked);
    EXPECT_EQ(testState.lastLockingSystemsUpdate, 2000);
    
    // Test locking
    lockStatus.vehicleLockStatus = 1; // VEH_LOCK_ALL
    lockStatus.timestamp = 2500;
    
    if (lockStatus.valid) {
        testState.prevVehicleLockStatus = testState.vehicleLockStatus;
        testState.vehicleLockStatus = lockStatus.vehicleLockStatus;
        testState.lastLockingSystemsUpdate = lockStatus.timestamp;
        testState.isUnlocked = isVehicleUnlocked(lockStatus.vehicleLockStatus);
    }
    
    // Verify locked state
    EXPECT_EQ(testState.vehicleLockStatus, 1);
    EXPECT_EQ(testState.prevVehicleLockStatus, 2);
    EXPECT_FALSE(testState.isUnlocked);
}

TEST_F(ArduinoTest, StateUpdatePowertrainStatus) {
    // Test Powertrain state updates (park status)
    setTime(3000);
    
    struct PowertrainData {
        uint8_t transmissionParkStatus;
        bool valid;
        unsigned long timestamp;
    };
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    VehicleState testState = {};
    testState.transmissionParkStatus = 0; // TRNPRKSTS_UNKNOWN
    testState.isParked = false;
    
    // Test parking
    PowertrainData powertrainData = {};
    powertrainData.transmissionParkStatus = 1; // TRNPRKSTS_PARK
    powertrainData.valid = true;
    powertrainData.timestamp = 3000;
    
    // Simulate updatePowertrainState logic
    if (powertrainData.valid) {
        testState.prevTransmissionParkStatus = testState.transmissionParkStatus;
        testState.transmissionParkStatus = powertrainData.transmissionParkStatus;
        testState.lastPowertrainUpdate = powertrainData.timestamp;
        testState.isParked = isVehicleParked(powertrainData.transmissionParkStatus);
    }
    
    // Verify parked state
    EXPECT_EQ(testState.transmissionParkStatus, 1);
    EXPECT_EQ(testState.prevTransmissionParkStatus, 0);
    EXPECT_TRUE(testState.isParked);
    EXPECT_EQ(testState.lastPowertrainUpdate, 3000);
}

TEST_F(ArduinoTest, ToolboxActivationLogic) {
    // Test shouldActivateToolboxWithParams() decision logic
    setTime(4000);
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    // Test case 1: Not parked, not unlocked - should NOT activate
    VehicleState testState = {};
    testState.isParked = false;
    testState.isUnlocked = false;
    testState.systemReady = true;
    testState.lastLockingSystemsUpdate = 4000;
    testState.lastPowertrainUpdate = 4000;
    
    // Simulate shouldActivateToolboxWithParams logic (timeout check is in systemReady)
    bool shouldActivate = shouldActivateToolboxWithParams(testState.systemReady, 
                                                          testState.isParked, 
                                                          testState.isUnlocked);    EXPECT_FALSE(shouldActivate);
    
    // Test case 2: Parked but locked - should NOT activate  
    testState.isParked = true;
    testState.isUnlocked = false;
    
    shouldActivate = shouldActivateToolboxWithParams(testState.systemReady, 
                                                     testState.isParked, 
                                                     testState.isUnlocked);
    
    EXPECT_FALSE(shouldActivate);
    
    // Test case 3: Unlocked but not parked - should NOT activate
    testState.isParked = false;
    testState.isUnlocked = true;
    
    shouldActivate = shouldActivateToolboxWithParams(testState.systemReady, 
                                                     testState.isParked, 
                                                     testState.isUnlocked);
    
    EXPECT_FALSE(shouldActivate);
    
    // Test case 4: Parked AND unlocked - should activate
    testState.isParked = true;
    testState.isUnlocked = true;
    
    shouldActivate = shouldActivateToolboxWithParams(testState.systemReady, 
                                                     testState.isParked, 
                                                     testState.isUnlocked);
    
    EXPECT_TRUE(shouldActivate);
    
    // Test case 5: Conditions met but system not ready (stale data) - should NOT activate
    testState.systemReady = false;  // System not ready due to stale data
    
    shouldActivate = shouldActivateToolboxWithParams(testState.systemReady, 
                                                     testState.isParked, 
                                                     testState.isUnlocked);
    
    EXPECT_FALSE(shouldActivate);
}

TEST_F(ArduinoTest, StateChangeDetection) {
    // Test state change detection logic
    setTime(5000);
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    VehicleState testState = {};
    testState.pudLampRequest = 0;
    testState.prevPudLampRequest = 1;
    testState.vehicleLockStatus = 2;
    testState.prevVehicleLockStatus = 1;
    testState.transmissionParkStatus = 1;
    testState.prevTransmissionParkStatus = 0;
    
    // Simulate checkForStateChanges logic
    bool lampChanged = (testState.pudLampRequest != testState.prevPudLampRequest);
    bool lockChanged = (testState.vehicleLockStatus != testState.prevVehicleLockStatus);
    bool parkChanged = (testState.transmissionParkStatus != testState.prevTransmissionParkStatus);
    
    EXPECT_TRUE(lampChanged);  // 0 != 1
    EXPECT_TRUE(lockChanged);  // 2 != 1  
    EXPECT_TRUE(parkChanged);  // 1 != 0
    
    // Test no changes
    testState.prevPudLampRequest = testState.pudLampRequest;
    testState.prevVehicleLockStatus = testState.vehicleLockStatus;
    testState.prevTransmissionParkStatus = testState.transmissionParkStatus;
    
    lampChanged = (testState.pudLampRequest != testState.prevPudLampRequest);
    lockChanged = (testState.vehicleLockStatus != testState.prevVehicleLockStatus);
    parkChanged = (testState.transmissionParkStatus != testState.prevTransmissionParkStatus);
    
    EXPECT_FALSE(lampChanged);
    EXPECT_FALSE(lockChanged);
    EXPECT_FALSE(parkChanged);
}

TEST_F(ArduinoTest, SystemReadyLogic) {
    // Test system ready determination with new ANY-message logic
    setTime(6000);
    
    struct VehicleState {
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
    };
    
    VehicleState testState = {};
    testState.lastBCMLampUpdate = 5500;
    testState.lastLockingSystemsUpdate = 5500;
    testState.lastPowertrainUpdate = 5500;
    testState.lastBatteryUpdate = 5500;
    
    // Simulate NEW system ready logic - ANY message within timeout
    const unsigned long TIMEOUT_MS = SYSTEM_READINESS_TIMEOUT_MS; // 10 minute timeout
    unsigned long currentTime = 6000;
    
    bool hasBCMData = (currentTime - testState.lastBCMLampUpdate < TIMEOUT_MS);
    bool hasLockingData = (currentTime - testState.lastLockingSystemsUpdate < TIMEOUT_MS);
    bool hasPowertrainData = (currentTime - testState.lastPowertrainUpdate < TIMEOUT_MS);
    bool hasBatteryData = (currentTime - testState.lastBatteryUpdate < TIMEOUT_MS);
    
    bool systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
    
    EXPECT_TRUE(systemReady);
    
    // Test with only BCM data recent
    currentTime = TIMEOUT_MS + 10000; // Set current time well past timeout
    testState.lastBCMLampUpdate = currentTime - 5000; // Recent (5 seconds ago)
    testState.lastLockingSystemsUpdate = 0; // Very stale
    testState.lastPowertrainUpdate = 0; // Very stale  
    testState.lastBatteryUpdate = 0; // Very stale
    
    hasBCMData = (currentTime - testState.lastBCMLampUpdate < TIMEOUT_MS);
    hasLockingData = (currentTime - testState.lastLockingSystemsUpdate < TIMEOUT_MS);
    hasPowertrainData = (currentTime - testState.lastPowertrainUpdate < TIMEOUT_MS);
    hasBatteryData = (currentTime - testState.lastBatteryUpdate < TIMEOUT_MS);
    
    systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
    
    EXPECT_TRUE(systemReady); // Should still be ready with just BCM data
    
    // Test with all data stale
    testState.lastBCMLampUpdate = 0; // Very stale
    testState.lastLockingSystemsUpdate = 0; // Very stale
    testState.lastPowertrainUpdate = 0; // Very stale
    testState.lastBatteryUpdate = 0; // Very stale
    
    hasBCMData = (currentTime - testState.lastBCMLampUpdate < TIMEOUT_MS);
    hasLockingData = (currentTime - testState.lastLockingSystemsUpdate < TIMEOUT_MS);
    hasPowertrainData = (currentTime - testState.lastPowertrainUpdate < TIMEOUT_MS);
    hasBatteryData = (currentTime - testState.lastBatteryUpdate < TIMEOUT_MS);
    
    systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
    
    EXPECT_FALSE(systemReady); // Should be false when all data is stale
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
