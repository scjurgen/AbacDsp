#pragma once

#include "Numbers/Convert.h"


#include <cassert>
#include <cmath>
#include <complex>
#include <iostream>
#include <random>
#include <valarray>
#include <vector>

/*
 * BasicFFT (allocates heap while computing)
 * KissFft (I guess save)
 */

namespace AbacDsp
{

// Window type tags
struct FftHannWindow
{
};
struct FftFlatTopWindow
{
};
struct FftRectangularWindow
{
};
struct FftBlackmanWindow
{
};

class BasicFFT
{
  public:
    template <typename T, typename WindowTag = FftFlatTopWindow>
    static void realDataToMagnitude(const std::vector<T>& in, std::vector<T>& magnitudes)
    {
        const auto N = in.size();
        if (!(N != 0 && ((N & (N - 1)) == 0)))
        {
            std::cerr << " FFT " << __FUNCTION__ << " array size must be multiple of 2^n" << std::endl;
            return;
        }

        std::valarray<std::complex<double>> inData(N);
        for (size_t i = 0; i < N; ++i)
        {
            inData[i] = std::complex<double>(in[i], 0.0);
        }

        applyWindow<WindowTag>(inData);
        fft(inData);

        magnitudes.resize(N / 2);
        for (size_t i = 0; i < N / 2; ++i)
        {
            magnitudes[i] = std::abs(inData[i]);
        }
    }

    // Legacy interface for backward compatibility
    template <typename T>
    static void realDataToMagnitude(const std::vector<T>& in, std::vector<T>& magnitudes, bool useFlatTopWindow)
    {
        if (useFlatTopWindow)
        {
            realDataToMagnitude<T, FftFlatTopWindow>(in, magnitudes);
        }
        else
        {
            realDataToMagnitude<T, FftHannWindow>(in, magnitudes);
        }
    }

  private:
    template <typename WindowTag>
    static void applyWindow(std::valarray<std::complex<double>>& x)
    {
        if constexpr (std::is_same_v<WindowTag, FftHannWindow>)
        {
            hannWindow(x);
        }
        else if constexpr (std::is_same_v<WindowTag, FftFlatTopWindow>)
        {
            flatTopWindow(x);
        }
        else if constexpr (std::is_same_v<WindowTag, FftRectangularWindow>)
        {
            // No windowing applied for rectangular window
        }
        else if constexpr (std::is_same_v<WindowTag, FftBlackmanWindow>)
        {
            blackmanWindow(x);
        }
    }

    static void hannWindow(std::valarray<std::complex<double>>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto w = static_cast<double>(i) / static_cast<double>(x.size() - 1);
            x[i] *= 0.5 * (1 - std::cos(2.0 * std::numbers::pi_v<float> * w));
        }
    }

    static void flatTopWindow(std::valarray<std::complex<double>>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto w = static_cast<double>(i) / static_cast<double>(x.size() - 1);
            x[i] *= 1 - 1.93 * std::cos(2.0 * std::numbers::pi_v<float> * w) +
                    1.29 * std::cos(4.0 * std::numbers::pi_v<float> * w) -
                    0.388 * std::cos(6.0 * std::numbers::pi_v<float> * w) +
                    0.0322 * std::cos(8.0 * std::numbers::pi_v<float> * w);
        }
    }

    static void blackmanWindow(std::valarray<std::complex<double>>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto w = static_cast<double>(i) / static_cast<double>(x.size() - 1);
            x[i] *= 0.42 - 0.5 * std::cos(2.0 * std::numbers::pi_v<float> * w) +
                    0.08 * std::cos(4.0 * std::numbers::pi_v<float> * w);
        }
    }

    static void fft(std::valarray<std::complex<double>>& x)
    {
        const size_t N = x.size();
        if (N == 0)
        {
            return;
        }

        std::valarray even = x[std::slice(0, N / 2, 2)];
        std::valarray odd = x[std::slice(1, N / 2, 2)];

        fft(even);
        fft(odd);

        for (size_t k = 0; k < N / 2; ++k)
        {
            const auto t =
                std::polar(1.0, -2 * std::numbers::pi_v<float> * static_cast<double>(k) / static_cast<double>(N)) *
                odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }
};


class FFTResponse
{
  public:
    static std::vector<float> generateNoiseSignal(const size_t NumFrames)
    {
        std::minstd_rand generator(31);
        std::uniform_real_distribution<float> dist(-1.f, 1.f);
        std::vector<float> signal(NumFrames);
        std::generate(signal.begin(), signal.end(), [&generator, &dist]() { return dist(generator); });
        return signal;
    }

    template <size_t interleave, size_t channelOffset>
    static std::vector<float> analyse(const std::vector<float>& resultBuffer, const size_t windowSize)
    {
        const size_t windowAdvance = windowSize / 4;
        static_assert(interleave > 0);
        static_assert(channelOffset < interleave);
        assert(resultBuffer.size() > windowSize * interleave + channelOffset);
        std::vector<float> vData(windowSize, 0.0f);
        std::vector<float> magnitude(windowSize, 0.0f);
        std::vector<float> sumUp(windowSize / 2 - 1, 0.0f); // 1 element less: remove dc
        for (size_t j = 0; j < resultBuffer.size() - windowSize * interleave - channelOffset;
             j += windowAdvance * interleave)
        {
            for (size_t i = 0; i < vData.size(); ++i)
            {
                vData[i] = resultBuffer[j + i * interleave + channelOffset];
            }
            BasicFFT::realDataToMagnitude(vData, magnitude);
            for (size_t i = 0; i < sumUp.size(); ++i)
            {
                sumUp[i] += magnitude[i + 1]; // skip dc
            }
        }
        const auto maxit = max_element(sumUp.begin(), sumUp.end());
        const float normFactor = 1.f / (*maxit);
        for (float& i : sumUp)
        {
            i *= normFactor;
        }
        return sumUp;
    }


    struct FrequencySlice
    {
        float minValue;
        float maxValue;
        std::vector<float> bins;
        std::vector<float> frequencies;
    };

    static FrequencySlice processFrequencies(const std::vector<float>& binFrequency, const std::vector<float>& binSum,
                                             float minValue)
    {
        FrequencySlice slice{};
        auto lastNonZero = std::find_if(binFrequency.rbegin(), binFrequency.rend(), [](float f) { return f != 0.0f; });
        auto firstGreaterThan =
            std::find_if(binFrequency.begin(), binFrequency.end(), [minValue](float f) { return f > minValue; });
        if (lastNonZero != binFrequency.rend() && firstGreaterThan != binFrequency.end())
        {
            size_t start = std::distance(binFrequency.begin(), firstGreaterThan);
            size_t end = binFrequency.size() - std::distance(binFrequency.rbegin(), lastNonZero);
            if (start < end)
            {
                slice.bins = std::vector<float>(binSum.begin() + start, binSum.begin() + end);
                slice.frequencies = std::vector<float>(binFrequency.begin() + start, binFrequency.begin() + end);
                auto [min, max] = std::minmax_element(slice.bins.begin(), slice.bins.end());
                slice.minValue = *min;
                slice.maxValue = *max;
            }
        }
        return slice;
    }

    template <size_t interleave, size_t channelOffset>
    static FrequencySlice analyseByOctaveBins(const std::vector<float>& resultBuffer, const size_t windowSize,
                                              const float sampleRate, const int binsPerOctave,
                                              const float startFreq = -1)
    {
        auto tmp = analyse<interleave, channelOffset>(resultBuffer, windowSize);
        auto numBins = static_cast<unsigned>(std::ceil(std::log(sampleRate) / std::log(2.f) * binsPerOctave));
        std::vector<float> binSum(numBins, 0);
        std::vector<unsigned> binCount(numBins, 0);
        std::vector<float> binFrequency(numBins, 0);

        for (size_t i = 1; i < tmp.size(); ++i)
        {
            const auto f = sampleRate / static_cast<float>(windowSize) * static_cast<float>(i);
            auto bin = static_cast<unsigned>(std::round(std::log(f) / std::log(2.0f) * binsPerOctave));
            if (binFrequency[bin] == 0)
            {
                binFrequency[bin] = f;
            }
            binSum[bin] += tmp[i];
            binCount[bin]++;
        }
        std::transform(binCount.begin(), binCount.end(), binSum.begin(), binSum.begin(),
                       [](auto cnt, auto sum) { return cnt > 0 ? sum / static_cast<float>(cnt) : 0; });
        const auto maxit = max_element(binSum.begin(), binSum.end());
        const float normFactor = 1.f / (*maxit);
        for (float& i : binSum)
        {
            i *= normFactor;
        }

        return processFrequencies(binFrequency, binSum, startFreq);
    }

    template <size_t interleave, size_t channelOffset>
    static std::vector<float> analyseByNoteBins(const std::vector<float>& resultBuffer, const size_t windowSize,
                                                const float sampleRate)
    {
        auto tmp = analyse<interleave, channelOffset>(resultBuffer, windowSize);
        constexpr size_t numBins = 12;
        std::vector<float> binSum(numBins, 0);
        std::vector<unsigned> binCount(numBins, 0);
        const auto fBin = sampleRate / static_cast<float>(windowSize);
        for (size_t i = 1; i < tmp.size(); ++i)
        {
            const auto f = fBin * static_cast<float>(i);
            const auto note = std::round(Convert::frequencyToNote(f));
            if (note > 0)
            {
                const auto bin = static_cast<unsigned>(note) % 12;
                binSum[bin] += tmp[i];
                binCount[bin]++;
            }
        }
        std::transform(binCount.begin(), binCount.end(), binSum.begin(), binSum.begin(),
                       [](auto cnt, auto sum) { return cnt > 0 ? sum / static_cast<float>(cnt) : 0; });
        const auto maxit = max_element(binSum.begin(), binSum.end());
        const float normFactor = 1.f / (*maxit);
        for (float& i : binSum)
        {
            i *= normFactor;
        }
        return binSum;
    }
};

/*
Copyright (c) 2003-2010 Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

template <typename T_Scalar>
class KissFft
{
  public:
    KissFft(const size_t nfft, const bool inverse)
        : _nfft(nfft)
        , _inverse(inverse)
    {
        prepare();
    }

    void resize(const size_t nfft)
    {
        _nfft = nfft;
        prepare();
    }

    void compute(const std::complex<T_Scalar>* src, std::complex<T_Scalar>* dst)
    {
        compute(0, dst, src, 1, 1);
    }

  private:
    void prepare()
    {
        _twiddles.resize(_nfft);

        const T_Scalar phInc =
            (_inverse ? 2 : -2) * acos(static_cast<T_Scalar>(-1)) / static_cast<float>(_twiddles.size());
        for (size_t i = 0; i < _twiddles.size(); ++i)
        {
            _twiddles[i] = exp(std::complex<T_Scalar>(0, static_cast<float>(i) * phInc));
        }

        // factorize
        // start factoring out 4's, then 2's, then 3,5,7,9,...
        _stageRadix.clear();
        _stageRemainder.clear();
        size_t n = _nfft;
        size_t p = 4;
        do
        {
            while (n % p)
            {
                switch (p)
                {
                    case 4:
                        p = 2;
                        break;
                    case 2:
                        p = 3;
                        break;
                    default:
                        p += 2;
                        break;
                }
                if (p * p > n) // no more factors
                {
                    p = n;
                }
            }
            n /= p;
            _stageRadix.push_back(p);
            _stageRemainder.push_back(n);
        } while (n > 1);
        _scratchbuf.resize(*std::max_element(_stageRadix.begin(), _stageRadix.end()));
    }

    void compute(const size_t stage, std::complex<T_Scalar>* fOut, const std::complex<T_Scalar>* f,
                 const size_t fStride, const size_t inStride)
    {
        std::complex<T_Scalar>* fOutBegin = fOut;
        std::complex<T_Scalar>* fOutEnd = fOut + _stageRadix[stage] * _stageRemainder[stage];

        if (_stageRemainder[stage] == 1)
        {
            do
            {
                *fOut = *f;
                f += fStride * inStride;
            } while (++fOut != fOutEnd);
        }
        else
        {
            do
            {
                // recursive call:
                // DFT of size m*p performed by doing
                // p instances of smaller DFTs of size m,
                // each one takes a decimated version of the input
                compute(stage + 1, fOut, f, fStride * _stageRadix[stage], inStride);
                f += fStride * inStride;
                fOut += _stageRemainder[stage];
            } while (fOut != fOutEnd);
        }

        fOut = fOutBegin;

        // recombine the p smaller DFTs
        switch (_stageRadix[stage])
        {
            case 2:
                butterfly2(fOut, fStride, _stageRemainder[stage]);
                break;
            case 3:
                butterfly3(fOut, fStride, _stageRemainder[stage]);
                break;
            case 4:
                butterfly4(fOut, fStride, _stageRemainder[stage]);
                break;
            case 5:
                butterfly5(fOut, fStride, _stageRemainder[stage]);
                break;
            default:
                butterfly_generic(fOut, fStride, _stageRemainder[stage], _stageRadix[stage]);
                break;
        }
    }

    void butterfly2(std::complex<T_Scalar>* fOut, const size_t fStride, const size_t m)
    {
        for (size_t k = 0; k < m; ++k)
        {
            const std::complex<T_Scalar> t = fOut[m + k] * _twiddles[k * fStride];
            fOut[m + k] = fOut[k] - t;
            fOut[k] += t;
        }
    }

    void butterfly4(std::complex<T_Scalar>* fOut, const size_t fStride, const size_t m)
    {
        const auto negative_if_inverse = static_cast<T_Scalar>(_inverse * -2 + 1);
        for (size_t k = 0; k < m; ++k)
        {
            std::complex<T_Scalar> scratch[7];
            scratch[0] = fOut[k + m] * _twiddles[k * fStride];
            scratch[1] = fOut[k + 2 * m] * _twiddles[k * fStride * 2];
            scratch[2] = fOut[k + 3 * m] * _twiddles[k * fStride * 3];
            scratch[5] = fOut[k] - scratch[1];

            fOut[k] += scratch[1];
            scratch[3] = scratch[0] + scratch[2];
            scratch[4] = scratch[0] - scratch[2];
            scratch[4] = std::complex<T_Scalar>(scratch[4].imag() * negative_if_inverse,
                                                -scratch[4].real() * negative_if_inverse);

            fOut[k + 2 * m] = fOut[k] - scratch[3];
            fOut[k] += scratch[3];
            fOut[k + m] = scratch[5] + scratch[4];
            fOut[k + 3 * m] = scratch[5] - scratch[4];
        }
    }

    void butterfly3(std::complex<T_Scalar>* fOut, const size_t fStride, const size_t m)
    {
        const size_t m2 = 2 * m;
        const std::complex<T_Scalar> epi3 = _twiddles[fStride * m];

        std::complex<T_Scalar>* tw1 = _twiddles.data();
        std::complex<T_Scalar>* tw2 = tw1;
        size_t k = m;
        do
        {
            std::complex<T_Scalar> scratch[5];
            scratch[1] = fOut[m] * *tw1;
            scratch[2] = fOut[m2] * *tw2;
            scratch[3] = scratch[1] + scratch[2];
            scratch[0] = scratch[1] - scratch[2];

            tw1 += fStride;
            tw2 += fStride * 2;
            fOut[m] = *fOut - std::complex<T_Scalar>(0.5, 0.0) * scratch[3];
            scratch[0] *= epi3.imag();
            *fOut += scratch[3];

            fOut[m2] = std::complex<T_Scalar>(fOut[m].real() + scratch[0].imag(), fOut[m].imag() - scratch[0].real());
            fOut[m] += std::complex<T_Scalar>(-scratch[0].imag(), scratch[0].real());
            ++fOut;
        } while (--k);
    }

    void butterfly5(std::complex<T_Scalar>* fOut, const size_t fStride, const size_t m)
    {
        const std::complex<T_Scalar> ya = _twiddles[fStride * m];
        const std::complex<T_Scalar> yb = _twiddles[fStride * 2 * m];
        std::complex<T_Scalar>* fOut0 = fOut;
        std::complex<T_Scalar>* fOut1 = fOut0 + m;
        std::complex<T_Scalar>* fOut2 = fOut0 + 2 * m;
        std::complex<T_Scalar>* fOut3 = fOut0 + 3 * m;
        std::complex<T_Scalar>* fOut4 = fOut0 + 4 * m;

        for (size_t u = 0; u < m; ++u, ++fOut0, ++fOut1, ++fOut2, ++fOut3, ++fOut4)
        {
            std::complex<T_Scalar> scratch[13];
            scratch[0] = *fOut0;

            scratch[1] = *fOut1 * _twiddles[u * fStride];
            scratch[2] = *fOut2 * _twiddles[2 * u * fStride];
            scratch[3] = *fOut3 * _twiddles[3 * u * fStride];
            scratch[4] = *fOut4 * _twiddles[4 * u * fStride];

            scratch[7] = scratch[1] + scratch[4];
            scratch[10] = scratch[1] - scratch[4];
            scratch[8] = scratch[2] + scratch[3];
            scratch[9] = scratch[2] - scratch[3];

            *fOut0 += scratch[7];
            *fOut0 += scratch[8];

            scratch[5] =
                scratch[0] + std::complex<T_Scalar>(scratch[7].real() * ya.real() + scratch[8].real() * yb.real(),
                                                    scratch[7].imag() * ya.real() + scratch[8].imag() * yb.real());

            scratch[6] = std::complex<T_Scalar>(scratch[10].imag() * ya.imag() + scratch[9].imag() * yb.imag(),
                                                -scratch[10].real() * ya.imag() - scratch[9].real() * yb.imag());

            *fOut1 = scratch[5] - scratch[6];
            *fOut4 = scratch[5] + scratch[6];

            scratch[11] =
                scratch[0] + std::complex<T_Scalar>(scratch[7].real() * yb.real() + scratch[8].real() * ya.real(),
                                                    scratch[7].imag() * yb.real() + scratch[8].imag() * ya.real());

            scratch[12] = std::complex<T_Scalar>(-scratch[10].imag() * yb.imag() + scratch[9].imag() * ya.imag(),
                                                 scratch[10].real() * yb.imag() - scratch[9].real() * ya.imag());

            *fOut2 = scratch[11] + scratch[12];
            *fOut3 = scratch[11] - scratch[12];

            ++fOut0;
            ++fOut1;
            ++fOut2;
            ++fOut3;
            ++fOut4;
        }
    }

    // perform butterfly for one stage of a mixed radix FFT
    void butterfly_generic(std::complex<T_Scalar>* fOut, const size_t fStride, size_t m, size_t p)
    {
        const auto nOriginal = _nfft;
        for (size_t u = 0; u < m; ++u)
        {
            {
                size_t k = u;
                for (size_t q1 = 0; q1 < p; ++q1)
                {
                    _scratchbuf[q1] = fOut[k];
                    k += m;
                }
            }
            {
                size_t k = u;
                for (size_t q1 = 0; q1 < p; ++q1, k += m)
                {
                    fOut[k] = _scratchbuf[0];
                    for (size_t q = 1, twIndex = 0; q < p; ++q)
                    {
                        twIndex += fStride * k;
                        if (twIndex >= nOriginal)
                        {
                            twIndex -= nOriginal;
                        }
                        std::complex<T_Scalar> t = _scratchbuf[q] * _twiddles[twIndex];
                        fOut[k] += t;
                    }
                }
            }
        }
    }

    size_t _nfft;
    bool _inverse;
    std::vector<std::complex<T_Scalar>> _twiddles;
    std::vector<size_t> _stageRadix;
    std::vector<size_t> _stageRemainder;
    std::vector<std::complex<T_Scalar>> _scratchbuf;
};

class HannWindowMagnitudesFft
{
  public:
    explicit HannWindowMagnitudesFft(const size_t N)
        : fft(N, false)
        , tmpIn(N)
        , tmpOut(N)
    {
        hannWindow(N);
    }

    void resize(const size_t N)
    {
        fft.resize(N);
        tmpIn.resize(N);
        tmpOut.resize(N);
        hannWindow(N);
    }

    void compute(const std::vector<float>& src, std::vector<float>& dst)
    {
        std::transform(src.begin(), src.end(), window.begin(), tmpIn.begin(),
                       [](float s, float w) { return std::complex<float>(s * w, 0.0f); });
        fft.compute(tmpIn.data(), tmpOut.data());
        realDataToMagnitude(dst);
    }

  private:
    void realDataToMagnitude(std::vector<float>& dst) const
    {
        float d = 1.f / static_cast<float>(window.size());
        std::transform(tmpOut.begin(), tmpOut.begin() + window.size() / 2, dst.begin(),
                       [d](const auto& complex_val) { return std::abs(complex_val) * d; });
    }

    void hannWindow(const size_t N)
    {
        window.resize(N);
        std::generate(window.begin(), window.end(),
                      [n = 0, N]() mutable
                      {
                          const auto w = static_cast<double>(n) / static_cast<double>(N - 1);
                          ++n;
                          return 0.5 * (1 - std::cos(2.0 * M_PI * w));
                      });
    }

    KissFft<float> fft;
    std::vector<float> window;
    std::vector<std::complex<float>> tmpIn;
    std::vector<std::complex<float>> tmpOut;
};

template <typename WindowFunction, size_t N>
class WindowedMagnitudesFft
{
  public:
    explicit WindowedMagnitudesFft()
        : fft(N, false)
        , tmpIn(N)
        , tmpOut(N)
    {
        applyWindow();
    }

    void compute(const std::vector<float>& src, std::vector<float>& dst)
    {
        std::transform(src.begin(), src.end(), window.begin(), tmpIn.begin(),
                       [](float s, float w) { return std::complex<float>(s * w, 0.0f); });
        fft.compute(tmpIn.data(), tmpOut.data());
        realDataToMagnitude(dst);
    }

  private:
    void realDataToMagnitude(std::vector<float>& dst) const
    {
        float d = 1.f / static_cast<float>(window.size());
        std::transform(tmpOut.begin(), tmpOut.begin() + window.size() / 2, dst.begin(),
                       [d](const auto& complex_val) { return std::abs(complex_val) * d; });
    }

    void applyWindow()
    {
        window.resize(N);
        std::generate(window.begin(), window.end(), [n = 0, this]() mutable { return this->windowFunction(n++, N); });
    }

    KissFft<float> fft;
    std::vector<float> window;
    std::vector<std::complex<float>> tmpIn;
    std::vector<std::complex<float>> tmpOut;
    WindowFunction windowFunction;
};

struct HannWindow
{
    float operator()(size_t n, size_t N) const
    {
        const auto w = static_cast<float>(n) / static_cast<float>(N - 1);
        return 0.5f * (1.f - std::cos(2.0f * static_cast<float>(M_PI) * w));
    }
};

struct BlackmanWindow
{
    float operator()(size_t n, size_t N) const
    {
        const float w = static_cast<float>(n) / static_cast<float>(N - 1);
        constexpr float a0 = 0.42f;
        constexpr float a1 = 0.5f;
        constexpr float a2 = 0.08f;
        return a0 - a1 * std::cos(2.0f * static_cast<float>(M_PI) * w) +
               a2 * std::cos(4.0f * static_cast<float>(M_PI) * w);
    }
};

template <size_t N>
using HannWindowedMagnitudesFft = WindowedMagnitudesFft<HannWindow, N>;

template <size_t N>
using BlackmanWindowedMagnitudesFft = WindowedMagnitudesFft<BlackmanWindow, N>;
}