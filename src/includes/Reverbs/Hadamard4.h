#pragma once

#include <array>

#include "Helpers/PlatformIntrinsics.h"

namespace AbacDsp
{

inline void hadamardFeed4(const std::array<float, 4>& col, std::array<float, 4>& sum) noexcept
{
    sum[0] = col[0] + col[1] + col[2] + col[3];
    sum[1] = col[0] - col[1] + col[2] - col[3];
    sum[2] = col[0] + col[1] - col[2] - col[3];
    sum[3] = col[0] - col[1] - col[2] + col[3];
}

#if defined(USE_SIMD_FRAMEWORK)

inline void hadamardFeed4_simd(const float* col, float* sum) noexcept
{
    simd_float4 v0 = simd_make_float4(col[0], col[1], col[2], col[3]);

    simd_float4 all_pos = simd_make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    simd_float4 alt_pn = simd_make_float4(1.0f, -1.0f, 1.0f, -1.0f);
    simd_float4 pp_nn = simd_make_float4(1.0f, 1.0f, -1.0f, -1.0f);
    simd_float4 p_nnp = simd_make_float4(1.0f, -1.0f, -1.0f, 1.0f);

    auto hsum = [](simd_float4 v) -> float { return simd_reduce_add(v); };

    sum[0] = hsum(v0 * all_pos);
    sum[1] = hsum(v0 * alt_pn);
    sum[2] = hsum(v0 * pp_nn);
    sum[3] = hsum(v0 * p_nnp);
}

#elif defined(USE_X86_INTRINSICS)

inline void hadamardFeed4_simd(const float* col, float* sum) noexcept
{
    const __m128 v0 = _mm_loadu_ps(&col[0]);
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

#else

inline void hadamardFeed4_simd(const float* col, float* sum) noexcept
{
    hadamardFeed4(*reinterpret_cast<const std::array<float, 4>*>(col), *reinterpret_cast<std::array<float, 4>*>(sum));
}

#endif

}
