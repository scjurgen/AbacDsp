#include <tuple>

#include "gtest/gtest.h"

#include "Synthesizer/ValueConnector.h"

namespace AbacDsp::Test
{

namespace
{
enum class CtrlSource
{
    FixValue,
    Key,
    Velocity,
    DimensionX,
    DimensionY,
    Lfo
};
}

TEST(FixedSmoothing, forceTargetJumpsImmediately)
{
    FixedSmoothing<128> sut;
    sut.forceTarget(0.0f);
    sut.setTarget(1.0f);
    EXPECT_GT(sut.getSmoothed(), 0.0f);
    EXPECT_LE(sut.getSmoothed(), 1.0f);
    sut.forceTarget(2.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 2.0f);
}

TEST(FixedSmoothing, rampReachesTargetAfterExactlyStepsCalls)
{
    FixedSmoothing<8> sut;
    sut.forceTarget(0.0f);
    sut.setTarget(1.0f);
    for (int i = 1; i < 8; ++i)
    {
        EXPECT_FLOAT_EQ(sut.getSmoothed(), static_cast<float>(i) / 8.0f);
    }
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 1.0f);
}

TEST(FixedSmoothing, rampHandlesNegativeDeltaSymmetrically)
{
    FixedSmoothing<8> sut;
    sut.forceTarget(1.0f);
    sut.setTarget(-1.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 3.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 2.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 1.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), 0.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), -1.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), -2.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), -3.0f / 4.0f);
    EXPECT_FLOAT_EQ(sut.getSmoothed(), -1.0f);
}

TEST(FixedSmoothing, isActiveReflectsWhetherARampIsInProgress)
{
    FixedSmoothing<8> sut;
    sut.forceTarget(0.0f);
    EXPECT_FALSE(sut.isActive());
    sut.setTarget(1.0f);
    EXPECT_TRUE(sut.isActive());
    for (int i = 0; i < 8; ++i)
    {
        std::ignore = sut.getSmoothed();
    }
    EXPECT_FALSE(sut.isActive());
}

TEST(ValueConnector, evaluatorSeesValuesIndexedByDeclarationOrderNotEnumValue)
{
    using Sut = ValueConnector<128, CtrlSource, CtrlSource::FixValue, CtrlSource::DimensionX>;
    Sut sut{[](const Sut::Values& v) { return v[0] + v[1]; }};
    sut.setForced(CtrlSource::FixValue, 2.0f);
    sut.setForced(CtrlSource::DimensionX, 0.25f);
    EXPECT_FLOAT_EQ(sut.get(), 2.25f);
}

TEST(ValueConnector, idNotInThisInstancesSetIsIgnored)
{
    using Sut = ValueConnector<128, CtrlSource, CtrlSource::FixValue>;
    Sut sut{[](const Sut::Values& v) { return v[0]; }};
    sut.setForced(CtrlSource::FixValue, 1.0f);
    sut.set(CtrlSource::Lfo, 999.0f);
    EXPECT_FLOAT_EQ(sut.get(), 1.0f);
}

TEST(ValueConnector, setRampsWhileSetForcedJumps)
{
    using Sut = ValueConnector<8, CtrlSource, CtrlSource::FixValue>;
    Sut sut{[](const Sut::Values& v) { return v[0]; }};
    sut.setForced(CtrlSource::FixValue, 0.0f);
    sut.set(CtrlSource::FixValue, 1.0f);
    EXPECT_TRUE(sut.hasChangedValue());
    for (int i = 0; i < 8; ++i)
    {
        std::ignore = sut.get();
    }
    EXPECT_FALSE(sut.hasChangedValue());
    EXPECT_FLOAT_EQ(sut.get(), 1.0f);
}

TEST(ValueConnectorWithPreset, presetFastAppliesBothFixedAndVariablePartsImmediately)
{
    using Sut = ValueConnectorWithPreset<128, CtrlSource, CtrlSource::FixValue, CtrlSource::Key, CtrlSource::DimensionX,
                                         CtrlSource::Velocity, CtrlSource::DimensionY>;
    Sut sut{[](const Sut::Values& v) { return v[0] * v[1]; },
            [](const float fixedPart, const Sut::Values& v) { return fixedPart + v[2] + v[3] * v[4]; }};
    sut.set(CtrlSource::FixValue, 0.5f);
    sut.set(CtrlSource::Key, 0.25f);
    sut.set(CtrlSource::DimensionX, 0.125f);
    sut.set(CtrlSource::Velocity, 1.0f);
    sut.set(CtrlSource::DimensionY, 0.75f);
    sut.presetFast();
    EXPECT_FLOAT_EQ(sut.get(), 1.0f);
}

TEST(ValueConnectorWithPreset, setAfterPresetRampsTowardNewVariablePartValue)
{
    using Sut = ValueConnectorWithPreset<128, CtrlSource, CtrlSource::FixValue, CtrlSource::Key, CtrlSource::Lfo>;
    Sut sut{[](const Sut::Values& v) { return v[0] * v[1]; },
            [](const float fixedPart, const Sut::Values& v) { return fixedPart + v[2]; }};

    sut.set(CtrlSource::FixValue, 2.0f);
    sut.set(CtrlSource::Key, 0.25f);
    sut.set(CtrlSource::Lfo, 0.5f);
    sut.presetFast();
    EXPECT_FLOAT_EQ(sut.get(), 1.0f);

    float previous = sut.get();
    sut.set(CtrlSource::Lfo, 0.2f);
    for (int i = 0; i < 128; ++i)
    {
        const float current = sut.get();
        EXPECT_NE(previous, current);
        EXPECT_LT(current, previous);
        previous = current;
    }
    EXPECT_FLOAT_EQ(sut.get(), 0.7f);
}

}
