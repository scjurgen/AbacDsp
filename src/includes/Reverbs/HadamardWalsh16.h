#pragma once

#include <array>

namespace AbacDsp
{

inline void hadamardWalsh16(const float* input, float* output) noexcept
{
    std::array<float, 16> temp1, temp2, temp3;

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

    // Stage 4: stride 8
    output[0] = temp3[0] + temp3[8];
    output[1] = temp3[1] + temp3[9];
    output[2] = temp3[2] + temp3[10];
    output[3] = temp3[3] + temp3[11];
    output[4] = temp3[4] + temp3[12];
    output[5] = temp3[5] + temp3[13];
    output[6] = temp3[6] + temp3[14];
    output[7] = temp3[7] + temp3[15];
    output[8] = temp3[0] - temp3[8];
    output[9] = temp3[1] - temp3[9];
    output[10] = temp3[2] - temp3[10];
    output[11] = temp3[3] - temp3[11];
    output[12] = temp3[4] - temp3[12];
    output[13] = temp3[5] - temp3[13];
    output[14] = temp3[6] - temp3[14];
    output[15] = temp3[7] - temp3[15];
}

inline void hadamardWalsh16(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16(input.data(), output.data());
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>

inline void hadamardWalsh16_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_load_ps(&input[0]);
    __m128 v1 = _mm_load_ps(&input[4]);
    __m128 v2 = _mm_load_ps(&input[8]);
    __m128 v3 = _mm_load_ps(&input[12]);

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

    // Stage 3: stride 4
    __m128 t3_0 = _mm_add_ps(t2_0, t2_1);
    __m128 t3_1 = _mm_sub_ps(t2_0, t2_1);
    __m128 t3_2 = _mm_add_ps(t2_2, t2_3);
    __m128 t3_3 = _mm_sub_ps(t2_2, t2_3);

    // Stage 4: stride 8
    __m128 out_0 = _mm_add_ps(t3_0, t3_2);
    __m128 out_1 = _mm_add_ps(t3_1, t3_3);
    __m128 out_2 = _mm_sub_ps(t3_0, t3_2);
    __m128 out_3 = _mm_sub_ps(t3_1, t3_3);

    _mm_store_ps(&output[0], out_0);
    _mm_store_ps(&output[4], out_1);
    _mm_store_ps(&output[8], out_2);
    _mm_store_ps(&output[12], out_3);
}

inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16_simd(input.data(), output.data());
}

#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

inline void hadamardWalsh16_simd(const float* input, float* output) noexcept
{
    alignas(16) static constexpr float sign_mask[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    float32x4_t sign = vld1q_f32(sign_mask);

    float32x4_t v0 = vld1q_f32(&input[0]);
    float32x4_t v1 = vld1q_f32(&input[4]);
    float32x4_t v2 = vld1q_f32(&input[8]);
    float32x4_t v3 = vld1q_f32(&input[12]);

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

    // Stage 3
    float32x4_t t3_0 = vaddq_f32(t2_0, t2_1);
    float32x4_t t3_1 = vsubq_f32(t2_0, t2_1);
    float32x4_t t3_2 = vaddq_f32(t2_2, t2_3);
    float32x4_t t3_3 = vsubq_f32(t2_2, t2_3);

    // Stage 4
    float32x4_t out_0 = vaddq_f32(t3_0, t3_2);
    float32x4_t out_1 = vaddq_f32(t3_1, t3_3);
    float32x4_t out_2 = vsubq_f32(t3_0, t3_2);
    float32x4_t out_3 = vsubq_f32(t3_1, t3_3);

    vst1q_f32(&output[0], out_0);
    vst1q_f32(&output[4], out_1);
    vst1q_f32(&output[8], out_2);
    vst1q_f32(&output[12], out_3);
}

inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16_simd(input.data(), output.data());
}

#else
inline void hadamardWalsh16_simd(const std::array<float, 16>& input, std::array<float, 16>& output) noexcept
{
    hadamardWalsh16(input.data(), output.data());
}
#endif

}
