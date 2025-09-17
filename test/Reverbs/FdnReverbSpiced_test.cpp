

#include <gtest/gtest.h>
#include <cmath>
#include <random>


#include "Reverbs/FdnTankSpiced.h"

namespace AbacDsp::Test
{
// use clinical sizes
// 547, 661, 811, 947, 1087, 1229, 1381, 1523, 1663, 1823, 1993, 2131, 2293, 2437, 2621, 2749, 2909, 3083, 3259, 3433,
// 3581, 3733, 3911, 4073, 4241, 4421, 4591, 4759, 4943, 5099, 5281, 5449

static const std::array<unsigned, 32> primeSizes32{113, 127, 131, 137, 139, 149, 151, 157, //
                                                   163, 167, 173, 179, 181, 191, 193, 197, //
                                                   199, 211, 223, 227, 229, 233, 239, 241, //
                                                   251, 257, 263, 269, 271, 277, 281, 283};
const std::vector expected{0.f

};

TEST(FdnTankSpiced, spicedStereo)
{
    constexpr size_t BlockSize{16};
    FdnTankSpiced<4096, 32, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primeSizes32.size(); ++i)
    {
        sut.setDirectSize(i, primeSizes32[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<std::array<float, BlockSize>, 2> out{};
    size_t indexExpected{0};

    for (size_t j = 0; j < 50; ++j)
    {
        if (j == 0)
        {
            sut.processBlockSplit(in.data(), out[0].data(), out[1].data());
        }
        else
        {
            sut.processBlockSplit(empty.data(), out[0].data(), out[1].data());
        }
        for (size_t s = 0; s < out[0].size(); ++s)
        {
            std::cout << indexExpected++ << "\t" << out[0][s] << "\t" << out[1][s] << std::endl;
            // EXPECT_NEAR(expected[indexExpected++], value, 1E-5f);
        }
    }
}


}
