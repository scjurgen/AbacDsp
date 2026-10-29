#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Analysis/LevelMeter.h"
#include "Analysis/PitchReading.h"

namespace Sonalysis
{

inline constexpr size_t kSpectrumBands{240};
inline constexpr size_t kXyPoints{2048};

struct ChannelLevels
{
    float peakDb{AbacDsp::LevelMeter::kFloorDb};
    float rmsDb{AbacDsp::LevelMeter::kFloorDb};
    float holdDb{AbacDsp::LevelMeter::kFloorDb};
    bool clipped{false};
};

struct LevelSnapshot
{
    std::array<ChannelLevels, 2> channels{};
};

struct SpectrumSnapshot
{
    std::array<float, kSpectrumBands> bandCenters{};
    std::array<float, kSpectrumBands> bandsDb{};
    std::array<float, kSpectrumBands> peakHoldDb{};
    size_t fftSize{0};
    float resolutionHz{0.f};
    bool showPeakHold{false};
};

struct PitchSnapshot
{
    bool valid{false};
    AbacDsp::PitchReading reading{};
    float confidence{0.f};
    float referenceA4{440.f};
};

struct XyPoint
{
    float x{0.f};
    float y{0.f};
};

// Oldest point first.
struct XySnapshot
{
    std::array<XyPoint, kXyPoints> points{};
};

struct BeatSnapshot
{
    bool valid{false};
    float bpm{0.f};
    float jitterMs{0.f};
    size_t beats{0};
    uint64_t onsetCount{0};
};

}
