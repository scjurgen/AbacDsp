
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Wavetables/WaveTableStorage.h"

namespace AbacDsp::Test
{


TEST(WaveTableStorageTest, wavetableStorageSingle)
{
    WaveTableSet& wtbl = WaveTableStore::getTableSet(BasicWave::Saw);
    EXPECT_EQ(wtbl.tables.size(), 16);
    EXPECT_EQ(wtbl.getIndexByFrequency(0.0001f), 0);
    EXPECT_EQ(wtbl.getIndexByFrequency(0.001f), 2);
    EXPECT_EQ(wtbl.getIndexByFrequency(0.01f), 7);
    EXPECT_EQ(wtbl.getIndexByFrequency(0.1f), 13);
    EXPECT_EQ(wtbl.getIndexByFrequency(0.5f), 15);
}


TEST(WaveTableStorageTest, checkLevels)
{
    std::vector<float> expected = {
        0.707281f,    0.865605f, 1.05304f, 0.924317f, 0.636625f, 1.21588f, 2.569484f,
        1.30268e-05f, // N.B.: Noise floor for triggering filter resonance
        1.32851e-05f, // N.B.: the white noise level is low and gets in the osc a special treatment
        1.03345f,     0.865603f,
    };
    for (size_t idx = 0; idx < static_cast<size_t>(BasicWave::Last); ++idx)
    {
        WaveTableSet& vwtbl = WaveTableStore::getTableSet(static_cast<BasicWave>(idx));

        float absMax = 0;
        for (size_t j = 0; j < vwtbl.tables.size(); ++j)
        {
            for (size_t s = 0; s < vwtbl.tables[j].data.size(); ++s)
            {
                absMax = std::max(absMax, std::abs(vwtbl.tables[j].data.data()[s]));
            }
        }
        EXPECT_NEAR(expected[idx], absMax, 1E-4f);
    }
}

TEST(WaveTableStorageTest, noiseFloorAndWhiteTablesAreNotTheSameNoise)
{
    const auto& floorTable = WaveTableStore::getTableSet(BasicWave::NoiseFloor).tables.front().data;
    const auto& whiteTable = WaveTableStore::getTableSet(BasicWave::White).tables.front().data;

    double dot = 0.0;
    double energyFloor = 0.0;
    double energyWhite = 0.0;
    for (size_t i = 0; i < floorTable.size(); ++i)
    {
        dot += static_cast<double>(floorTable[i]) * static_cast<double>(whiteTable[i]);
        energyFloor += static_cast<double>(floorTable[i]) * static_cast<double>(floorTable[i]);
        energyWhite += static_cast<double>(whiteTable[i]) * static_cast<double>(whiteTable[i]);
    }
    EXPECT_LT(std::abs(dot) / std::sqrt(energyFloor * energyWhite), 0.2);
}

}
