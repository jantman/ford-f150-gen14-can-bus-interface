/**
 * Production Code C++ Wrapper for Native Testing
 * 
 * This file includes the C++ production code implementation to make it available
 * to the native test environment. This is a workaround for PlatformIO's
 * build_src_filter limitations in native testing environments.
 */

#include "../src/gpio_controller.cpp"
#include "../src/arduino_interface.cpp"
#include "../src/native_arduino_compat.cpp"
