#pragma once

#include <array>

namespace AbacDsp
{

inline void hadamardWalsh32(const float* input, float* output) noexcept
{
    std::array<float, 32> temp1, temp2, temp3, temp4;

    // Stage 1: stride 1
    temp1[0] = input[0] + input[1];
    temp1[1] = input[0] - input[1];
    temp1[2] = input[2] + input[3];
    temp1[3] = input[2] - input[3];
    temp1[4] = input[4] + input[5];
    temp1[5] = input[4] - input[5];
    temp1[6] = input[6] + input[7];
    temp1[7] = input[6] - input[7];
    temp1[8] = input[8] + input[9];
    temp1[9] = input[8] - input[9];
    temp1[10] = input[10] + input[11];
    temp1[11] = input[10] - input[11];
    temp1[12] = input[12] + input[13];
    temp1[13] = input[12] - input[13];
    temp1[14] = input[14] + input[15];
    temp1[15] = input[14] - input[15];
    temp1[16] = input[16] + input[17];
    temp1[17] = input[16] - input[17];
    temp1[18] = input[18] + input[19];
    temp1[19] = input[18] - input[19];
    temp1[20] = input[20] + input[21];
    temp1[21] = input[20] - input[21];
    temp1[22] = input[22] + input[23];
    temp1[23] = input[22] - input[23];
    temp1[24] = input[24] + input[25];
    temp1[25] = input[24] - input[25];
    temp1[26] = input[26] + input[27];
    temp1[27] = input[26] - input[27];
    temp1[28] = input[28] + input[29];
    temp1[29] = input[28] - input[29];
    temp1[30] = input[30] + input[31];
    temp1[31] = input[30] - input[31];

    // Stage 2: stride 2
    temp2[0] = temp1[0] + temp1[2];
    temp2[1] = temp1[1] + temp1[3];
    temp2[2] = temp1[0] - temp1[2];
    temp2[3] = temp1[1] - temp1[3];
    temp2[4] = temp1[4] + temp1[6];
    temp2[5] = temp1[5] + temp1[7];
    temp2[6] = temp1[4] - temp1[6];
    temp2[7] = temp1[5] - temp1[7];
    temp2[8] = temp1[8] + temp1[10];
    temp2[9] = temp1[9] + temp1[11];
    temp2[10] = temp1[8] - temp1[10];
    temp2[11] = temp1[9] - temp1[11];
    temp2[12] = temp1[12] + temp1[14];
    temp2[13] = temp1[13] + temp1[15];
    temp2[14] = temp1[12] - temp1[14];
    temp2[15] = temp1[13] - temp1[15];
    temp2[16] = temp1[16] + temp1[18];
    temp2[17] = temp1[17] + temp1[19];
    temp2[18] = temp1[16] - temp1[18];
    temp2[19] = temp1[17] - temp1[19];
    temp2[20] = temp1[20] + temp1[22];
    temp2[21] = temp1[21] + temp1[23];
    temp2[22] = temp1[20] - temp1[22];
    temp2[23] = temp1[21] - temp1[23];
    temp2[24] = temp1[24] + temp1[26];
    temp2[25] = temp1[25] + temp1[27];
    temp2[26] = temp1[24] - temp1[26];
    temp2[27] = temp1[25] - temp1[27];
    temp2[28] = temp1[28] + temp1[30];
    temp2[29] = temp1[29] + temp1[31];
    temp2[30] = temp1[28] - temp1[30];
    temp2[31] = temp1[29] - temp1[31];

    // Stage 3: stride 4
    temp3[0] = temp2[0] + temp2[4];
    temp3[1] = temp2[1] + temp2[5];
    temp3[2] = temp2[2] + temp2[6];
    temp3[3] = temp2[3] + temp2[7];
    temp3[4] = temp2[0] - temp2[4];
    temp3[5] = temp2[1] - temp2[5];
    temp3[6] = temp2[2] - temp2[6];
    temp3[7] = temp2[3] - temp2[7];
    temp3[8] = temp2[8] + temp2[12];
    temp3[9] = temp2[9] + temp2[13];
    temp3[10] = temp2[10] + temp2[14];
    temp3[11] = temp2[11] + temp2[15];
    temp3[12] = temp2[8] - temp2[12];
    temp3[13] = temp2[9] - temp2[13];
    temp3[14] = temp2[10] - temp2[14];
    temp3[15] = temp2[11] - temp2[15];
    temp3[16] = temp2[16] + temp2[20];
    temp3[17] = temp2[17] + temp2[21];
    temp3[18] = temp2[18] + temp2[22];
    temp3[19] = temp2[19] + temp2[23];
    temp3[20] = temp2[16] - temp2[20];
    temp3[21] = temp2[17] - temp2[21];
    temp3[22] = temp2[18] - temp2[22];
    temp3[23] = temp2[19] - temp2[23];
    temp3[24] = temp2[24] + temp2[28];
    temp3[25] = temp2[25] + temp2[29];
    temp3[26] = temp2[26] + temp2[30];
    temp3[27] = temp2[27] + temp2[31];
    temp3[28] = temp2[24] - temp2[28];
    temp3[29] = temp2[25] - temp2[29];
    temp3[30] = temp2[26] - temp2[30];
    temp3[31] = temp2[27] - temp2[31];

    // Stage 4: stride 8
    temp4[0] = temp3[0] + temp3[8];
    temp4[1] = temp3[1] + temp3[9];
    temp4[2] = temp3[2] + temp3[10];
    temp4[3] = temp3[3] + temp3[11];
    temp4[4] = temp3[4] + temp3[12];
    temp4[5] = temp3[5] + temp3[13];
    temp4[6] = temp3[6] + temp3[14];
    temp4[7] = temp3[7] + temp3[15];
    temp4[8] = temp3[0] - temp3[8];
    temp4[9] = temp3[1] - temp3[9];
    temp4[10] = temp3[2] - temp3[10];
    temp4[11] = temp3[3] - temp3[11];
    temp4[12] = temp3[4] - temp3[12];
    temp4[13] = temp3[5] - temp3[13];
    temp4[14] = temp3[6] - temp3[14];
    temp4[15] = temp3[7] - temp3[15];
    temp4[16] = temp3[16] + temp3[24];
    temp4[17] = temp3[17] + temp3[25];
    temp4[18] = temp3[18] + temp3[26];
    temp4[19] = temp3[19] + temp3[27];
    temp4[20] = temp3[20] + temp3[28];
    temp4[21] = temp3[21] + temp3[29];
    temp4[22] = temp3[22] + temp3[30];
    temp4[23] = temp3[23] + temp3[31];
    temp4[24] = temp3[16] - temp3[24];
    temp4[25] = temp3[17] - temp3[25];
    temp4[26] = temp3[18] - temp3[26];
    temp4[27] = temp3[19] - temp3[27];
    temp4[28] = temp3[20] - temp3[28];
    temp4[29] = temp3[21] - temp3[29];
    temp4[30] = temp3[22] - temp3[30];
    temp4[31] = temp3[23] - temp3[31];

    // Stage 5: stride 16
    output[0] = temp4[0] + temp4[16];
    output[1] = temp4[1] + temp4[17];
    output[2] = temp4[2] + temp4[18];
    output[3] = temp4[3] + temp4[19];
    output[4] = temp4[4] + temp4[20];
    output[5] = temp4[5] + temp4[21];
    output[6] = temp4[6] + temp4[22];
    output[7] = temp4[7] + temp4[23];
    output[8] = temp4[8] + temp4[24];
    output[9] = temp4[9] + temp4[25];
    output[10] = temp4[10] + temp4[26];
    output[11] = temp4[11] + temp4[27];
    output[12] = temp4[12] + temp4[28];
    output[13] = temp4[13] + temp4[29];
    output[14] = temp4[14] + temp4[30];
    output[15] = temp4[15] + temp4[31];
    output[16] = temp4[0] - temp4[16];
    output[17] = temp4[1] - temp4[17];
    output[18] = temp4[2] - temp4[18];
    output[19] = temp4[3] - temp4[19];
    output[20] = temp4[4] - temp4[20];
    output[21] = temp4[5] - temp4[21];
    output[22] = temp4[6] - temp4[22];
    output[23] = temp4[7] - temp4[23];
    output[24] = temp4[8] - temp4[24];
    output[25] = temp4[9] - temp4[25];
    output[26] = temp4[10] - temp4[26];
    output[27] = temp4[11] - temp4[27];
    output[28] = temp4[12] - temp4[28];
    output[29] = temp4[13] - temp4[29];
    output[30] = temp4[14] - temp4[30];
    output[31] = temp4[15] - temp4[31];
}

inline void hadamardWalsh32(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32(input.data(), output.data());
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>

inline void hadamardWalsh32_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_load_ps(&input[0]);
    __m128 v1 = _mm_load_ps(&input[4]);
    __m128 v2 = _mm_load_ps(&input[8]);
    __m128 v3 = _mm_load_ps(&input[12]);
    __m128 v4 = _mm_load_ps(&input[16]);
    __m128 v5 = _mm_load_ps(&input[20]);
    __m128 v6 = _mm_load_ps(&input[24]);
    __m128 v7 = _mm_load_ps(&input[28]);

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

    __m128 s2_lo_1 = _mm_unpacklo_ps(t1_1, t1_1);
    __m128 s2_hi_1 = _mm_unpackhi_ps(t1_1, t1_1);
    __m128 add_1 = _mm_add_ps(s2_lo_1, s2_hi_1);
    __m128 sub_1 = _mm_sub_ps(s2_lo_1, s2_hi_1);
    __m128 t2_1 = _mm_shuffle_ps(add_1, sub_1, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_2 = _mm_unpacklo_ps(t1_2, t1_2);
    __m128 s2_hi_2 = _mm_unpackhi_ps(t1_2, t1_2);
    __m128 add_2 = _mm_add_ps(s2_lo_2, s2_hi_2);
    __m128 sub_2 = _mm_sub_ps(s2_lo_2, s2_hi_2);
    __m128 t2_2 = _mm_shuffle_ps(add_2, sub_2, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_3 = _mm_unpacklo_ps(t1_3, t1_3);
    __m128 s2_hi_3 = _mm_unpackhi_ps(t1_3, t1_3);
    __m128 add_3 = _mm_add_ps(s2_lo_3, s2_hi_3);
    __m128 sub_3 = _mm_sub_ps(s2_lo_3, s2_hi_3);
    __m128 t2_3 = _mm_shuffle_ps(add_3, sub_3, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_4 = _mm_unpacklo_ps(t1_4, t1_4);
    __m128 s2_hi_4 = _mm_unpackhi_ps(t1_4, t1_4);
    __m128 add_4 = _mm_add_ps(s2_lo_4, s2_hi_4);
    __m128 sub_4 = _mm_sub_ps(s2_lo_4, s2_hi_4);
    __m128 t2_4 = _mm_shuffle_ps(add_4, sub_4, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_5 = _mm_unpacklo_ps(t1_5, t1_5);
    __m128 s2_hi_5 = _mm_unpackhi_ps(t1_5, t1_5);
    __m128 add_5 = _mm_add_ps(s2_lo_5, s2_hi_5);
    __m128 sub_5 = _mm_sub_ps(s2_lo_5, s2_hi_5);
    __m128 t2_5 = _mm_shuffle_ps(add_5, sub_5, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_6 = _mm_unpacklo_ps(t1_6, t1_6);
    __m128 s2_hi_6 = _mm_unpackhi_ps(t1_6, t1_6);
    __m128 add_6 = _mm_add_ps(s2_lo_6, s2_hi_6);
    __m128 sub_6 = _mm_sub_ps(s2_lo_6, s2_hi_6);
    __m128 t2_6 = _mm_shuffle_ps(add_6, sub_6, _MM_SHUFFLE(0, 2, 0, 2));

    __m128 s2_lo_7 = _mm_unpacklo_ps(t1_7, t1_7);
    __m128 s2_hi_7 = _mm_unpackhi_ps(t1_7, t1_7);
    __m128 add_7 = _mm_add_ps(s2_lo_7, s2_hi_7);
    __m128 sub_7 = _mm_sub_ps(s2_lo_7, s2_hi_7);
    __m128 t2_7 = _mm_shuffle_ps(add_7, sub_7, _MM_SHUFFLE(0, 2, 0, 2));

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

    _mm_store_ps(&output[0], out_0);
    _mm_store_ps(&output[4], out_1);
    _mm_store_ps(&output[8], out_2);
    _mm_store_ps(&output[12], out_3);
    _mm_store_ps(&output[16], out_4);
    _mm_store_ps(&output[20], out_5);
    _mm_store_ps(&output[24], out_6);
    _mm_store_ps(&output[28], out_7);
}

inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32_simd(input.data(), output.data());
}

#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

inline void hadamardWalsh32_simd(const float* input, float* output) noexcept
{
    alignas(16) static constexpr float sign_mask[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    float32x4_t sign = vld1q_f32(sign_mask);

    float32x4_t v0 = vld1q_f32(&input[0]);
    float32x4_t v1 = vld1q_f32(&input[4]);
    float32x4_t v2 = vld1q_f32(&input[8]);
    float32x4_t v3 = vld1q_f32(&input[12]);
    float32x4_t v4 = vld1q_f32(&input[16]);
    float32x4_t v5 = vld1q_f32(&input[20]);
    float32x4_t v6 = vld1q_f32(&input[24]);
    float32x4_t v7 = vld1q_f32(&input[28]);

    // Stage 1
    float32x2_t v0_lo = vget_low_f32(v0);
    float32x2_t v0_hi = vget_high_f32(v0);
    float32x4_t s1_lo_0 = vcombine_f32(vdup_lane_f32(v0_lo, 0), vdup_lane_f32(v0_hi, 0));
    float32x4_t s1_hi_0 = vcombine_f32(vdup_lane_f32(v0_lo, 1), vdup_lane_f32(v0_hi, 1));
    float32x4_t t1_0 = vaddq_f32(s1_lo_0, vmulq_f32(s1_hi_0, sign));

    float32x2_t v1_lo = vget_low_f32(v1);
    float32x2_t v1_hi = vget_high_f32(v1);
    float32x4_t s1_lo_1 = vcombine_f32(vdup_lane_f32(v1_lo, 0), vdup_lane_f32(v1_hi, 0));
    float32x4_t s1_hi_1 = vcombine_f32(vdup_lane_f32(v1_lo, 1), vdup_lane_f32(v1_hi, 1));
    float32x4_t t1_1 = vaddq_f32(s1_lo_1, vmulq_f32(s1_hi_1, sign));

    float32x2_t v2_lo = vget_low_f32(v2);
    float32x2_t v2_hi = vget_high_f32(v2);
    float32x4_t s1_lo_2 = vcombine_f32(vdup_lane_f32(v2_lo, 0), vdup_lane_f32(v2_hi, 0));
    float32x4_t s1_hi_2 = vcombine_f32(vdup_lane_f32(v2_lo, 1), vdup_lane_f32(v2_hi, 1));
    float32x4_t t1_2 = vaddq_f32(s1_lo_2, vmulq_f32(s1_hi_2, sign));

    float32x2_t v3_lo = vget_low_f32(v3);
    float32x2_t v3_hi = vget_high_f32(v3);
    float32x4_t s1_lo_3 = vcombine_f32(vdup_lane_f32(v3_lo, 0), vdup_lane_f32(v3_hi, 0));
    float32x4_t s1_hi_3 = vcombine_f32(vdup_lane_f32(v3_lo, 1), vdup_lane_f32(v3_hi, 1));
    float32x4_t t1_3 = vaddq_f32(s1_lo_3, vmulq_f32(s1_hi_3, sign));

    float32x2_t v4_lo = vget_low_f32(v4);
    float32x2_t v4_hi = vget_high_f32(v4);
    float32x4_t s1_lo_4 = vcombine_f32(vdup_lane_f32(v4_lo, 0), vdup_lane_f32(v4_hi, 0));
    float32x4_t s1_hi_4 = vcombine_f32(vdup_lane_f32(v4_lo, 1), vdup_lane_f32(v4_hi, 1));
    float32x4_t t1_4 = vaddq_f32(s1_lo_4, vmulq_f32(s1_hi_4, sign));

    float32x2_t v5_lo = vget_low_f32(v5);
    float32x2_t v5_hi = vget_high_f32(v5);
    float32x4_t s1_lo_5 = vcombine_f32(vdup_lane_f32(v5_lo, 0), vdup_lane_f32(v5_hi, 0));
    float32x4_t s1_hi_5 = vcombine_f32(vdup_lane_f32(v5_lo, 1), vdup_lane_f32(v5_hi, 1));
    float32x4_t t1_5 = vaddq_f32(s1_lo_5, vmulq_f32(s1_hi_5, sign));

    float32x2_t v6_lo = vget_low_f32(v6);
    float32x2_t v6_hi = vget_high_f32(v6);
    float32x4_t s1_lo_6 = vcombine_f32(vdup_lane_f32(v6_lo, 0), vdup_lane_f32(v6_hi, 0));
    float32x4_t s1_hi_6 = vcombine_f32(vdup_lane_f32(v6_lo, 1), vdup_lane_f32(v6_hi, 1));
    float32x4_t t1_6 = vaddq_f32(s1_lo_6, vmulq_f32(s1_hi_6, sign));

    float32x2_t v7_lo = vget_low_f32(v7);
    float32x2_t v7_hi = vget_high_f32(v7);
    float32x4_t s1_lo_7 = vcombine_f32(vdup_lane_f32(v7_lo, 0), vdup_lane_f32(v7_hi, 0));
    float32x4_t s1_hi_7 = vcombine_f32(vdup_lane_f32(v7_lo, 1), vdup_lane_f32(v7_hi, 1));
    float32x4_t t1_7 = vaddq_f32(s1_lo_7, vmulq_f32(s1_hi_7, sign));

    // Stage 2
    float32x2_t t1_0_lo = vget_low_f32(t1_0);
    float32x2_t t1_0_hi = vget_high_f32(t1_0);
    float32x4_t s2_lo_0 = vcombine_f32(vdup_lane_f32(t1_0_lo, 0), vdup_lane_f32(t1_0_lo, 1));
    float32x4_t s2_hi_0 = vcombine_f32(vdup_lane_f32(t1_0_hi, 0), vdup_lane_f32(t1_0_hi, 1));
    float32x4_t add_0 = vaddq_f32(s2_lo_0, s2_hi_0);
    float32x4_t sub_0 = vsubq_f32(s2_lo_0, s2_hi_0);
    float32x4_t t2_0 = vuzpq_f32(add_0, sub_0).val[0];

    float32x2_t t1_1_lo = vget_low_f32(t1_1);
    float32x2_t t1_1_hi = vget_high_f32(t1_1);
    float32x4_t s2_lo_1 = vcombine_f32(vdup_lane_f32(t1_1_lo, 0), vdup_lane_f32(t1_1_lo, 1));
    float32x4_t s2_hi_1 = vcombine_f32(vdup_lane_f32(t1_1_hi, 0), vdup_lane_f32(t1_1_hi, 1));
    float32x4_t add_1 = vaddq_f32(s2_lo_1, s2_hi_1);
    float32x4_t sub_1 = vsubq_f32(s2_lo_1, s2_hi_1);
    float32x4_t t2_1 = vuzpq_f32(add_1, sub_1).val[0];

    float32x2_t t1_2_lo = vget_low_f32(t1_2);
    float32x2_t t1_2_hi = vget_high_f32(t1_2);
    float32x4_t s2_lo_2 = vcombine_f32(vdup_lane_f32(t1_2_lo, 0), vdup_lane_f32(t1_2_lo, 1));
    float32x4_t s2_hi_2 = vcombine_f32(vdup_lane_f32(t1_2_hi, 0), vdup_lane_f32(t1_2_hi, 1));
    float32x4_t add_2 = vaddq_f32(s2_lo_2, s2_hi_2);
    float32x4_t sub_2 = vsubq_f32(s2_lo_2, s2_hi_2);
    float32x4_t t2_2 = vuzpq_f32(add_2, sub_2).val[0];

    float32x2_t t1_3_lo = vget_low_f32(t1_3);
    float32x2_t t1_3_hi = vget_high_f32(t1_3);
    float32x4_t s2_lo_3 = vcombine_f32(vdup_lane_f32(t1_3_lo, 0), vdup_lane_f32(t1_3_lo, 1));
    float32x4_t s2_hi_3 = vcombine_f32(vdup_lane_f32(t1_3_hi, 0), vdup_lane_f32(t1_3_hi, 1));
    float32x4_t add_3 = vaddq_f32(s2_lo_3, s2_hi_3);
    float32x4_t sub_3 = vsubq_f32(s2_lo_3, s2_hi_3);
    float32x4_t t2_3 = vuzpq_f32(add_3, sub_3).val[0];

    float32x2_t t1_4_lo = vget_low_f32(t1_4);
    float32x2_t t1_4_hi = vget_high_f32(t1_4);
    float32x4_t s2_lo_4 = vcombine_f32(vdup_lane_f32(t1_4_lo, 0), vdup_lane_f32(t1_4_lo, 1));
    float32x4_t s2_hi_4 = vcombine_f32(vdup_lane_f32(t1_4_hi, 0), vdup_lane_f32(t1_4_hi, 1));
    float32x4_t add_4 = vaddq_f32(s2_lo_4, s2_hi_4);
    float32x4_t sub_4 = vsubq_f32(s2_lo_4, s2_hi_4);
    float32x4_t t2_4 = vuzpq_f32(add_4, sub_4).val[0];

    float32x2_t t1_5_lo = vget_low_f32(t1_5);
    float32x2_t t1_5_hi = vget_high_f32(t1_5);
    float32x4_t s2_lo_5 = vcombine_f32(vdup_lane_f32(t1_5_lo, 0), vdup_lane_f32(t1_5_lo, 1));
    float32x4_t s2_hi_5 = vcombine_f32(vdup_lane_f32(t1_5_hi, 0), vdup_lane_f32(t1_5_hi, 1));
    float32x4_t add_5 = vaddq_f32(s2_lo_5, s2_hi_5);
    float32x4_t sub_5 = vsubq_f32(s2_lo_5, s2_hi_5);
    float32x4_t t2_5 = vuzpq_f32(add_5, sub_5).val[0];

    float32x2_t t1_6_lo = vget_low_f32(t1_6);
    float32x2_t t1_6_hi = vget_high_f32(t1_6);
    float32x4_t s2_lo_6 = vcombine_f32(vdup_lane_f32(t1_6_lo, 0), vdup_lane_f32(t1_6_lo, 1));
    float32x4_t s2_hi_6 = vcombine_f32(vdup_lane_f32(t1_6_hi, 0), vdup_lane_f32(t1_6_hi, 1));
    float32x4_t add_6 = vaddq_f32(s2_lo_6, s2_hi_6);
    float32x4_t sub_6 = vsubq_f32(s2_lo_6, s2_hi_6);
    float32x4_t t2_6 = vuzpq_f32(add_6, sub_6).val[0];

    float32x2_t t1_7_lo = vget_low_f32(t1_7);
    float32x2_t t1_7_hi = vget_high_f32(t1_7);
    float32x4_t s2_lo_7 = vcombine_f32(vdup_lane_f32(t1_7_lo, 0), vdup_lane_f32(t1_7_lo, 1));
    float32x4_t s2_hi_7 = vcombine_f32(vdup_lane_f32(t1_7_hi, 0), vdup_lane_f32(t1_7_hi, 1));
    float32x4_t add_7 = vaddq_f32(s2_lo_7, s2_hi_7);
    float32x4_t sub_7 = vsubq_f32(s2_lo_7, s2_hi_7);
    float32x4_t t2_7 = vuzpq_f32(add_7, sub_7).val[0];

    // Stage 3
    float32x4_t t3_0 = vaddq_f32(t2_0, t2_1);
    float32x4_t t3_1 = vsubq_f32(t2_0, t2_1);
    float32x4_t t3_2 = vaddq_f32(t2_2, t2_3);
    float32x4_t t3_3 = vsubq_f32(t2_2, t2_3);
    float32x4_t t3_4 = vaddq_f32(t2_4, t2_5);
    float32x4_t t3_5 = vsubq_f32(t2_4, t2_5);
    float32x4_t t3_6 = vaddq_f32(t2_6, t2_7);
    float32x4_t t3_7 = vsubq_f32(t2_6, t2_7);

    // Stage 4
    float32x4_t t4_0 = vaddq_f32(t3_0, t3_2);
    float32x4_t t4_1 = vaddq_f32(t3_1, t3_3);
    float32x4_t t4_2 = vsubq_f32(t3_0, t3_2);
    float32x4_t t4_3 = vsubq_f32(t3_1, t3_3);
    float32x4_t t4_4 = vaddq_f32(t3_4, t3_6);
    float32x4_t t4_5 = vaddq_f32(t3_5, t3_7);
    float32x4_t t4_6 = vsubq_f32(t3_4, t3_6);
    float32x4_t t4_7 = vsubq_f32(t3_5, t3_7);

    // Stage 5
    float32x4_t out_0 = vaddq_f32(t4_0, t4_4);
    float32x4_t out_1 = vaddq_f32(t4_1, t4_5);
    float32x4_t out_2 = vaddq_f32(t4_2, t4_6);
    float32x4_t out_3 = vaddq_f32(t4_3, t4_7);
    float32x4_t out_4 = vsubq_f32(t4_0, t4_4);
    float32x4_t out_5 = vsubq_f32(t4_1, t4_5);
    float32x4_t out_6 = vsubq_f32(t4_2, t4_6);
    float32x4_t out_7 = vsubq_f32(t4_3, t4_7);

    vst1q_f32(&output[0], out_0);
    vst1q_f32(&output[4], out_1);
    vst1q_f32(&output[8], out_2);
    vst1q_f32(&output[12], out_3);
    vst1q_f32(&output[16], out_4);
    vst1q_f32(&output[20], out_5);
    vst1q_f32(&output[24], out_6);
    vst1q_f32(&output[28], out_7);
}

inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32_simd(input.data(), output.data());
}

#else
inline void hadamardWalsh32_simd(const std::array<float, 32>& input, std::array<float, 32>& output) noexcept
{
    hadamardWalsh32(input.data(), output.data());
}
#endif

}
