/**
 * Production Code Wrapper for Native Testing
 * 
 * This file includes the production code implementation to make it available
 * to the native test environment. This is a workaround for PlatformIO's
 * build_src_filter limitations in native testing environments.
 */

#include "../src/bit_utils.c"
#include "../src/can_protocol.c"

// Include C++ files by wrapping them in extern "C" blocks for C compatibility
#ifdef __cplusplus
extern "C" {
#endif

// The C++ files need to be compiled as C++ and linked separately
// We'll handle this by ensuring they're in the build_src_filter

#ifdef __cplusplus
}
#endif
