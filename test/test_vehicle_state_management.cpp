#include <gtest/gtest.h>
#include "mocks/mock_arduino.h"
#include "common/test_config.h"

// Import production code structures and functions
extern "C" {
    #include "../src/can_protocol.h"
}

/**
 * Vehicle State Management Test Suite
 * 
 * These tests validate our C++ vehicle state management logic against the behavior
 * defined in the Python can_embedded_logger.py implementation. The Python code shows
 * us the expected signal values and how they should be interpreted for vehicle state.
 * 
 * Key Python signal interpretations:
 * - PudLamp_D_Rq: {0: "OFF", 1: "ON", 2: "RAMP_UP", 3: "RAMP_DOWN"} 
 * - Veh_Lock_Status: {0: "LOCK_DBL", 1: "LOCK_ALL", 2: "UNLOCK_ALL", 3: "UNLOCK_DRV"}
 * - TrnPrkSys_D_Actl: {0: "NotKnown", 1: "Park", 2-5: various transitions, 6+: errors}
 * - BSBattSOC: Raw percentage value (0-127)
 * 
 * This suite tests the state logic that determines when to activate outputs based on
 * combinations of these signals.
 */

class VehicleStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoMock::instance().reset();
        
        // Initialize mock vehicle state
        initializeVehicleState();
    }
    
    void TearDown() override {
        ArduinoMock::instance().reset();
    }
    
    // Mock vehicle state structure based on our application
    struct MockVehicleState {
        // Current signal values
        uint8_t pudLampRequest;
        uint8_t vehicleLockStatus;
        uint8_t transmissionParkStatus;
        uint8_t batterySOC;
        
        // Previous signal values for change detection
        uint8_t prevPudLampRequest;
        uint8_t prevVehicleLockStatus;
        uint8_t prevTransmissionParkStatus;
        uint8_t prevBatterySOC;
        
        // Last update timestamps
        unsigned long lastBCMLampUpdate;
        unsigned long lastLockingSystemsUpdate;
        unsigned long lastPowertrainUpdate;
        unsigned long lastBatteryUpdate;
        
        // Derived state flags
        bool isUnlocked;
        bool isParked;
        bool bedlightShouldBeOn;
        bool systemReady;
        
        // Output control flags
        bool toolboxShouldOpen;
        bool parkedLEDShouldBeOn;
        bool unlockedLEDShouldBeOn;
    } vehicleState;
    
    void initializeVehicleState() {
        memset(&vehicleState, 0, sizeof(vehicleState));
        vehicleState.pudLampRequest = 0xFF; // Invalid initial value
        vehicleState.vehicleLockStatus = 0xFF;
        vehicleState.transmissionParkStatus = 0xFF;
        vehicleState.batterySOC = 0xFF;
        vehicleState.prevPudLampRequest = 0xFF;
        vehicleState.prevVehicleLockStatus = 0xFF;
        vehicleState.prevTransmissionParkStatus = 0xFF;
        vehicleState.prevBatterySOC = 0xFF;
    }
    
    // Simulate receiving a BCM Lamp Status message
    void updateBCMLampStatus(uint8_t pudLamp, uint8_t illuminatedEntry, uint8_t courtesyLight, unsigned long timestamp) {
        vehicleState.prevPudLampRequest = vehicleState.pudLampRequest;
        vehicleState.pudLampRequest = pudLamp;
        vehicleState.lastBCMLampUpdate = timestamp;
        
        // Update derived state - bedlight should be on when puddle lamps are ON or RAMP_UP
        vehicleState.bedlightShouldBeOn = (pudLamp == PUDLAMP_ON || pudLamp == PUDLAMP_RAMP_UP);
    }
    
    // Simulate receiving a Locking Systems message
    void updateLockingSystemsStatus(uint8_t lockStatus, unsigned long timestamp) {
        vehicleState.prevVehicleLockStatus = vehicleState.vehicleLockStatus;
        vehicleState.vehicleLockStatus = lockStatus;
        vehicleState.lastLockingSystemsUpdate = timestamp;
        
        // Update derived state - unlocked when UNLOCK_ALL or UNLOCK_DRV
        vehicleState.isUnlocked = (lockStatus == VEH_UNLOCK_ALL || lockStatus == VEH_UNLOCK_DRV);
        vehicleState.unlockedLEDShouldBeOn = vehicleState.isUnlocked;
    }
    
    // Simulate receiving a Powertrain message
    void updatePowertrainStatus(uint8_t parkStatus, unsigned long timestamp) {
        vehicleState.prevTransmissionParkStatus = vehicleState.transmissionParkStatus;
        vehicleState.transmissionParkStatus = parkStatus;
        vehicleState.lastPowertrainUpdate = timestamp;
        
        // Update derived state - parked when status is PARK
        vehicleState.isParked = (parkStatus == TRNPRKSTS_PARK);
        vehicleState.parkedLEDShouldBeOn = vehicleState.isParked;
    }
    
    // Simulate receiving a Battery Management message
    void updateBatteryStatus(uint8_t soc, unsigned long timestamp) {
        vehicleState.prevBatterySOC = vehicleState.batterySOC;
        vehicleState.batterySOC = soc;
        vehicleState.lastBatteryUpdate = timestamp;
    }
    
    // Update system ready status based on message freshness
    void updateSystemReady(unsigned long currentTime) {
        const unsigned long TIMEOUT_MS = SYSTEM_READINESS_TIMEOUT_MS; // 10 minute timeout for ANY message
        
        // System is ready if ANY message type has been received within the timeout window
        bool hasBCMData = (currentTime - vehicleState.lastBCMLampUpdate) < TIMEOUT_MS;
        bool hasLockingData = (currentTime - vehicleState.lastLockingSystemsUpdate) < TIMEOUT_MS;
        bool hasPowertrainData = (currentTime - vehicleState.lastPowertrainUpdate) < TIMEOUT_MS;
        bool hasBatteryData = (currentTime - vehicleState.lastBatteryUpdate) < TIMEOUT_MS;
        
        vehicleState.systemReady = hasBCMData || hasLockingData || hasPowertrainData || hasBatteryData;
    }
    
    // Check if toolbox should be activated based on current state
    void updateToolboxActivation() {
        // Toolbox should open when vehicle is parked AND unlocked AND system is ready
        vehicleState.toolboxShouldOpen = vehicleState.systemReady && 
                                       vehicleState.isParked && 
                                       vehicleState.isUnlocked;
    }
    
    // Helper to advance time and update all derived states
    void advanceTimeAndUpdate(unsigned long newTime) {
        ArduinoMock::instance().setMillis(newTime);
        updateSystemReady(newTime);
        updateToolboxActivation();
    }
};

// ===============================================
// Test Suite: Basic Signal Value Interpretation
// ===============================================

TEST_F(VehicleStateTest, PudLampSignalInterpretation) {
    // Test PudLamp_D_Rq signal interpretation matching Python values
    
    unsigned long baseTime = 1000;
    
    // Test OFF state (Python: {0: "OFF"})
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime);
    EXPECT_EQ(vehicleState.pudLampRequest, PUDLAMP_OFF);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn);
    
    // Test ON state (Python: {1: "ON"})  
    updateBCMLampStatus(PUDLAMP_ON, 0, 0, baseTime + 100);
    EXPECT_EQ(vehicleState.pudLampRequest, PUDLAMP_ON);
    EXPECT_TRUE(vehicleState.bedlightShouldBeOn);
    
    // Test RAMP_UP state (Python: {2: "RAMP_UP"})
    updateBCMLampStatus(PUDLAMP_RAMP_UP, 0, 0, baseTime + 200);
    EXPECT_EQ(vehicleState.pudLampRequest, PUDLAMP_RAMP_UP);
    EXPECT_TRUE(vehicleState.bedlightShouldBeOn);
    
    // Test RAMP_DOWN state (Python: {3: "RAMP_DOWN"})
    updateBCMLampStatus(PUDLAMP_RAMP_DOWN, 0, 0, baseTime + 300);
    EXPECT_EQ(vehicleState.pudLampRequest, PUDLAMP_RAMP_DOWN);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn); // Should be off during ramp down
}

TEST_F(VehicleStateTest, VehicleLockSignalInterpretation) {
    // Test Veh_Lock_Status signal interpretation matching Python values
    
    unsigned long baseTime = 2000;
    
    // Test LOCK_DBL state (Python: {0: "LOCK_DBL"})
    updateLockingSystemsStatus(VEH_LOCK_DBL, baseTime);
    EXPECT_EQ(vehicleState.vehicleLockStatus, VEH_LOCK_DBL);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.unlockedLEDShouldBeOn);
    
    // Test LOCK_ALL state (Python: {1: "LOCK_ALL"})
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime + 100);
    EXPECT_EQ(vehicleState.vehicleLockStatus, VEH_LOCK_ALL);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.unlockedLEDShouldBeOn);
    
    // Test UNLOCK_ALL state (Python: {2: "UNLOCK_ALL"})
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime + 200);
    EXPECT_EQ(vehicleState.vehicleLockStatus, VEH_UNLOCK_ALL);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_TRUE(vehicleState.unlockedLEDShouldBeOn);
    
    // Test UNLOCK_DRV state (Python: {3: "UNLOCK_DRV"})
    updateLockingSystemsStatus(VEH_UNLOCK_DRV, baseTime + 300);
    EXPECT_EQ(vehicleState.vehicleLockStatus, VEH_UNLOCK_DRV);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_TRUE(vehicleState.unlockedLEDShouldBeOn);
}

TEST_F(VehicleStateTest, TransmissionParkSignalInterpretation) {
    // Test TrnPrkSys_D_Actl signal interpretation matching Python values
    
    unsigned long baseTime = 3000;
    
    // Test NotKnown state (Python: {0: "NotKnown"})
    updatePowertrainStatus(TRNPRKSTS_UNKNOWN, baseTime);
    EXPECT_EQ(vehicleState.transmissionParkStatus, TRNPRKSTS_UNKNOWN);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.parkedLEDShouldBeOn);
    
    // Test Park state (Python: {1: "Park"})
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime + 100);
    EXPECT_EQ(vehicleState.transmissionParkStatus, TRNPRKSTS_PARK);
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_TRUE(vehicleState.parkedLEDShouldBeOn);
    
    // Test transition states (should not be considered "parked")
    updatePowertrainStatus(TRNPRKSTS_TRANSITION_CLOSE_TO_PARK, baseTime + 200);
    EXPECT_EQ(vehicleState.transmissionParkStatus, TRNPRKSTS_TRANSITION_CLOSE_TO_PARK);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.parkedLEDShouldBeOn);
    
    updatePowertrainStatus(TRNPRKSTS_OUT_OF_PARK, baseTime + 300);
    EXPECT_EQ(vehicleState.transmissionParkStatus, TRNPRKSTS_OUT_OF_PARK);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.parkedLEDShouldBeOn);
}

TEST_F(VehicleStateTest, BatterySOCSignalInterpretation) {
    // Test BSBattSOC signal interpretation (Python: Raw percentage value)
    
    unsigned long baseTime = 4000;
    
    // Test various SOC values
    uint8_t testSOCs[] = {0, 25, 50, 75, 85, 100, 127};
    
    for (uint8_t soc : testSOCs) {
        updateBatteryStatus(soc, baseTime);
        EXPECT_EQ(vehicleState.batterySOC, soc);
        baseTime += 100;
    }
}

// ===============================================
// Test Suite: State Change Detection
// ===============================================

TEST_F(VehicleStateTest, StateChangeDetection) {
    unsigned long baseTime = 5000;
    
    // Initialize with known values
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime);
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_UNKNOWN, baseTime);
    updateBatteryStatus(50, baseTime);
    
    // Change PudLamp status
    updateBCMLampStatus(PUDLAMP_ON, 0, 0, baseTime + 100);
    EXPECT_NE(vehicleState.pudLampRequest, vehicleState.prevPudLampRequest);
    EXPECT_EQ(vehicleState.prevPudLampRequest, PUDLAMP_OFF);
    EXPECT_EQ(vehicleState.pudLampRequest, PUDLAMP_ON);
    
    // Change lock status
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime + 200);
    EXPECT_NE(vehicleState.vehicleLockStatus, vehicleState.prevVehicleLockStatus);
    EXPECT_EQ(vehicleState.prevVehicleLockStatus, VEH_LOCK_ALL);
    EXPECT_EQ(vehicleState.vehicleLockStatus, VEH_UNLOCK_ALL);
    
    // Change park status
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime + 300);
    EXPECT_NE(vehicleState.transmissionParkStatus, vehicleState.prevTransmissionParkStatus);
    EXPECT_EQ(vehicleState.prevTransmissionParkStatus, TRNPRKSTS_UNKNOWN);
    EXPECT_EQ(vehicleState.transmissionParkStatus, TRNPRKSTS_PARK);
    
    // Change battery SOC
    updateBatteryStatus(85, baseTime + 400);
    EXPECT_NE(vehicleState.batterySOC, vehicleState.prevBatterySOC);
    EXPECT_EQ(vehicleState.prevBatterySOC, 50);
    EXPECT_EQ(vehicleState.batterySOC, 85);
}

// ===============================================
// Test Suite: System Ready Logic
// ===============================================

TEST_F(VehicleStateTest, SystemReadyLogic) {
    unsigned long baseTime = 6000;
    
    // Test 1: System should be ready when ALL messages are recent
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime);
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_UNKNOWN, baseTime);
    updateBatteryStatus(75, baseTime);
    
    advanceTimeAndUpdate(baseTime + 1000); // 1 second later
    EXPECT_TRUE(vehicleState.systemReady);
    
    // Test 2: System should be ready with just ONE recent message (BCM only)
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS + 1000); // Well past timeout for all messages
    EXPECT_FALSE(vehicleState.systemReady); // Should be false now
    
    updateBCMLampStatus(PUDLAMP_ON, 0, 0, baseTime + SYSTEM_READINESS_TIMEOUT_MS + 1500); // Fresh BCM only
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS + 1600);
    EXPECT_TRUE(vehicleState.systemReady); // Should be ready with just BCM data
    
    // Test 3: System should be ready with just Locking data
    // Advance far enough that the previous BCM update (at baseTime + TIMEOUT + 1500) is stale
    unsigned long test3Time = baseTime + SYSTEM_READINESS_TIMEOUT_MS + 1500 + SYSTEM_READINESS_TIMEOUT_MS + 1000;
    advanceTimeAndUpdate(test3Time); // Well past timeout for previous BCM update
    EXPECT_FALSE(vehicleState.systemReady); // Should be false now
    
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, test3Time + 100); // Fresh Locking only
    advanceTimeAndUpdate(test3Time + 200);
    EXPECT_TRUE(vehicleState.systemReady); // Should be ready with just Locking data
    
    // Test 4: System should be ready with just Powertrain data
    // Advance far enough that the previous Locking update is stale
    unsigned long test4Time = test3Time + 100 + SYSTEM_READINESS_TIMEOUT_MS + 1000;
    advanceTimeAndUpdate(test4Time); // Well past timeout for previous Locking update
    EXPECT_FALSE(vehicleState.systemReady); // Should be false now
    
    updatePowertrainStatus(TRNPRKSTS_PARK, test4Time + 100); // Fresh Powertrain only
    advanceTimeAndUpdate(test4Time + 200);
    EXPECT_TRUE(vehicleState.systemReady); // Should be ready with just Powertrain data
    
    // Test 5: System should be ready with just Battery data
    // Advance far enough that the previous Powertrain update is stale
    unsigned long test5Time = test4Time + 100 + SYSTEM_READINESS_TIMEOUT_MS + 1000;
    advanceTimeAndUpdate(test5Time); // Well past timeout for previous Powertrain update
    EXPECT_FALSE(vehicleState.systemReady); // Should be false now
    
    updateBatteryStatus(85, test5Time + 100); // Fresh Battery only
    advanceTimeAndUpdate(test5Time + 200);
    EXPECT_TRUE(vehicleState.systemReady); // Should be ready with just Battery data
    
    // Test 6: System should NOT be ready when NO messages are recent
    // Advance far enough that the previous Battery update is stale
    unsigned long test6Time = test5Time + 100 + SYSTEM_READINESS_TIMEOUT_MS + 1000;
    advanceTimeAndUpdate(test6Time); // Well past timeout for previous Battery update
    EXPECT_FALSE(vehicleState.systemReady); // Should be false when all data is stale
}

// ===============================================
// Test Suite: Toolbox Activation Logic
// ===============================================

TEST_F(VehicleStateTest, ToolboxActivationLogic) {
    unsigned long baseTime = 7000;
    
    // Initialize all systems as fresh
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime);
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_UNKNOWN, baseTime);
    updateBatteryStatus(80, baseTime);
    
    // Test case 1: Not parked, not unlocked - should NOT activate
    advanceTimeAndUpdate(baseTime + 100);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // Test case 2: Parked but locked - should NOT activate
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime + 200);
    advanceTimeAndUpdate(baseTime + 300);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // Test case 3: Unlocked but not parked - should NOT activate
    updatePowertrainStatus(TRNPRKSTS_OUT_OF_PARK, baseTime + 400);
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime + 500);
    advanceTimeAndUpdate(baseTime + 600);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // Test case 4: Parked AND unlocked - should activate
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime + 700);
    advanceTimeAndUpdate(baseTime + 800);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_TRUE(vehicleState.toolboxShouldOpen);
    
    // Test case 5: Conditions met but system not ready - should NOT activate
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS + 10000); // Make system stale (past 10 minute timeout)
    EXPECT_FALSE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.isParked); // State values don't change, just freshness
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
}

// ===============================================
// Test Suite: Real-world Scenarios
// ===============================================

TEST_F(VehicleStateTest, RealWorldScenarioApproachingVehicle) {
    // Simulate real-world scenario: User approaches parked, locked vehicle
    
    unsigned long baseTime = 8000;
    
    // Initial state: Vehicle parked and locked, puddle lamps off
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime);
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime);
    updateBatteryStatus(85, baseTime);
    
    advanceTimeAndUpdate(baseTime + 100);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // User approaches: Puddle lamps start ramping up
    updateBCMLampStatus(PUDLAMP_RAMP_UP, 1, 1, baseTime + 500);
    advanceTimeAndUpdate(baseTime + 600);
    EXPECT_TRUE(vehicleState.bedlightShouldBeOn);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Still locked
    
    // Puddle lamps fully on
    updateBCMLampStatus(PUDLAMP_ON, 1, 1, baseTime + 1000);
    advanceTimeAndUpdate(baseTime + 1100);
    EXPECT_TRUE(vehicleState.bedlightShouldBeOn);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Still locked
    
    // User unlocks vehicle
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime + 1500);
    advanceTimeAndUpdate(baseTime + 1600);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_TRUE(vehicleState.toolboxShouldOpen); // Now should open: parked + unlocked + system ready
    
    // User locks vehicle again
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime + 2000);
    advanceTimeAndUpdate(baseTime + 2100);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Should close: now locked
    
    // Puddle lamps ramp down
    updateBCMLampStatus(PUDLAMP_RAMP_DOWN, 0, 0, baseTime + 2500);
    advanceTimeAndUpdate(baseTime + 2600);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn);
    
    // Puddle lamps off
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime + 3000);
    advanceTimeAndUpdate(baseTime + 3100);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn);
}

TEST_F(VehicleStateTest, RealWorldScenarioDrivingAway) {
    // Simulate real-world scenario: User gets in vehicle and drives away
    
    unsigned long baseTime = 9000;
    
    // Initial state: Vehicle parked and unlocked, puddle lamps on
    updateBCMLampStatus(PUDLAMP_ON, 1, 1, baseTime);
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime);
    updateBatteryStatus(90, baseTime);
    
    advanceTimeAndUpdate(baseTime + 100);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_TRUE(vehicleState.bedlightShouldBeOn);
    EXPECT_TRUE(vehicleState.toolboxShouldOpen);
    
    // User starts vehicle: transmission transitions out of park
    updatePowertrainStatus(TRNPRKSTS_TRANSITION_CLOSE_TO_OUT_OF_PARK, baseTime + 500);
    advanceTimeAndUpdate(baseTime + 600);
    EXPECT_FALSE(vehicleState.isParked); // No longer in park
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Should close: not parked
    
    // Transmission fully out of park
    updatePowertrainStatus(TRNPRKSTS_OUT_OF_PARK, baseTime + 1000);
    advanceTimeAndUpdate(baseTime + 1100);
    EXPECT_FALSE(vehicleState.isParked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // Vehicle locks while driving
    updateLockingSystemsStatus(VEH_LOCK_ALL, baseTime + 1500);
    advanceTimeAndUpdate(baseTime + 1600);
    EXPECT_FALSE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen);
    
    // Puddle lamps turn off
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime + 2000);
    advanceTimeAndUpdate(baseTime + 2100);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn);
}

// ===============================================
// Test Suite: Edge Cases and Error Conditions
// ===============================================

TEST_F(VehicleStateTest, EdgeCasesUnknownSignalValues) {
    unsigned long baseTime = 10000;
    
    // Test with unknown/invalid signal values
    updateBCMLampStatus(0xFF, 0xFF, 0xFF, baseTime); // Invalid values
    updateLockingSystemsStatus(0xFF, baseTime);
    updatePowertrainStatus(0xFF, baseTime);
    updateBatteryStatus(0xFF, baseTime);
    
    advanceTimeAndUpdate(baseTime + 100);
    
    // System should handle invalid values gracefully
    EXPECT_EQ(vehicleState.pudLampRequest, 0xFF);
    EXPECT_EQ(vehicleState.vehicleLockStatus, 0xFF);
    EXPECT_EQ(vehicleState.transmissionParkStatus, 0xFF);
    EXPECT_EQ(vehicleState.batterySOC, 0xFF);
    
    // Derived states should be safe defaults
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn); // Unknown pudlamp state -> no bedlight
    EXPECT_FALSE(vehicleState.isUnlocked); // Unknown lock state -> assume locked
    EXPECT_FALSE(vehicleState.isParked); // Unknown park state -> assume not parked
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Unknown states -> no activation
}

TEST_F(VehicleStateTest, EdgeCasesMessageTimeouts) {
    unsigned long baseTime = 11000;
    
    // Set up valid initial state
    updateBCMLampStatus(PUDLAMP_ON, 1, 1, baseTime);
    updateLockingSystemsStatus(VEH_UNLOCK_ALL, baseTime);
    updatePowertrainStatus(TRNPRKSTS_PARK, baseTime);
    updateBatteryStatus(95, baseTime);
    
    advanceTimeAndUpdate(baseTime + 100);
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.toolboxShouldOpen);
    
    // Advance time to just before timeout (10 minutes = 600000 ms)
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS - 1000); // 9:59 minutes
    EXPECT_TRUE(vehicleState.systemReady);
    EXPECT_TRUE(vehicleState.toolboxShouldOpen);
    
    // Advance time to just after timeout
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS + 1000); // 10:01 minutes
    EXPECT_FALSE(vehicleState.systemReady);
    EXPECT_FALSE(vehicleState.toolboxShouldOpen); // Should deactivate due to stale data
    
    // Test that system becomes ready again with just ONE fresh message
    updateBCMLampStatus(PUDLAMP_OFF, 0, 0, baseTime + SYSTEM_READINESS_TIMEOUT_MS + 2000);
    advanceTimeAndUpdate(baseTime + SYSTEM_READINESS_TIMEOUT_MS + 2100);
    EXPECT_TRUE(vehicleState.systemReady); // Should be ready again with just BCM data
    
    // Individual state values should still be preserved
    EXPECT_TRUE(vehicleState.isParked);
    EXPECT_TRUE(vehicleState.isUnlocked);
    EXPECT_FALSE(vehicleState.bedlightShouldBeOn); // Should reflect new BCM state
}

// Main function removed - using unified test runner in test_main.cpp
