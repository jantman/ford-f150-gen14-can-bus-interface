#include "diagnostic_commands.h"
#include "config.h"
#include "can_manager.h"
#include "state_manager.h"
#include "gpio_controller.h"

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
    } else if (command == "can_status" || command == "cs") {
        cmd_can_status();
    } else if (command == "can_debug" || command == "cd") {
        cmd_can_debug();
    } else if (command == "can_reset" || command == "cr") {
        cmd_can_reset();
    } else if (command == "system_info" || command == "si") {
        cmd_system_info();
    } else if (command.length() > 0) {
        LOG_ERROR("Unknown command: '%s'. Type 'help' for available commands.", command.c_str());
    }
}

void cmd_help() {
    LOG_INFO("=== DIAGNOSTIC COMMANDS ===");
    LOG_INFO("help (h)        - Show this help");
    LOG_INFO("can_status (cs) - Show CAN bus status");
    LOG_INFO("can_debug (cd)  - Debug CAN message reception");
    LOG_INFO("can_reset (cr)  - Reset CAN system");
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
    LOG_INFO("Toolbox Opener: %s", gpioState.toolboxOpener ? "ACTIVE" : "INACTIVE");
    LOG_INFO("Button State: %s", gpioState.toolboxButton ? "PRESSED" : "RELEASED");
}
