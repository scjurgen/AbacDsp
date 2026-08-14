// Generates two views of AbacDsp::OrnsteinUhlenbeckProcess at different "speeds" (sigma):
// a time-series timeline, and its empirical autocorrelation against the process's known
// theoretical decay exp(-theta*tau) - the latter is the actual quantitative check that the
// "speed" differences are real, since amplitude and speed both scale with sigma and can look
// deceptively similar on a per-plot auto-scaled timeline. Output is plain text in
// documentation/Plot/PyConPlot.py's "#group name" / "x y" format; see README.md.

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
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

// Excluded from the mean/autocorrelation estimate below so the initial relaxation from
// x(0) = 0 towards the stationary distribution doesn't bias them; ~3 time constants of the
// slowest sigma here (theta ~= 1.4, so 1/theta ~= 0.7s).
constexpr size_t kBurnInSteps = kNumSteps / 5;

constexpr float kMaxLagSeconds = 3.f;
constexpr size_t kLagStride = 10;

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

// Normalized empirical autocorrelation (post burn-in, own sample mean removed) alongside
// the process's theoretical exp(-theta*tau) decay, so a reader can see directly whether the
// simulated "speed" matches theta rather than just eyeballing the raw timeline.
void writeAutocorrelation(std::ofstream& out, const std::vector<float>& deviations, const float theta)
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

    out << "#theoretical exp(-theta*tau)\n";
    for (size_t lag = 0; lag <= maxLag; lag += kLagStride)
    {
        const float tau = static_cast<float>(lag) / kStepRate;
        out << tau << " " << std::exp(-theta * tau) << "\n";
    }
}
}

int main(int argc, char* argv[])
{
    const std::string timelinePath = argc > 1 ? argv[1] : "ou_timelines.txt";
    const std::string autocorrPath = argc > 2 ? argv[2] : "ou_autocorr.txt";
    std::ofstream timelineOut(timelinePath);
    std::ofstream autocorrOut(autocorrPath);
    if (!timelineOut || !autocorrOut)
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
        writeAutocorrelation(autocorrOut, deviations, theta);
    }
    std::cout << "OrnsteinUhlenbeckTimeline: wrote " << kSigmas.size() << " timelines to " << timelinePath << " and "
              << kSigmas.size() << " autocorrelations to " << autocorrPath << std::endl;
}
