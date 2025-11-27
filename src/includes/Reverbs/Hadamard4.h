#pragma once

#include <array>

namespace AbacDsp
{

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <immintrin.h>

inline void hadamardFeed4_simd(const float* col, float* sum) noexcept
{
    const __m128 v0 = _mm_load_ps(&col[0]);

    const __m128 all_pos = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
    const __m128 alt_pn = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);
    const __m128 pp_nn = _mm_set_ps(-1.0f, -1.0f, 1.0f, 1.0f);
    const __m128 p_nnp = _mm_set_ps(1.0f, -1.0f, -1.0f, 1.0f);

    auto hsum = [](__m128 v) -> float
    {
        const __m128 shuf = _mm_movehdup_ps(v);
        const __m128 sums = _mm_add_ps(v, shuf);
        const __m128 shuf2 = _mm_movehl_ps(sums, sums);
        const __m128 result = _mm_add_ss(sums, shuf2);
        return _mm_cvtss_f32(result);
    };

    sum[0] = hsum(_mm_mul_ps(v0, all_pos));
    sum[1] = hsum(_mm_mul_ps(v0, alt_pn));
    sum[2] = hsum(_mm_mul_ps(v0, pp_nn));
    sum[3] = hsum(_mm_mul_ps(v0, p_nnp));
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

inline void hadamardFeed4_simd(const float* col, float* sum) noexcept
{
    const float32x4_t v0 = vld1q_f32(&col[0]);

    const float32x4_t all_pos = vdupq_n_f32(1.0f);
    const float alt_pn_data[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    const float pp_nn_data[4] = {1.0f, 1.0f, -1.0f, -1.0f};
    const float p_nnp_data[4] = {1.0f, -1.0f, -1.0f, 1.0f};

    const float32x4_t alt_pn = vld1q_f32(alt_pn_data);
    const float32x4_t pp_nn = vld1q_f32(pp_nn_data);
    const float32x4_t p_nnp = vld1q_f32(p_nnp_data);

    auto hsum = [](float32x4_t v) -> float
    {
        float32x2_t sum = vadd_f32(vget_low_f32(v), vget_high_f32(v));
        return vget_lane_f32(vpadd_f32(sum, sum), 0);
    };

    sum[0] = hsum(vmulq_f32(v0, all_pos));
    sum[1] = hsum(vmulq_f32(v0, alt_pn));
    sum[2] = hsum(vmulq_f32(v0, pp_nn));
    sum[3] = hsum(vmulq_f32(v0, p_nnp));
}

#endif

inline void hadamardFeed4(const std::array<float, 4>& col, std::array<float, 4>& sum) noexcept
{
    sum[0] = col[0] + col[1] + col[2] + col[3];
    sum[1] = col[0] - col[1] + col[2] - col[3];
    sum[2] = col[0] + col[1] - col[2] - col[3];
    sum[3] = col[0] - col[1] - col[2] + col[3];
}

}
