#pragma once

#include <array>

namespace AbacDsp
{

inline void hadamardWalsh8(const float* input, float* output) noexcept
{
    std::array<float, 8> temp1, temp2;

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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <immintrin.h>
inline void hadamardWalsh8_simd(const float* input, float* output) noexcept
{
    __m128 v0 = _mm_load_ps(&input[0]);
    __m128 v1 = _mm_load_ps(&input[4]);

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

    _mm_store_ps(&output[0], out_lo);
    _mm_store_ps(&output[4], out_hi);
}


inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8_simd(input.data(), output.data());
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

inline void hadamardWalsh8_simd(const float* input, float* output) noexcept
{
    alignas(16) static constexpr float sign_mask[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    float32x4_t sign = vld1q_f32(sign_mask);

    float32x4_t v0 = vld1q_f32(&input[0]);
    float32x4_t v1 = vld1q_f32(&input[4]);

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

    // Stage 3
    float32x4_t out_lo = vaddq_f32(t2_0, t2_1);
    float32x4_t out_hi = vsubq_f32(t2_0, t2_1);

    vst1q_f32(&output[0], out_lo);
    vst1q_f32(&output[4], out_hi);
}

inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8_simd(input.data(), output.data());
}

#else

inline void hadamardWalsh8_simd(const std::array<float, 8>& input, std::array<float, 8>& output) noexcept
{
    hadamardWalsh8(input.data(), output.data());
}

#endif

} // namespace AbacDsp
