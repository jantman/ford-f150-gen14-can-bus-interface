/**
 * Production Code Wrapper for Native Testing
 * 
 * This file includes the production code implementation to make it available
 * to the native test environment. This is a workaround for PlatformIO's
 * build_src_filter limitations in native testing environments.
 */

#include "../src/bit_utils.c"
#include "../src/can_protocol.c"
