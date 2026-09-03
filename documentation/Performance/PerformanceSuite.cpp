// Single-thread capacity benchmark: how many instances of each DSP module family can
// this machine run in one thread in real time. See README.md for the methodology.

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "Filters/Biquad.h"
#include "Filters/PoleMixingFilter.h"
#include "Filters/Sinc/sinc_4.h"
#include "Generators/KarplusStrongVoice.h"
#include "Generators/SynthLfo.h"
#include "HtmlReport.h"
#include "PerfConfig.h"
#include "PerfHarness.h"
#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/FdnTankSpicedBase.h"
#include "Sampler/SamplePlayerBasic.h"
#include "SamplerateConverter/SrPushConverter.h"
#include "Synthesizer/MorphexsynthVoice.h"
#include "SystemInfo.h"
#include "Wavetables/WaveTableOscillator.h"

namespace
{

using AbacDsp::Perf::kBlockSize;
using AbacDsp::Perf::kSampleRate;
using AbacDsp::Perf::NoiseBlocks;

/// @brief A slow control-rate LFO, stepped once per block rather than once per sample.
[[nodiscard]] AbacDsp::LfoGenerators makeBlockRateLfo(const float lfoHz)
{
    AbacDsp::LfoGenerators lfo(kSampleRate / static_cast<float>(kBlockSize));
    lfo.setWaveForm(AbacDsp::LfoType::Sine);
    lfo.setFrequency(lfoHz);
    return lfo;
}

// ---- Reverb: FdnTankGlide / FdnTankSpicedBase / FdnTankBlockDelayWalshSIMD, order 8/16/32 ----

// Multiple of kBlockSize (required by FdnTankBlockDelayWalshSIMD). setMaxSize below computes
// to ~48000 samples before prime-length rounding (which can overshoot by up to ~500), leaving
// ~1664 samples of headroom so that rounding can never write past the delay-line array.
constexpr size_t kReverbMaxSizePerElement = 49664;
constexpr float kReverbMinSizeM = 133.f;  // ~19200 samples, ~0.4s, before prime rounding
constexpr float kReverbMaxSizeM = 333.3f; // ~48000 samples, ~1.0s, before prime rounding
constexpr float kReverbDecayMs = 2000.f;

template <size_t Order>
using GlideTankN = AbacDsp::FdnTankGlide<kReverbMaxSizePerElement, Order, kBlockSize>;
template <size_t Order>
using SpicedTankN = AbacDsp::FdnTankSpicedBase<kReverbMaxSizePerElement, Order, kBlockSize>;
template <size_t Order>
using WalshTankN = AbacDsp::FdnTankBlockDelayWalshSIMD<kReverbMaxSizePerElement, Order, kBlockSize>;

template <typename Tank>
struct ReverbSut
{
    Tank tank{kSampleRate};
    std::array<float, kBlockSize> scratch{};

    ReverbSut()
    {
        tank.setMinSize(kReverbMinSizeM);
        tank.setMaxSize(kReverbMaxSizeM);
        tank.setDecay(kReverbDecayMs);
    }
};

template <typename Tank>
void processReverb(ReverbSut<Tank>& sut, const NoiseBlocks& noise)
{
    sut.tank.processBlock(noise.mono.data(), sut.scratch.data());
}

template <typename Tank>
void addReverbResult(std::vector<AbacDsp::Perf::SutResult>& results, const std::string& variant,
                     const NoiseBlocks& noise, const double secondsPerProbe)
{
    results.push_back(AbacDsp::Perf::benchmark(
        "Reverb", variant, [] { return ReverbSut<Tank>{}; }, processReverb<Tank>, noise, secondsPerProbe));
}

// ---- Biquad: LowPass / BandPass / Peak, cutoff swept by an LFO each block ----

template <AbacDsp::BiquadFilterType Type>
struct BiquadSut
{
    AbacDsp::Biquad<Type> filter{};
    AbacDsp::LfoGenerators lfo{makeBlockRateLfo(0.25f)};
    std::array<float, kBlockSize> scratch{};

    BiquadSut()
    {
        filter.computeCoefficients(kSampleRate, 1000.f, 0.707f, 6.f);
    }
};

template <AbacDsp::BiquadFilterType Type>
void processBiquad(BiquadSut<Type>& sut, const NoiseBlocks& noise)
{
    const float sweep = 0.5f + 0.5f * sut.lfo.step();
    sut.filter.computeCoefficients(kSampleRate, 200.f + sweep * 3800.f, 0.707f, 6.f);
    sut.filter.processBlock(noise.mono.data(), sut.scratch.data(), kBlockSize);
}

// ---- PoleMixingFilter family: Lp24Smooth, cutoff swept the same way ----

struct PoleMixingSut
{
    AbacDsp::Lp24Smooth filter{kSampleRate};
    AbacDsp::LfoGenerators lfo{makeBlockRateLfo(0.25f)};
    std::array<float, kBlockSize> scratch{};

    PoleMixingSut()
    {
        filter.setResonance(0.5f);
    }
};

void processPoleMixing(PoleMixingSut& sut, const NoiseBlocks& noise)
{
    const float sweep = 0.5f + 0.5f * sut.lfo.step();
    sut.filter.setCutoff(200.f + sweep * 3800.f);
    sut.filter.processBlock(noise.mono.data(), sut.scratch.data(), kBlockSize);
}

// ---- Wavetables: WaveTableOscillator, frequency swept across ~2 octaves ----

struct OscillatorSut
{
    AbacDsp::WaveTableOscillator osc{kSampleRate};
    AbacDsp::LfoGenerators lfo{makeBlockRateLfo(0.2f)};
    std::array<float, kBlockSize> scratch{};

    OscillatorSut()
    {
        osc.setFrequency(110.f);
    }
};

void processOscillator(OscillatorSut& sut, const NoiseBlocks&)
{
    const float sweep = 0.5f + 0.5f * sut.lfo.step();
    sut.osc.setFrequency(110.f * std::pow(2.f, sweep * 2.f));
    sut.osc.processBlock(sut.scratch.data(), kBlockSize);
}

// ---- Generators: KarplusStrongVoice, plucked string feeding a resonant VCF ----

constexpr size_t kKarplusRetriggerBlocks = 512;

struct KarplusStrongSut
{
    AbacDsp::KarplusStrongVoice<2048> voice{kSampleRate};
    std::array<float, kBlockSize> scratch{};
    size_t blocksSincePluck{0};

    KarplusStrongSut()
    {
        voice.trigger(60.f, 1.f, 440.f);
    }
};

void processKarplusStrong(KarplusStrongSut& sut, const NoiseBlocks&)
{
    if (++sut.blocksSincePluck >= kKarplusRetriggerBlocks)
    {
        sut.blocksSincePluck = 0;
        sut.voice.trigger(60.f, 1.f, 440.f);
    }
    for (auto& s : sut.scratch)
    {
        s = sut.voice.step();
    }
}

// ---- Synthesizer: MorphexsynthVoice, the full ported synth voice ----

constexpr size_t kSynthRetriggerBlocks = 512;

struct SynthVoiceSut
{
    AbacDsp::MorphexsynthVoice voice;
    std::array<float, kBlockSize> left{};
    std::array<float, kBlockSize> right{};
    size_t blocksSinceTrigger{0};

    SynthVoiceSut(const AbacDsp::WaveShaperTableStore& tables, const AbacDsp::MpeCurveMap& curveMap)
        : voice(kSampleRate, tables, curveMap)
    {
        voice.triggerVoice(60, 100, 60);
    }
};

void processSynthVoice(SynthVoiceSut& sut, const NoiseBlocks&)
{
    if (++sut.blocksSinceTrigger >= kSynthRetriggerBlocks)
    {
        sut.blocksSinceTrigger = 0;
        sut.voice.triggerVoice(60, 100, 60);
    }
    sut.voice.processBlock(sut.left.data(), sut.right.data(), kBlockSize);
}

// ---- Diffuser: DiffuserDelayChain, 4-stage Schroeder chain ----

using DiffuserChain = AbacDsp::DiffuserDelayChain<2048, 4, AbacDsp::AllpassFeedbackStyle::Schroeder>;

// The chain holds std::atomic level-meter members (see its own doc comment), so it is
// neither copyable nor movable - own it through a unique_ptr so DiffuserSut itself stays
// movable, which std::vector<DiffuserSut> in the harness requires.
struct DiffuserSut
{
    std::unique_ptr<DiffuserChain> chain{std::make_unique<DiffuserChain>(kSampleRate, kBlockSize)};
    std::array<float, kBlockSize> scratch{};

    DiffuserSut()
    {
        chain->resetDiffuser(4, 0.5f, 0.5f, 0.7f, 7.f, AbacDsp::skipSmoothing);
    }
};

void processDiffuser(DiffuserSut& sut, const NoiseBlocks& noise)
{
    sut.chain->processBlock(noise.mono.data(), sut.scratch.data(), kBlockSize);
}

// ---- PitchShift: BlockProc::Pitch, crossfade engine vs. phase-vocoder engine ----

template <bool UsePhaseVocoder>
struct PitchShiftSut
{
    AbacDsp::BlockProc::Pitch<kBlockSize> pitcher{kSampleRate};
    std::array<float, kBlockSize> buffer{};

    PitchShiftSut()
    {
        if constexpr (UsePhaseVocoder)
        {
            pitcher.setPhaseVocoderEnabled(true);
        }
        pitcher.setPitch(5.f);
    }
};

template <bool UsePhaseVocoder>
void processPitchShift(PitchShiftSut<UsePhaseVocoder>& sut, const NoiseBlocks& noise)
{
    sut.buffer = noise.mono;
    sut.pitcher.process(sut.buffer);
}

// ---- Analysis: YinPitchDetector ----

struct YinSut
{
    AbacDsp::YinPitchDetector detector{kSampleRate};
    std::array<float, kBlockSize> scratch{};
};

void processYin(YinSut& sut, const NoiseBlocks& noise)
{
    sut.detector.processBlock(std::span<const float>{noise.mono}, std::span<float>{sut.scratch});
}

// ---- Sample players: SamplePlayerBasic looping a shared noise sample ----

struct SamplePlayerSut
{
    AbacDsp::SamplePlayerBasic player{kSampleRate};
    AbacDsp::LfoGenerators lfo{makeBlockRateLfo(0.3f)};
    std::array<float, kBlockSize> left{};
    std::array<float, kBlockSize> right{};

    explicit SamplePlayerSut(const std::shared_ptr<std::vector<float>>& sampleData)
    {
        player.runStereo(sampleData);
        player.setLoop(true);
        player.setPlaybackRate(1.f);
    }
};

void processSamplePlayer(SamplePlayerSut& sut, const NoiseBlocks&)
{
    const float sweep = sut.lfo.step();
    sut.player.setPlaybackRate(1.f + sweep * 0.05f);
    sut.left.fill(0.f);
    sut.right.fill(0.f);
    sut.player.processBlock(sut.left.data(), sut.right.data(), kBlockSize);
}

// ---- Up/down sampling: SrPushConverter at fixed ratios ----

struct ResamplerSut
{
    AbacDsp::SrPushConverter<2> converter;
    std::array<float, 4 * kBlockSize> scratch{};

    explicit ResamplerSut(const std::shared_ptr<AbacDsp::SincFilter>& sincFilter)
        : converter(sincFilter)
    {
    }
};

template <bool Upsample>
void processResample(ResamplerSut& sut, const NoiseBlocks& noise)
{
    constexpr float ratio = Upsample ? (48000.f / 44100.f) : (44100.f / 48000.f);
    static_cast<void>(sut.converter.fetchBlock(ratio, noise.interleavedStereo.data(), kBlockSize, sut.scratch.data(),
                                               sut.scratch.size()));
}

void printUsage()
{
    std::cout << "Usage: PerformanceSuite --comment TEXT [OPTIONS]\n"
                 "Options:\n"
                 "  --comment TEXT  Free-text note embedded in the report header (mandatory)\n"
                 "  --seconds N     Measurement window per probe, in seconds (default: 0.3)\n"
                 "  --out PATH      Report output path (default: generated/performance_report.html)\n"
                 "  -h, --help      Display this help message and exit\n";
}

}

int main(const int argc, char* argv[])
{
    double secondsPerProbe = 0.3;
    std::optional<std::string> comment;
    std::string outPath = "generated/performance_report.html";

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--seconds" && i + 1 < argc)
        {
            secondsPerProbe = std::stod(argv[++i]);
        }
        else if (arg == "--comment" && i + 1 < argc)
        {
            comment = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            outPath = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            printUsage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 2;
        }
    }

    if (!comment)
    {
        std::cerr << "Missing required argument: --comment\n";
        printUsage();
        return 2;
    }

#ifndef NDEBUG
    std::cerr << "WARNING: this is a Debug build; timings will not be meaningful.\n";
#endif

    const auto noise = AbacDsp::Perf::makeNoiseBlocks();
    const auto sampleData = std::make_shared<std::vector<float>>(noise.longInterleavedStereo);
    const auto sincFilter = std::make_shared<AbacDsp::SincFilter>(sinc4);
    const AbacDsp::WaveShaperTableStore waveShaperTables{};
    const AbacDsp::MpeCurveMap curveMap{};

    const auto start = std::chrono::steady_clock::now();
    std::vector<AbacDsp::Perf::SutResult> results;

    addReverbResult<GlideTankN<8>>(results, "FdnTankGlide (order 8)", noise, secondsPerProbe);
    addReverbResult<GlideTankN<16>>(results, "FdnTankGlide (order 16)", noise, secondsPerProbe);
    addReverbResult<GlideTankN<32>>(results, "FdnTankGlide (order 32)", noise, secondsPerProbe);
    addReverbResult<SpicedTankN<8>>(results, "FdnTankSpicedBase (order 8)", noise, secondsPerProbe);
    addReverbResult<SpicedTankN<16>>(results, "FdnTankSpicedBase (order 16)", noise, secondsPerProbe);
    addReverbResult<SpicedTankN<32>>(results, "FdnTankSpicedBase (order 32)", noise, secondsPerProbe);
    addReverbResult<WalshTankN<8>>(results, "FdnTankBlockDelayWalshSIMD (order 8)", noise, secondsPerProbe);
    addReverbResult<WalshTankN<16>>(results, "FdnTankBlockDelayWalshSIMD (order 16)", noise, secondsPerProbe);
    addReverbResult<WalshTankN<32>>(results, "FdnTankBlockDelayWalshSIMD (order 32)", noise, secondsPerProbe);

    results.push_back(AbacDsp::Perf::benchmark(
        "Biquad", "LowPass", [] { return BiquadSut<AbacDsp::BiquadFilterType::LowPass>{}; },
        processBiquad<AbacDsp::BiquadFilterType::LowPass>, noise, secondsPerProbe));
    results.push_back(AbacDsp::Perf::benchmark(
        "Biquad", "BandPass", [] { return BiquadSut<AbacDsp::BiquadFilterType::BandPass>{}; },
        processBiquad<AbacDsp::BiquadFilterType::BandPass>, noise, secondsPerProbe));
    results.push_back(AbacDsp::Perf::benchmark(
        "Biquad", "Peak", [] { return BiquadSut<AbacDsp::BiquadFilterType::Peak>{}; },
        processBiquad<AbacDsp::BiquadFilterType::Peak>, noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "PoleMixingFilter", "Lp24Smooth", [] { return PoleMixingSut{}; }, processPoleMixing, noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "Wavetables", "WaveTableOscillator", [] { return OscillatorSut{}; }, processOscillator, noise,
        secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "Generators", "KarplusStrongVoice", [] { return KarplusStrongSut{}; }, processKarplusStrong, noise,
        secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "Synthesizer", "MorphexsynthVoice", [&waveShaperTables, &curveMap]
        { return SynthVoiceSut(waveShaperTables, curveMap); }, processSynthVoice, noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "Diffuser", "DiffuserDelayChain (4-stage Schroeder)", [] { return DiffuserSut{}; }, processDiffuser, noise,
        secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "PitchShift", "Pitch (crossfade engine)", [] { return PitchShiftSut<false>{}; }, processPitchShift<false>,
        noise, secondsPerProbe));
    results.push_back(AbacDsp::Perf::benchmark(
        "PitchShift", "Pitch (phase vocoder engine)", [] { return PitchShiftSut<true>{}; }, processPitchShift<true>,
        noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "Analysis", "YinPitchDetector", [] { return YinSut{}; }, processYin, noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "SamplePlayer", "SamplePlayerBasic", [sampleData] { return SamplePlayerSut(sampleData); }, processSamplePlayer,
        noise, secondsPerProbe));

    results.push_back(AbacDsp::Perf::benchmark(
        "SampleRateConversion", "Upsample 44.1kHz->48kHz", [sincFilter] { return ResamplerSut(sincFilter); },
        processResample<true>, noise, secondsPerProbe));
    results.push_back(AbacDsp::Perf::benchmark(
        "SampleRateConversion", "Downsample 48kHz->44.1kHz", [sincFilter] { return ResamplerSut(sincFilter); },
        processResample<false>, noise, secondsPerProbe));

    const auto totalSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    AbacDsp::Perf::ReportHeader header;
    header.comment = *comment;
    header.git = AbacDsp::Perf::getGitInfo();
    header.cpu = AbacDsp::Perf::getProcessorInfo();
#ifdef NDEBUG
    header.isReleaseBuild = true;
#else
    header.isReleaseBuild = false;
#endif
    header.totalRunSeconds = totalSeconds;

    AbacDsp::Perf::writeHtmlReport(outPath, header, results);

    std::cout << std::left << std::setw(38) << "Variant" << std::right << std::setw(12) << "ns/block" << std::setw(14)
              << "samples/s" << std::setw(11) << "realtime" << std::setw(16) << "max instances"
              << "\n";
    for (const auto& r : results)
    {
        std::cout << std::left << std::setw(38) << r.variant << std::right << std::fixed << std::setprecision(1)
                  << std::setw(12) << r.nsPerBlock << std::setprecision(0) << std::setw(14) << r.samplesPerSecond
                  << std::setprecision(1) << std::setw(10) << r.realtimeMultiple << "x" << std::setw(15)
                  << (std::to_string(r.maxInstances) + (r.cappedAtLimit ? "+" : "")) << "\n";
    }
    std::cout << "\nTotal run time: " << std::fixed << std::setprecision(1) << totalSeconds << " s\n";
    std::cout << "Report written to " << outPath << "\n";
    return 0;
}
