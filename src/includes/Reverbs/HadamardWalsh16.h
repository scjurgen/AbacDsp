#pragma once

#include <array>

#include "Helpers/PlatformIntrinsics.h"

namespace AbacDsp
{

/// @ingroup reverbs
/// @brief Unnormalised order-16 Hadamard mix via the fast Walsh-Hadamard butterfly.
/// 4 stages of pairwise add/subtract at doubling stride: 64 adds against 256 for the flat form.
/// Produces exactly the same matrix and row order as hadamardFeed16(), only faster.
inline void hadamardWalsh16(const float* input, float* output) noexcept
{
    std::array<float, 16> temp1, temp2, temp3;

    // Stage 1: stride 1
    for (size_t i = 0; i < 16; i += 2)
    {
        temp1[i] = input[i] + input[i + 1];
        temp1[i + 1] = input[i] - input[i + 1];
    }

    // Stage 2: stride 2
    for (size_t i = 0; i < 16; i += 4)
    {
        temp2[i] = temp1[i] + temp1[i + 2];
        temp2[i + 1] = temp1[i + 1] + temp1[i + 3];
        temp2[i + 2] = temp1[i] - temp1[i + 2];
        temp2[i + 3] = temp1[i + 1] - temp1[i + 3];
    }

    // Stage 3: stride 4
    for (size_t i = 0; i < 16; i += 8)
    {
        for (size_t j = 0; j < 4; ++j)
        {
            temp3[i + j] = temp2[i + j] + temp2[i + j + 4];
            temp3[i + j + 4] = temp2[i + j] - temp2[i + j + 4];
        }
    }

    // Stage 4: stride 8
    for (size_t i = 0; i < 8; i++)
    {
        output[i] = temp3[i] + temp3[i + 8];
        output[i + 8] = temp3[i] - temp3[i + 8];
    }
}

inline void hadamardWalsh16(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16(input.data(), output.data());
}

#if defined(USE_SIMD_FRAMEWORK)

/// @ingroup reverbs
/// @brief SIMD form of hadamardWalsh16(), each butterfly stage as a shuffle and a signed add.
inline void hadamardWalsh16_simd(const float* input, float* output) noexcept
{
    const simd_float4 v0 = simd_make_float4(input[0], input[1], input[2], input[3]);
    const simd_float4 v1 = simd_make_float4(input[4], input[5], input[6], input[7]);
    const simd_float4 v2 = simd_make_float4(input[8], input[9], input[10], input[11]);
    const simd_float4 v3 = simd_make_float4(input[12], input[13], input[14], input[15]);

    // Stage 1: stride 1
    const simd_float4 t1_0 = simd_make_float4(v0.x + v0.y, v0.x - v0.y, v0.z + v0.w, v0.z - v0.w);
    const simd_float4 t1_1 = simd_make_float4(v1.x + v1.y, v1.x - v1.y, v1.z + v1.w, v1.z - v1.w);
    const simd_float4 t1_2 = simd_make_float4(v2.x + v2.y, v2.x - v2.y, v2.z + v2.w, v2.z - v2.w);
    const simd_float4 t1_3 = simd_make_float4(v3.x + v3.y, v3.x - v3.y, v3.z + v3.w, v3.z - v3.w);

    // Stage 2: stride 2
    const simd_float4 t2_0 = simd_make_float4(t1_0.x + t1_0.z, t1_0.y + t1_0.w, t1_0.x - t1_0.z, t1_0.y - t1_0.w);
    const simd_float4 t2_1 = simd_make_float4(t1_1.x + t1_1.z, t1_1.y + t1_1.w, t1_1.x - t1_1.z, t1_1.y - t1_1.w);
    const simd_float4 t2_2 = simd_make_float4(t1_2.x + t1_2.z, t1_2.y + t1_2.w, t1_2.x - t1_2.z, t1_2.y - t1_2.w);
    const simd_float4 t2_3 = simd_make_float4(t1_3.x + t1_3.z, t1_3.y + t1_3.w, t1_3.x - t1_3.z, t1_3.y - t1_3.w);

    // Stage 3: stride 4
    const simd_float4 t3_0 = t2_0 + t2_1;
    const simd_float4 t3_1 = t2_0 - t2_1;
    const simd_float4 t3_2 = t2_2 + t2_3;
    const simd_float4 t3_3 = t2_2 - t2_3;

    // Stage 4: stride 8
    const simd_float4 out_0 = t3_0 + t3_2;
    const simd_float4 out_1 = t3_1 + t3_3;
    const simd_float4 out_2 = t3_0 - t3_2;
    const simd_float4 out_3 = t3_1 - t3_3;

    output[0] = out_0.x;
    output[1] = out_0.y;
    output[2] = out_0.z;
    output[3] = out_0.w;
    output[4] = out_1.x;
    output[5] = out_1.y;
    output[6] = out_1.z;
    output[7] = out_1.w;
    output[8] = out_2.x;
    output[9] = out_2.y;
    output[10] = out_2.z;
    output[11] = out_2.w;
    output[12] = out_3.x;
    output[13] = out_3.y;
    output[14] = out_3.z;
    output[15] = out_3.w;
}

inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16_simd(input.data(), output.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardWalsh16_simd(const float* input, float* output) noexcept
{
    const __m128 v0 = _mm_loadu_ps(&input[0]);
    const __m128 v1 = _mm_loadu_ps(&input[4]);
    const __m128 v2 = _mm_loadu_ps(&input[8]);
    const __m128 v3 = _mm_loadu_ps(&input[12]);

    // Stage 1: stride 1
    __m128 s1_lo = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(2, 2, 0, 0));
    __m128 s1_hi = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_0 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_1 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v2, v2, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v2, v2, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_2 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_3 = _mm_addsub_ps(s1_lo, s1_hi);

    // Stage 2: stride 2
    __m128 s2_lo_0 = _mm_unpacklo_ps(t1_0, t1_0);
    __m128 s2_hi_0 = _mm_unpackhi_ps(t1_0, t1_0);
    __m128 add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    __m128 sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    const __m128 t2_0 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_1, t1_1);
    s2_hi_0 = _mm_unpackhi_ps(t1_1, t1_1);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    const __m128 t2_1 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_2, t1_2);
    s2_hi_0 = _mm_unpackhi_ps(t1_2, t1_2);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    const __m128 t2_2 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_3, t1_3);
    s2_hi_0 = _mm_unpackhi_ps(t1_3, t1_3);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    const __m128 t2_3 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    // Stage 3: stride 4
    const __m128 t3_0 = _mm_add_ps(t2_0, t2_1);
    const __m128 t3_1 = _mm_sub_ps(t2_0, t2_1);
    const __m128 t3_2 = _mm_add_ps(t2_2, t2_3);
    const __m128 t3_3 = _mm_sub_ps(t2_2, t2_3);

    // Stage 4: stride 8
    const __m128 out_0 = _mm_add_ps(t3_0, t3_2);
    const __m128 out_1 = _mm_add_ps(t3_1, t3_3);
    const __m128 out_2 = _mm_sub_ps(t3_0, t3_2);
    const __m128 out_3 = _mm_sub_ps(t3_1, t3_3);

    _mm_storeu_ps(&output[0], out_0);
    _mm_storeu_ps(&output[4], out_1);
    _mm_storeu_ps(&output[8], out_2);
    _mm_storeu_ps(&output[12], out_3);
}

inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16_simd(input.data(), output.data());
}

#else

inline void hadamardWalsh16_simd(const float* input, float* output) noexcept
{
    hadamardWalsh16(input, output);
}

inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16(input, output);
}

#endif

}
