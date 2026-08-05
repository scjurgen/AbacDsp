#pragma once

#include <charconv>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * @file
 * @ingroup helpers
 * @brief Formats sample buffers as C++ literals for pasting into tests.
 *
 * Turns a measured buffer into a golden vector, so a reference is captured from
 * a known-good run instead of being written by hand. Test-support only: it
 * allocates, streams and formats.
 */

namespace CreateExpectedSet
{

[[nodiscard]] inline std::string format_float(const float value, const int precision)
{
    if (std::isnan(value))
    {
        return "nan";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-inf" : "inf";
    }
    if (value == 0.0f)
    {
        return "0";
    }

    const auto abs_val = std::abs(value);
    const int exponent = static_cast<int>(std::floor(std::log10(abs_val)));

    if (exponent >= -1 && exponent < precision)
    {
        char buffer[32];
        auto [ptr, ec] =
            std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision - exponent - 1);
        if (ec != std::errc())
        {
            return "";
        }

        std::string result(buffer, ptr);
        const auto dot_pos = result.find('.');
        if (dot_pos != std::string::npos)
        {
            result.erase(result.find_last_not_of('0') + 1);
            if (result.back() == '.')
            {
                result += '0';
            }
        }
        return result;
    }

    const float mantissa = value / std::pow(10.0f, static_cast<float>(exponent));

    char mantissa_buffer[32];
    auto [m_ptr, m_ec] = std::to_chars(mantissa_buffer, mantissa_buffer + sizeof(mantissa_buffer), mantissa,
                                       std::chars_format::fixed, precision - 1);
    if (m_ec != std::errc())
    {
        return "";
    }

    std::string mantissa_str(mantissa_buffer, m_ptr);
    const auto dot_pos = mantissa_str.find('.');
    if (dot_pos != std::string::npos)
    {
        const auto last_nonzero = mantissa_str.find_last_not_of('0');
        if (mantissa_str[last_nonzero] == '.')
        {
            mantissa_str.erase(dot_pos);
        }
        else
        {
            mantissa_str.erase(last_nonzero + 1);
        }
    }

    return mantissa_str + "e" + (exponent < 0 ? "-" : "+") + std::to_string(std::abs(exponent));
}

inline void toStream(std::ostream& os, const std::vector<float>& data, const int precision = 6,
                     const int columnsPerRow = 8)
{
    os << "const std::vector<float> expected{";

    for (size_t i = 0; i < data.size(); ++i)
    {
        if (i % columnsPerRow == 0)
        {
            os << "\n    ";
        }

        const auto value = data[i];

        if (std::floor(value) == value && std::abs(value) < 1e7f)
        {
            if (value < 0.f)
            {
                os << "-" << static_cast<int>(std::abs(value)) << ".f";
            }
            else
            {
                os << static_cast<int>(value) << ".f";
            }
        }
        else
        {
            os << format_float(value, precision);
            os << 'f';
        }

        if (i < data.size() - 1)
        {
            os << ",";
        }

        if ((i + 1) % columnsPerRow != 0 && i < data.size() - 1)
        {
            os << " ";
        }
    }

    os << "\n};\n";
}

inline void printAsTestVector(const std::vector<float>& data, const int precision = 6, const int columnsPerRow = 8)
{
    toStream(std::cout, data, precision, columnsPerRow);
}

[[nodiscard]] inline std::string toString(const std::vector<float>& data, const int precision = 6,
                                          const int columnsPerRow = 8)
{
    std::stringstream ss;
    toStream(ss, data, precision, columnsPerRow);
    return ss.str();
}

}
