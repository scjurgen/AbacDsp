// Generates three views of AbacDsp::OrnsteinUhlenbeckProcess at different "speeds" (sigma):
// a time-series timeline, several independent runs' empirical autocorrelation overlaid
// against the process's known theoretical decay exp(-theta*tau), and its stationary value
// distribution against the theoretical Gaussian N(0, sigma^2/(2*theta)) the process should
// settle into. Output is plain text in documentation/Plot/PyConPlot.py's "#group name" /
// "x y" format; see README.md.

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include "Generators/OrnsteinUhlenbeckProcess.h"

namespace
{
// dt = 1 / kStepRate for the process's Euler-Maruyama integration; not an audio rate, just
// fine enough for smooth-looking timelines at this plot's resolution.
constexpr float kStepRate = 2000.f;
constexpr size_t kNumSteps = 20000;

// theta = sigma * 20 + 1 (see OrnsteinUhlenbeckProcess::setSigma), so these also cover a
// roughly 4x range of reversion rate - the "speeds" being checked.
constexpr std::array<float, 4> kSigmas{0.02f, 0.05f, 0.1f, 0.2f};

// Excluded from the mean/autocorrelation estimate so x(0)=0's relaxation toward the
// stationary distribution doesn't bias them (~3 time constants of the slowest sigma).
constexpr size_t kBurnInSteps = kNumSteps / 5;

constexpr float kMaxLagSeconds = 3.f;
constexpr size_t kLagStride = 10;
// Independent runs overlaid in the autocorrelation plot, so its spread/consistency around
// the theoretical curve is visible directly rather than judging one noisy realization.
constexpr int kNumAutocorrRuns = 100;

// A much longer, separate run: the timeline's 20000 samples are plenty to look at as a
// trace but far too few to bin into a clean histogram.
constexpr size_t kDistributionNumSteps = 10'000'000;
constexpr size_t kDistributionBurnInSteps = kDistributionNumSteps / 20;
constexpr int kNumBins = 61;
constexpr float kBinRangeStdDevs = 5.f;

// setSigma() also sets mu = sigma, so subtracting it centres the whole run on zero -
// otherwise larger sigma would just shift the run up instead of visibly changing how much
// it moves.
[[nodiscard]] std::vector<float> simulate(const float sigma, const unsigned seed)
{
    AbacDsp::OrnsteinUhlenbeckProcess process(kStepRate);
    process.seed(seed);
    process.setSigma(sigma);

    std::vector<float> deviations(kNumSteps);
    for (auto& d : deviations)
    {
        d = process.step() - sigma;
    }
    return deviations;
}

void writeTimeline(std::ofstream& out, const std::vector<float>& deviations)
{
    for (size_t i = 0; i < deviations.size(); ++i)
    {
        out << (static_cast<float>(i) / kStepRate) << " " << deviations[i] << "\n";
    }
}

// Normalized empirical autocorrelation (post burn-in, own sample mean removed) for one run.
void writeOneAutocorrelationRun(std::ofstream& out, const std::vector<float>& deviations)
{
    const std::vector<float> stationary(deviations.begin() + static_cast<std::ptrdiff_t>(kBurnInSteps),
                                        deviations.end());
    const size_t n = stationary.size();

    float mean = 0.f;
    for (const float d : stationary)
    {
        mean += d;
    }
    mean /= static_cast<float>(n);

    const auto maxLag = static_cast<size_t>(kMaxLagSeconds * kStepRate);
    float variance = 0.f;
    for (const float d : stationary)
    {
        variance += (d - mean) * (d - mean);
    }

    out << "#empirical\n";
    for (size_t lag = 0; lag <= maxLag; lag += kLagStride)
    {
        float covariance = 0.f;
        for (size_t i = 0; i + lag < n; ++i)
        {
            covariance += (stationary[i] - mean) * (stationary[i + lag] - mean);
        }
        out << (static_cast<float>(lag) / kStepRate) << " " << (covariance / variance) << "\n";
    }
}

// kNumAutocorrRuns independent runs, each its own "#empirical" section (PyConPlot.py
// overlays repeated names, see its docs), then the theoretical curve written last so it
// draws on top - the actual check that "speed" matches theta, not one noisy run's guess.
void writeAutocorrelation(std::ofstream& out, const float sigma, const float theta, const unsigned baseSeed)
{
    for (int run = 0; run < kNumAutocorrRuns; ++run)
    {
        writeOneAutocorrelationRun(out, simulate(sigma, baseSeed + static_cast<unsigned>(run)));
    }

    const auto maxLag = static_cast<size_t>(kMaxLagSeconds * kStepRate);
    out << "#theoretical exp(-theta*tau)\n";
    for (size_t lag = 0; lag <= maxLag; lag += kLagStride)
    {
        const float tau = static_cast<float>(lag) / kStepRate;
        out << tau << " " << std::exp(-theta * tau) << "\n";
    }
}

// Runs its own long, separate simulation (binned directly, not kept as a vector - far
// longer than the timeline's) and writes the resulting density histogram alongside the
// theoretical stationary Gaussian - the actual settle check, not just a visual impression.
void writeDistribution(std::ofstream& out, const float sigma, const float theta, const unsigned seed)
{
    AbacDsp::OrnsteinUhlenbeckProcess process(kStepRate);
    process.seed(seed);
    process.setSigma(sigma);

    // OrnsteinUhlenbeckProcess's driving noise is N(0, (1/2.33)^2), not unit variance (see
    // m_normalDist) - textbook sigma^2/(2*theta) overstates this class's spread without it.
    constexpr float kNoiseStdDevScale = 1.f / 2.33f;
    const float stddev = sigma * kNoiseStdDevScale / std::sqrt(2.f * theta);
    const float range = kBinRangeStdDevs * stddev;
    const float binWidth = 2.f * range / static_cast<float>(kNumBins);

    std::array<size_t, kNumBins> counts{};
    size_t totalCounted = 0;
    for (size_t i = 0; i < kDistributionNumSteps; ++i)
    {
        const float d = process.step() - sigma;
        if (i < kDistributionBurnInSteps)
        {
            continue;
        }
        const auto bin = static_cast<int>((d + range) / binWidth);
        if (bin >= 0 && bin < kNumBins)
        {
            ++counts[static_cast<size_t>(bin)];
            ++totalCounted;
        }
    }

    out << "#empirical\n";
    for (int b = 0; b < kNumBins; ++b)
    {
        const float binCenter = -range + (static_cast<float>(b) + 0.5f) * binWidth;
        const float density =
            static_cast<float>(counts[static_cast<size_t>(b)]) / (static_cast<float>(totalCounted) * binWidth);
        out << binCenter << " " << density << "\n";
    }

    out << "#theoretical Gaussian\n";
    constexpr int kCurvePoints = 200;
    for (int i = 0; i <= kCurvePoints; ++i)
    {
        const float x = -range + 2.f * range * static_cast<float>(i) / static_cast<float>(kCurvePoints);
        const float density =
            std::exp(-x * x / (2.f * stddev * stddev)) / (stddev * std::sqrt(2.f * std::numbers::pi_v<float>));
        out << x << " " << density << "\n";
    }
}
}

int main(int argc, char* argv[])
{
    const std::string timelinePath = argc > 1 ? argv[1] : "ou_timelines.txt";
    const std::string autocorrPath = argc > 2 ? argv[2] : "ou_autocorr.txt";
    const std::string distributionPath = argc > 3 ? argv[3] : "ou_distribution.txt";
    std::ofstream timelineOut(timelinePath);
    std::ofstream autocorrOut(autocorrPath);
    std::ofstream distributionOut(distributionPath);
    if (!timelineOut || !autocorrOut || !distributionOut)
    {
        std::cerr << "OrnsteinUhlenbeckTimeline: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    for (size_t i = 0; i < kSigmas.size(); ++i)
    {
        const float sigma = kSigmas[i];
        const float theta = sigma * 20.f + 1.f;
        const std::vector<float> deviations = simulate(sigma, static_cast<unsigned>(1000 + i));

        timelineOut << "@New plot: title=\"sigma=" << sigma << "\"\n#sigma=" << sigma << "\n";
        writeTimeline(timelineOut, deviations);

        autocorrOut << "@New plot: title=\"sigma=" << sigma << " (theta=" << theta << ")\"\n";
        writeAutocorrelation(autocorrOut, sigma, theta, static_cast<unsigned>(3000 + i * 100));

        distributionOut << "@New plot: title=\"sigma=" << sigma << " (theta=" << theta << ")\"\n";
        writeDistribution(distributionOut, sigma, theta, static_cast<unsigned>(2000 + i));
    }
    std::cout << "OrnsteinUhlenbeckTimeline: wrote " << kSigmas.size() << " timelines to " << timelinePath << ", "
              << kSigmas.size() << " autocorrelations to " << autocorrPath << ", and " << kSigmas.size()
              << " distributions to " << distributionPath << std::endl;
}
