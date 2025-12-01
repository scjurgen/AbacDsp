#pragma once

#include "Helpers/PlatformIntrinsics.h"
#include <array>

namespace AbacDsp
{

inline void hadamardWalsh4(const float* input, float* output) noexcept
{
    std::array<float, 4> temp1;

    // Stage 1: stride 1
    temp1[0] = input[0] + input[1];
    temp1[1] = input[0] - input[1];
    temp1[2] = input[2] + input[3];
    temp1[3] = input[2] - input[3];

    // Stage 2: stride 2
    output[0] = temp1[0] + temp1[2];
    output[1] = temp1[1] + temp1[3];
    output[2] = temp1[0] - temp1[2];
    output[3] = temp1[1] - temp1[3];
}

inline void hadamardWalsh4(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4(input.data(), output.data());
}

#if defined(USE_SIMD_FRAMEWORK)

inline void hadamardWalsh4_simd(const float* input, float* output) noexcept
{
    simd_float4 v0 = simd_make_float4(input[0], input[1], input[2], input[3]);

    // Stage 1: stride 1 - pair-wise add/sub
    simd_float4 s1_lo = simd_make_float4(v0.x, v0.x, v0.z, v0.z);
    simd_float4 s1_hi = simd_make_float4(v0.y, v0.y, v0.w, v0.w);
    simd_float4 t1_0 = s1_lo + s1_hi * simd_make_float4(1.f, -1.f, 1.f, -1.f);

    // Stage 2: stride 2
    simd_float4 s2_lo = simd_make_float4(t1_0.x, t1_0.y, t1_0.x, t1_0.y);
    simd_float4 s2_hi = simd_make_float4(t1_0.z, t1_0.w, t1_0.z, t1_0.w);
    simd_float4 add_0 = s2_lo + s2_hi;
    simd_float4 sub_0 = s2_lo - s2_hi;

    simd_float4 out_0 = simd_make_float4(add_0.x, add_0.y, sub_0.x, sub_0.y);
    output[0] = out_0.x;
    output[1] = out_0.y;
    output[2] = out_0.z;
    output[3] = out_0.w;
}

inline void hadamardWalsh4_simd(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4_simd(input.data(), output.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardWalsh4_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_load_ps(&input[0]);

    // Stage 1: stride 1
    __m128 s1_lo = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(2, 2, 0, 0));
    __m128 s1_hi = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_0 = _mm_addsub_ps(s1_lo, s1_hi);

    // Stage 2: stride 2
    __m128 s2_lo_0 = _mm_unpacklo_ps(t1_0, t1_0);
    __m128 s2_hi_0 = _mm_unpackhi_ps(t1_0, t1_0);
    __m128 add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    __m128 sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 out_0 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    _mm_store_ps(&output[0], out_0);
}

inline void hadamardWalsh4_simd(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4_simd(input.data(), output.data());
}

#else

inline void hadamardWalsh4_simd(const float* input, float* output) noexcept
{
    hadamardWalsh4(input, output);
}

inline void hadamardWalsh4_simd(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4(input, output);
}

#endif

}
