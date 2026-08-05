#pragma once

#include <array>

#include "Helpers/PlatformIntrinsics.h"

namespace AbacDsp
{

/// @ingroup reverbs
/// @brief Unnormalised order-8 Hadamard mix via the fast Walsh-Hadamard butterfly.
/// 3 stages of pairwise add/subtract at doubling stride: 24 adds against 64 for the flat form.
/// Produces exactly the same matrix and row order as hadamardFeed8(), only faster.
inline void hadamardWalsh8(const float* input, float* output) noexcept
{
    std::array<float, 8> temp1{};
    std::array<float, 8> temp2{};

    // Stage 1: stride 1
    temp1[0] = input[0] + input[1];
    temp1[1] = input[0] - input[1];
    temp1[2] = input[2] + input[3];
    temp1[3] = input[2] - input[3];
    temp1[4] = input[4] + input[5];
    temp1[5] = input[4] - input[5];
    temp1[6] = input[6] + input[7];
    temp1[7] = input[6] - input[7];

    // Stage 2: stride 2
    temp2[0] = temp1[0] + temp1[2];
    temp2[1] = temp1[1] + temp1[3];
    temp2[2] = temp1[0] - temp1[2];
    temp2[3] = temp1[1] - temp1[3];
    temp2[4] = temp1[4] + temp1[6];
    temp2[5] = temp1[5] + temp1[7];
    temp2[6] = temp1[4] - temp1[6];
    temp2[7] = temp1[5] - temp1[7];

    // Stage 3: stride 4
    output[0] = temp2[0] + temp2[4];
    output[1] = temp2[1] + temp2[5];
    output[2] = temp2[2] + temp2[6];
    output[3] = temp2[3] + temp2[7];
    output[4] = temp2[0] - temp2[4];
    output[5] = temp2[1] - temp2[5];
    output[6] = temp2[2] - temp2[6];
    output[7] = temp2[3] - temp2[7];
}

inline void hadamardWalsh8(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8(input.data(), output.data());
}

#if defined(USE_SIMD_FRAMEWORK)

/// @ingroup reverbs
/// @brief SIMD form of hadamardWalsh8(), each butterfly stage as a shuffle and a signed add.
inline void hadamardWalsh8_simd(const float* input, float* output) noexcept
{
    simd_float4 v0 = simd_make_float4(input[0], input[1], input[2], input[3]);
    simd_float4 v1 = simd_make_float4(input[4], input[5], input[6], input[7]);

    // Stage 1: stride 1
    simd_float4 t1_0 = simd_make_float4(v0.x + v0.y, v0.x - v0.y, v0.z + v0.w, v0.z - v0.w);
    simd_float4 t1_1 = simd_make_float4(v1.x + v1.y, v1.x - v1.y, v1.z + v1.w, v1.z - v1.w);

    // Stage 2: stride 2
    simd_float4 t2_0 = simd_make_float4(t1_0.x + t1_0.z, t1_0.y + t1_0.w, t1_0.x - t1_0.z, t1_0.y - t1_0.w);
    simd_float4 t2_1 = simd_make_float4(t1_1.x + t1_1.z, t1_1.y + t1_1.w, t1_1.x - t1_1.z, t1_1.y - t1_1.w);

    // Stage 3: stride 4
    simd_float4 out_lo = t2_0 + t2_1;
    simd_float4 out_hi = t2_0 - t2_1;

    output[0] = out_lo.x;
    output[1] = out_lo.y;
    output[2] = out_lo.z;
    output[3] = out_lo.w;
    output[4] = out_hi.x;
    output[5] = out_hi.y;
    output[6] = out_hi.z;
    output[7] = out_hi.w;
}

inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8_simd(input.data(), output.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardWalsh8_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_loadu_ps(&input[0]);
    __m128 v1 = _mm_loadu_ps(&input[4]);

    // Stage 1: stride 1
    __m128 s1_lo = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(2, 2, 0, 0));
    __m128 s1_hi = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_0 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_1 = _mm_addsub_ps(s1_lo, s1_hi);

    // Stage 2: stride 2
    __m128 s2_lo_0 = _mm_unpacklo_ps(t1_0, t1_0);
    __m128 s2_hi_0 = _mm_unpackhi_ps(t1_0, t1_0);
    __m128 add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    __m128 sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_0 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_1 = _mm_unpacklo_ps(t1_1, t1_1);
    __m128 s2_hi_1 = _mm_unpackhi_ps(t1_1, t1_1);
    __m128 add_1 = _mm_add_ps(s2_lo_1, s2_hi_1);
    __m128 sub_1 = _mm_sub_ps(s2_lo_1, s2_hi_1);
    __m128 t2_1 = _mm_shuffle_ps(add_1, sub_1, _MM_SHUFFLE(0, 2, 0, 2));

    // Stage 3: stride 4
    __m128 out_lo = _mm_add_ps(t2_0, t2_1);
    __m128 out_hi = _mm_sub_ps(t2_0, t2_1);

    _mm_storeu_ps(&output[0], out_lo);
    _mm_storeu_ps(&output[4], out_hi);
}

inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8_simd(input.data(), output.data());
}

#else

inline void hadamardWalsh8_simd(const float* input, float* output) noexcept
{
    hadamardWalsh8(input, output);
}

inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8(input, output);
}

#endif

}
