#include <gtest/gtest.h>
#include "../../common/test_helpers.h"
#include "../../common/test_config.h"

// GPIO state structure
extern "C" {
    struct GPIOState {
        bool bedlight;
        bool parkedLED;
        bool unlockedLED;
        bool toolboxOpener;
        bool toolboxButton;
        unsigned long toolboxOpenerStartTime;
    };
    
    bool initializeGPIO();
    void setBedlight(bool state);
    void setParkedLED(bool state);
    void setUnlockedLED(bool state);
    void setToolboxOpener(bool state);
    bool readToolboxButton();
    void updateToolboxOpenerTiming();
    GPIOState getGPIOState();
}

// Global GPIO state for testing
static GPIOState gpioState;
static bool gpioInitialized = false;

// Implement GPIO functions for testing
bool initializeGPIO() {
    // Initialize all outputs to off
    gpioState.bedlight = false;
    gpioState.parkedLED = false;
    gpioState.unlockedLED = false;
    gpioState.toolboxOpener = false;
    gpioState.toolboxButton = false;
    gpioState.toolboxOpenerStartTime = 0;
    
    // Set pin modes in mock
    ArduinoMock::instance().setPinMode(BEDLIGHT_PIN, OUTPUT);
    ArduinoMock::instance().setPinMode(PARKED_LED_PIN, OUTPUT);
    ArduinoMock::instance().setPinMode(UNLOCKED_LED_PIN, OUTPUT);
    ArduinoMock::instance().setPinMode(TOOLBOX_OPENER_PIN, OUTPUT);
    ArduinoMock::instance().setPinMode(TOOLBOX_BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize outputs to LOW
    ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, LOW);
    ArduinoMock::instance().setDigitalWrite(PARKED_LED_PIN, LOW);
    ArduinoMock::instance().setDigitalWrite(UNLOCKED_LED_PIN, LOW);
    ArduinoMock::instance().setDigitalWrite(TOOLBOX_OPENER_PIN, LOW);
    
    // Initialize button reading (pullup means HIGH when not pressed)
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, HIGH);
    gpioState.toolboxButton = false;  // Not pressed
    
    gpioInitialized = true;
    return true;
}

void setBedlight(bool state) {
    if (!gpioInitialized) return;
    
    if (gpioState.bedlight != state) {
        gpioState.bedlight = state;
        ArduinoMock::instance().setDigitalWrite(BEDLIGHT_PIN, state ? HIGH : LOW);
    }
}

void setParkedLED(bool state) {
    if (!gpioInitialized) return;
    
    if (gpioState.parkedLED != state) {
        gpioState.parkedLED = state;
        ArduinoMock::instance().setDigitalWrite(PARKED_LED_PIN, state ? HIGH : LOW);
    }
}

void setUnlockedLED(bool state) {
    if (!gpioInitialized) return;
    
    if (gpioState.unlockedLED != state) {
        gpioState.unlockedLED = state;
        ArduinoMock::instance().setDigitalWrite(UNLOCKED_LED_PIN, state ? HIGH : LOW);
    }
}

void setToolboxOpener(bool state) {
    if (!gpioInitialized) return;
    
    if (state && !gpioState.toolboxOpener) {
        // Starting toolbox opener
        gpioState.toolboxOpener = true;
        gpioState.toolboxOpenerStartTime = millis();
        ArduinoMock::instance().setDigitalWrite(TOOLBOX_OPENER_PIN, HIGH);
    } else if (!state && gpioState.toolboxOpener) {
        // Stopping toolbox opener
        gpioState.toolboxOpener = false;
        gpioState.toolboxOpenerStartTime = 0;
        ArduinoMock::instance().setDigitalWrite(TOOLBOX_OPENER_PIN, LOW);
    }
}

bool readToolboxButton() {
    if (!gpioInitialized) return false;
    
    // Read from mock (active low with pullup)
    bool buttonPressed = ArduinoMock::instance().getDigitalRead(TOOLBOX_BUTTON_PIN) == LOW;
    gpioState.toolboxButton = buttonPressed;
    return buttonPressed;
}

void updateToolboxOpenerTiming() {
    if (!gpioInitialized || !gpioState.toolboxOpener) return;
    
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - gpioState.toolboxOpenerStartTime;
    
    if (elapsed >= TOOLBOX_OPENER_DURATION_MS) {
        setToolboxOpener(false);
    }
}

GPIOState getGPIOState() {
    return gpioState;
}

// Test Suite for GPIO Initialization
class GPIOInitTest : public ArduinoTest {
protected:
    void SetUp() override {
        ArduinoTest::SetUp();
        gpioInitialized = false;
    }
    
    void TearDown() override {
        gpioInitialized = false;
        ArduinoTest::TearDown();
    }
};

TEST_F(GPIOInitTest, InitializationSuccess) {
    EXPECT_TRUE(initializeGPIO());
    
    GPIOState state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_FALSE(state.parkedLED);
    EXPECT_FALSE(state.unlockedLED);
    EXPECT_FALSE(state.toolboxOpener);
    EXPECT_FALSE(state.toolboxButton);
    EXPECT_EQ(state.toolboxOpenerStartTime, 0);
}

TEST_F(GPIOInitTest, InitializationSetsPinModes) {
    initializeGPIO();
    
    EXPECT_EQ(ArduinoMock::instance().getPinMode(BEDLIGHT_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(PARKED_LED_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(UNLOCKED_LED_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(TOOLBOX_OPENER_PIN), OUTPUT);
    EXPECT_EQ(ArduinoMock::instance().getPinMode(TOOLBOX_BUTTON_PIN), INPUT_PULLUP);
}

TEST_F(GPIOInitTest, InitializationSetsOutputsLow) {
    initializeGPIO();
    
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(BEDLIGHT_PIN), LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(PARKED_LED_PIN), LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(UNLOCKED_LED_PIN), LOW);
    EXPECT_EQ(ArduinoMock::instance().getDigitalState(TOOLBOX_OPENER_PIN), LOW);
}

// Test Suite for Basic Output Control
class BasicOutputTest : public ArduinoTest {
protected:
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIO();
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

TEST_F(BasicOutputTest, ParkedLEDControl) {
    setParkedLED(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.parkedLED);
    EXPECT_TRUE(isGPIOHigh(PARKED_LED_PIN));
    
    setParkedLED(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.parkedLED);
    EXPECT_FALSE(isGPIOHigh(PARKED_LED_PIN));
}

TEST_F(BasicOutputTest, UnlockedLEDControl) {
    setUnlockedLED(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_TRUE(isGPIOHigh(UNLOCKED_LED_PIN));
    
    setUnlockedLED(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.unlockedLED);
    EXPECT_FALSE(isGPIOHigh(UNLOCKED_LED_PIN));
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
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIO();
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
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIO();
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
        gpioInitialized = false;  // Explicitly not initialized
    }
};

TEST_F(UninitializedOutputTest, OutputsIgnoredWhenNotInitialized) {
    // These should not cause crashes or change any mock state
    setBedlight(true);
    setParkedLED(true);
    setUnlockedLED(true);
    setToolboxOpener(true);
    
    // Button reading should return false when not initialized
    EXPECT_FALSE(readToolboxButton());
    
    // Timing update should not crash
    updateToolboxOpenerTiming();
}

// Test Suite for Output Control Integration
class OutputControlIntegrationTest : public ArduinoTest {
protected:
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIO();
        setTime(3000);
    }
};

TEST_F(OutputControlIntegrationTest, AllOutputsIndependent) {
    // Turn on all outputs
    setBedlight(true);
    setParkedLED(true);
    setUnlockedLED(true);
    setToolboxOpener(true);
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(state.parkedLED);
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_TRUE(state.toolboxOpener);
    
    // Turn off one output - others should remain on
    setBedlight(false);
    
    state = getGPIOState();
    EXPECT_FALSE(state.bedlight);
    EXPECT_TRUE(state.parkedLED);
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_TRUE(state.toolboxOpener);
}

TEST_F(OutputControlIntegrationTest, ToolboxOpenerWithOtherOutputs) {
    // Activate toolbox opener with other outputs
    setBedlight(true);
    setParkedLED(true);
    setToolboxOpener(true);
    
    // Advance time to auto-shutoff
    advanceTime(TOOLBOX_OPENER_DURATION_MS + 100);
    updateToolboxOpenerTiming();
    
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);    // Should remain on
    EXPECT_TRUE(state.parkedLED);   // Should remain on
    EXPECT_FALSE(state.toolboxOpener);  // Should be auto-shut off
}

TEST_F(OutputControlIntegrationTest, ButtonReadingWithOutputs) {
    // Set some outputs
    setBedlight(true);
    setParkedLED(true);
    
    // Test button reading
    ArduinoMock::instance().setDigitalRead(TOOLBOX_BUTTON_PIN, LOW);
    EXPECT_TRUE(readToolboxButton());
    
    // Outputs should not be affected by button reading
    GPIOState state = getGPIOState();
    EXPECT_TRUE(state.bedlight);
    EXPECT_TRUE(state.parkedLED);
}

// Test Suite for Real-world Output Scenarios
class RealWorldOutputTest : public ArduinoTest {
protected:
    void SetUp() override {
        ArduinoTest::SetUp();
        initializeGPIO();
        setTime(4000);
    }
    
    void simulateVehicleUnlocked() {
        setUnlockedLED(true);
    }
    
    void simulateVehicleParked() {
        setParkedLED(true);
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
    EXPECT_FALSE(state.unlockedLED);
    EXPECT_FALSE(state.parkedLED);
    
    // Vehicle gets unlocked
    simulateVehicleUnlocked();
    state = getGPIOState();
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_FALSE(state.parkedLED);
    
    // Vehicle gets parked
    simulateVehicleParked();
    state = getGPIOState();
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_TRUE(state.parkedLED);
    
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
    EXPECT_TRUE(state.unlockedLED);
    EXPECT_TRUE(state.parkedLED);
    EXPECT_TRUE(state.toolboxOpener);
    
    // Wait for toolbox to auto-shutoff
    advanceTime(TOOLBOX_OPENER_DURATION_MS + 50);
    updateToolboxOpenerTiming();
    
    state = getGPIOState();
    EXPECT_TRUE(state.bedlight);    // Should remain on
    EXPECT_TRUE(state.unlockedLED); // Should remain on
    EXPECT_TRUE(state.parkedLED);   // Should remain on
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
