#pragma once

#include <array>

#include "Helpers/PlatformIntrinsics.h"

namespace AbacDsp
{

inline void hadamardFeed16(const float* col, float* sum) noexcept
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

inline void hadamardFeed16(const std::array<float, 16>& col, std::array<float, 16>& sum) noexcept
{
    hadamardFeed16(col.data(), sum.data());
}

#if defined(USE_SIMD_FRAMEWORK)

inline void hadamardFeed16_simd(const float* col, float* sum) noexcept
{
    simd_float4 v0 = simd_make_float4(col[0], col[1], col[2], col[3]);
    simd_float4 v1 = simd_make_float4(col[4], col[5], col[6], col[7]);
    simd_float4 v2 = simd_make_float4(col[8], col[9], col[10], col[11]);
    simd_float4 v3 = simd_make_float4(col[12], col[13], col[14], col[15]);

    auto hsum = [](simd_float4 v) -> float { return simd_reduce_add(v); };

    simd_float4 all_pos = simd_make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    simd_float4 all_neg = simd_make_float4(-1.0f, -1.0f, -1.0f, -1.0f);
    simd_float4 alt_pn = simd_make_float4(1.0f, -1.0f, 1.0f, -1.0f);
    simd_float4 alt_np = simd_make_float4(-1.0f, 1.0f, -1.0f, 1.0f);
    simd_float4 pp_nn = simd_make_float4(1.0f, 1.0f, -1.0f, -1.0f);
    simd_float4 nn_pp = simd_make_float4(-1.0f, -1.0f, 1.0f, 1.0f);
    simd_float4 p_nnp = simd_make_float4(1.0f, -1.0f, -1.0f, 1.0f);
    simd_float4 n_ppn = simd_make_float4(-1.0f, 1.0f, 1.0f, -1.0f);

    sum[0] = hsum(v0 * all_pos + v1 * all_pos + v2 * all_pos + v3 * all_pos);
    sum[1] = hsum(v0 * alt_pn + v1 * alt_pn + v2 * alt_pn + v3 * alt_pn);
    sum[2] = hsum(v0 * pp_nn + v1 * pp_nn + v2 * pp_nn + v3 * pp_nn);
    sum[3] = hsum(v0 * p_nnp + v1 * p_nnp + v2 * p_nnp + v3 * p_nnp);
    sum[4] = hsum(v0 * all_pos + v1 * all_neg + v2 * all_pos + v3 * all_neg);
    sum[5] = hsum(v0 * alt_pn + v1 * alt_np + v2 * alt_pn + v3 * alt_np);
    sum[6] = hsum(v0 * pp_nn + v1 * nn_pp + v2 * pp_nn + v3 * nn_pp);
    sum[7] = hsum(v0 * p_nnp + v1 * n_ppn + v2 * p_nnp + v3 * n_ppn);
    sum[8] = hsum(v0 * all_pos + v1 * all_pos + v2 * all_neg + v3 * all_neg);
    sum[9] = hsum(v0 * alt_pn + v1 * alt_pn + v2 * alt_np + v3 * alt_np);
    sum[10] = hsum(v0 * pp_nn + v1 * pp_nn + v2 * nn_pp + v3 * nn_pp);
    sum[11] = hsum(v0 * p_nnp + v1 * p_nnp + v2 * n_ppn + v3 * n_ppn);
    sum[12] = hsum(v0 * all_pos + v1 * all_neg + v2 * all_neg + v3 * all_pos);
    sum[13] = hsum(v0 * alt_pn + v1 * alt_np + v2 * alt_np + v3 * alt_pn);
    sum[14] = hsum(v0 * pp_nn + v1 * nn_pp + v2 * nn_pp + v3 * pp_nn);
    sum[15] = hsum(v0 * p_nnp + v1 * n_ppn + v2 * n_ppn + v3 * p_nnp);
}

inline void hadamardFeed16_simd(const std::array<float, 16>& col, std::array<float, 16>& sum) noexcept
{
    hadamardFeed16_simd(col.data(), sum.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardFeed16_simd(const float* col, float* sum) noexcept
{
    const __m128 v0 = _mm_loadu_ps(&col[0]);
    const __m128 v1 = _mm_loadu_ps(&col[4]);
    const __m128 v2 = _mm_loadu_ps(&col[8]);
    const __m128 v3 = _mm_loadu_ps(&col[12]);

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

    sum[0] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                             _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_pos))));
    sum[1] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_pn))));
    sum[2] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                             _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, pp_nn))));
    sum[3] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                             _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, p_nnp))));
    sum[4] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                             _mm_add_ps(_mm_mul_ps(v2, all_pos), _mm_mul_ps(v3, all_neg))));
    sum[5] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_pn), _mm_mul_ps(v3, alt_np))));
    sum[6] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                             _mm_add_ps(_mm_mul_ps(v2, pp_nn), _mm_mul_ps(v3, nn_pp))));
    sum[7] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                             _mm_add_ps(_mm_mul_ps(v2, p_nnp), _mm_mul_ps(v3, n_ppn))));
    sum[8] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)),
                             _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_neg))));
    sum[9] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)),
                             _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_np))));
    sum[10] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)),
                              _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, nn_pp))));
    sum[11] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)),
                              _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, n_ppn))));
    sum[12] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)),
                              _mm_add_ps(_mm_mul_ps(v2, all_neg), _mm_mul_ps(v3, all_pos))));
    sum[13] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)),
                              _mm_add_ps(_mm_mul_ps(v2, alt_np), _mm_mul_ps(v3, alt_pn))));
    sum[14] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)),
                              _mm_add_ps(_mm_mul_ps(v2, nn_pp), _mm_mul_ps(v3, pp_nn))));
    sum[15] = hsum(_mm_add_ps(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)),
                              _mm_add_ps(_mm_mul_ps(v2, n_ppn), _mm_mul_ps(v3, p_nnp))));
}

inline void hadamardFeed16_simd(const std::array<float, 16>& col, std::array<float, 16>& sum) noexcept
{
    hadamardFeed16_simd(col.data(), sum.data());
}

#else

inline void hadamardFeed16_simd(const float* col, float* sum) noexcept
{
    hadamardFeed16(col, sum);
}

inline void hadamardFeed16_simd(const std::array<float, 16>& col, std::array<float, 16>& sum) noexcept
{
    hadamardFeed16(col, sum);
}

#endif

}
