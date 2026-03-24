#include "Numbers/TimeDistanceSmoother.h"

#include <gtest/gtest.h>

namespace AbacDsp::Test
{

TEST(TimeDistanceSmootherTest, writeHeadVariableRatio)
{
    static constexpr size_t BufferSize = 4096;
    static constexpr size_t InternalBlockSize = 16;
    static constexpr float TargetDistance = 512.f;
    static constexpr float kSR = 48000.f;

    TimeDistanceSmoother smoother(kSR);
    smoother.setWrapPosition(static_cast<float>(BufferSize));
    smoother.setCorrectionTime(0.005f);
    smoother.forceReadPositionDistance(TargetDistance);

    auto simulateProducedFrames = [](const float ratio, double& accumulator) -> size_t
    {
        accumulator += InternalBlockSize * ratio;
        const size_t frames = static_cast<size_t>(accumulator);
        accumulator -= static_cast<double>(frames);
        return frames;
    };

    size_t writeHead = 0;
    const float ratios[] = {
        1.0f,
        1.05946f, // +1 semitone
        1.12246f, // +2 semitones
        0.94387f, // -1 semitone
        0.89090f, // -2 semitones
        1.33484f, // +5 semitones
        1.49831f, // +7 semitones (fifth)
        0.74915f, // -5 semitones
        2.0f,     // +1 octave
        0.5f,     // -1 octave
        1.0f,
    };

    for (const float ratio : ratios)
    {
        static constexpr int blocksPerPhase = 500;
        double acc = 0.0;
        for (int block = 0; block < blocksPerPhase; ++block)
        {
            const size_t produced = simulateProducedFrames(ratio, acc);
            writeHead = (writeHead + produced) % BufferSize;
            smoother.setCurrentWritePosition(static_cast<float>(writeHead), ratio);
            for (size_t i = 0; i < InternalBlockSize; ++i)
                smoother.advancePosition();
        }

        const float delta = smoother.getCurrentDelta();
        auto wrappedDelta = [](float d, float wrap) -> float
        {
            if (d > wrap * 0.5f)
                d -= wrap;
            if (d < -wrap * 0.5f)
                d += wrap;
            return d;
        };
        const float wd = wrappedDelta(delta, static_cast<float>(BufferSize));
        // std::cout << "ratio=" << ratio << " writeHead=" << writeHead;
        // std::cout << " rawDelta=" << delta << " wrappedDelta=" << wd;
        // std::cout << " deviation=" << (wd - TargetDistance) << "\n";

        EXPECT_NEAR(wd, TargetDistance, 1.f) << "ratio=" << ratio << " delta=" << delta;
    }
}

TEST(TimeDistanceSmootherTest, delayWrapBoundary)
{
    static constexpr size_t BufferSize = 4096;
    static constexpr size_t InternalBlockSize = 16;
    static constexpr float TargetDistance = 512.f;
    static constexpr float kSR = 48000.f;

    TimeDistanceSmoother smoother(kSR);
    smoother.setWrapPosition(static_cast<float>(BufferSize));
    smoother.setCorrectionTime(0.005f);

    const size_t initialWrite = BufferSize - 8;
    smoother.setCurrentWritePosition(static_cast<float>(initialWrite), 1.f);
    smoother.forceReadPositionDistance(TargetDistance);

    size_t writeHead = initialWrite;
    static constexpr int totalBlocks = 1000;
    float maxDeltaDeviation = 0.f;
    int maxDeviationBlock = 0;

    for (int block = 0; block < totalBlocks; ++block)
    {
        writeHead = (writeHead + InternalBlockSize) % BufferSize;
        smoother.setCurrentWritePosition(static_cast<float>(writeHead), 1.f);
        for (size_t i = 0; i < InternalBlockSize; ++i)
            smoother.advancePosition();

        if (block > 200)
        {
            float d = smoother.getCurrentDelta();
            if (d > static_cast<float>(BufferSize) * 0.5f)
                d -= static_cast<float>(BufferSize);
            const float dev = std::abs(d - TargetDistance);
            if (dev > maxDeltaDeviation)
            {
                maxDeltaDeviation = dev;
                maxDeviationBlock = block;
            }
        }
    }

    std::cout << "wrapBoundary: maxDeviation=" << maxDeltaDeviation << " at block=" << maxDeviationBlock
              << " writeHead=" << writeHead << "\n";

    EXPECT_LT(maxDeltaDeviation, 20.f);
}

TEST(TimeDistanceSmootherTest, driftDoubleVsFloat)
{
    static constexpr double kWrap = 4096.0;
    const float ratios[] = {
        1.0f,
        1.05946f, // +1 semitone
        1.12246f, // +2 semitones
        0.94387f, // -1 semitone
        0.89090f, // -2 semitones
        1.33484f, // +5 semitones
        1.49831f, // +7 semitones (fifth)
        0.74915f, // -5 semitones
        2.0f,     // +1 octave
        0.5f,     // -1 octave
        1.0f,
    };
    for (auto r : ratios)
    {
        auto makeSmoother = [&]<typename T>(float ratio)
        {
            TimeDistanceSmoother<T> s(48000.0);
            s.setCurrentWritePosition(1024.f, ratio);
            s.setWrapPosition(static_cast<float>(kWrap));
            s.setCorrectionTime(0.01f);
            s.setDistanceCorrectionThreshold(0.1);
            return s;
        };

        auto sff = makeSmoother.operator()<float>(r);
        auto sfd = makeSmoother.operator()<double>(r);
        auto sfld = makeSmoother.operator()<long double>(r);

        long double deltaError1{0};
        long double deltaError2{0};
        float stopValue{0};

        auto wrappedNear = [](long double v, const long double wrap) -> long double
        {
            if (v > wrap * 0.5f)
            {
                v -= wrap;
            }
            if (v < -wrap * 0.5f)
            {
                v += wrap;
            }
            return v;
        };
        for (size_t i = 0; i < 1000; ++i)
        {
            sff.advancePosition();
            sfd.advancePosition();
            sfld.advancePosition();
            deltaError1 += wrappedNear(std::abs(sff.getCurrentDelta() - sfd.getCurrentDelta()), kWrap);
            deltaError2 += wrappedNear(std::abs(sfd.getCurrentDelta() - sfld.getCurrentDelta()), kWrap);
            if (sff.getCurrentDelta() == stopValue)
            {
                EXPECT_EQ(i, 481);
                break;
            }
            stopValue = sff.getCurrentDelta();
        }

        EXPECT_NEAR(wrappedNear(stopValue, static_cast<float>(kWrap)), 0, 1E-1);
        EXPECT_GT(deltaError1, 0.01);
        EXPECT_LT(deltaError1, 1.0);
        EXPECT_LT(deltaError2, 1E-7);
    }
}

}
