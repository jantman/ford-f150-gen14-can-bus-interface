#pragma once

#ifdef NATIVE_ENV
// Arduino constants for native environment
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2

#include <climits>

// Mock Serial for native testing
class MockSerial {
public:
    template<typename... Args>
    void printf(const char* format, Args... args) {
        // Stub - could print to stdout if needed for debugging
        // printf(format, args...);
    }
};

extern MockSerial Serial;

#endif // NATIVE_ENV
