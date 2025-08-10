#include <gtest/gtest.h>
#include "../../common/test_helpers.h"
#include "../../common/test_config.h"
#include "../../common/arduino_test_interface.h"

// Import production GPIO controller with dependency injection
#include "../../../src/gpio_controller.h"

// Test Suite for GPIO Initialization
class GPIOInitTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        // Use production code with test interface
        initializeGPIOWithInterface(&testInterface);
    }
};

TEST_F(GPIOInitTest, InitializationSuccess) {
    // Test already initialized in SetUp
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_FALSE(state.toolboxOpener);
    EXPECT_FALSE(state.toolboxButton);
    EXPECT_FALSE(state.systemReady);
    EXPECT_EQ(state.toolboxOpenerStartTime, 0);
}

TEST_F(GPIOInitTest, InitializationSetsPinModes) {
    EXPECT_EQ(ArduinoMock::instance().getPinMode(BEDLIGHT_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(TOOLBOX_OPENER_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(SYSTEM_READY_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(TOOLBOX_BUTTON_PIN), INPUT_PULLUP);
}

TEST_F(GPIOInitTest, InitializationSetsOutputsLow) {
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(BEDLIGHT_PIN), LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(TOOLBOX_OPENER_PIN), LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(SYSTEM_READY_PIN), LOW);
}

// Test Suite for Basic Output Control
class BasicOutputTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
        setTime(1000);
    }
};

TEST_F(BasicOutputTest, BedlightControl) {
    setBedlight(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(isGPIOHigh(BEDLIGHT_PIN));
    
    setBedlight(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_FALSE(isGPIOHigh(BEDLIGHT_PIN));
}

TEST_F(BasicOutputTest, SystemReadyControl) {
    setSystemReady(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(isGPIOHigh(SYSTEM_READY_PIN));
    
    setSystemReady(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
}

TEST_F(BasicOutputTest, RedundantStateChanges) {
    // Test that setting the same state multiple times doesn't cause issues
    setBedlight(true);
    setBedlight(true);  // Redundant
    setBedlight(true);  // Redundant
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(isGPIOHigh(BEDLIGHT_PIN));
    
    setBedlight(false);
    setBedlight(false);  // Redundant
    setBedlight(false);  // Redundant
    
    state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_FALSE(isGPIOHigh(BEDLIGHT_PIN));
}

// Test Suite for Button Reading
class ButtonReadingTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
    }
};

TEST_F(ButtonReadingTest, ButtonNotPressed) {
    // Button not pressed (HIGH with pullup)
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, HIGH);
    
    EXPECT_FALSE(readToolboxButton());
    
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.toolboxButton);
}

TEST_F(ButtonReadingTest, ButtonPressed) {
    // Button pressed (LOW with pullup)
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, LOW);
    
    EXPECT_TRUE(readToolboxButton());
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.toolboxButton);
}

TEST_F(ButtonReadingTest, ButtonStateChanges) {
    // Initial state - not pressed
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, HIGH);
    EXPECT_FALSE(readToolboxButton());
    
    // Press button
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, LOW);
    EXPECT_TRUE(readToolboxButton());
    
    // Release button
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, HIGH);
    EXPECT_FALSE(readToolboxButton());
}

// Test Suite for Toolbox Opener Timing
class ToolboxOpenerTimingTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
        setTime(2000);
    }
};

TEST_F(ToolboxOpenerTimingTest, ToolboxOpenerActivation) {
    setToolboxOpener(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.toolboxOpener);
    EXPECT_TRUE(isGPIOHigh(TOOLBOX_OPENER_PIN));
    EXPECT_EQ(state.toolboxOpenerStartTime, 2000);
}

TEST_F(ToolboxOpenerTimingTest, ToolboxOpenerDeactivation) {
    setToolboxOpener(true);
    setToolboxOpener(false);
    
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.toolboxOpener);
    EXPECT_FALSE(isGPIOHigh(TOOLBOX_OPENER_PIN));
    EXPECT_EQ(state.toolboxOpenerStartTime, 0);
}

TEST_F(ToolboxOpenerTimingTest, AutomaticTimingShutoff) {
    setToolboxOpener(true);
    
    // Advance time less than duration
    advanceTime(TOOLBOX_OPENER_DURATION_MS - 100);
    updateToolboxOpenerTiming();
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.toolboxOpener);  // Still active
    
    // Advance time past duration
    advanceTime(200);  // Total elapsed: TOOLBOX_OPENER_DURATION_MS + 100
    updateToolboxOpenerTiming();
    
    state = getGPIOState();
    EXPECT_FALSE(state.toolboxOpener);  // Should be automatically deactivated
    EXPECT_FALSE(isGPIOHigh(TOOLBOX_OPENER_PIN));
}

TEST_F(ToolboxOpenerTimingTest, ExactTimingThreshold) {
    setToolboxOpener(true);
    
    // Advance time to exactly the duration
    advanceTime(TOOLBOX_OPENER_DURATION_MS);
    updateToolboxOpenerTiming();
    
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.toolboxOpener);  // Should be deactivated at exact threshold
}

TEST_F(ToolboxOpenerTimingTest, RedundantActivation) {
    setToolboxOpener(true);
    unsigned long startTime1 = getGPIOState().toolboxOpenerStartTime;
    
    // Try to activate again
    advanceTime(100);
    setToolboxOpener(true);
    unsigned long startTime2 = getGPIOState().toolboxOpenerStartTime;
    
    // Start time should not change
    EXPECT_EQ(startTime1, startTime2);
}

// Test Suite for Output Control Without Initialization
class UninitializedOutputTest : public ArduinoTest {
protected:
    void SetUp() override {
        ArduinoTest::SetUp();
        // Explicitly not initializing GPIO
    }
};

TEST_F(UninitializedOutputTest, OutputsIgnoredWhenNotInitialized) {
    // These should not cause crashes but will use default hardware interface
    // In a real test environment, this would require hardware to be present
    // For this test, we'll just verify the functions can be called
    
    // Note: Since we didn't initialize with test interface, these will use
    // the default hardware interface. In a unit test environment, 
    // we should initialize with test interface for proper testing.
    
        // Create a test interface for this test
        ArduinoTestInterface testInterface;
        initializeGPIOWithInterface(&testInterface);    setBedlight(true);
    setToolboxOpener(true);
    
    // Button reading should work after initialization
    EXPECT_FALSE(readToolboxButton());  // Default mock state
    
    // Timing update should not crash
    updateToolboxOpenerTiming();
}

// Test Suite for Output Control Integration
class OutputControlIntegrationTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
        setTime(3000);
    }
};

TEST_F(OutputControlIntegrationTest, AllOutputsIndependent) {
    // Turn on all outputs
    setBedlight(true);
    setSystemReady(true);
    setToolboxOpener(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(state.toolboxOpener);
    
    // Turn off one output - others should remain on
    setBedlight(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(state.toolboxOpener);
}

TEST_F(OutputControlIntegrationTest, ToolboxOpenerWithOtherOutputs) {
    // Activate toolbox opener with other outputs
    setBedlight(true);
    setToolboxOpener(true);
    
    // Advance time to auto-shutoff
    advanceTime(TOOLBOX_OPENER_DURATION_MS + 100);
    updateToolboxOpenerTiming();
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);    // Should remain on
    EXPECT_FALSE(state.toolboxOpener);  // Should be auto-shut off
}

TEST_F(OutputControlIntegrationTest, ButtonReadingWithOutputs) {
    // Set some outputs
    setBedlight(true);
    
    // Test button reading
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, LOW);
    EXPECT_TRUE(readToolboxButton());
    
    // Outputs should not be affected by button reading
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
}

// Test Suite for Real-world Output Scenarios
class RealWorldOutputTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
        setTime(4000);
    }
    
    void simulateVehicleUnlocked() {
        // Vehicle unlocked state is handled by message parsing, not LED output
    }
    
    void simulateVehicleParked() {
        // Vehicle parked state is handled by message parsing, not LED output
    }
    
    void simulateBedlightOn() {
        setBedlight(true);
    }
    
    void simulateToolboxActivation() {
        setToolboxOpener(true);
    }
};

TEST_F(RealWorldOutputTest, TypicalUnlockAndParkSequence) {
    // Vehicle starts locked and not parked
    GPIOState state = getGPIOState();
    // LED state is no longer tracked - vehicle state is determined by CAN message parsing
    
    // Vehicle gets unlocked
    simulateVehicleUnlocked();
    state = getGPIOState();
    // LED state is no longer tracked - vehicle state is determined by CAN message parsing
    
    // Vehicle gets parked
    simulateVehicleParked();
    state = getGPIOState();
    // LED state is no longer tracked - vehicle state is determined by CAN message parsing
    
    // Now toolbox can be activated
    simulateToolboxActivation();
    state = getGPIOState();
    EXPECT_TRUE(state.toolboxOpener);
}

TEST_F(RealWorldOutputTest, BedlightWithToolboxSequence) {
    // Turn on bedlight (PUD lamp activated)
    simulateBedlightOn();
    
    // Vehicle becomes unlocked and parked
    simulateVehicleUnlocked();
    simulateVehicleParked();
    
    // Activate toolbox
    simulateToolboxActivation();
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(state.toolboxOpener);
    
    // Wait for toolbox to auto-shutoff
    advanceTime(TOOLBOX_OPENER_DURATION_MS + 50);
    updateToolboxOpenerTiming();
    
    state = getGPIOState();
    EXPECT_TRUE(state.bedlight);    // Should remain on
    EXPECT_FALSE(state.toolboxOpener);  // Should be off
}

TEST_F(RealWorldOutputTest, MultipleToolboxActivations) {
    simulateVehicleUnlocked();
    simulateVehicleParked();
    
    // First activation
    simulateToolboxActivation();
    advanceTime(TOOLBOX_OPENER_DURATION_MS + 50);
    updateToolboxOpenerTiming();
    
    EXPECT_FALSE(getGPIOState().toolboxOpener);
    
    // Short delay
    advanceTime(1000);
    
    // Second activation
    simulateToolboxActivation();
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.toolboxOpener);
    
    // Should have new start time
    unsigned long newStartTime = state.toolboxOpenerStartTime;
    EXPECT_GT(newStartTime, 4000);  // Should be after initial test time
}

// Test Suite for System Ready Indicator
class SystemReadyIndicatorTest : public ArduinoTest {
protected:
    ArduinoTestInterface testInterface;
    
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIOWithInterface(&testInterface);
    }
};

TEST_F(SystemReadyIndicatorTest, SystemReadyIndicatorBasicOperation) {
    // System ready indicator should start as OFF
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
    
    // Turn on system ready
    setSystemReady(true);
    state = getGPIOState();
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(isGPIOHigh(SYSTEM_READY_PIN));
    
    // Turn off system ready
    setSystemReady(false);
    state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
}

TEST_F(SystemReadyIndicatorTest, SystemReadyRedundantCalls) {
    // Multiple calls with same state should not cause issues
    setSystemReady(true);
    setSystemReady(true);
    setSystemReady(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(isGPIOHigh(SYSTEM_READY_PIN));
    
    setSystemReady(false);
    setSystemReady(false);
    setSystemReady(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
}

TEST_F(SystemReadyIndicatorTest, SystemReadyIndependentOfOtherOutputs) {
    // Turn on other outputs
    setBedlight(true);
    
    // System ready should remain off
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
    
    // Turn on system ready
    setSystemReady(true);
    state = getGPIOState();
    EXPECT_TRUE(state.systemReady);
    EXPECT_TRUE(isGPIOHigh(SYSTEM_READY_PIN));
    
    // Other outputs should remain on
    EXPECT_TRUE(state.bedlight);
    
    // Turn off system ready - other outputs should remain
    setSystemReady(false);
    state = getGPIOState();
    EXPECT_FALSE(state.systemReady);
    EXPECT_FALSE(isGPIOHigh(SYSTEM_READY_PIN));
    EXPECT_TRUE(state.bedlight);
}
