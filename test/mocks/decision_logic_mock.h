#pragma once

#include <cstdint>

// C++ declarations for decision logic functions provided by mock_arduino.cpp
// These can be used by C++ test files that expect C++ linkage

bool shouldEnableBedlight(uint8_t pudLampRequest);
bool isVehicleUnlocked(uint8_t vehicleLockStatus);
bool isVehicleParked(uint8_t transmissionParkStatus);
bool shouldActivateToolboxWithParams(bool systemReady, bool isParked, bool isUnlocked);
bool shouldActivateToolbox(bool systemReady, bool isParked, bool isUnlocked);
