#include "diagnostic_commands.h"
#include "config.h"
#include "can_manager.h"
#include "state_manager.h"
#include "gpio_controller.h"

// External global variables
extern SystemHealth systemHealth;

void processSerialCommands() {
    if (!Serial.available()) {
        return;
    }
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    LOG_INFO("Received command: '%s'", command.c_str());
    
    if (command == "help" || command == "h") {
        cmd_help();
    } else if (command == "status" || command == "t") {
        cmd_status();
    } else if (command == "can_status" || command == "cs") {
        cmd_can_status();
    } else if (command == "can_debug" || command == "cd") {
        cmd_can_debug();
    } else if (command == "can_reset" || command == "cr") {
        cmd_can_reset();
    } else if (command == "can_buffers" || command == "cb") {
        cmd_can_buffers();
    } else if (command == "system_info" || command == "si") {
        cmd_system_info();
    } else if (command.length() > 0) {
        LOG_ERROR("Unknown command: '%s'. Type 'help' for available commands.", command.c_str());
    }
}

void cmd_help() {
    LOG_INFO("=== Ford F150 Gen14 CAN Bus Interface ===");
    LOG_INFO("Project: https://github.com/jantman/ford-f150-gen14-can-bus-interface");
    LOG_INFO("Firmware Version: %s", FIRMWARE_VERSION);
    LOG_INFO("Build Date: %s %s", BUILD_DATE, BUILD_TIME);
    LOG_INFO("");
    LOG_INFO("=== DIAGNOSTIC COMMANDS ===");
    LOG_INFO("help (h)        - Show this help");
    LOG_INFO("status (t)      - Show complete system status");
    LOG_INFO("can_status (cs) - Show CAN bus status");
    LOG_INFO("can_debug (cd)  - Debug CAN message reception");
    LOG_INFO("can_reset (cr)  - Reset CAN system");
    LOG_INFO("can_buffers (cb)- Show CAN buffer status and message loss");
    LOG_INFO("system_info (si)- Show system information");
    LOG_INFO("============================");
}

void cmd_can_status() {
    LOG_INFO("=== CAN BUS STATUS ===");
    printCANStatistics();
    checkRawCANActivity();
    
    // Show current state
    VehicleState state = getCurrentState();
    LOG_INFO("=== VEHICLE STATE ===");
    LOG_INFO("System Ready: %s", state.systemReady ? "YES" : "NO");
    LOG_INFO("Bed Light Should Be On: %s", state.bedlightShouldBeOn ? "YES" : "NO");
    LOG_INFO("Is Parked: %s", state.isParked ? "YES" : "NO");
    LOG_INFO("Is Unlocked: %s", state.isUnlocked ? "YES" : "NO");
}

void cmd_can_debug() {
    LOG_INFO("=== CAN DEBUG MODE (10 seconds) ===");
    LOG_INFO("Monitoring ALL CAN messages for 10 seconds...");
    
    unsigned long startTime = millis();
    int totalMessages = 0;
    
    while (millis() - startTime < 10000) { // 10 seconds
        debugReceiveAllMessages();
        delay(100); // Small delay between checks
        
        // Update total count
        // This is a simple way to show we're still running
        if ((millis() - startTime) % 2000 == 0) {
            LOG_INFO("Debug monitoring... %lu seconds elapsed", (millis() - startTime) / 1000);
        }
    }
    
    LOG_INFO("=== CAN DEBUG COMPLETE ===");
    printCANStatistics();
}

void cmd_can_reset() {
    LOG_INFO("=== RESETTING CAN SYSTEM ===");
    if (recoverCANSystem()) {
        LOG_INFO("CAN system reset successful");
    } else {
        LOG_ERROR("CAN system reset failed");
    }
    printCANStatistics();
}

void cmd_can_buffers() {
    LOG_INFO("=== CAN BUFFER STATUS ===");
    LOG_INFO("Controller: MCP2515 (Hardware buffers only)");
    LOG_INFO("RX Buffer Count: 2 (RXB0, RXB1)");
    LOG_INFO("Buffer Size: 1 message per buffer");
    LOG_INFO("Total Hardware Capacity: 2 messages");
    LOG_INFO("Overflow Detection: Heuristic-based (pattern analysis)");
    
    uint32_t lossCount = getMessageLossCount();
    LOG_INFO("Suspected Buffer Overflows: %lu", lossCount);
    
    if (lossCount > 0) {
        LOG_WARN("WARNING: Potential buffer overflows detected!");
        LOG_WARN("Detection method: Monitoring for sudden message cessation during active periods");
        LOG_WARN("This indicates the system may not be processing messages fast enough");
        LOG_WARN("Recommendations:");
        LOG_WARN("  1. Reduce main loop delay (currently 10ms)");
        LOG_WARN("  2. Increase MAX_MESSAGES_PER_LOOP (currently 10)");
        LOG_WARN("  3. Consider using ESP32 TWAI instead of MCP2515 for larger buffers");
        LOG_WARN("  4. Filter messages to reduce processing load");
        LOG_WARN("  5. Monitor 'can_debug' output for message burst patterns");
    } else {
        LOG_INFO("No suspected buffer overflows - system appears to be keeping up");
    }
    
    // Show current processing limits
    LOG_INFO("=== PROCESSING LIMITS ===");
    LOG_INFO("MAX_MESSAGES_PER_LOOP: 10");
    LOG_INFO("Main loop delay: 10ms");
    LOG_INFO("Overflow check interval: 100ms");
    LOG_INFO("Detection threshold: 5 seconds of silence during active periods");
    
    // Force an immediate overflow check
    LOG_INFO("Performing immediate buffer status check...");
    checkMessageLoss();
    
    LOG_INFO("=== BUFFER MONITORING NOTES ===");
    LOG_INFO("The MCP2515 has very limited (2-message) hardware buffers");
    LOG_INFO("Unlike software queues, these cannot be increased");
    LOG_INFO("Monitor for patterns: busy periods followed by silence, then bursts");
    LOG_INFO("Use 'can_debug' to observe actual message timing and patterns");
    
    LOG_INFO("=== END BUFFER STATUS ===");
}

void cmd_system_info() {
    LOG_INFO("=== SYSTEM INFORMATION ===");
    LOG_INFO("Chip Model: %s", ESP.getChipModel());
    LOG_INFO("Chip Revision: %d", ESP.getChipRevision());
    LOG_INFO("CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
    LOG_INFO("Flash Size: %d bytes", ESP.getFlashChipSize());
    LOG_INFO("Free Heap: %d bytes", ESP.getFreeHeap());
    LOG_INFO("SDK Version: %s", ESP.getSdkVersion());
    LOG_INFO("Uptime: %lu ms", millis());
    
    // GPIO status
    GPIOState gpioState = getGPIOState();
    LOG_INFO("=== GPIO STATUS ===");
    LOG_INFO("Bedlight: %s", gpioState.bedlight ? "ON" : "OFF");
    LOG_INFO("Parked LED: %s", gpioState.parkedLED ? "ON" : "OFF");
    LOG_INFO("Unlocked LED: %s", gpioState.unlockedLED ? "ON" : "OFF");
    LOG_INFO("System Ready: %s", gpioState.systemReady ? "ON" : "OFF");
    LOG_INFO("Toolbox Opener: %s", gpioState.toolboxOpener ? "ACTIVE" : "INACTIVE");
    LOG_INFO("Button State: %s", gpioState.toolboxButton ? "PRESSED" : "RELEASED");
}

void cmd_status() {
    LOG_INFO("=== SYSTEM STATUS ===");
    
    // Get current states
    VehicleState vehicleState = getCurrentState();
    ButtonState buttonState = getButtonState();
    GPIOState gpioState = getGPIOState();
    unsigned long currentTime = millis();
    
    // Vehicle State Flags
    LOG_INFO("Vehicle State: Ready=%s Parked=%s Unlocked=%s BedLight=%s", 
             vehicleState.systemReady ? "Y" : "N",
             vehicleState.isParked ? "Y" : "N", 
             vehicleState.isUnlocked ? "Y" : "N",
             vehicleState.bedlightShouldBeOn ? "Y" : "N");
    
    // Vehicle State Raw Values
    LOG_INFO("Raw Values: PUD=%d Lock=%d Park=%d SOC=%d%%", 
             vehicleState.pudLampRequest, vehicleState.vehicleLockStatus,
             vehicleState.transmissionParkStatus, vehicleState.batterySOC);
    
    // GPIO States
    LOG_INFO("GPIO Outputs: Bed=%s Park=%s Unlock=%s SysReady=%s Toolbox=%s Button=%s",
             gpioState.bedlight ? "ON" : "OFF",
             gpioState.parkedLED ? "ON" : "OFF", 
             gpioState.unlockedLED ? "ON" : "OFF",
             gpioState.systemReady ? "ON" : "OFF",
             gpioState.toolboxOpener ? "ACTIVE" : "OFF",
             gpioState.toolboxButton ? "PRESSED" : "RELEASED");
    
    // Button State (essential info only)
    LOG_INFO("Button: State=%s Held=%s Count=%lu",
             buttonState.currentState ? "PRESSED" : "RELEASED",
             buttonState.isHeld ? "Y" : "N",
             buttonState.pressCount);
    
    // System Health (key metrics only)
    LOG_INFO("Health: CAN=%s Errors(C/P/Cr)=%lu/%lu/%lu Recovery=%s",
             isCANConnected() ? "OK" : "FAIL",
             systemHealth.canErrors, systemHealth.parseErrors, systemHealth.criticalErrors,
             systemHealth.recoveryMode ? "Y" : "N");
}
