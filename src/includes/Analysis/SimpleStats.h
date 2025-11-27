#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace AbacDsp
{

template <typename T>
class SimpleStats
{
  public:
    SimpleStats() {}

    void addDataPoint(T value)
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

    double getMean()
    {
        if (m_dirty)
        {
            compute();
        }
        return m_mean;
    }

    double getVariance()
    {
        if (m_dirty)
        {
            compute();
        }
        return m_variance;
    }

    double getStdDev()
    {
        if (m_dirty)
        {
            compute();
        }
        return std::sqrt(m_variance);
    }

    double getMin()
    {
        if (m_dirty)
        {
            compute();
        }
        return m_min;
    }

    double getMax()
    {
        if (m_dirty)
        {
            compute();
        }
        return m_max;
    }

    double getRange()
    {
        if (m_dirty)
        {
            compute();
        }
        return m_max - m_min;
    }

    size_t getCount() const
    {
        return m_count;
    }

    // Percentile calculation (0.0 to 1.0)
    double getPercentile(const double p)
    {
        if (m_dirty)
        {
            compute();
        }
        if (m_dataPoints.empty())
        {
            return 0.0;
        }
        auto sorted = m_dataPoints;
        std::sort(sorted.begin(), sorted.end());
        const auto index = p * (sorted.size() - 1);
        const auto lower = static_cast<size_t>(std::floor(index));
        const auto upper = static_cast<size_t>(std::ceil(index));
        if (lower == upper)
        {
            return sorted[lower];
        }
        const auto weight = index - lower;
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    }

    double getMedian()
    {
        return getPercentile(0.5);
    }

    double getQ1()
    {
        return getPercentile(0.25);
    }

    double getQ3()
    {
        return getPercentile(0.75);
    }

    double getIQR()
    {
        return getQ3() - getQ1();
    }

    // Check if value is an outlier using IQR method
    bool isOutlier(const T value, const double multiplier = 1.5)
    {
        if (m_dirty)
        {
            compute();
        }
        const auto q1 = getQ1();
        const auto q3 = getQ3();
        const auto iqr = q3 - q1;
        const auto lower = q1 - multiplier * iqr;
        const auto upper = q3 + multiplier * iqr;
        return static_cast<double>(value) < lower || static_cast<double>(value) > upper;
    }

    // Root Mean Square Error from target value
    double getRMSE(const double target = 0.0)
    {
        if (m_dirty)
        {
            compute();
        }
        auto sum_squared_error = 0.0;
        for (const auto value : m_dataPoints)
        {
            const auto error = value - target;
            sum_squared_error += error * error;
        }
        return std::sqrt(sum_squared_error / m_dataPoints.size());
    }

    // Mean Absolute Error from target value
    double getMAE(const double target = 0.0)
    {
        if (m_dirty)
        {
            compute();
        }
        auto sum_abs_error = 0.0;
        for (const auto value : m_dataPoints)
        {
            sum_abs_error += std::abs(value - target);
        }
        return sum_abs_error / m_dataPoints.size();
    }

    // Statistical summary with stream support (vertical layout)
    void printSummary(std::ostream& os, const std::string& name = "", const double target = 1.0) const
    {
        if (!name.empty())
        {
            os << name << " Statistics:\n";
        }
        os << std::fixed << std::setprecision(m_precision);
        os << "  Count:   " << m_count << "\n";
        os << "  Mean:    " << const_cast<SimpleStats*>(this)->getMean() << "\n";
        os << "  Std Dev: " << const_cast<SimpleStats*>(this)->getStdDev() << "\n";
        os << "  Min:     " << const_cast<SimpleStats*>(this)->getMin() << "\n";
        os << "  Max:     " << const_cast<SimpleStats*>(this)->getMax() << "\n";
        os << "  Range:   " << const_cast<SimpleStats*>(this)->getRange() << "\n";
        os << "  Median:  " << const_cast<SimpleStats*>(this)->getMedian() << "\n";
        os << "  Q1:      " << const_cast<SimpleStats*>(this)->getQ1() << "\n";
        os << "  Q3:      " << const_cast<SimpleStats*>(this)->getQ3() << "\n";
        os << "  IQR:     " << const_cast<SimpleStats*>(this)->getIQR() << "\n";
        os << "  MAE:     " << const_cast<SimpleStats*>(this)->getMAE(target) << "\n";
        os << "  RMSE:    " << const_cast<SimpleStats*>(this)->getRMSE(target) << "\n";
    }

    // Print horizontal header for table output
    void printHorizontalSummaryHeader(std::ostream& os, const std::string& nameHeader = "Name") const
    {
        const int fieldWidth = m_precision + 8;
        os << std::left << std::setw(30) << nameHeader << std::right << std::setw(10) << "Count"
           << std::setw(fieldWidth) << "Mean" << std::setw(fieldWidth) << "StdDev" << std::setw(fieldWidth) << "Min"
           << std::setw(fieldWidth) << "Max" << std::setw(fieldWidth) << "Median" << std::setw(fieldWidth) << "MAE"
           << std::setw(fieldWidth) << "RMSE"
           << "\n";
    }

    // Statistical summary with horizontal layout (table row)
    void printHorizontalSummary(std::ostream& os, const std::string& name = "", const double target = 1.0) const
    {
        const int fieldWidth = m_precision + 8;
        os << std::left << std::setw(30) << name << std::right << std::setw(10) << m_count << std::fixed
           << std::setprecision(m_precision) << std::setw(fieldWidth) << const_cast<SimpleStats*>(this)->getMean()
           << std::setw(fieldWidth) << const_cast<SimpleStats*>(this)->getStdDev() << std::setw(fieldWidth)
           << const_cast<SimpleStats*>(this)->getMin() << std::setw(fieldWidth)
           << const_cast<SimpleStats*>(this)->getMax() << std::setw(fieldWidth)
           << const_cast<SimpleStats*>(this)->getMedian() << std::setw(fieldWidth)
           << const_cast<SimpleStats*>(this)->getMAE(target) << std::setw(fieldWidth)
           << const_cast<SimpleStats*>(this)->getRMSE(target) << "\n";
    }

  private:
    size_t m_count{};
    int m_precision{6};
    bool m_dirty{true};
    std::vector<double> m_dataPoints{};
    double m_mean{};
    double m_variance{};
    double m_min{};
    double m_max{};

    void compute()
    {
        if (m_dataPoints.empty())
        {
            m_mean = m_variance = m_min = m_max = 0.0;
            m_dirty = false;
            return;
        }

        // Calculate mean
        const auto sum = std::accumulate(m_dataPoints.begin(), m_dataPoints.end(), 0.0);
        m_mean = sum / m_dataPoints.size();

        // Calculate variance (sample variance, n-1)
        auto variance_sum = 0.0;
        for (const auto value : m_dataPoints)
        {
            const auto diff = value - m_mean;
            variance_sum += diff * diff;
        }
        m_variance = (m_dataPoints.size() > 1) ? variance_sum / (m_dataPoints.size() - 1) : 0.0;

        // Find min and max
        const auto minmax = std::minmax_element(m_dataPoints.begin(), m_dataPoints.end());
        m_min = *minmax.first;
        m_max = *minmax.second;

        m_dirty = false;
    }
};

}
