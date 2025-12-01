#pragma once

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