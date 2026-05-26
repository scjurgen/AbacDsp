#pragma once

#include <array>

#include "Helpers/PlatformIntrinsics.h"

namespace AbacDsp
{

inline void hadamardFeed32(const float* col, float* sum) noexcept
{
    sum[0] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] + col[8] + col[9] + col[10] +
             col[11] + col[12] + col[13] + col[14] + col[15] + col[16] + col[17] + col[18] + col[19] + col[20] +
             col[21] + col[22] + col[23] + col[24] + col[25] + col[26] + col[27] + col[28] + col[29] + col[30] +
             col[31];
    sum[1] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] + col[8] - col[9] + col[10] -
             col[11] + col[12] - col[13] + col[14] - col[15] + col[16] - col[17] + col[18] - col[19] + col[20] -
             col[21] + col[22] - col[23] + col[24] - col[25] + col[26] - col[27] + col[28] - col[29] + col[30] -
             col[31];
    sum[2] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] + col[8] + col[9] - col[10] -
             col[11] + col[12] + col[13] - col[14] - col[15] + col[16] + col[17] - col[18] - col[19] + col[20] +
             col[21] - col[22] - col[23] + col[24] + col[25] - col[26] - col[27] + col[28] + col[29] - col[30] -
             col[31];
    sum[3] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] + col[8] - col[9] - col[10] +
             col[11] + col[12] - col[13] - col[14] + col[15] + col[16] - col[17] - col[18] + col[19] + col[20] -
             col[21] - col[22] + col[23] + col[24] - col[25] - col[26] + col[27] + col[28] - col[29] - col[30] +
             col[31];
    sum[4] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] + col[8] + col[9] + col[10] +
             col[11] - col[12] - col[13] - col[14] - col[15] + col[16] + col[17] + col[18] + col[19] - col[20] -
             col[21] - col[22] - col[23] + col[24] + col[25] + col[26] + col[27] - col[28] - col[29] - col[30] -
             col[31];
    sum[5] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] + col[8] - col[9] + col[10] -
             col[11] - col[12] + col[13] - col[14] + col[15] + col[16] - col[17] + col[18] - col[19] - col[20] +
             col[21] - col[22] + col[23] + col[24] - col[25] + col[26] - col[27] - col[28] + col[29] - col[30] +
             col[31];
    sum[6] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] + col[8] + col[9] - col[10] -
             col[11] - col[12] - col[13] + col[14] + col[15] + col[16] + col[17] - col[18] - col[19] - col[20] -
             col[21] + col[22] + col[23] + col[24] + col[25] - col[26] - col[27] - col[28] - col[29] + col[30] +
             col[31];
    sum[7] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] + col[8] - col[9] - col[10] +
             col[11] - col[12] + col[13] + col[14] - col[15] + col[16] - col[17] - col[18] + col[19] - col[20] +
             col[21] + col[22] - col[23] + col[24] - col[25] - col[26] + col[27] - col[28] + col[29] + col[30] -
             col[31];
    sum[8] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] - col[8] - col[9] - col[10] -
             col[11] - col[12] - col[13] - col[14] - col[15] + col[16] + col[17] + col[18] + col[19] + col[20] +
             col[21] + col[22] + col[23] - col[24] - col[25] - col[26] - col[27] - col[28] - col[29] - col[30] -
             col[31];
    sum[9] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] - col[8] + col[9] - col[10] +
             col[11] - col[12] + col[13] - col[14] + col[15] + col[16] - col[17] + col[18] - col[19] + col[20] -
             col[21] + col[22] - col[23] - col[24] + col[25] - col[26] + col[27] - col[28] + col[29] - col[30] +
             col[31];
    sum[10] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] - col[8] - col[9] + col[10] +
              col[11] - col[12] - col[13] + col[14] + col[15] + col[16] + col[17] - col[18] - col[19] + col[20] +
              col[21] - col[22] - col[23] - col[24] - col[25] + col[26] + col[27] - col[28] - col[29] + col[30] +
              col[31];
    sum[11] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] - col[8] + col[9] + col[10] -
              col[11] - col[12] + col[13] + col[14] - col[15] + col[16] - col[17] - col[18] + col[19] + col[20] -
              col[21] - col[22] + col[23] - col[24] + col[25] + col[26] - col[27] - col[28] + col[29] + col[30] -
              col[31];
    sum[12] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] - col[8] - col[9] - col[10] -
              col[11] + col[12] + col[13] + col[14] + col[15] + col[16] + col[17] + col[18] + col[19] - col[20] -
              col[21] - col[22] - col[23] - col[24] - col[25] - col[26] - col[27] + col[28] + col[29] + col[30] +
              col[31];
    sum[13] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] - col[8] + col[9] - col[10] +
              col[11] + col[12] - col[13] + col[14] - col[15] + col[16] - col[17] + col[18] - col[19] - col[20] +
              col[21] - col[22] + col[23] - col[24] + col[25] - col[26] + col[27] + col[28] - col[29] + col[30] -
              col[31];
    sum[14] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] - col[8] - col[9] + col[10] +
              col[11] + col[12] + col[13] - col[14] - col[15] + col[16] + col[17] - col[18] - col[19] - col[20] -
              col[21] + col[22] + col[23] - col[24] - col[25] + col[26] + col[27] + col[28] + col[29] - col[30] -
              col[31];
    sum[15] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] - col[8] + col[9] + col[10] -
              col[11] + col[12] - col[13] - col[14] + col[15] + col[16] - col[17] - col[18] + col[19] - col[20] +
              col[21] + col[22] - col[23] - col[24] + col[25] + col[26] - col[27] + col[28] - col[29] - col[30] +
              col[31];
    sum[16] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] + col[8] + col[9] + col[10] +
              col[11] + col[12] + col[13] + col[14] + col[15] - col[16] - col[17] - col[18] - col[19] - col[20] -
              col[21] - col[22] - col[23] - col[24] - col[25] - col[26] - col[27] - col[28] - col[29] - col[30] -
              col[31];
    sum[17] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] + col[8] - col[9] + col[10] -
              col[11] + col[12] - col[13] + col[14] - col[15] - col[16] + col[17] - col[18] + col[19] - col[20] +
              col[21] - col[22] + col[23] - col[24] + col[25] - col[26] + col[27] - col[28] + col[29] - col[30] +
              col[31];
    sum[18] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] + col[8] + col[9] - col[10] -
              col[11] + col[12] + col[13] - col[14] - col[15] - col[16] - col[17] + col[18] + col[19] - col[20] -
              col[21] + col[22] + col[23] - col[24] - col[25] + col[26] + col[27] - col[28] - col[29] + col[30] +
              col[31];
    sum[19] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] + col[8] - col[9] - col[10] +
              col[11] + col[12] - col[13] - col[14] + col[15] - col[16] + col[17] + col[18] - col[19] - col[20] +
              col[21] + col[22] - col[23] - col[24] + col[25] + col[26] - col[27] - col[28] + col[29] + col[30] -
              col[31];
    sum[20] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] + col[8] + col[9] + col[10] +
              col[11] - col[12] - col[13] - col[14] - col[15] - col[16] - col[17] - col[18] - col[19] + col[20] +
              col[21] + col[22] + col[23] - col[24] - col[25] - col[26] - col[27] + col[28] + col[29] + col[30] +
              col[31];
    sum[21] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] + col[8] - col[9] + col[10] -
              col[11] - col[12] + col[13] - col[14] + col[15] - col[16] + col[17] - col[18] + col[19] + col[20] -
              col[21] + col[22] - col[23] - col[24] + col[25] - col[26] + col[27] + col[28] - col[29] + col[30] -
              col[31];
    sum[22] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] + col[8] + col[9] - col[10] -
              col[11] - col[12] - col[13] + col[14] + col[15] - col[16] - col[17] + col[18] + col[19] + col[20] +
              col[21] - col[22] - col[23] - col[24] - col[25] + col[26] + col[27] + col[28] + col[29] - col[30] -
              col[31];
    sum[23] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] + col[8] - col[9] - col[10] +
              col[11] - col[12] + col[13] + col[14] - col[15] - col[16] + col[17] + col[18] - col[19] + col[20] -
              col[21] - col[22] + col[23] - col[24] + col[25] + col[26] - col[27] + col[28] - col[29] - col[30] +
              col[31];
    sum[24] = col[0] + col[1] + col[2] + col[3] + col[4] + col[5] + col[6] + col[7] - col[8] - col[9] - col[10] -
              col[11] - col[12] - col[13] - col[14] - col[15] - col[16] - col[17] - col[18] - col[19] - col[20] -
              col[21] - col[22] - col[23] + col[24] + col[25] + col[26] + col[27] + col[28] + col[29] + col[30] +
              col[31];
    sum[25] = col[0] - col[1] + col[2] - col[3] + col[4] - col[5] + col[6] - col[7] - col[8] + col[9] - col[10] +
              col[11] - col[12] + col[13] - col[14] + col[15] - col[16] + col[17] - col[18] + col[19] - col[20] +
              col[21] - col[22] + col[23] + col[24] - col[25] + col[26] - col[27] + col[28] - col[29] + col[30] -
              col[31];
    sum[26] = col[0] + col[1] - col[2] - col[3] + col[4] + col[5] - col[6] - col[7] - col[8] - col[9] + col[10] +
              col[11] - col[12] - col[13] + col[14] + col[15] - col[16] - col[17] + col[18] + col[19] - col[20] -
              col[21] + col[22] + col[23] + col[24] + col[25] - col[26] - col[27] + col[28] + col[29] - col[30] -
              col[31];
    sum[27] = col[0] - col[1] - col[2] + col[3] + col[4] - col[5] - col[6] + col[7] - col[8] + col[9] + col[10] -
              col[11] - col[12] + col[13] + col[14] - col[15] - col[16] + col[17] + col[18] - col[19] - col[20] +
              col[21] + col[22] - col[23] + col[24] - col[25] - col[26] + col[27] + col[28] - col[29] - col[30] +
              col[31];
    sum[28] = col[0] + col[1] + col[2] + col[3] - col[4] - col[5] - col[6] - col[7] - col[8] - col[9] - col[10] -
              col[11] + col[12] + col[13] + col[14] + col[15] - col[16] - col[17] - col[18] - col[19] + col[20] +
              col[21] + col[22] + col[23] + col[24] + col[25] + col[26] + col[27] - col[28] - col[29] - col[30] -
              col[31];
    sum[29] = col[0] - col[1] + col[2] - col[3] - col[4] + col[5] - col[6] + col[7] - col[8] + col[9] - col[10] +
              col[11] + col[12] - col[13] + col[14] - col[15] - col[16] + col[17] - col[18] + col[19] + col[20] -
              col[21] + col[22] - col[23] + col[24] - col[25] + col[26] - col[27] - col[28] + col[29] - col[30] +
              col[31];
    sum[30] = col[0] + col[1] - col[2] - col[3] - col[4] - col[5] + col[6] + col[7] - col[8] - col[9] + col[10] +
              col[11] + col[12] + col[13] - col[14] - col[15] - col[16] - col[17] + col[18] + col[19] + col[20] +
              col[21] - col[22] - col[23] + col[24] + col[25] - col[26] - col[27] - col[28] - col[29] + col[30] +
              col[31];
    sum[31] = col[0] - col[1] - col[2] + col[3] - col[4] + col[5] + col[6] - col[7] - col[8] + col[9] + col[10] -
              col[11] + col[12] - col[13] - col[14] + col[15] - col[16] + col[17] + col[18] - col[19] + col[20] -
              col[21] - col[22] + col[23] + col[24] - col[25] - col[26] + col[27] - col[28] + col[29] + col[30] -
              col[31];
}

inline void hadamardFeed32(const std::array<float, 32>& col, std::array<float, 32>& sum) noexcept
{
    hadamardFeed32(col.data(), sum.data());
}

#if defined(USE_X86_INTRINSICS)

inline void hadamardFeed32_simd(const float* col, float* sum) noexcept
{
    const __m128 v0 = _mm_loadu_ps(&col[0]);
    const __m128 v1 = _mm_loadu_ps(&col[4]);
    const __m128 v2 = _mm_loadu_ps(&col[8]);
    const __m128 v3 = _mm_loadu_ps(&col[12]);
    const __m128 v4 = _mm_loadu_ps(&col[16]);
    const __m128 v5 = _mm_loadu_ps(&col[20]);
    const __m128 v6 = _mm_loadu_ps(&col[24]);
    const __m128 v7 = _mm_loadu_ps(&col[28]);

    const __m128 all_pos = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
    const __m128 all_neg = _mm_set_ps(-1.0f, -1.0f, -1.0f, -1.0f);
    const __m128 alt_pn = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);
    const __m128 alt_np = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f);
    const __m128 pp_nn = _mm_set_ps(-1.0f, -1.0f, 1.0f, 1.0f);
    const __m128 nn_pp = _mm_set_ps(1.0f, 1.0f, -1.0f, -1.0f);
    const __m128 p_nnp = _mm_set_ps(1.0f, -1.0f, -1.0f, 1.0f);
    const __m128 n_ppn = _mm_set_ps(-1.0f, 1.0f, 1.0f, -1.0f);

    auto hsum = [](__m128 v) -> float
    {
        const __m128 shuf = _mm_movehdup_ps(v);
        const __m128 sums = _mm_add_ps(v, shuf);
        const __m128 shuf2 = _mm_movehl_ps(sums, sums);
        const __m128 result = _mm_add_ss(sums, shuf2);
        return _mm_cvtss_f32(result);
    };

    sum[0] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                                        _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_pos))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_pos), _mm_mul_ps(v5, all_pos)),
                                        _mm_add_ps(_mm_mul_ps(v6, all_pos), _mm_mul_ps(v7, all_pos)))));
    sum[1] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                                        _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_pn))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_pn), _mm_mul_ps(v5, alt_pn)),
                                        _mm_add_ps(_mm_mul_ps(v6, alt_pn), _mm_mul_ps(v7, alt_pn)))));
    sum[2] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                                        _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, pp_nn))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, pp_nn), _mm_mul_ps(v5, pp_nn)),
                                        _mm_add_ps(_mm_mul_ps(v6, pp_nn), _mm_mul_ps(v7, pp_nn)))));
    sum[3] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                                        _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, p_nnp))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, p_nnp), _mm_mul_ps(v5, p_nnp)),
                                        _mm_add_ps(_mm_mul_ps(v6, p_nnp), _mm_mul_ps(v7, p_nnp)))));
    sum[4] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                                        _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_neg))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_pos), _mm_mul_ps(v5, all_neg)),
                                        _mm_add_ps(_mm_mul_ps(v6, all_pos), _mm_mul_ps(v7, all_neg)))));
    sum[5] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                                        _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_np))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_pn), _mm_mul_ps(v5, alt_np)),
                                        _mm_add_ps(_mm_mul_ps(v6, alt_pn), _mm_mul_ps(v7, alt_np)))));
    sum[6] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                                        _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, nn_pp))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, pp_nn), _mm_mul_ps(v5, nn_pp)),
                                        _mm_add_ps(_mm_mul_ps(v6, pp_nn), _mm_mul_ps(v7, nn_pp)))));
    sum[7] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                                        _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, n_ppn))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, p_nnp), _mm_mul_ps(v5, n_ppn)),
                                        _mm_add_ps(_mm_mul_ps(v6, p_nnp), _mm_mul_ps(v7, n_ppn)))));
    sum[8] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                                        _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_neg))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_pos), _mm_mul_ps(v5, all_pos)),
                                        _mm_add_ps(_mm_mul_ps(v6, all_neg), _mm_mul_ps(v7, all_neg)))));
    sum[9] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                                        _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_np))),
                             _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_pn), _mm_mul_ps(v5, alt_pn)),
                                        _mm_add_ps(_mm_mul_ps(v6, alt_np), _mm_mul_ps(v7, alt_np)))));
    sum[10] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, nn_pp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, pp_nn), _mm_mul_ps(v5, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v6, nn_pp), _mm_mul_ps(v7, nn_pp)))));
    sum[11] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, n_ppn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, p_nnp), _mm_mul_ps(v5, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v6, n_ppn), _mm_mul_ps(v7, n_ppn)))));
    sum[12] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_pos))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_pos), _mm_mul_ps(v5, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v6, all_neg), _mm_mul_ps(v7, all_pos)))));
    sum[13] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_pn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_pn), _mm_mul_ps(v5, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v6, alt_np), _mm_mul_ps(v7, alt_pn)))));
    sum[14] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, pp_nn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, pp_nn), _mm_mul_ps(v5, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v6, nn_pp), _mm_mul_ps(v7, pp_nn)))));
    sum[15] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, p_nnp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, p_nnp), _mm_mul_ps(v5, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v6, n_ppn), _mm_mul_ps(v7, p_nnp)))));
    sum[16] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                                         _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_pos))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_neg), _mm_mul_ps(v5, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v6, all_neg), _mm_mul_ps(v7, all_neg)))));
    sum[17] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                                         _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_pn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_np), _mm_mul_ps(v5, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v6, alt_np), _mm_mul_ps(v7, alt_np)))));
    sum[18] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, pp_nn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, nn_pp), _mm_mul_ps(v5, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v6, nn_pp), _mm_mul_ps(v7, nn_pp)))));
    sum[19] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, p_nnp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, n_ppn), _mm_mul_ps(v5, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v6, n_ppn), _mm_mul_ps(v7, n_ppn)))));
    sum[20] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_neg))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_neg), _mm_mul_ps(v5, all_pos)),
                                         _mm_add_ps(_mm_mul_ps(v6, all_neg), _mm_mul_ps(v7, all_pos)))));
    sum[21] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_np))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_np), _mm_mul_ps(v5, alt_pn)),
                                         _mm_add_ps(_mm_mul_ps(v6, alt_np), _mm_mul_ps(v7, alt_pn)))));
    sum[22] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, nn_pp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, nn_pp), _mm_mul_ps(v5, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v6, nn_pp), _mm_mul_ps(v7, pp_nn)))));
    sum[23] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, n_ppn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, n_ppn), _mm_mul_ps(v5, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v6, n_ppn), _mm_mul_ps(v7, p_nnp)))));
    sum[24] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                                         _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_neg))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_neg), _mm_mul_ps(v5, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v6, all_pos), _mm_mul_ps(v7, all_pos)))));
    sum[25] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                                         _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_np))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_np), _mm_mul_ps(v5, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v6, alt_pn), _mm_mul_ps(v7, alt_pn)))));
    sum[26] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, nn_pp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, nn_pp), _mm_mul_ps(v5, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v6, pp_nn), _mm_mul_ps(v7, pp_nn)))));
    sum[27] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, n_ppn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, n_ppn), _mm_mul_ps(v5, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v6, p_nnp), _mm_mul_ps(v7, p_nnp)))));
    sum[28] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                                         _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_pos))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, all_neg), _mm_mul_ps(v5, all_pos)),
                                         _mm_add_ps(_mm_mul_ps(v6, all_pos), _mm_mul_ps(v7, all_neg)))));
    sum[29] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                                         _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_pn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, alt_np), _mm_mul_ps(v5, alt_pn)),
                                         _mm_add_ps(_mm_mul_ps(v6, alt_pn), _mm_mul_ps(v7, alt_np)))));
    sum[30] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                                         _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, pp_nn))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, nn_pp), _mm_mul_ps(v5, pp_nn)),
                                         _mm_add_ps(_mm_mul_ps(v6, pp_nn), _mm_mul_ps(v7, nn_pp)))));
    sum[31] = hsum(_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                                         _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, p_nnp))),
                              _mm_add_ps(_mm_add_ps(_mm_mul_ps(v4, n_ppn), _mm_mul_ps(v5, p_nnp)),
                                         _mm_add_ps(_mm_mul_ps(v6, p_nnp), _mm_mul_ps(v7, n_ppn)))));
}

inline void hadamardFeed32_simd(const std::array<float, 32>& col, std::array<float, 32>& sum) noexcept
{
    hadamardFeed32_simd(col.data(), sum.data());
}

#else

inline void hadamardFeed32_simd(const float* col, float* sum) noexcept
{
    hadamardFeed32(col, sum);
}

inline void hadamardFeed32_simd(const std::array<float, 32>& col, std::array<float, 32>& sum) noexcept
{
    hadamardFeed32(col, sum);
}

#endif

}
