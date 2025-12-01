#pragma once

#include "Helpers/PlatformIntrinsics.h"
#include <array>

namespace AbacDsp
{

inline void hadamardWalsh32(const float* input, float* output) noexcept
{
    std::array<float, 32> temp1, temp2, temp3, temp4;

    // Stage 1: stride 1
    for (int i = 0; i < 32; i += 2)
    {
        temp1[i] = input[i] + input[i + 1];
        temp1[i + 1] = input[i] - input[i + 1];
    }

    // Stage 2: stride 2
    for (int i = 0; i < 32; i += 4)
    {
        temp2[i] = temp1[i] + temp1[i + 2];
        temp2[i + 1] = temp1[i + 1] + temp1[i + 3];
        temp2[i + 2] = temp1[i] - temp1[i + 2];
        temp2[i + 3] = temp1[i + 1] - temp1[i + 3];
    }

    // Stage 3: stride 4
    for (int i = 0; i < 32; i += 8)
    {
        for (int j = 0; j < 4; j++)
        {
            temp3[i + j] = temp2[i + j] + temp2[i + j + 4];
            temp3[i + j + 4] = temp2[i + j] - temp2[i + j + 4];
        }
    }

    // Stage 4: stride 8
    for (int i = 0; i < 32; i += 16)
    {
        for (int j = 0; j < 8; j++)
        {
            temp4[i + j] = temp3[i + j] + temp3[i + j + 8];
            temp4[i + j + 8] = temp3[i + j] - temp3[i + j + 8];
        }
    }

    // Stage 5: stride 16
    for (int i = 0; i < 16; i++)
    {
        output[i] = temp4[i] + temp4[i + 16];
        output[i + 16] = temp4[i] - temp4[i + 16];
    }
}

inline void hadamardWalsh32(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32(input.data(), output.data());
}

#if defined(USE_SIMD_FRAMEWORK)

inline void hadamardWalsh32_simd(const float* input, float* output) noexcept
{
    simd_float4 v0 = simd_make_float4(input[0], input[1], input[2], input[3]);
    simd_float4 v1 = simd_make_float4(input[4], input[5], input[6], input[7]);
    simd_float4 v2 = simd_make_float4(input[8], input[9], input[10], input[11]);
    simd_float4 v3 = simd_make_float4(input[12], input[13], input[14], input[15]);
    simd_float4 v4 = simd_make_float4(input[16], input[17], input[18], input[19]);
    simd_float4 v5 = simd_make_float4(input[20], input[21], input[22], input[23]);
    simd_float4 v6 = simd_make_float4(input[24], input[25], input[26], input[27]);
    simd_float4 v7 = simd_make_float4(input[28], input[29], input[30], input[31]);

    // Stage 1: stride 1
    simd_float4 t1_0 = simd_make_float4(v0.x + v0.y, v0.x - v0.y, v0.z + v0.w, v0.z - v0.w);
    simd_float4 t1_1 = simd_make_float4(v1.x + v1.y, v1.x - v1.y, v1.z + v1.w, v1.z - v1.w);
    simd_float4 t1_2 = simd_make_float4(v2.x + v2.y, v2.x - v2.y, v2.z + v2.w, v2.z - v2.w);
    simd_float4 t1_3 = simd_make_float4(v3.x + v3.y, v3.x - v3.y, v3.z + v3.w, v3.z - v3.w);
    simd_float4 t1_4 = simd_make_float4(v4.x + v4.y, v4.x - v4.y, v4.z + v4.w, v4.z - v4.w);
    simd_float4 t1_5 = simd_make_float4(v5.x + v5.y, v5.x - v5.y, v5.z + v5.w, v5.z - v5.w);
    simd_float4 t1_6 = simd_make_float4(v6.x + v6.y, v6.x - v6.y, v6.z + v6.w, v6.z - v6.w);
    simd_float4 t1_7 = simd_make_float4(v7.x + v7.y, v7.x - v7.y, v7.z + v7.w, v7.z - v7.w);

    // Stage 2: stride 2
    simd_float4 t2_0 = simd_make_float4(t1_0.x + t1_0.z, t1_0.y + t1_0.w, t1_0.x - t1_0.z, t1_0.y - t1_0.w);
    simd_float4 t2_1 = simd_make_float4(t1_1.x + t1_1.z, t1_1.y + t1_1.w, t1_1.x - t1_1.z, t1_1.y - t1_1.w);
    simd_float4 t2_2 = simd_make_float4(t1_2.x + t1_2.z, t1_2.y + t1_2.w, t1_2.x - t1_2.z, t1_2.y - t1_2.w);
    simd_float4 t2_3 = simd_make_float4(t1_3.x + t1_3.z, t1_3.y + t1_3.w, t1_3.x - t1_3.z, t1_3.y - t1_3.w);
    simd_float4 t2_4 = simd_make_float4(t1_4.x + t1_4.z, t1_4.y + t1_4.w, t1_4.x - t1_4.z, t1_4.y - t1_4.w);
    simd_float4 t2_5 = simd_make_float4(t1_5.x + t1_5.z, t1_5.y + t1_5.w, t1_5.x - t1_5.z, t1_5.y - t1_5.w);
    simd_float4 t2_6 = simd_make_float4(t1_6.x + t1_6.z, t1_6.y + t1_6.w, t1_6.x - t1_6.z, t1_6.y - t1_6.w);
    simd_float4 t2_7 = simd_make_float4(t1_7.x + t1_7.z, t1_7.y + t1_7.w, t1_7.x - t1_7.z, t1_7.y - t1_7.w);

    // Stage 3: stride 4
    simd_float4 t3_0 = t2_0 + t2_1;
    simd_float4 t3_1 = t2_0 - t2_1;
    simd_float4 t3_2 = t2_2 + t2_3;
    simd_float4 t3_3 = t2_2 - t2_3;
    simd_float4 t3_4 = t2_4 + t2_5;
    simd_float4 t3_5 = t2_4 - t2_5;
    simd_float4 t3_6 = t2_6 + t2_7;
    simd_float4 t3_7 = t2_6 - t2_7;

    // Stage 4: stride 8
    simd_float4 t4_0 = t3_0 + t3_2;
    simd_float4 t4_1 = t3_1 + t3_3;
    simd_float4 t4_2 = t3_0 - t3_2;
    simd_float4 t4_3 = t3_1 - t3_3;
    simd_float4 t4_4 = t3_4 + t3_6;
    simd_float4 t4_5 = t3_5 + t3_7;
    simd_float4 t4_6 = t3_4 - t3_6;
    simd_float4 t4_7 = t3_5 - t3_7;

    // Stage 5: stride 16
    simd_float4 out_0 = t4_0 + t4_4;
    simd_float4 out_1 = t4_1 + t4_5;
    simd_float4 out_2 = t4_2 + t4_6;
    simd_float4 out_3 = t4_3 + t4_7;
    simd_float4 out_4 = t4_0 - t4_4;
    simd_float4 out_5 = t4_1 - t4_5;
    simd_float4 out_6 = t4_2 - t4_6;
    simd_float4 out_7 = t4_3 - t4_7;

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
    output[16] = out_4.x;
    output[17] = out_4.y;
    output[18] = out_4.z;
    output[19] = out_4.w;
    output[20] = out_5.x;
    output[21] = out_5.y;
    output[22] = out_5.z;
    output[23] = out_5.w;
    output[24] = out_6.x;
    output[25] = out_6.y;
    output[26] = out_6.z;
    output[27] = out_6.w;
    output[28] = out_7.x;
    output[29] = out_7.y;
    output[30] = out_7.z;
    output[31] = out_7.w;
}

inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32_simd(input.data(), output.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardWalsh32_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_loadu_ps(&input[0]);
    __m128 v1 = _mm_loadu_ps(&input[4]);
    __m128 v2 = _mm_loadu_ps(&input[8]);
    __m128 v3 = _mm_loadu_ps(&input[12]);
    __m128 v4 = _mm_loadu_ps(&input[16]);
    __m128 v5 = _mm_loadu_ps(&input[20]);
    __m128 v6 = _mm_loadu_ps(&input[24]);
    __m128 v7 = _mm_loadu_ps(&input[28]);

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

    s1_lo = _mm_shuffle_ps(v4, v4, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v4, v4, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_4 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v5, v5, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v5, v5, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_5 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v6, v6, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v6, v6, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_6 = _mm_addsub_ps(s1_lo, s1_hi);

    s1_lo = _mm_shuffle_ps(v7, v7, _MM_SHUFFLE(2, 2, 0, 0));
    s1_hi = _mm_shuffle_ps(v7, v7, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 t1_7 = _mm_addsub_ps(s1_lo, s1_hi);

    // Stage 2: stride 2
    __m128 s2_lo_0 = _mm_unpacklo_ps(t1_0, t1_0);
    __m128 s2_hi_0 = _mm_unpackhi_ps(t1_0, t1_0);
    __m128 add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    __m128 sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_0 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_1, t1_1);
    s2_hi_0 = _mm_unpackhi_ps(t1_1, t1_1);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_1 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_2, t1_2);
    s2_hi_0 = _mm_unpackhi_ps(t1_2, t1_2);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_2 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_3, t1_3);
    s2_hi_0 = _mm_unpackhi_ps(t1_3, t1_3);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_3 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_4, t1_4);
    s2_hi_0 = _mm_unpackhi_ps(t1_4, t1_4);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_4 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_5, t1_5);
    s2_hi_0 = _mm_unpackhi_ps(t1_5, t1_5);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_5 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_6, t1_6);
    s2_hi_0 = _mm_unpackhi_ps(t1_6, t1_6);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_6 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    s2_lo_0 = _mm_unpacklo_ps(t1_7, t1_7);
    s2_hi_0 = _mm_unpackhi_ps(t1_7, t1_7);
    add_0 = _mm_add_ps(s2_lo_0, s2_hi_0);
    sub_0 = _mm_sub_ps(s2_lo_0, s2_hi_0);
    __m128 t2_7 = _mm_shuffle_ps(add_0, sub_0, _MM_SHUFFLE(0, 2, 0, 2));

    // Stage 3: stride 4
    __m128 t3_0 = _mm_add_ps(t2_0, t2_1);
    __m128 t3_1 = _mm_sub_ps(t2_0, t2_1);
    __m128 t3_2 = _mm_add_ps(t2_2, t2_3);
    __m128 t3_3 = _mm_sub_ps(t2_2, t2_3);
    __m128 t3_4 = _mm_add_ps(t2_4, t2_5);
    __m128 t3_5 = _mm_sub_ps(t2_4, t2_5);
    __m128 t3_6 = _mm_add_ps(t2_6, t2_7);
    __m128 t3_7 = _mm_sub_ps(t2_6, t2_7);

    // Stage 4: stride 8
    __m128 t4_0 = _mm_add_ps(t3_0, t3_2);
    __m128 t4_1 = _mm_add_ps(t3_1, t3_3);
    __m128 t4_2 = _mm_sub_ps(t3_0, t3_2);
    __m128 t4_3 = _mm_sub_ps(t3_1, t3_3);
    __m128 t4_4 = _mm_add_ps(t3_4, t3_6);
    __m128 t4_5 = _mm_add_ps(t3_5, t3_7);
    __m128 t4_6 = _mm_sub_ps(t3_4, t3_6);
    __m128 t4_7 = _mm_sub_ps(t3_5, t3_7);

    // Stage 5: stride 16
    __m128 out_0 = _mm_add_ps(t4_0, t4_4);
    __m128 out_1 = _mm_add_ps(t4_1, t4_5);
    __m128 out_2 = _mm_add_ps(t4_2, t4_6);
    __m128 out_3 = _mm_add_ps(t4_3, t4_7);
    __m128 out_4 = _mm_sub_ps(t4_0, t4_4);
    __m128 out_5 = _mm_sub_ps(t4_1, t4_5);
    __m128 out_6 = _mm_sub_ps(t4_2, t4_6);
    __m128 out_7 = _mm_sub_ps(t4_3, t4_7);

    _mm_storeu_ps(&output[0], out_0);
    _mm_storeu_ps(&output[4], out_1);
    _mm_storeu_ps(&output[8], out_2);
    _mm_storeu_ps(&output[12], out_3);
    _mm_storeu_ps(&output[16], out_4);
    _mm_storeu_ps(&output[20], out_5);
    _mm_storeu_ps(&output[24], out_6);
    _mm_storeu_ps(&output[28], out_7);
}

inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32_simd(input.data(), output.data());
}

#else

inline void hadamardWalsh32_simd(const float* input, float* output) noexcept
{
    hadamardWalsh32(input, output);
}

inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32(input, output);
}

#endif

}
