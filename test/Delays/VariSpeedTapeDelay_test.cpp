#include "gtest/gtest.h"

#include <cmath>
#include "Delays/VariSpeedTapeDelay.h"

TEST(VariSpeedTapeDelayTest, simpleProcess)
{
    AbacDsp::VarispeedTapeDelay<10000> sut{48000.f};
    const std::vector<float> signal{0.5f, 1.f, 0.5f, 0.f, -0.5f, -1.f, -0.5f, 0.f};
    const std::vector<float> empty(8, 0);
    std::vector<float> result(8, 0);
    sut.setTapeSpeed(1.f);
    sut.setDelayTimeMsecs(5.f);
    sut.setTapeLengthInMsecs(50.f);
    sut.reset();
    sut.process<8>(signal.data(), result.data());
    for (size_t i = 0; i < 100; ++i)
    {
        sut.process<8>(empty.data(), result.data());
        for (size_t j = 0; j < result.size(); ++j)
        {
            if (result[j] != 0.f)
            {
                std::cout << i * 8 + j << "\t" << result[j] << std::endl;
            }
        }
    }
}
