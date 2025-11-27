#pragma once

#include <array>

namespace AbacDsp
{
inline void hadamardFeed8(const float* col, float* sum) noexcept

{
    sum[0] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7];
    sum[1] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7];
    sum[2] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7];
    sum[3] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7];
    sum[4] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7];
    sum[5] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7];
    sum[6] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7];
    sum[7] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7];
}
inline void hadamardFeed8(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8(col.data(), sum.data());
}
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <immintrin.h>

inline void hadamardFeed8_simd(const float* col, float* sum) noexcept
{
    // Load input data
    const __m128 v0 = _mm_load_ps(&col[0]); // [0,1,2,3]
    const __m128 v1 = _mm_load_ps(&col[4]); // [4,5,6,7]

    // Define coefficient patterns (note: reversed order for _mm_set_ps)
    const __m128 all_pos = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
    const __m128 all_neg = _mm_set_ps(-1.0f, -1.0f, -1.0f, -1.0f);
    const __m128 alt_pn = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f); // [+1,-1,+1,-1]
    const __m128 alt_np = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f); // [-1,+1,-1,+1]
    const __m128 pp_nn = _mm_set_ps(-1.0f, -1.0f, 1.0f, 1.0f);  // [+1,+1,-1,-1]
    const __m128 nn_pp = _mm_set_ps(1.0f, 1.0f, -1.0f, -1.0f);  // [-1,-1,+1,+1]
    const __m128 p_nnp = _mm_set_ps(1.0f, -1.0f, -1.0f, 1.0f);  // [+1,-1,-1,+1]
    const __m128 n_ppn = _mm_set_ps(-1.0f, 1.0f, 1.0f, -1.0f);  // [-1,+1,+1,-1]

    // Helper lambda for horizontal sum
    auto hsum = [](__m128 v) -> float
    {
        const __m128 shuf = _mm_movehdup_ps(v);
        const __m128 sums = _mm_add_ps(v, shuf);
        const __m128 shuf2 = _mm_movehl_ps(sums, sums);
        const __m128 result = _mm_add_ss(sums, shuf2);
        return _mm_cvtss_f32(result);
    };

    // Row 0: all_pos all_pos
    sum[0] = hsum(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)));

    // Row 1: alt_pn alt_pn
    sum[1] = hsum(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)));

    // Row 2: pp_nn pp_nn
    sum[2] = hsum(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)));

    // Row 3: p_nnp p_nnp
    sum[3] = hsum(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)));

    // Row 4: all_pos all_neg
    sum[4] = hsum(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)));

    // Row 5: alt_pn alt_np
    sum[5] = hsum(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)));

    // Row 6: pp_nn nn_pp
    sum[6] = hsum(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)));

    // Row 7: p_nnp n_ppn
    sum[7] = hsum(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)));
}
inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8_simd(col.data(), sum.data());
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

inline void hadamardFeed8_simd(const float* col, float* sum) noexcept
{
    // Load input data
    const float32x4_t v0 = vld1q_f32(&col[0]); // [0,1,2,3]
    const float32x4_t v1 = vld1q_f32(&col[4]); // [4,5,6,7]

    // Define coefficient patterns
    const float32x4_t all_pos = vdupq_n_f32(1.0f);
    const float32x4_t all_neg = vdupq_n_f32(-1.0f);
    const float alt_pn_data[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    const float alt_np_data[4] = {-1.0f, 1.0f, -1.0f, 1.0f};
    const float pp_nn_data[4] = {1.0f, 1.0f, -1.0f, -1.0f};
    const float nn_pp_data[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    const float p_nnp_data[4] = {1.0f, -1.0f, -1.0f, 1.0f};
    const float n_ppn_data[4] = {-1.0f, 1.0f, 1.0f, -1.0f};

    const float32x4_t alt_pn = vld1q_f32(alt_pn_data);
    const float32x4_t alt_np = vld1q_f32(alt_np_data);
    const float32x4_t pp_nn = vld1q_f32(pp_nn_data);
    const float32x4_t nn_pp = vld1q_f32(nn_pp_data);
    const float32x4_t p_nnp = vld1q_f32(p_nnp_data);
    const float32x4_t n_ppn = vld1q_f32(n_ppn_data);

    // Helper lambda for horizontal sum
    auto hsum = [](float32x4_t v) -> float
    {
        float32x2_t sum = vadd_f32(vget_low_f32(v), vget_high_f32(v));
        return vget_lane_f32(vpadd_f32(sum, sum), 0);
    };

    // Row 0: all_pos all_pos
    sum[0] = hsum(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_pos)));

    // Row 1: alt_pn alt_pn
    sum[1] = hsum(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_pn)));

    // Row 2: pp_nn pp_nn
    sum[2] = hsum(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, pp_nn)));

    // Row 3: p_nnp p_nnp
    sum[3] = hsum(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, p_nnp)));

    // Row 4: all_pos all_neg
    sum[4] = hsum(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_neg)));

    // Row 5: alt_pn alt_np
    sum[5] = hsum(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_np)));

    // Row 6: pp_nn nn_pp
    sum[6] = hsum(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, nn_pp)));

    // Row 7: p_nnp n_ppn
    sum[7] = hsum(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, n_ppn)));
}
inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8_simd(col.data(), sum.data());
}
#else
inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8(col.data(), sum.data());
}
#endif

}
