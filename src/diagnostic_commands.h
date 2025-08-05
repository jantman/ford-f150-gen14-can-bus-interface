#ifndef DIAGNOSTIC_COMMANDS_H
#define DIAGNOSTIC_COMMANDS_H

#include <Arduino.h>

// Function to process serial diagnostic commands
void processSerialCommands();

// Individual diagnostic functions
void cmd_can_status();
void cmd_can_debug();
void cmd_can_reset();
void cmd_can_buffers();
void cmd_system_info();
void cmd_status();
void cmd_help();

#endif // DIAGNOSTIC_COMMANDS_H
