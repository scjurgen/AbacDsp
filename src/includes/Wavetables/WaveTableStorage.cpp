
#include "../../../test/Wavetables/WaveTableStorage.h"

#include <utility>

namespace AbacDsp
{

WaveTableSet WaveTableStore::fftFromSlice(const std::vector<float>& slice)
{
    std::vector freqWaveIm = slice;
    std::vector freqWaveRe(WaveTableSize, 0.f);
    smallFft(freqWaveRe, freqWaveIm);
    return addSet(freqWaveRe, freqWaveIm);
}

WaveTableSet WaveTableStore::addSet(std::vector<float>& freqWaveRe, std::vector<float>& freqWaveIm)
{
    // we create more fft filtered slices if narrowing the gap, this will decrease
    // slight differences in the top harmonic content but raise the memory footprint
    constexpr float minTop = 0.4f;
    constexpr float maxTop = 0.6f;

    // Zero DC offset and Nyquist !!
    freqWaveRe[0] = 0.0f;
    freqWaveIm[0] = 0.0f;
    freqWaveRe[WaveTableSize / 2] = 0.0f;
    freqWaveIm[WaveTableSize / 2] = 0.0f;

    const auto ar = freqWaveRe;
    const auto ai = freqWaveIm;
    float scale = 0.0f;

    WaveTableSet newset;

    unsigned int maxHarmonic = WaveTableSize / 2;
    while (maxHarmonic > 1)
    {
        const float topFreq = maxTop / static_cast<float>(maxHarmonic);
        // we will not optimize the table for dominant harmonics,
        // otherwise morphing between tables becomes very complicate.
        // Instead, we assume the same topFrequency for every slice.
        // the only looser in this scheme is the Sine wave or boring sin(p)+sin(p*2)*a harmonics

        // Prepare harmonic data by setting everything over the topFreq to 0
        std::vector harmonicRe(WaveTableSize, 0.0f);
        std::vector harmonicIm(WaveTableSize, 0.0f);
        for (int idx = 1; idx <= maxHarmonic; idx++)
        {
            harmonicRe[idx] = ar[idx];
            harmonicIm[idx] = ai[idx];
            harmonicRe[WaveTableSize - idx] = ar[WaveTableSize - idx];
            harmonicIm[WaveTableSize - idx] = ai[WaveTableSize - idx];
        }
        scale = makeWaveTable(harmonicRe, harmonicIm, scale, topFreq, newset.tables);
        // Prepare for next table
        const auto newHarmonic = static_cast<int>(std::floor(minTop / topFreq + 0.5f));
        if (maxHarmonic == newHarmonic)
        {
            break;
        }
        maxHarmonic = newHarmonic;
    }
    return newset;
}

float WaveTableStore::makeWaveTable(std::vector<float>& ar, std::vector<float>& ai, float scale, const float topFreq,
                                    std::vector<WaveTable>& tables)
{
    assert(ar.size() == WaveTableSize && ai.size() == WaveTableSize);

    smallFft(ar, ai);

    if (scale == 0.0f)
    {
        const auto maxIter = std::ranges::max_element(std::as_const(ai), [](const float a, const float b)
                                                      { return std::abs(a) < std::abs(b); });
        scale = 0.9999f / std::abs(*maxIter); // Prevent clipping
    }

    WaveTable newTable;
    newTable.topFreq = topFreq;

    std::ranges::transform(std::as_const(ai), newTable.data.begin(), [scale](const float v) { return v * scale; });

    // Add wrap sample for interpolation
    newTable.data[WaveTableSize] = newTable.data[0];
    tables.push_back(newTable);
    return scale;
}

void WaveTableStore::smallFft(std::vector<float>& ar, std::vector<float>& ai)
{
    const size_t len = ar.size();
    assert(len > 1 && ((len & (len - 1)) == 0));

    for (size_t jx = 0, i = 0; i < len; ++i)
    {
        if (i < jx)
        {
            std::swap(ar[i], ar[jx]);
            std::swap(ai[i], ai[jx]);
        }
        size_t m = len >> 1;
        while (m && jx >= m)
        {
            jx -= m;
            m >>= 1;
        }
        jx += m;
    }

    for (size_t s = 1; s <= static_cast<size_t>(std::log2(len)); ++s)
    {
        const size_t m = 1 << s;
        const float theta = -std::numbers::pi_v<float> / static_cast<float>(m / 2);
        const float wpr = std::cos(theta);
        const float wpi = std::sin(theta);
        for (size_t k = 0; k < len; k += m)
        {
            auto wr = 1.0f, wi = 0.0f;
            for (size_t j = 0; j < m / 2; ++j)
            {
                const size_t idx1 = k + j;
                const size_t idx2 = idx1 + m / 2;
                const float tr = wr * ar[idx2] - wi * ai[idx2];
                const float ti = wr * ai[idx2] + wi * ar[idx2];
                ar[idx2] = ar[idx1] - tr;
                ai[idx2] = ai[idx1] - ti;
                ar[idx1] += tr;
                ai[idx1] += ti;
                const float wr_temp = wr;
                wr = wr * wpr - wi * wpi;
                wi = wr_temp * wpi + wi * wpr;
            }
        }
    }
}

std::vector<WaveTableSet> WaveTableStore::s_wtbls;
}