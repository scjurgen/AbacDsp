// Checks AbacDsp::ExactOrnsteinUhlenbeck against theory at step rates from 7 Hz to 2 kHz: autocorrelation, stationary
// distribution and stationary deviation against an Euler-Maruyama step. Writes PyConPlot.py text files into the
// folder named by EXPLORE_OUT_DIR (default: the current folder); see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <tuple>
#include <vector>

#include "Generators/ExactOrnsteinUhlenbeck.h"
#include "Numbers/Random.h"

namespace
{
using AbacDsp::ExactOrnsteinUhlenbeck;

constexpr float kSigma{0.5f};
constexpr std::array<float, 3> kStepRates{7.0f, 100.0f, 2000.0f};

constexpr int kAutocorrRuns{30};
constexpr size_t kAutocorrSteps{20000};
constexpr size_t kAutocorrBurnIn{200};
constexpr float kMaxLagCorrelationTimes{5.0f};

constexpr size_t kDistributionSteps{2'000'000};
constexpr size_t kDistributionBurnIn{20'000};
constexpr int kNumBins{61};
constexpr float kBinRangeStdDevs{5.0f};

constexpr float kStabilitySigma{1.0f};
constexpr int kStabilityPoints{25};
constexpr float kStabilityLowestRate{4.0f};
constexpr float kStabilityHighestRate{5000.0f};
constexpr float kStabilityCorrelationTimes{50000.0f};
constexpr size_t kStabilityMinSteps{100'000};
constexpr double kRatioCeiling{5.0};

[[nodiscard]] std::vector<float> simulate(const float stepRate, const float sigma, const uint64_t seed,
                                          const size_t count, const size_t burnIn)
{
    ExactOrnsteinUhlenbeck process(stepRate, seed);
    process.setSigma(sigma);
    for (size_t i = 0; i < burnIn; ++i)
    {
        std::ignore = process.step();
    }
    std::vector<float> deviations(count);
    for (auto& d : deviations)
    {
        d = process.step() - sigma;
    }
    return deviations;
}

[[nodiscard]] double deviationOf(const std::vector<float>& values)
{
    double sum = 0.0;
    double sumSquares = 0.0;
    for (const float v : values)
    {
        sum += static_cast<double>(v);
        sumSquares += static_cast<double>(v) * static_cast<double>(v);
    }
    const double mean = sum / static_cast<double>(values.size());
    return std::sqrt(sumSquares / static_cast<double>(values.size()) - mean * mean);
}

[[nodiscard]] size_t maxLagSteps(const float stepRate, const float theta)
{
    return std::max<size_t>(2, static_cast<size_t>(std::ceil(kMaxLagCorrelationTimes * stepRate / theta)));
}

void writeOneAutocorrelationRun(std::ofstream& out, const std::vector<float>& deviations, const float stepRate,
                                const size_t maxLag)
{
    double variance = 0.0;
    for (const float d : deviations)
    {
        variance += static_cast<double>(d) * static_cast<double>(d);
    }
    out << "#empirical\n";
    const size_t lagStride = std::max<size_t>(1, maxLag / 100);
    for (size_t lag = 0; lag <= maxLag; lag += lagStride)
    {
        double covariance = 0.0;
        for (size_t i = 0; i + lag < deviations.size(); ++i)
        {
            covariance += static_cast<double>(deviations[i]) * static_cast<double>(deviations[i + lag]);
        }
        const auto count = static_cast<double>(deviations.size());
        const double normalised = covariance * count / (count - static_cast<double>(lag)) / variance;
        out << (static_cast<float>(lag) / stepRate) << " " << normalised << "\n";
    }
}

void writeAutocorrelation(std::ofstream& out, const float stepRate)
{
    ExactOrnsteinUhlenbeck reference(stepRate);
    reference.setSigma(kSigma);
    const float theta = reference.getTheta();
    const size_t maxLag = maxLagSteps(stepRate, theta);
    out << "@New plot: title=\"step rate " << stepRate << " Hz, theta dt = " << theta / stepRate << "\"\n";
    for (int run = 0; run < kAutocorrRuns; ++run)
    {
        const auto deviations =
            simulate(stepRate, kSigma, 100 + static_cast<uint64_t>(run), kAutocorrSteps, kAutocorrBurnIn);
        writeOneAutocorrelationRun(out, deviations, stepRate, maxLag);
    }
    out << "#theoretical exp(-theta*tau)\n";
    const size_t lagStride = std::max<size_t>(1, maxLag / 100);
    for (size_t lag = 0; lag <= maxLag; lag += lagStride)
    {
        const float tau = static_cast<float>(lag) / stepRate;
        out << tau << " " << std::exp(-theta * tau) << "\n";
    }
}

void writeDistribution(std::ofstream& out, const float stepRate)
{
    ExactOrnsteinUhlenbeck process(stepRate, 7);
    process.setSigma(kSigma);
    const float stddev = process.getStationaryDeviation();
    const float range = kBinRangeStdDevs * stddev;
    const float binWidth = 2.0f * range / static_cast<float>(kNumBins);

    std::array<size_t, kNumBins> counts{};
    size_t counted = 0;
    for (size_t i = 0; i < kDistributionSteps + kDistributionBurnIn; ++i)
    {
        const float d = process.step() - process.getMu();
        const auto bin = static_cast<int>(std::floor((d + range) / binWidth));
        if (i >= kDistributionBurnIn && bin >= 0 && bin < kNumBins)
        {
            ++counts[static_cast<size_t>(bin)];
            ++counted;
        }
    }
    out << "@New plot: title=\"step rate " << stepRate << " Hz\"\n#empirical\n";
    for (int b = 0; b < kNumBins; ++b)
    {
        const float centre = -range + (static_cast<float>(b) + 0.5f) * binWidth;
        const float density =
            static_cast<float>(counts[static_cast<size_t>(b)]) / (static_cast<float>(counted) * binWidth);
        out << centre << " " << density << "\n";
    }
    out << "#theoretical Gaussian\n";
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -range + 2.0f * range * static_cast<float>(i) / 200.0f;
        const float density =
            std::exp(-x * x / (2.0f * stddev * stddev)) / (stddev * std::sqrt(2.0f * std::numbers::pi_v<float>));
        out << x << " " << density << "\n";
    }
}

// The textbook step x' = x + theta (mu - x) dt + sigma_w sqrt(dt) N(0, 1), on the same noise source.
[[nodiscard]] std::vector<float> simulateEuler(const float stepRate, const float sigma, const size_t count,
                                               const uint64_t seed)
{
    ExactOrnsteinUhlenbeck parameters(stepRate);
    parameters.setSigma(sigma);
    const float theta = parameters.getTheta();
    const float mu = parameters.getMu();
    const float dt = 1.0f / stepRate;
    const float noise = sigma / ExactOrnsteinUhlenbeck::kNoiseDivisor * std::sqrt(dt);
    AbacDsp::Random random(seed);
    float x = mu;
    std::vector<float> deviations(count);
    for (auto& d : deviations)
    {
        x += theta * (mu - x) * dt + noise * random.gaussian();
        d = x - mu;
    }
    return deviations;
}

[[nodiscard]] double ratioToTheory(const std::vector<float>& deviations, const double theory)
{
    const double ratio = deviationOf(deviations) / theory;
    return std::isfinite(ratio) ? std::min(ratio, kRatioCeiling) : kRatioCeiling;
}

[[nodiscard]] size_t stabilitySteps(const float stepRate, const float theta)
{
    return std::max(kStabilityMinSteps, static_cast<size_t>(kStabilityCorrelationTimes * stepRate / theta));
}

void writeStability(std::ofstream& out)
{
    ExactOrnsteinUhlenbeck reference(1.0f);
    reference.setSigma(kStabilitySigma);
    const double theory = static_cast<double>(reference.getStationaryDeviation());
    std::vector<std::array<double, 3>> rows;
    for (int i = 0; i < kStabilityPoints; ++i)
    {
        const double fraction = static_cast<double>(i) / (kStabilityPoints - 1);
        const auto rate =
            static_cast<float>(kStabilityLowestRate * std::pow(kStabilityHighestRate / kStabilityLowestRate, fraction));
        const size_t steps = stabilitySteps(rate, reference.getTheta());
        const auto seed = 3 + static_cast<uint64_t>(i);
        const auto exact = simulate(rate, kStabilitySigma, seed, steps, 1000 + steps / 50);
        rows.push_back({static_cast<double>(rate), ratioToTheory(exact, theory),
                        ratioToTheory(simulateEuler(rate, kStabilitySigma, steps, seed + 100), theory)});
    }
    out << "@New plot: title=\"stationary deviation over theory (clipped at " << kRatioCeiling << "), sigma "
        << kStabilitySigma << " (theta " << reference.getTheta() << ")\"\n";
    out << "#theory\n" << kStabilityLowestRate << " 1\n" << kStabilityHighestRate << " 1\n";
    const std::array<std::pair<const char*, size_t>, 2> series{{{"exact transition", 1}, {"Euler-Maruyama step", 2}}};
    for (const auto& [name, column] : series)
    {
        out << "#" << name << "\n";
        for (const auto& row : rows)
        {
            out << row[0] << " " << row[column] << "\n";
        }
    }
}
}

int main()
{
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    const std::string directory = folder != nullptr ? folder : ".";
    std::ofstream autocorrOut(directory + "/ou_exact_autocorr.txt");
    std::ofstream distributionOut(directory + "/ou_exact_distribution.txt");
    std::ofstream stabilityOut(directory + "/ou_exact_stability.txt");
    if (!autocorrOut || !distributionOut || !stabilityOut)
    {
        std::cerr << "ExactOrnsteinUhlenbeckExplore: ERROR - failed to open output files in " << directory << "\n";
        return 1;
    }
    for (const float stepRate : kStepRates)
    {
        writeAutocorrelation(autocorrOut, stepRate);
        writeDistribution(distributionOut, stepRate);
    }
    writeStability(stabilityOut);
    std::cout << "wrote ou_exact_autocorr.txt, ou_exact_distribution.txt, ou_exact_stability.txt to " << directory
              << "\n";
    return 0;
}
