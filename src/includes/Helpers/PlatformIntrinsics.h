#pragma once

/**
 * @file
 * @ingroup helpers
 * @brief Selects the SIMD backend and defines the macro the vector paths switch on.
 *
 * Apple platforms get simd/simd.h and USE_SIMD_FRAMEWORK; x86-64 elsewhere gets
 * the SSE intrinsics and USE_X86_INTRINSICS. Non-Apple arm64 is a hard `#error`
 * rather than a silent scalar fallback, so an unported build fails loudly
 * instead of quietly running several times slower.
 */

// clang-format off

#if defined(__APPLE__)
    #define USE_SIMD_FRAMEWORK
    #include <simd/simd.h>
#else  // not apple
    #if defined(__aarch64__)  // arm (mostly valgrind / linux)
        #error "arm64 not supported. Needs implementation"
    #else  // not apple, not arm
        #if defined(__x86_64__) || defined(_M_X64)
            #define USE_X86_INTRINSICS
            #include <xmmintrin.h>
            #include <pmmintrin.h>
        #endif
    #endif
#endif

// clang-format on