#include "native_arduino_compat.h"

#ifdef NATIVE_ENV
// Define the global Serial object for native testing
MockSerial Serial;
#endif
