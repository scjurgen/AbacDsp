
#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <type_traits>

namespace AbacDsp
{
/*
 * bulge table
 * concept:
 * using a single normalized parameter (-1...+1) control the geometric sizes of parallel delay buffers.
 * Negative values pull to the left causing longer delay values

example::
        5 elements in range 1000 ... 2000

    | bulge       |  el1 |  el2 |  el3 |  el4 |  el5 |
    | factor      |      |      |      |      |      |
    +=============+======+======+======+======+======+
    | -1.00       | 1000 | 1684 | 1938 | 1996 | 2000 |
    | -0.75       | 1000 | 1557 | 1859 | 1980 | 2000 |
    | -0.50       | 1000 | 1438 | 1750 | 1938 | 2000 |
    | -0.25       | 1000 | 1334 | 1625 | 1859 | 2000 |
    +-------------+------+------+------+------+------+
    | +0.00       | 1000 | 1250 | 1500 | 1750 | 2000 |
    +-------------+------+------+------+------+------+
    | +0.25       | 1000 | 1141 | 1375 | 1666 | 2000 |
    | +0.50       | 1000 | 1063 | 1250 | 1563 | 2000 |
    | +0.75       | 1000 | 1020 | 1141 | 1443 | 2000 |
    | +1.00       | 1000 | 1004 | 1063 | 1316 | 2000 |
    +-------------+------+------+------+------+------+

bulge
       +----------------------------------------------------------------+
       |    +          +          +          +          +          +    | -0.5 *******
     1 |-+                                                     **     +-|  0.0 #######
       |                                               ********##       |  0.5 $$$$$$$
       |                                       ********     ###         |
       |                                 ******          ###    $       |
   0.8 |-+                          *****              ##     $$      +-|
       |                      ******                ###      $          |
       |                    **                   ###        $           |
       |                 ***                   ##          $            |
   0.6 |-+            ***                   ###           $           +-|
       |            **                   ###             $              |
y      |          **                #####              $$               |
   0.4 |-+      **               ###                 $$               +-|
       |       *              ###                  $$                   |
       |       *            ##                   $$                     |
       |      *          ###                   $$                       |
   0.2 |-+    *       ###                   $$$                       +-|
       |     *      ##                   $$$                            |
       |     *   ###                  $$$                               |
       |    * ###             $$$$$$$$                                  |
     0 |-+  $$$$$$$$$$$$$$$$$$                                        +-|
       |    +          +          +          +          +          +    |
       +----------------------------------------------------------------+
            0         0.2        0.4        0.6        0.8         1

*/


class Bulge
{
  public:
    template <typename T>
    static T getValue(const T x, const T bulge, const T bulgePower = 4.0)
    {
        if (bulge < 0)
        {
            if (x >= 1.f)
            {
                return 1;
            }
            auto f = std::pow(bulgePower, -bulge);
            return 1 - std::pow(1 - x, f);
        }
        return std::pow(x, std::pow(bulgePower, bulge));
    }

    template <typename InplaceIterator, typename T>
    static auto fillNormalizedTable(InplaceIterator s, const size_t items, const T bulge)
        -> std::enable_if_t<std::is_same_v<typename std::iterator_traits<InplaceIterator>::value_type, T>, void>
    {
        if (items <= 1)
        {
            *s = 0;
            return;
        }
        size_t index = 0;
        std::generate_n(s, items,
                        [&index, items, bulge]()
                        {
                            const auto x = static_cast<float>(index) / static_cast<float>(items - 1);
                            const auto y = Bulge::getValue(x, bulge);
                            ++index;
                            return y;
                        });
    }

    template <typename InplaceIterator, typename T>
    static auto fillTable(InplaceIterator s, const size_t items, const float bulge, const T low, const T high)
        -> std::enable_if_t<std::is_same_v<typename std::iterator_traits<InplaceIterator>::value_type, T>, void>
    {
        if (items == 0)
        {
            return;
        }
        if (items == 1)
        {
            *s = low;
            return;
        }
        size_t index = 0;
        std::generate_n(s, items,
                        [&index, low, high, items, bulge]()
                        {
                            const auto f =
                                Bulge::getValue(static_cast<float>(index) / static_cast<float>(items - 1), bulge);
                            const auto y =
                                static_cast<T>(round(static_cast<float>(low) + static_cast<float>(high - low) * f));
                            ++index;
                            return y;
                        });
    }
};
}