

#include <gtest/gtest.h>
#include <cmath>
#include <random>

#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"
#include "Reverbs/FdnTankBlockDelaySIMD.h"
#include "Reverbs/FdnTankBlockDelay.h"
#include "Reverbs/FdnTankRef.h"

namespace AbacDsp::Test
{
// use clinical sizes

static const std::array<unsigned, 8> primes4{113, 127, 131, 137};

const std::vector expected{
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    1.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           1.f,          0.f,
    0.f,          0.f,           1.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    1.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          -0.290772f,    0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          -0.562658f,
    0.f,          0.f,           0.f,         -0.557491f,   0.f,         0.f,           0.f,          0.f,
    0.f,          -0.549924f,    0.f,         0.f,          0.f,         0.271886f,     0.f,          0.f,
    0.f,          -0.538604f,    0.f,         0.f,          0.f,         0.266719f,     0.f,          0.531037f,
    0.f,          0.f,           0.f,         0.52587f,     0.f,         0.f,           0.f,          0.f,
    0.f,          -0.259151f,    0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.0845486f,  0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.242663f,    0.f,           0.f,         0.f,          0.239658f,   0.f,           0.f,          0.f,
    0.f,          0.f,           0.235257f,   0.f,          0.f,         0.f,           -0.0739219f,  0.f,
    0.f,          0.f,           0.458257f,   0.f,          0.f,         0.f,           -0.0711388f,  0.f,
    -0.140919f,   0.f,           0.f,         0.f,          -0.064319f,  0.f,           0.f,          0.f,
    -0.0739219f,  0.f,           0.217868f,   0.f,          -0.0711388f, 0.f,           0.214841f,    0.f,
    0.0711388f,   0.f,           -0.145034f,  0.f,          0.f,         0.f,           0.20938f,     0.f,
    -0.0671594f,  0.f,           0.f,         0.f,          -0.0671594f, 0.f,           0.f,          0.f,
    0.f,          0.f,           0.0671594f,  0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         -0.0245844f,  0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          -0.0935472f,   0.f,         0.f,          0.f,         -0.0922365f,   0.f,          0.f,
    0.f,          0.f,           0.f,         -0.0903171f,  0.f,         0.f,           0.f,          -0.00149312f,
    0.f,          0.f,           0.f,         -0.263131f,   0.f,         0.f,           0.f,          -0.00186548f,
    0.f,          -0.00392306f,  0.f,         0.f,          0.f,         -0.00286879f,  0.f,          0.f,
    0.f,          -0.000961099f, 0.f,         -0.124317f,   0.f,         -0.00214506f,  0.f,          0.00499899f,
    0.f,          0.00171117f,   0.f,         0.181028f,    0.f,         0.f,           0.f,          0.00519275f,
    0.f,          0.0787632f,    0.f,         -0.039058f,   0.f,         0.155207f,     0.f,          0.000367593f,
    0.f,          -0.0764591f,   0.f,         -0.0570147f,  0.f,         -0.0753631f,   0.f,          0.000897272f,
    0.f,          0.0742812f,    0.f,         0.0737581f,   0.f,         0.f,           0.f,          0.000523055f,
    0.f,          -0.000855248f, 0.f,         0.f,          0.f,         -0.000508219f, 0.f,          0.f,
    0.f,          0.f,           0.f,         -0.0174044f,  0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.00714847f, 0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         0.f,          0.f,         0.f,           0.f,          0.f,
    0.f,          0.f,           0.0338851f,  0.f,          0.f,         0.f,           0.033377f,    0.f,
    0.f,          0.f,           0.f,         0.f,          0.0326328f,  0.f,           0.f,          0.f,
    0.0133683f,   0.f,           0.f,         0.f,          0.12654f,    0.f,           0.f,          0.f,
    0.0131142f,   0.f,           0.0261105f,  0.f,          0.f,         0.f,           0.0133564f,   0.f,
    0.f,          0.f,           0.0367873f,  0.f,          0.0595537f,  0.f,           0.0367873f,   0.f,
    -0.036329f,   0.f,           -0.0120294f, 0.f,          -0.0610946f, 0.f,           0.f,          0.f,
    -0.0583374f,  0.f,           -0.0331522f, 0.f,          0.0332314f,  0.f,           -0.00824616f, 0.f,
    -0.0224961f,  0.f,           0.0255772f,  0.f,          0.0602859f,  0.f,           0.0247795f,   0.f,
    0.0329001f,   0.f,           0.0128081f,  0.f,          0.00681769f, 0.f,           -0.0103175f,  0.f,
    0.0112234f,   0.f,           -0.0154835f, 0.f,          -0.0105174f, 0.f,           -0.00506074f, 0.f,
    -0.0198591f,  0.f,           0.00945046f, 0.f,          0.0494826f,  0.f,           0.00927085f,  0.f,
    -0.0099291f,  0.f,           0.00983429f, 0.f,          -0.0187482f, 0.f,           0.f,          0.f,
    -0.00955528f, 0.f,           0.00495366f, 0.f,          0.f,         -0.00207858f,  0.00477379f,  0.f,
    0.f,          0.f,           0.f,         0.f,          0.00451038f, 0.f,           0.f,          0.f,
    0.f,          0.f,           0.f,         -0.0117964f,  0.f,         0.f,           0.f,          -0.0116117f,
    0.f,          0.f,           0.f,         0.f,          0.f,         -0.0113413f,   0.f,          0.f,
    0.f,          -0.00946536f,  0.f,         0.f,          0.f,         -0.0549068f,   0.f,          0.f,
    0.f,          -0.0092177f,   0.f,         -0.0183171f,  0.f,         0.f,           0.f,          -0.0110414f,
    0.f,          0.f,           0.f,         -0.0350301f,  0.f,         -0.0257734f,   0.f,          -0.0347031f,
    0.f,          0.0202817f,    0.f,         0.00670627f,  0.f,         0.00694831f,   0.f,          0.f,
    0.f,          0.0392826f,    0.f,         0.00596084f,  0.f,         -0.0289883f,   0.f,          -0.0133899f,
    0.f,          0.0190204f,    0.f,         0.0232908f,   0.f,         -0.0359059f,   0.f,          0.0105295f,
    0.f,          -0.00881306f,  0.f,         -0.0122436f,  0.f,         0.0339968f,    0.f,          0.00574713f,
    0.f,          -0.0195017f,   0.f,         0.00886222f,  0.f,         -0.00310222f,  0.f,          0.0385716f,
    0.f,          -0.0138251f,   0.f,         0.0144696f,   0.f,         -0.0119231f,   0.f,          0.00305406f,
    0.f,          -0.00714036f,  0.f,         0.0116397f,   0.f,         0.0204389f,    0.f,          -0.0055308f,
    0.f,          -0.00985704f,  0.f,         0.000149633f, 0.f,         -0.00252061f,  0.000604394f, 0.00536602f,
    0.f,          -0.00539699f,  0.f,         0.00537169f,  0.f,         -0.00124329f,  0.f,          0.00531757f,


};

TEST(FdnReverb4, checkReference)
{
    constexpr size_t BlockSize{16};
    FdnTankRef<4096, 4, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setOrder(4);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes4.size(); ++i)
    {
        sut.setDirectSize(i, primes4[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 50; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f);
        }
    }
}

TEST(FdnReverb4, checkBlockDelay)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelay<4096, 4, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes4.size(); ++i)
    {
        sut.setDirectSize(i, primes4[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
        // std::cout << "0\n";
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        // std::cout << j << ":" << std::endl;
        for (float value : out)
        {
            // std::cout << value << "\n";

            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

TEST(FdnReverb4, checkSimd)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelaySIMD<4096, 4, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);


    for (size_t i = 0; i < primes4.size(); ++i)
    {
        sut.setDirectSize(i, primes4[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
        // std::cout << "0\n";
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

TEST(FdnReverb4, checkSimdWalsh)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelaySIMDWalsh<4096, 4, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes4.size(); ++i)
    {
        sut.setDirectSize(i, primes4[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

}
