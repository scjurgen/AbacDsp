#pragma once

#include <array>

namespace AbacDsp
{

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <immintrin.h>

inline void hadamardFeed16_simd(const float* col, float* sum) noexcept
{
    // Load input data
    const __m128 v0 = _mm_load_ps(&col[0]);  // [0,1,2,3]
    const __m128 v1 = _mm_load_ps(&col[4]);  // [4,5,6,7]
    const __m128 v2 = _mm_load_ps(&col[8]);  // [8,9,10,11]
    const __m128 v3 = _mm_load_ps(&col[12]); // [12,13,14,15]

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
        const __m128 shuf = _mm_movehdup_ps(v);         // duplicate elements 1,3 to 0,2
        const __m128 sums = _mm_add_ps(v, shuf);        // add pairs
        const __m128 shuf2 = _mm_movehl_ps(sums, sums); // move elements 2,3 to 0,1
        const __m128 result = _mm_add_ss(sums, shuf2);  // add remaining pair
        return _mm_cvtss_f32(result);
    };

    // Row 0: all_pos all_pos all_pos all_pos
    sum[0] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                             _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_pos))));

    // Row 1: alt_pn alt_pn alt_pn alt_pn
    sum[1] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_pn))));

    // Row 2: pp_nn pp_nn pp_nn pp_nn
    sum[2] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                             _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, pp_nn))));

    // Row 3: p_nnp p_nnp p_nnp p_nnp
    sum[3] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                             _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, p_nnp))));

    // Row 4: all_pos all_neg all_pos all_neg
    sum[4] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                             _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_neg))));

    // Row 5: alt_pn alt_np alt_pn alt_np
    sum[5] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_np))));

    // Row 6: pp_nn nn_pp pp_nn nn_pp
    sum[6] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                             _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, nn_pp))));

    // Row 7: p_nnp n_ppn p_nnp n_ppn
    sum[7] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                             _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, n_ppn))));

    // Row 8: all_pos all_pos all_neg all_neg
    sum[8] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                             _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_neg))));

    // Row 9: alt_pn alt_pn alt_np alt_np
    sum[9] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_np))));

    // Row 10: pp_nn pp_nn nn_pp nn_pp
    sum[10] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                              _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, nn_pp))));

    // Row 11: p_nnp p_nnp n_ppn n_ppn
    sum[11] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                              _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, n_ppn))));

    // Row 12: all_pos all_neg all_neg all_pos
    sum[12] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                              _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_pos))));

    // Row 13: alt_pn alt_np alt_np alt_pn
    sum[13] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                              _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_pn))));

    // Row 14: pp_nn nn_pp nn_pp pp_nn
    sum[14] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                              _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, pp_nn))));

    // Row 15: p_nnp n_ppn n_ppn p_nnp
    sum[15] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                              _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, p_nnp))));
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

inline void hadamardFeed16_simd(const float* col, float* sum) noexcept
{
    // Load input data
    const float32x4_t v0 = vld1q_f32(&col[0]);  // [0,1,2,3]
    const float32x4_t v1 = vld1q_f32(&col[4]);  // [4,5,6,7]
    const float32x4_t v2 = vld1q_f32(&col[8]);  // [8,9,10,11]
    const float32x4_t v3 = vld1q_f32(&col[12]); // [12,13,14,15]

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

    // Row 0: all_pos all_pos all_pos all_pos
    sum[0] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_pos)),
                            vaddq_f32(vmulq_f32(v2, all_pos), vmulq_f32(v3, all_pos))));

    // Row 1: alt_pn alt_pn alt_pn alt_pn
    sum[1] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_pn)),
                            vaddq_f32(vmulq_f32(v2, alt_pn), vmulq_f32(v3, alt_pn))));

    // Row 2: pp_nn pp_nn pp_nn pp_nn
    sum[2] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, pp_nn)),
                            vaddq_f32(vmulq_f32(v2, pp_nn), vmulq_f32(v3, pp_nn))));

    // Row 3: p_nnp p_nnp p_nnp p_nnp
    sum[3] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, p_nnp)),
                            vaddq_f32(vmulq_f32(v2, p_nnp), vmulq_f32(v3, p_nnp))));

    // Row 4: all_pos all_neg all_pos all_neg
    sum[4] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_neg)),
                            vaddq_f32(vmulq_f32(v2, all_pos), vmulq_f32(v3, all_neg))));

    // Row 5: alt_pn alt_np alt_pn alt_np
    sum[5] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_np)),
                            vaddq_f32(vmulq_f32(v2, alt_pn), vmulq_f32(v3, alt_np))));

    // Row 6: pp_nn nn_pp pp_nn nn_pp
    sum[6] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, nn_pp)),
                            vaddq_f32(vmulq_f32(v2, pp_nn), vmulq_f32(v3, nn_pp))));

    // Row 7: p_nnp n_ppn p_nnp n_ppn
    sum[7] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, n_ppn)),
                            vaddq_f32(vmulq_f32(v2, p_nnp), vmulq_f32(v3, n_ppn))));

    // Row 8: all_pos all_pos all_neg all_neg
    sum[8] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_pos)),
                            vaddq_f32(vmulq_f32(v2, all_neg), vmulq_f32(v3, all_neg))));

    // Row 9: alt_pn alt_pn alt_np alt_np
    sum[9] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_pn)),
                            vaddq_f32(vmulq_f32(v2, alt_np), vmulq_f32(v3, alt_np))));

    // Row 10: pp_nn pp_nn nn_pp nn_pp
    sum[10] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, pp_nn)),
                             vaddq_f32(vmulq_f32(v2, nn_pp), vmulq_f32(v3, nn_pp))));

    // Row 11: p_nnp p_nnp n_ppn n_ppn
    sum[11] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, p_nnp)),
                             vaddq_f32(vmulq_f32(v2, n_ppn), vmulq_f32(v3, n_ppn))));

    // Row 12: all_pos all_neg all_neg all_pos
    sum[12] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, all_pos), vmulq_f32(v1, all_neg)),
                             vaddq_f32(vmulq_f32(v2, all_neg), vmulq_f32(v3, all_pos))));

    // Row 13: alt_pn alt_np alt_np alt_pn
    sum[13] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, alt_pn), vmulq_f32(v1, alt_np)),
                             vaddq_f32(vmulq_f32(v2, alt_np), vmulq_f32(v3, alt_pn))));

    // Row 14: pp_nn nn_pp nn_pp pp_nn
    sum[14] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, pp_nn), vmulq_f32(v1, nn_pp)),
                             vaddq_f32(vmulq_f32(v2, nn_pp), vmulq_f32(v3, pp_nn))));

    // Row 15: p_nnp n_ppn n_ppn p_nnp
    sum[15] = hsum(vaddq_f32(vaddq_f32(vmulq_f32(v0, p_nnp), vmulq_f32(v1, n_ppn)),
                             vaddq_f32(vmulq_f32(v2, n_ppn), vmulq_f32(v3, p_nnp))));
}

#endif

inline void hadamardFeed16(const std::array<float, 16>& col, std::array<float, 16>& sum) noexcept
{
    sum[0] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] + col[8] + col[9] + col[10] +
             col[11] + col[12] + col[13] + col[14] + col[15];

    sum[1] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] + col[8] - col[9] + col[10] -
             col[11] + col[12] - col[13] + col[14] - col[15];

    sum[2] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] + col[8] + col[9] - col[10] -
             col[11] + col[12] + col[13] - col[14] - col[15];

    sum[3] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] + col[8] - col[9] - col[10] +
             col[11] + col[12] - col[13] - col[14] + col[15];

    sum[4] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] + col[8] + col[9] + col[10] +
             col[11] - col[12] - col[13] - col[14] - col[15];

    sum[5] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] + col[8] - col[9] + col[10] -
             col[11] - col[12] + col[13] - col[14] + col[15];

    sum[6] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] + col[8] + col[9] - col[10] -
             col[11] - col[12] - col[13] + col[14] + col[15];

    sum[7] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] + col[8] - col[9] - col[10] +
             col[11] - col[12] + col[13] + col[14] - col[15];

    sum[8] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] - col[8] - col[9] - col[10] -
             col[11] - col[12] - col[13] - col[14] - col[15];

    sum[9] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] - col[8] + col[9] - col[10] +
             col[11] - col[12] + col[13] - col[14] + col[15];

    sum[10] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] - col[8] - col[9] + col[10] +
              col[11] - col[12] - col[13] + col[14] + col[15];

    sum[11] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] - col[8] + col[9] + col[10] -
              col[11] - col[12] + col[13] + col[14] - col[15];

    sum[12] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] - col[8] - col[9] - col[10] -
              col[11] + col[12] + col[13] + col[14] + col[15];

    sum[13] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] - col[8] + col[9] - col[10] +
              col[11] + col[12] - col[13] + col[14] - col[15];

    sum[14] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] - col[8] - col[9] + col[10] +
              col[11] + col[12] + col[13] - col[14] - col[15];

    sum[15] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] - col[8] + col[9] + col[10] -
              col[11] + col[12] - col[13] - col[14] + col[15];
}

}
