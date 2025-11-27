#pragma once

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

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>

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

#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

inline void hadamardWalsh4_simd(const float* input, float* output) noexcept
{
    alignas(16) static constexpr float sign_mask[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    float32x4_t sign = vld1q_f32(sign_mask);

    float32x4_t v0 = vld1q_f32(&input[0]);

    // Stage 1
    float32x2_t v0_lo = vget_low_f32(v0);
    float32x2_t v0_hi = vget_high_f32(v0);
    float32x4_t s1_lo_0 = vcombine_f32(vdup_lane_f32(v0_lo, 0), vdup_lane_f32(v0_hi, 0));
    float32x4_t s1_hi_0 = vcombine_f32(vdup_lane_f32(v0_lo, 1), vdup_lane_f32(v0_hi, 1));
    float32x4_t t1_0 = vaddq_f32(s1_lo_0, vmulq_f32(s1_hi_0, sign));

    // Stage 2
    float32x2_t t1_0_lo = vget_low_f32(t1_0);
    float32x2_t t1_0_hi = vget_high_f32(t1_0);
    float32x4_t s2_lo_0 = vcombine_f32(vdup_lane_f32(t1_0_lo, 0), vdup_lane_f32(t1_0_lo, 1));
    float32x4_t s2_hi_0 = vcombine_f32(vdup_lane_f32(t1_0_hi, 0), vdup_lane_f32(t1_0_hi, 1));
    float32x4_t add_0 = vaddq_f32(s2_lo_0, s2_hi_0);
    float32x4_t sub_0 = vsubq_f32(s2_lo_0, s2_hi_0);
    float32x4_t out_0 = vuzpq_f32(add_0, sub_0).val[0];

    vst1q_f32(&output[0], out_0);
}

inline void hadamardWalsh4_simd(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4_simd(input.data(), output.data());
}

#else
inline void hadamardWalsh4_simd(const std::array<float, 4>& input, std::array<float, 4>& output) noexcept
{
    hadamardWalsh4(input.data(), output.data());
}
#endif

} // namespace AbacDsp
