#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cmath>

namespace CreateExpectedSet
{

inline void toStream(std::ostream& os, const std::vector<float>& data, int precision = 6, int columnsPerRow = 8)
{
    os << "const std::vector<float> expected{";

    for (size_t i = 0; i < data.size(); ++i)
    {
        if (i % columnsPerRow == 0)
            os << "\n    ";

        float value = data[i];

        // Check if value is integral
        if (std::floor(value) == value && std::abs(value) < 1e7f)
        {
            // Format as integer with explicit decimal point
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
            std::ostringstream temp;
            temp << std::scientific << std::setprecision(precision) << value;
            std::string str = temp.str();

            // Ensure 'E' is uppercase and append 'f' suffix
            size_t e_pos = str.find('e');
            if (e_pos != std::string::npos)
            {
                str[e_pos] = 'E';
            }
            str += 'f';

            // Output
            os << str;
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

inline void printAsTestVector(const std::vector<float>& data, int precision = 6, int columnsPerRow = 8)
{
    toStream(std::cout, data, precision, columnsPerRow);
}

inline std::string toString(const std::vector<float>& data, int precision = 6, int columnsPerRow = 8)
{
    std::stringstream ss;
    toStream(ss, data, precision, columnsPerRow);
    return ss.str();
}

} // namespace CreateExpectedSet
