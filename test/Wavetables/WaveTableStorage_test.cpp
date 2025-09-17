
#include "WaveTables/WaveTableOscillator.h"

#include "gtest/gtest.h"

#include <numbers>
#include <vector>

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
        0.707281f,    0.865605f, 1.05304f, 0.924317f, 0.636625f, 1.21355f, 2.56014f,
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
}
