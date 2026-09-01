// Verification plots for src/includes/Wavetables/: mipmap band-limiting across the pitch
// range, at a mip boundary, under PWM/morph, setFrequency() vs. changeFrequency(), MPE
// pitch-bend headroom, and a continuous-bend spectrogram. See README.md.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Analysis/Spectrogram.h"
#include "Wavetables/WaveTableOscillator.h"
#include "Wavetables/WaveTableStorage.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kFftSize = 16384;      // ~2.9Hz resolution, resolves 55Hz-spaced harmonics
constexpr size_t kSettleSamples = 4096; // block padding ahead of the captured FFT tail

void writeSpectrum(std::ofstream& out, AbacDsp::WaveTableOscillator& osc)
{
    std::vector<float> block(kSettleSamples + kFftSize);
    osc.processBlock(block.data(), block.size());
    const std::vector<float> tail(block.end() - static_cast<std::ptrdiff_t>(kFftSize), block.end());
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(tail, magnitude);
    for (size_t bin = 1; bin < kFftSize / 2; ++bin)
    {
        const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

// Mirrors MorphexsynthVoice::kFGranularity (16): setFrequency() is called once per that
// many samples, never per-sample, so that is the real re-selection cadence to test.
// octaves may be negative (bend down); returns the rendered audio for further use.
[[nodiscard]] std::vector<float> renderContinuousBend(AbacDsp::WaveTableOscillator& osc, const float baseFreq,
                                                      const float octaves, const float durationSeconds)
{
    constexpr size_t kStepSamples = 16;
    const auto numSteps = static_cast<size_t>(durationSeconds * kSampleRate / static_cast<float>(kStepSamples));
    std::vector<float> rendered((numSteps + 1) * kStepSamples);
    for (size_t step = 0; step <= numSteps; ++step)
    {
        const float semitones = 12.f * octaves * static_cast<float>(step) / static_cast<float>(numSteps);
        osc.setFrequency(baseFreq * std::pow(2.f, semitones / 12.f));
        osc.processBlock(rendered.data() + step * kStepSamples, kStepSamples);
    }
    return rendered;
}

// ---- 1. Spectrum across the pitch range ----

void writePitchRangeSpectra(std::ofstream& out)
{
    AbacDsp::WaveTableOscillator osc(kSampleRate);
    osc.setWaveset(0, AbacDsp::BasicWave::Saw);
    osc.setMorph(-1.f);
    for (const auto& [note, freq] :
         {std::pair{"A1", 55.f}, std::pair{"A3", 220.f}, std::pair{"A5", 880.f}, std::pair{"A7", 3520.f}})
    {
        osc.setFrequency(freq);
        out << "@New plot: title=\"Saw spectrum at " << note << " (" << freq << "Hz, Nyquist=" << kSampleRate / 2.f
            << "Hz)\"\n#Saw\n";
        writeSpectrum(out, osc);
    }
}

// ---- 2. Mip-level transition boundary ----

void writeMipBoundarySpectra(std::ofstream& out)
{
    const auto& tableSet = AbacDsp::WaveTableStore::getTableSet(AbacDsp::BasicWave::Saw);
    const size_t boundaryIdx = tableSet.tables.size() / 2;
    // table selection uses 2x the phase increment (see WaveTableOscillator's own doc comment)
    const float boundaryHz = tableSet.tables[boundaryIdx].topFreq * kSampleRate / 2.f;

    AbacDsp::WaveTableOscillator osc(kSampleRate);
    osc.setWaveset(0, AbacDsp::BasicWave::Saw);
    osc.setMorph(-1.f);
    for (const float factor : {0.99f, 1.01f})
    {
        const float freq = boundaryHz * factor;
        osc.setFrequency(freq);
        out << "@New plot: title=\"Saw spectrum across mip boundary #" << boundaryIdx << " (" << freq
            << "Hz, boundary=" << boundaryHz << "Hz)\"\n#Saw\n";
        writeSpectrum(out, osc);
    }
}

// ---- 3. PWM ----

void writePwmSpectra(std::ofstream& out)
{
    constexpr float freq = 880.f;
    for (const auto& [name, mode] : {std::pair{"Off", AbacDsp::WaveTableOscillator::PwmMode::Off},
                                     std::pair{"Soft", AbacDsp::WaveTableOscillator::PwmMode::Soft},
                                     std::pair{"Strong", AbacDsp::WaveTableOscillator::PwmMode::Strong}})
    {
        AbacDsp::WaveTableOscillator osc(kSampleRate);
        osc.setWaveset(0, AbacDsp::BasicWave::Square);
        osc.setMorph(-1.f);
        osc.setFrequency(freq);
        osc.setPwmMode(mode);
        osc.setPwm(0.3f);
        out << "@New plot: title=\"Square + PWM=" << name << " spectrum (" << freq << "Hz)\"\n#Square\n";
        writeSpectrum(out, osc);
    }
}

// ---- 4. Morph crossfade ----

void writeMorphSpectra(std::ofstream& out)
{
    constexpr float freq = 880.f;
    AbacDsp::WaveTableOscillator osc(kSampleRate);
    osc.setWaveset(0, AbacDsp::BasicWave::Saw);
    osc.setWaveset(1, AbacDsp::BasicWave::Square);
    osc.setFrequency(freq);
    for (const float morph : {-1.f, -0.5f, 0.f})
    {
        osc.setMorph(morph);
        out << "@New plot: title=\"Saw/Square morph=" << morph << " spectrum (" << freq << "Hz)\"\n#Morph\n";
        writeSpectrum(out, osc);
    }
}

// ---- 5. changeFrequency() vs. setFrequency() ----

void writeChangeVsSetFrequency(std::ofstream& out)
{
    constexpr float lowFreq = 55.f;
    constexpr float highFreq = 3520.f;
    std::vector<float> warmup(kSettleSamples);

    AbacDsp::WaveTableOscillator setOsc(kSampleRate);
    setOsc.setWaveset(0, AbacDsp::BasicWave::Saw);
    setOsc.setMorph(-1.f);
    setOsc.setFrequency(lowFreq);
    setOsc.processBlock(warmup.data(), warmup.size());
    setOsc.setFrequency(highFreq);
    out << "@New plot: title=\"setFrequency() jump 55Hz->3520Hz: re-selects mip table\"\n#setFrequency\n";
    writeSpectrum(out, setOsc);

    AbacDsp::WaveTableOscillator changeOsc(kSampleRate);
    changeOsc.setWaveset(0, AbacDsp::BasicWave::Saw);
    changeOsc.setMorph(-1.f);
    changeOsc.setFrequency(lowFreq);
    changeOsc.processBlock(warmup.data(), warmup.size());
    changeOsc.changeFrequency(highFreq);
    out << "@New plot: title=\"changeFrequency() jump 55Hz->3520Hz: mip table NOT re-selected\"\n#changeFrequency\n";
    writeSpectrum(out, changeOsc);
}

// ---- 6. MPE pitch-bend headroom (up to 1 octave) ----

constexpr float kMpeBendSeconds = 0.2f; // fast, realistic MPE slide

void writePitchBendSpectra(std::ofstream& out)
{
    for (const auto& [note, freq] : {std::pair{"A4", 440.f}, std::pair{"A6", 1760.f}})
    {
        AbacDsp::WaveTableOscillator refOsc(kSampleRate);
        refOsc.setWaveset(0, AbacDsp::BasicWave::Saw);
        refOsc.setMorph(-1.f);
        refOsc.setFrequency(freq);
        out << "@New plot: title=\"Saw at " << note << " (" << freq << "Hz), no bend\"\n#Saw\n";
        writeSpectrum(out, refOsc);

        AbacDsp::WaveTableOscillator bentOsc(kSampleRate);
        bentOsc.setWaveset(0, AbacDsp::BasicWave::Saw);
        bentOsc.setMorph(-1.f);
        bentOsc.setFrequency(freq);
        std::ignore = renderContinuousBend(bentOsc, freq, 1.f, kMpeBendSeconds);
        out << "@New plot: title=\"Saw at " << note << ", bent up 1 octave to " << freq * 2.f
            << "Hz (16-sample steps)\"\n#Saw\n";
        writeSpectrum(out, bentOsc);
    }
}

// ---- 7. Continuous pitch-bend spectrogram ----

constexpr unsigned kSpecFftLength = 2048;
constexpr float kSpecBendSeconds = 1.f;

// SimpleSpectrogram's FFT runs on a background worker; feeding it in hop-sized chunks with
// a short sleep between (rather than one bulk call) avoids overrunning its 4-slot queue -
// the same idiom test/Analysis/Spectrogram_test.cpp uses for offline/batch feeding.
void writeSpectrogramGrid(std::ofstream& out, const std::vector<float>& audio)
{
    AbacDsp::SimpleSpectrogram spec;
    spec.setSampleRate(kSampleRate);
    spec.setFftLength(kSpecFftLength);
    const size_t hop = spec.forwardLength();
    const size_t expectedFrames = audio.size() >= kSpecFftLength ? (audio.size() - kSpecFftLength) / hop + 1 : 0;
    spec.setSlices(expectedFrames + 8); // headroom so the ring never wraps

    for (size_t fed = 0; fed < audio.size();)
    {
        const size_t chunk = std::min(hop, audio.size() - fed);
        spec.processBlock(audio.data() + fed, chunk);
        fed += chunk;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (spec.getImageSet().activeSlice < expectedFrames && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    const auto img = spec.getImageSet();
    out << img.activeSlice << " " << img.height << " " << img.sampleRate << " " << img.fftLength << " " << hop << "\n";
    for (size_t row = 0; row < img.activeSlice; ++row)
    {
        for (size_t bin = 0; bin < img.height; ++bin)
        {
            const float db = 20.f * std::log10(std::max(img.data[row * img.height + bin], 1e-9f));
            out << db << (bin + 1 < img.height ? ' ' : '\n');
        }
    }
}

void writePitchBendSpectrogram(std::ofstream& upOut, std::ofstream& downOut)
{
    constexpr float freq = 880.f;

    AbacDsp::WaveTableOscillator upOsc(kSampleRate);
    upOsc.setWaveset(0, AbacDsp::BasicWave::Sine);
    upOsc.setMorph(-1.f);
    upOsc.setFrequency(freq);
    writeSpectrogramGrid(upOut, renderContinuousBend(upOsc, freq, 1.f, kSpecBendSeconds));

    AbacDsp::WaveTableOscillator downOsc(kSampleRate);
    downOsc.setWaveset(0, AbacDsp::BasicWave::Sine);
    downOsc.setMorph(-1.f);
    downOsc.setFrequency(freq);
    writeSpectrogramGrid(downOut, renderContinuousBend(downOsc, freq, -1.f, kSpecBendSeconds));
}
}

int main(int argc, char* argv[])
{
    const std::string pitchRangePath = argc > 1 ? argv[1] : "wt_pitch_range.txt";
    const std::string mipBoundaryPath = argc > 2 ? argv[2] : "wt_mip_boundary.txt";
    const std::string pwmPath = argc > 3 ? argv[3] : "wt_pwm.txt";
    const std::string morphPath = argc > 4 ? argv[4] : "wt_morph.txt";
    const std::string changeVsSetPath = argc > 5 ? argv[5] : "wt_change_vs_set_frequency.txt";
    const std::string pitchBendPath = argc > 6 ? argv[6] : "wt_pitch_bend.txt";
    const std::string specUpPath = argc > 7 ? argv[7] : "wt_pitch_bend_spectrogram_up.txt";
    const std::string specDownPath = argc > 8 ? argv[8] : "wt_pitch_bend_spectrogram_down.txt";

    std::ofstream pitchRangeOut(pitchRangePath);
    std::ofstream mipBoundaryOut(mipBoundaryPath);
    std::ofstream pwmOut(pwmPath);
    std::ofstream morphOut(morphPath);
    std::ofstream changeVsSetOut(changeVsSetPath);
    std::ofstream pitchBendOut(pitchBendPath);
    std::ofstream specUpOut(specUpPath);
    std::ofstream specDownOut(specDownPath);
    if (!pitchRangeOut || !mipBoundaryOut || !pwmOut || !morphOut || !changeVsSetOut || !pitchBendOut || !specUpOut ||
        !specDownOut)
    {
        std::cerr << "WavetablesExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writePitchRangeSpectra(pitchRangeOut);
    writeMipBoundarySpectra(mipBoundaryOut);
    writePwmSpectra(pwmOut);
    writeMorphSpectra(morphOut);
    writeChangeVsSetFrequency(changeVsSetOut);
    writePitchBendSpectra(pitchBendOut);
    writePitchBendSpectrogram(specUpOut, specDownOut);

    std::cout << "WavetablesExplore: wrote " << pitchRangePath << ", " << mipBoundaryPath << ", " << pwmPath << ", "
              << morphPath << ", " << changeVsSetPath << ", " << pitchBendPath << ", " << specUpPath << ", and "
              << specDownPath << std::endl;
}
