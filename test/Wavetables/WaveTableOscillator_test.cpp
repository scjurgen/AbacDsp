#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Wavetables/WaveTableOscillator.h"

namespace AbacDsp::Test
{

TEST(WaveTableOscillator, pwmAt20Percent)
{
    constexpr size_t sampleRate{48000};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::Square);
    osc.setPwm(0.2f);
    const std::vector expected{
        0.000488323f,  -0.186704f,    -0.302582f,    -0.302883f,    -0.187494f,    -0.000488323f, 0.186704f,
        0.302582f,     0.302883f,     0.187494f,     0.000488085f,  -0.186705f,    -0.302582f,    -0.302883f,
        -0.187494f,    -0.000488085f, 0.186705f,     0.302582f,     0.302883f,     0.187493f,     0.000487846f,
        -0.186705f,    -0.302582f,    -0.302883f,    -0.187493f,    -0.000487846f, 0.186705f,     0.302582f,
        0.302883f,     0.187493f,     0.000487608f,  -0.186705f,    -0.302582f,    -0.302883f,    -0.187493f,
        -0.000487608f, 0.186705f,     0.302582f,     0.302883f,     0.187493f,     0.000487369f,  -0.186705f,
        -0.302582f,    -0.302883f,    -0.187493f,    -0.000487369f, 0.186705f,     0.302582f,     0.302883f,
        0.187493f,     0.000487131f,  -0.186705f,    -0.302582f,    -0.302883f,    -0.187493f,    -0.000487131f,
        0.186705f,     0.302582f,     0.302883f,     0.187493f,     0.000486892f,  -0.186705f,    -0.302582f,
        -0.302883f,    -0.187493f,    -0.000486892f, 0.186706f,     0.302582f,     0.302883f,     0.187492f,
        0.000486654f,  -0.186706f,    -0.302582f,    -0.302883f,    -0.187492f,    -0.000486654f, 0.186706f,
        0.302582f,     0.302883f,     0.187492f,     0.000486416f,  -0.186706f,    -0.302582f,    -0.302883f,
        -0.187492f,    -0.000486416f, 0.186706f,     0.302582f,     0.302883f,     0.187492f,     0.000486177f,
        -0.186706f,    -0.302582f,    -0.302883f,    -0.187492f,    -0.000486177f, 0.186706f,     0.302582f,
        0.302883f,     0.187492f};
    constexpr int numSamples = 100;
    std::vector<float> soundBuf(numSamples);

    osc.setFrequency(4800);
    std::vector<float> tmp(numSamples);
    osc.processBlock(tmp.data(), tmp.size());
    std::transform(tmp.data(), tmp.data() + numSamples, soundBuf.data(), [](const float in) { return 0.5f * in; });
    for (size_t i = 0; i < soundBuf.size(); ++i)
    {
        EXPECT_NEAR(soundBuf[i], expected[i], 1E-6f) << "failed at sample " << i;
    }
}

TEST(WaveTableOscillator, setFrequencySameValueIsNoop)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator oscA{sampleRate};
    WaveTableOscillator oscB{sampleRate};
    oscA.setFrequency(440.f);
    oscB.setFrequency(440.f);

    std::vector<float> bufA(64);
    oscA.processBlock(bufA.data(), bufA.size());

    oscB.setFrequency(440.f); // redundant call must take the early-return branch
    std::vector<float> bufB(64);
    oscB.processBlock(bufB.data(), bufB.size());

    for (size_t i = 0; i < bufA.size(); ++i)
    {
        EXPECT_FLOAT_EQ(bufA[i], bufB[i]) << "failed at sample " << i;
    }
}

TEST(WaveTableOscillator, changeFrequencySameValueIsNoop)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator oscA{sampleRate};
    WaveTableOscillator oscB{sampleRate};
    oscA.setFrequency(440.f);
    oscB.setFrequency(440.f);
    oscB.changeFrequency(440.f); // redundant call must take the early-return branch

    std::vector<float> bufA(64);
    std::vector<float> bufB(64);
    oscA.processBlock(bufA.data(), bufA.size());
    oscB.processBlock(bufB.data(), bufB.size());

    for (size_t i = 0; i < bufA.size(); ++i)
    {
        EXPECT_FLOAT_EQ(bufA[i], bufB[i]) << "failed at sample " << i;
    }
}

TEST(WaveTableOscillator, changeFrequencyAltersPitch)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::Saw);
    osc.setMorph(-1.f); // pure waveset 0
    osc.setFrequency(220.f);

    std::vector<float> before(960);
    osc.processBlock(before.data(), before.size());

    osc.changeFrequency(440.f);
    std::vector<float> after(960);
    osc.processBlock(after.data(), after.size());

    const auto countSignChanges = [](const std::vector<float>& buf)
    {
        size_t crossings = 0;
        for (size_t i = 1; i < buf.size(); ++i)
        {
            if ((buf[i - 1] < 0.f) != (buf[i] < 0.f))
            {
                ++crossings;
            }
        }
        return crossings;
    };

    EXPECT_GT(countSignChanges(after), countSignChanges(before));
}

TEST(WaveTableOscillator, blockExactlyMatchingPeriodWrapsCleanly)
{
    constexpr float sampleRate{8192.f};
    WaveTableOscillator osc{sampleRate};
    osc.setFrequency(128.f); // period is exactly 64 samples at this rate

    std::vector<float> firstPeriod(64);
    osc.processBlock(firstPeriod.data(), firstPeriod.size());
    for (const float v : firstPeriod)
    {
        EXPECT_TRUE(std::isfinite(v));
    }

    std::vector<float> secondPeriod(64);
    osc.processBlock(secondPeriod.data(), secondPeriod.size());
    for (size_t i = 0; i < firstPeriod.size(); ++i)
    {
        EXPECT_NEAR(firstPeriod[i], secondPeriod[i], 1E-5f) << "failed at sample " << i;
    }
}

TEST(WaveTableOscillator, pwmOffsetWrapsExactlyAtBlockBoundaries)
{
    constexpr float sampleRate{8192.f};
    WaveTableOscillator osc{sampleRate};
    osc.setPwmMode(WaveTableOscillator::PwmMode::Soft);
    osc.setFrequency(128.f); // period is exactly 64 samples at this rate

    // Block exactly matching one period: the pwm offset phasor reaches 1.0f
    // at the very last sample of the "samplesUntilWrap >= numSamples" branch.
    std::vector<float> onePeriod(64);
    osc.processBlock(onePeriod.data(), onePeriod.size());
    for (const float v : onePeriod)
    {
        EXPECT_TRUE(std::isfinite(v));
    }

    // Block spanning two periods: the pwm offset phasor wraps mid-block,
    // inside the "samplesUntilWrap > 1" branch of the wrap-handling else path.
    std::vector<float> twoPeriods(128);
    osc.processBlock(twoPeriods.data(), twoPeriods.size());
    for (const float v : twoPeriods)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

TEST(WaveTableOscillator, setMorphClampIsContinuousAtUpperBound)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator oscBelow{sampleRate};
    WaveTableOscillator oscAt{sampleRate};
    oscBelow.setFrequency(440.f);
    oscAt.setFrequency(440.f);
    oscBelow.setMorph(0.999999f);
    oscAt.setMorph(1.f); // scaled index hits the size()-1 clamp branch

    std::vector<float> bufBelow(32);
    std::vector<float> bufAt(32);
    oscBelow.processBlock(bufBelow.data(), bufBelow.size());
    oscAt.processBlock(bufAt.data(), bufAt.size());

    for (size_t i = 0; i < bufBelow.size(); ++i)
    {
        EXPECT_NEAR(bufBelow[i], bufAt[i], 1E-3f) << "failed at sample " << i;
    }
}

TEST(WaveTableOscillator, noiseAddedWhenBothMorphTargetsAreWhite)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::White);
    osc.setWaveset(1, BasicWave::White);
    osc.setMorph(-0.5f); // selects the (0, 1) pair, both white
    osc.setFrequency(220.f);

    std::vector<float> buf(200);
    osc.processBlock(buf.data(), buf.size());

    float maxAbs = 0.f;
    for (const float v : buf)
    {
        ASSERT_TRUE(std::isfinite(v));
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    EXPECT_GT(maxAbs, 0.05f);
    EXPECT_LT(maxAbs, 2.5f);
}

TEST(WaveTableOscillator, noiseAddedWhenOnlyFirstMorphTargetIsWhite)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::White);
    osc.setWaveset(1, BasicWave::Sine);
    osc.setMorph(-0.5f); // selects the (0, 1) pair, only index 0 is white
    osc.setFrequency(220.f);

    std::vector<float> buf(200);
    osc.processBlock(buf.data(), buf.size());

    float maxAbs = 0.f;
    for (const float v : buf)
    {
        ASSERT_TRUE(std::isfinite(v));
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    EXPECT_GT(maxAbs, 0.02f);
}

TEST(WaveTableOscillator, noiseAddedWhenOnlySecondMorphTargetIsWhite)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::Sine);
    osc.setWaveset(1, BasicWave::White);
    osc.setMorph(-0.5f); // selects the (0, 1) pair, only index 1 is white
    osc.setFrequency(220.f);

    std::vector<float> buf(200);
    osc.processBlock(buf.data(), buf.size());

    float maxAbs = 0.f;
    for (const float v : buf)
    {
        ASSERT_TRUE(std::isfinite(v));
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    EXPECT_GT(maxAbs, 0.02f);
}

class WaveTableOscillatorPwmModeTest : public ::testing::TestWithParam<WaveTableOscillator::PwmMode>
{
};

TEST_P(WaveTableOscillatorPwmModeTest, coversAllBlockWrapBranches)
{
    constexpr float sampleRate{48000.f};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, BasicWave::Square);
    osc.setPwmMode(GetParam());
    osc.setFrequency(1200.f); // period is 40 samples
    osc.setPwm(0.3f);         // hasStoppedSmoothing() is false for the first calls

    constexpr size_t totalSamples = 400;
    std::vector<float> soundBuf;
    soundBuf.reserve(totalSamples);
    size_t idx = 0;
    while (idx < totalSamples)
    {
        size_t inc = 1 + rand() % 50; // includes 1-sample blocks to hit the tight-wrap branch
        if (idx + inc > totalSamples)
        {
            inc = totalSamples - idx;
        }
        std::vector<float> tmp(inc);
        osc.processBlock(tmp.data(), tmp.size());
        for (const float v : tmp)
        {
            EXPECT_TRUE(std::isfinite(v));
            soundBuf.push_back(v);
        }
        idx += inc;
    }
    ASSERT_EQ(soundBuf.size(), totalSamples);
}

INSTANTIATE_TEST_SUITE_P(WaveTableOscillatorPwmModes, WaveTableOscillatorPwmModeTest,
                         ::testing::Values(WaveTableOscillator::PwmMode::Soft, WaveTableOscillator::PwmMode::Strong));

struct OscTestParams
{
    std::string namedSet;
    int waveset;
    std::vector<float> expected;
};

class WaveTableOscillatorParamTest : public ::testing::TestWithParam<OscTestParams>
{
};

inline void PrintTo(const OscTestParams& tcfg, std::ostream* os)
{
    *os << "failed with wave form " << tcfg.namedSet;
}

TEST_P(WaveTableOscillatorParamTest, correctWave)
{
    constexpr size_t sampleRate{48000};
    WaveTableOscillator osc{sampleRate};
    osc.setWaveset(0, static_cast<BasicWave>(GetParam().waveset));
    osc.setPwm(0.f);
    osc.setMorph(-1.f); // pure waveset 0, so setWaveset(0, ...) above actually matters
    const std::vector<float> frequencySet{220.f, 400.f, 800.f, 1900.f, 3000.f};
    const auto& expected = GetParam().expected;
    std::vector<float> soundBuf;
    size_t pushIdx = 0;
    for (const auto& f : frequencySet)
    {
        constexpr auto numSamples = 500u;
        osc.setFrequency(f);
        size_t idx = 0;
        while (idx < numSamples)
        {
            size_t randInc = 2 + rand() % 100;
            if (idx + randInc > numSamples)
            {
                randInc = numSamples - idx;
            }
            std::vector<float> tmp(randInc);
            osc.processBlock(tmp.data(), tmp.size());
            for (const float v : tmp)
            {
                if (pushIdx++ % 10 == 0) // thin out test data
                {
                    soundBuf.push_back(v * 0.5f);
                }
            }
            idx += randInc;
        }
    }
    ASSERT_EQ(soundBuf.size(), 250);
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(soundBuf[i], expected[i], 1E-6f) << "failed at sample " << i;
    }
}

static std::vector testParamsOscTestParams{
    {.namedSet = "Sine",
     .waveset = 0,
     .expected =
         {
             6.02393e-08f, -0.100439f,  -0.192606f,  -0.268909f,  -0.323065f,    -0.350614f, -0.349286f,  -0.31919f,
             -0.262806f,   -0.184776f,  -0.0915291f, 0.00925641f, 0.109281f,     0.200304f,  0.27483f,    0.326721f,
             0.351703f,    0.347717f,   0.315093f,   0.256518f,   0.176816f,     0.0825506f, -0.0185135f, -0.118052f,
             -0.207869f,   -0.280564f,  -0.330153f,  -0.35255f,   -0.345911f,    -0.310782f, -0.250057f,  -0.168738f,
             -0.073521f,   0.027751f,   0.126738f,   0.215287f,   0.286104f,     0.333358f,  0.353155f,   0.343867f,
             0.306256f,    0.243422f,   0.16054f,    0.0644352f,  -0.0369762f,   -0.135342f, -0.222561f,  -0.291449f,
             -0.336334f,   -0.353518f,  -0.341587f,  -0.250053f,  -0.0915186f,   0.0915385f, 0.250068f,   0.341592f,
             0.341587f,    0.250054f,   0.0915196f,  -0.0915376f, -0.250068f,    -0.341592f, -0.341587f,  -0.250054f,
             -0.0915198f,  0.0915373f,  0.250067f,   0.341591f,   0.341587f,     0.250055f,  0.0915207f,  -0.0915363f,
             -0.250067f,   -0.341591f,  -0.341587f,  -0.250055f,  -0.0915211f,   0.091536f,  0.250066f,   0.341591f,
             0.341588f,    0.250056f,   0.091522f,   -0.0915351f, -0.250066f,    -0.341591f, -0.341588f,  -0.250056f,
             -0.0915223f,  0.0915347f,  0.250065f,   0.341591f,   0.341588f,     0.250057f,  0.0915233f,  -0.0915338f,
             -0.250065f,   -0.341591f,  -0.341588f,  -0.250057f,  -0.0915236f,   0.250064f,  0.341588f,   0.0915244f,
             -0.250064f,   -0.341588f,  -0.0915241f, 0.250064f,   0.341588f,     0.0915249f, -0.250064f,  -0.341589f,
             -0.0915246f,  0.250064f,   0.341589f,   0.0915254f,  -0.250063f,    -0.341589f, -0.0915251f, 0.250063f,
             0.341589f,    0.0915259f,  -0.250063f,  -0.341589f,  -0.0915256f,   0.250063f,  0.341589f,   0.0915265f,
             -0.250062f,   -0.341589f,  -0.0915261f, 0.250063f,   0.341589f,     0.091527f,  -0.250062f,  -0.341589f,
             -0.0915266f,  0.250062f,   0.341589f,   0.0915275f,  -0.250062f,    -0.341589f, -0.0915271f, 0.250062f,
             0.341589f,    0.091528f,   -0.250061f,  -0.341589f,  -0.0915277f,   0.250061f,  0.341589f,   -0.326721f,
             0.17682f,     0.0461591f,  -0.250061f,  0.350614f,   -0.306261f,    0.135332f,  0.0915288f,  -0.280561f,
             0.35364f,     -0.28056f,   0.0915282f,  0.135333f,   -0.306261f,    0.350614f,  -0.25006f,   0.0461583f,
             0.17682f,     -0.326721f,  0.341589f,   -0.215282f,  -1.32641e-06f, 0.215283f,  -0.34159f,   0.32672f,
             -0.176819f,   -0.0461608f, 0.250062f,   -0.350614f,  0.30626f,      -0.135331f, -0.0915304f, 0.280562f,
             -0.35364f,    0.280559f,   -0.0915266f, -0.135334f,  0.306262f,     -0.350614f, 0.250059f,   -0.0461566f,
             -0.176822f,   0.326722f,   -0.341589f,  0.21528f,    3.11184e-06f,  -0.215284f, 0.34159f,    -0.326719f,
             0.176817f,    -0.341588f,  0.306262f,   -0.0915318f, -0.176817f,    0.341588f,  -0.306262f,  0.0915318f,
             0.176817f,    -0.341588f,  0.306262f,   -0.0915318f, -0.176817f,    0.341588f,  -0.306262f,  0.0915318f,
             0.176817f,    -0.341588f,  0.306262f,   -0.0915318f, -0.176817f,    0.341588f,  -0.306262f,  0.0915318f,
             0.176817f,    -0.341588f,  0.306262f,   -0.0915318f, -0.176817f,    0.341588f,  -0.306262f,  0.0915318f,
             0.176817f,    -0.341588f,  0.306262f,   -0.0915318f, -0.176817f,    0.341588f,  -0.306262f,  0.0915318f,
             0.176817f,    -0.341588f,
         }},
    OscTestParams{.namedSet = "Triangle",
                  .waveset = 1,
                  .expected =
                      {
                          0.429887f,   0.353286f,   0.274112f,    0.194824f,   0.115413f,   0.0360178f,  -0.0432801f,
                          -0.122573f,  -0.201971f,  -0.281412f,   -0.360689f,  -0.426505f,  -0.346226f,  -0.266809f,
                          -0.187548f,  -0.108252f,  -0.0288516f,  0.0505449f,  0.129844f,   0.209123f,   0.288531f,
                          0.368087f,   0.418711f,   0.339168f,    0.259673f,   0.180268f,   0.100983f,   0.0216847f,
                          -0.0577104f, -0.137113f,  -0.216406f,   -0.295649f,  -0.375063f,  -0.41064f,   -0.331822f,
                          -0.252543f,  -0.173114f,  -0.0937163f,  -0.0144208f, 0.0648764f,  0.144273f,   0.223691f,
                          0.302975f,   0.382059f,   0.403821f,    0.324467f,   0.245249f,   0.165958f,   0.0865513f,
                          0.00715637f, -0.0719397f, -0.21643f,    -0.361262f,  -0.361248f,  -0.216415f,  -0.0719241f,
                          0.0719389f,  0.21643f,    0.361261f,    0.361249f,   0.216415f,   0.0719244f,  -0.0719388f,
                          -0.216429f,  -0.361261f,  -0.361249f,   -0.216415f,  -0.071925f,  0.071938f,   0.216429f,
                          0.36126f,    0.36125f,    0.216416f,    0.0719254f,  -0.0719378f, -0.216428f,  -0.36126f,
                          -0.36125f,   -0.216417f,  -0.071926f,   0.071937f,   0.216428f,   0.361259f,   0.361251f,
                          0.216417f,   0.0719264f,  -0.0719367f,  -0.216427f,  -0.361259f,  -0.361251f,  -0.216418f,
                          -0.0719271f, 0.071936f,   0.216426f,    0.361258f,   0.361252f,   0.216418f,   0.0719274f,
                          -0.0719357f, -0.216426f,  -0.362344f,   -0.21665f,   0.0721897f,  0.362343f,   0.21665f,
                          -0.0721895f, -0.362344f,  -0.21665f,    0.0721893f,  0.362343f,   0.216651f,   -0.072189f,
                          -0.362343f,  -0.21665f,   0.0721888f,   0.362342f,   0.216651f,   -0.0721886f, -0.362343f,
                          -0.216651f,  0.0721884f,  0.362342f,    0.216651f,   -0.0721882f, -0.362342f,  -0.216651f,
                          0.0721879f,  0.362341f,   0.216652f,    -0.0721877f, -0.362342f,  -0.216652f,  0.0721875f,
                          0.362341f,   0.216652f,   -0.0721873f,  -0.362341f,  -0.216652f,  0.0721871f,  0.36234f,
                          0.216653f,   -0.0721869f, -0.362341f,   -0.216653f,  0.0721866f,  0.36234f,    0.216653f,
                          -0.0721864f, -0.36234f,   -0.216653f,   0.0767896f,  0.111203f,   -0.291663f,  0.394958f,
                          -0.210578f,  0.0394161f,  0.143445f,    -0.333659f,  0.370055f,   -0.175719f,  -3.76318e-07f,
                          0.175719f,   -0.370057f,  0.333657f,    -0.143444f,  -0.0394169f, 0.210579f,   -0.39496f,
                          0.291661f,   -0.111203f,  -0.0767903f,  0.249493f,   -0.403829f,  0.249489f,   -0.0767883f,
                          -0.111204f,  0.291664f,   -0.394958f,   0.210577f,   -0.0394148f, -0.143446f,  0.33366f,
                          -0.370054f,  0.175717f,   1.79976e-06f, -0.17572f,   0.370058f,   -0.333655f,  0.143443f,
                          0.0394183f,  -0.21058f,   0.394961f,    -0.291659f,  0.111201f,   0.0767916f,  -0.249494f,
                          0.403829f,   -0.249488f,  0.0767869f,   0.111206f,   -0.303818f,  0.0632375f,  0.136425f,
                          -0.366422f,  0.303818f,   -0.0632375f,  -0.136425f,  0.366422f,   -0.303818f,  0.0632375f,
                          0.136425f,   -0.366422f,  0.303818f,    -0.0632375f, -0.136425f,  0.366422f,   -0.303818f,
                          0.0632375f,  0.136425f,   -0.366422f,   0.303818f,   -0.0632375f, -0.136425f,  0.366422f,
                          -0.303818f,  0.0632375f,  0.136425f,    -0.366422f,  0.303818f,   -0.0632375f, -0.136425f,
                          0.366422f,   -0.303818f,  0.0632375f,   0.136425f,   -0.366422f,  0.303818f,   -0.0632375f,
                          -0.136425f,  0.366422f,   -0.303818f,   0.0632375f,
                      }},
    OscTestParams{
        .namedSet = "SharkFin",
        .waveset = 3,
        .expected =
            {
                -0.413257f,  -0.389867f,   -0.350118f,  -0.309455f,  -0.267656f,  -0.225784f,  -0.184623f,  -0.143767f,
                -0.102442f,  -0.0607065f,  -0.0192543f, 0.0217187f,  0.0628084f,  0.104427f,   0.14611f,    0.187224f,
                0.228038f,   0.269446f,    0.311546f,   0.353141f,   0.39293f,    0.378328f,   -0.427904f,  -0.385364f,
                -0.342682f,  -0.300702f,   -0.259808f,  -0.219021f,  -0.177547f,  -0.135758f,  -0.0944155f, -0.0534934f,
                -0.0123035f, 0.0293699f,   0.0709438f,  0.111985f,   0.152943f,   0.194473f,   0.236329f,   0.277593f,
                0.318111f,   0.3591f,      0.40213f,    0.197711f,   -0.416704f,  -0.375189f,  -0.335045f,  -0.29436f,
                -0.252637f,  -0.210782f,   -0.16932f,   -0.0950142f, -0.0197936f, 0.0560543f,  0.130826f,   0.205249f,
                0.281226f,   0.357826f,    0.403426f,   -0.395714f,  -0.32125f,   -0.24469f,   -0.16932f,   -0.0950147f,
                -0.0197941f, 0.0560538f,   0.130826f,   0.205248f,   0.281225f,   0.357826f,   0.40343f,    -0.395714f,
                -0.32125f,   -0.24469f,    -0.169321f,  -0.0950152f, -0.0197947f, 0.0560532f,  0.130825f,   0.205248f,
                0.281225f,   0.357825f,    0.403435f,   -0.395714f,  -0.321251f,  -0.244691f,  -0.169321f,  -0.0950157f,
                -0.0197953f, 0.0560527f,   0.130825f,   0.205247f,   0.281224f,   0.357825f,   0.403439f,   -0.395714f,
                -0.321252f,  -0.244691f,   -0.169322f,  -0.0950162f, -0.0122965f, 0.138111f,   0.287667f,   0.350702f,
                -0.316311f,  -0.163148f,   -0.0122967f, 0.138111f,   0.287667f,   0.350705f,   -0.316311f,  -0.163148f,
                -0.0122969f, 0.13811f,     0.287667f,   0.350708f,   -0.316312f,  -0.163149f,  -0.0122971f, 0.13811f,
                0.287667f,   0.35071f,     -0.316312f,  -0.163149f,  -0.0122974f, 0.13811f,    0.287667f,   0.350713f,
                -0.316313f,  -0.163149f,   -0.0122976f, 0.13811f,    0.287667f,   0.350716f,   -0.316313f,  -0.16315f,
                -0.0122978f, 0.13811f,     0.287667f,   0.350718f,   -0.316314f,  -0.16315f,   -0.0122981f, 0.13811f,
                0.287667f,   0.350721f,    -0.316314f,  -0.16315f,   -0.0122983f, 0.13811f,    0.239287f,   -0.235653f,
                0.0819239f,  0.00729713f,  -0.0821155f, 0.235934f,   -0.240357f,  0.0808064f,  0.183553f,   -0.0889791f,
                0.242765f,   -0.266316f,   0.0759219f,  0.327385f,   -0.107852f,  0.246476f,   -0.314657f,  0.0606083f,
                0.41705f,    -0.139435f,   0.237039f,   -0.374905f,  0.0333872f,  0.445986f,   -0.178092f,  0.211612f,
                -0.426708f,  -0.00144562f, 0.42345f,    -0.214164f,  0.174955f,   -0.445782f,  -0.0359345f, 0.370078f,
                -0.238398f,  0.136537f,    -0.412129f,  -0.0622921f, 0.310137f,   -0.246569f,  0.10584f,    -0.317662f,
                -0.0766355f, 0.263356f,    -0.242163f,  0.0880421f,  -0.170266f,  -0.0809201f, 0.239287f,   -0.235652f,
                0.12316f,    -0.236059f,   0.399166f,   -0.0377664f, -0.396695f,  0.159682f,   -0.125632f,  0.114143f,
                0.12316f,    -0.236059f,   0.399166f,   -0.0377664f, -0.396695f,  0.159682f,   -0.125632f,  0.114143f,
                0.12316f,    -0.236059f,   0.399166f,   -0.0377664f, -0.396695f,  0.159682f,   -0.125632f,  0.114143f,
                0.12316f,    -0.236059f,   0.399166f,   -0.0377664f, -0.396695f,  0.159682f,   -0.125632f,  0.114143f,
                0.12316f,    -0.236059f,   0.399166f,   -0.0377664f, -0.396695f,  0.159682f,   -0.125632f,  0.114143f,
                0.12316f,    -0.236059f,
            }},
    OscTestParams{
        .namedSet = "Pulse",
        .waveset = 5,
        .expected = {
            0.203539f,   -0.126108f, -0.128923f,  -0.124617f,   -0.123063f,  -0.124883f, -0.125894f, -0.124698f,
            -0.123895f,  -0.124932f, -0.12571f,   -0.124583f,   -0.123617f,  -0.125142f, -0.126814f, -0.124116f,
            -0.118848f,  -0.126632f, 0.523104f,   0.498052f,    0.489564f,   0.501067f,  -0.178158f, -0.123818f,
            -0.121087f,  -0.124951f, -0.126434f,  -0.124708f,   -0.123727f,  -0.124892f, -0.125687f, -0.124651f,
            -0.123866f,  -0.125014f, -0.126015f,  -0.124431f,   -0.122634f,  -0.125523f, -0.131778f, -0.124225f,
            0.481836f,   0.501922f,  0.51102f,    0.499422f,    -0.0967966f, -0.125548f, -0.128152f, -0.124654f,
            -0.123236f,  -0.124878f, -0.11664f,   -0.12723f,    -0.131233f,  -0.118286f, -0.120144f, -0.141706f,
            -0.0793298f, 0.493697f,  0.485761f,   -0.101172f,   -0.136911f,  -0.126804f, -0.116639f, -0.127229f,
            -0.131234f,  -0.118286f, -0.120143f,  -0.141705f,   -0.0793424f, 0.493697f,  0.485762f,  -0.10117f,
            -0.13691f,   -0.126805f, -0.116639f,  -0.127228f,   -0.131234f,  -0.118287f, -0.120142f, -0.141705f,
            -0.0793565f, 0.493697f,  0.485764f,   -0.101167f,   -0.13691f,   -0.126806f, -0.116639f, -0.127228f,
            -0.131234f,  -0.118287f, -0.120141f,  -0.141705f,   -0.0793706f, 0.493697f,  0.485766f,  -0.101164f,
            -0.136909f,  -0.126807f, -0.116639f,  -0.127227f,   -0.127393f,  -0.11546f,  0.0633343f, 0.539407f,
            -0.153354f,  -0.136374f, -0.127394f,  -0.11546f,    0.0633307f,  0.539407f,  -0.153355f, -0.136374f,
            -0.127394f,  -0.115461f, 0.0633271f,  0.539407f,    -0.153355f,  -0.136375f, -0.127394f, -0.115461f,
            0.0633236f,  0.539407f,  -0.153355f,  -0.136375f,   -0.127395f,  -0.115462f, 0.06332f,   0.539407f,
            -0.153355f,  -0.136375f, -0.127395f,  -0.115462f,   0.0633164f,  0.539406f,  -0.153355f, -0.136375f,
            -0.127395f,  -0.115462f, 0.0633128f,  0.539406f,    -0.153356f,  -0.136376f, -0.127396f, -0.115463f,
            0.0633092f,  0.539406f,  -0.153356f,  -0.136376f,   -0.127396f,  -0.115463f, 0.107138f,  -0.106899f,
            -0.108021f,  0.296425f,  -0.11616f,   -0.00565568f, -0.117111f,  -0.116886f, 0.424101f,  -0.123646f,
            -0.0900227f, -0.134472f, -0.127574f,  0.527817f,    -0.131945f,  -0.141722f, -0.153098f, -0.135778f,
            0.59199f,    -0.137647f, -0.162936f,  -0.164027f,   -0.138669f,  0.606677f,  -0.13815f,  -0.160846f,
            -0.157036f,  -0.135671f, 0.569575f,   -0.132723f,   -0.145313f,  -0.123118f, -0.128437f, 0.486481f,
            -0.12297f,   -0.126288f, -0.0570314f, -0.120072f,   0.370137f,   -0.112508f, -0.111553f, 0.040691f,
            -0.113942f,  0.237646f,  -0.105869f,  -0.105337f,   0.162904f,   -0.112445f, 0.107129f,  -0.1069f,
            -0.100989f,  -0.112231f, 0.377038f,   -0.135244f,   -0.150633f,  -0.170008f, -0.125416f, 0.417483f,
            -0.100989f,  -0.112231f, 0.377038f,   -0.135244f,   -0.150633f,  -0.170008f, -0.125416f, 0.417483f,
            -0.100989f,  -0.112231f, 0.377038f,   -0.135244f,   -0.150633f,  -0.170008f, -0.125416f, 0.417483f,
            -0.100989f,  -0.112231f, 0.377038f,   -0.135244f,   -0.150633f,  -0.170008f, -0.125416f, 0.417483f,
            -0.100989f,  -0.112231f, 0.377038f,   -0.135244f,   -0.150633f,  -0.170008f, -0.125416f, 0.417483f,
            -0.100989f,  -0.112231f,
        }}};

INSTANTIATE_TEST_SUITE_P(WaveTableOscillatorTests, WaveTableOscillatorParamTest,
                         ::testing::ValuesIn(testParamsOscTestParams));

}
