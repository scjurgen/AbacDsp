#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Accumulates values and reports mean, variance, min, max and percentiles.
 *
 * Every data point is retained, because percentiles cannot be computed from a
 * running summary. That also fixes what this is: an offline tool, since memory
 * grows without bound.
 *
 * Statistics are computed lazily and cached, so adding a point stays a push_back.
 */
template <typename T>
class SimpleStats
{
  public:
    SimpleStats() = default;

    void addDataPoint(const T value)
    {
        m_dataPoints.push_back(static_cast<double>(value));
        m_count++;
        m_dirty = true;
    }

    void clear()
    {
        m_dataPoints.clear();
        m_count = 0;
        m_dirty = true;
        m_mean = m_variance = m_min = m_max = 0;
    }

    void setPrecision(const int points)
    {
        m_precision = points;
    }

    [[nodiscard]] double getMean() const
    {
        computeIfDirty();
        return m_mean;
    }

    [[nodiscard]] double getVariance() const
    {
        computeIfDirty();
        return m_variance;
    }

    [[nodiscard]] double getStdDev() const
    {
        computeIfDirty();
        return std::sqrt(m_variance);
    }

    [[nodiscard]] double getMin() const
    {
        computeIfDirty();
        return m_min;
    }

    [[nodiscard]] double getMax() const
    {
        computeIfDirty();
        return m_max;
    }

    [[nodiscard]] double getRange() const
    {
        computeIfDirty();
        return m_max - m_min;
    }

    [[nodiscard]] size_t getCount() const noexcept
    {
        return m_count;
    }

    [[nodiscard]] double getPercentile(const double p) const
    {
        computeIfDirty();
        if (m_dataPoints.empty())
        {
            return 0.0;
        }
        auto sorted = m_dataPoints;
        std::ranges::sort(sorted);
        const double index = p * static_cast<double>(sorted.size() - 1);
        const auto lower = static_cast<size_t>(std::floor(index));
        const auto upper = static_cast<size_t>(std::ceil(index));
        if (lower == upper)
        {
            return sorted[lower];
        }
        const double weight = index - static_cast<double>(lower);
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    }

    [[nodiscard]] double getMedian() const
    {
        return getPercentile(0.5);
    }

    [[nodiscard]] double getQ1() const
    {
        return getPercentile(0.25);
    }

    [[nodiscard]] double getQ3() const
    {
        return getPercentile(0.75);
    }

    [[nodiscard]] double getIQR() const
    {
        return getQ3() - getQ1();
    }

    [[nodiscard]] bool isOutlier(const T value, const double multiplier = 1.5) const
    {
        computeIfDirty();
        const auto q1 = getQ1();
        const auto q3 = getQ3();
        const auto iqr = q3 - q1;
        const auto lowerBound = q1 - multiplier * iqr;
        const auto upperBound = q3 + multiplier * iqr;
        return static_cast<double>(value) < lowerBound || static_cast<double>(value) > upperBound;
    }

    [[nodiscard]] double getRMSE(const double target = 0.0) const
    {
        computeIfDirty();
        const auto sumSq = std::transform_reduce(m_dataPoints.begin(), m_dataPoints.end(), 0.0, std::plus{},
                                                 [target](const double v)
                                                 {
                                                     const auto e = v - target;
                                                     return e * e;
                                                 });
        return std::sqrt(sumSq / static_cast<double>(m_dataPoints.size()));
    }

    [[nodiscard]] double getMAE(const double target = 0.0) const
    {
        computeIfDirty();
        const auto sumAbs = std::transform_reduce(m_dataPoints.begin(), m_dataPoints.end(), 0.0, std::plus{},
                                                  [target](const double v) { return std::abs(v - target); });
        return sumAbs / static_cast<double>(m_dataPoints.size());
    }

    void printSummary(std::ostream& os, const std::string& name = "", const double target = 1.0) const
    {
        if (!name.empty())
        {
            os << name << " Statistics:\n";
        }
        os << std::fixed << std::setprecision(m_precision);
        os << "  Count:   " << m_count << "\n";
        os << "  Mean:    " << getMean() << "\n";
        os << "  Std Dev: " << getStdDev() << "\n";
        os << "  Min:     " << getMin() << "\n";
        os << "  Max:     " << getMax() << "\n";
        os << "  Range:   " << getRange() << "\n";
        os << "  Median:  " << getMedian() << "\n";
        os << "  Q1:      " << getQ1() << "\n";
        os << "  Q3:      " << getQ3() << "\n";
        os << "  IQR:     " << getIQR() << "\n";
        os << "  MAE:     " << getMAE(target) << "\n";
        os << "  RMSE:    " << getRMSE(target) << "\n";
    }

    void printHorizontalSummaryHeader(std::ostream& os, const std::string& nameHeader = "Name") const
    {
        const int fieldWidth = m_precision + 8;
        os << std::left << std::setw(30) << nameHeader << std::right << std::setw(10) << "Count"
           << std::setw(fieldWidth) << "Mean" << std::setw(fieldWidth) << "StdDev" << std::setw(fieldWidth) << "Min"
           << std::setw(fieldWidth) << "Max" << std::setw(fieldWidth) << "Median" << std::setw(fieldWidth) << "MAE"
           << std::setw(fieldWidth) << "RMSE"
           << "\n";
    }

    void printHorizontalSummary(std::ostream& os, const std::string& name = "", const double target = 1.0) const
    {
        const int fieldWidth = m_precision + 8;
        os << std::left << std::setw(30) << name << std::right << std::setw(10) << m_count << std::fixed
           << std::setprecision(m_precision) << std::setw(fieldWidth) << getMean() << std::setw(fieldWidth)
           << getStdDev() << std::setw(fieldWidth) << getMin() << std::setw(fieldWidth) << getMax()
           << std::setw(fieldWidth) << getMedian() << std::setw(fieldWidth) << getMAE(target) << std::setw(fieldWidth)
           << getRMSE(target) << "\n";
    }

  private:
    size_t m_count{};
    int m_precision{6};
    mutable bool m_dirty{true};
    std::vector<double> m_dataPoints{};
    mutable double m_mean{};
    mutable double m_variance{};
    mutable double m_min{};
    mutable double m_max{};

    void computeIfDirty() const
    {
        if (!m_dirty)
        {
            return;
        }
        if (m_dataPoints.empty())
        {
            m_mean = m_variance = m_min = m_max = 0.0;
            m_dirty = false;
            return;
        }

        m_mean =
            std::accumulate(m_dataPoints.begin(), m_dataPoints.end(), 0.0) / static_cast<double>(m_dataPoints.size());

        const auto varianceSum =
            std::transform_reduce(m_dataPoints.begin(), m_dataPoints.end(), 0.0, std::plus{},
                                  [mean = m_mean](const double v) { return (v - mean) * (v - mean); });
        m_variance = (m_dataPoints.size() > 1) ? varianceSum / static_cast<double>(m_dataPoints.size() - 1) : 0.0;

        const auto [minIt, maxIt] = std::minmax_element(m_dataPoints.begin(), m_dataPoints.end());
        m_min = *minIt;
        m_max = *maxIt;

        m_dirty = false;
    }
};

}
