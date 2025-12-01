#pragma once

#include "Helpers/PlatformIntrinsics.h"
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

#if defined(USE_SIMD_FRAMEWORK)

inline void hadamardFeed8_simd(const float* col, float* sum) noexcept
{
    const simd_float4 v0 = simd_make_float4(col[0], col[1], col[2], col[3]);
    const simd_float4 v1 = simd_make_float4(col[4], col[5], col[6], col[7]);

    const simd_float4 all_pos = simd_make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    const simd_float4 all_neg = simd_make_float4(-1.0f, -1.0f, -1.0f, -1.0f);
    const simd_float4 alt_pn = simd_make_float4(1.0f, -1.0f, 1.0f, -1.0f);
    const simd_float4 alt_np = simd_make_float4(-1.0f, 1.0f, -1.0f, 1.0f);
    const simd_float4 pp_nn = simd_make_float4(1.0f, 1.0f, -1.0f, -1.0f);
    const simd_float4 nn_pp = simd_make_float4(-1.0f, -1.0f, 1.0f, 1.0f);
    const simd_float4 p_nnp = simd_make_float4(1.0f, -1.0f, -1.0f, 1.0f);
    const simd_float4 n_ppn = simd_make_float4(-1.0f, 1.0f, 1.0f, -1.0f);

    auto hsum = [](simd_float4 v) -> float { return simd_reduce_add(v); };

    sum[0] = hsum(v0 * all_pos + v1 * all_pos);
    sum[1] = hsum(v0 * alt_pn + v1 * alt_pn);
    sum[2] = hsum(v0 * pp_nn + v1 * pp_nn);
    sum[3] = hsum(v0 * p_nnp + v1 * p_nnp);
    sum[4] = hsum(v0 * all_pos + v1 * all_neg);
    sum[5] = hsum(v0 * alt_pn + v1 * alt_np);
    sum[6] = hsum(v0 * pp_nn + v1 * nn_pp);
    sum[7] = hsum(v0 * p_nnp + v1 * n_ppn);
}

inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8_simd(col.data(), sum.data());
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardFeed8_simd(const float* col, float* sum) noexcept
{
    const __m128 v0 = _mm_loadu_ps(&col[0]);
    const __m128 v1 = _mm_loadu_ps(&col[4]);

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

    sum[0] = hsum(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_pos)));
    sum[1] = hsum(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_pn)));
    sum[2] = hsum(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, pp_nn)));
    sum[3] = hsum(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, p_nnp)));
    sum[4] = hsum(_mm_add_ps(_mm_mul_ps(v0, all_pos), _mm_mul_ps(v1, all_neg)));
    sum[5] = hsum(_mm_add_ps(_mm_mul_ps(v0, alt_pn), _mm_mul_ps(v1, alt_np)));
    sum[6] = hsum(_mm_add_ps(_mm_mul_ps(v0, pp_nn), _mm_mul_ps(v1, nn_pp)));
    sum[7] = hsum(_mm_add_ps(_mm_mul_ps(v0, p_nnp), _mm_mul_ps(v1, n_ppn)));
}

inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8_simd(col.data(), sum.data());
}

#else

inline void hadamardFeed8_simd(const float* col, float* sum) noexcept
{
    hadamardFeed8(col, sum);
}

inline void hadamardFeed8_simd(const std::array<float, 8>& col, std::array<float, 8>& sum) noexcept
{
    hadamardFeed8(col.data(), sum.data());
}

#endif

}
