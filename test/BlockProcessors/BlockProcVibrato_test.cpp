

#include <gtest/gtest.h>
#include <cmath>
#include <random>

#include "BlockProcessors/BlockProcVibrato.h"

namespace AbacDsp::Test
{
// use clinical sizes
TEST(BlockProcVibratoTest, basicOperation)
{
    BlockProc::Vibrato<16> sut{48000.f};
    std::array<float, 16> feed{1};
    sut.setModDepth(1.f);
    sut.setModSpeed(100);
    sut.process(feed);
    for (size_t i = 0; i < feed.size(); ++i)
    {
        std::cout << feed[i] << ", ";
    }
    std::cout << "\n";
    for (size_t i = 0; i < 1000; ++i)
    {
        std::array<float, 16> inplace{};
        sut.process(inplace);
        // for (size_t j = 0; j < inplace.size(); ++j)
        // {
        //     std::cout << inplace[j] << ", ";
        // }
        // std::cout << "\n";
    }
}

}
