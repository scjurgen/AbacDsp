
#pragma once

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <vector>

namespace AbacDsp
{
/*
 * prime number are needed for delay buffer chains (without modulation)
 * to avoid phase cancellation effects
 *
 */
[[nodiscard]] inline bool isPrimeNumber(size_t n)
{
    if (n == 2 || n == 3)
    {
        return true;
    }
    if (n <= 1 || n % 2 == 0 || n % 3 == 0)
    {
        return false;
    }
    for (size_t i = 5; i * i <= n; i += 6)
    {
        if (n % i == 0 || n % (i + 2) == 0)
        {
            return false;
        }
    }
    return true;
}

/*
 * this does NOT work for MINVALUE  less than 3
 */
template <size_t MINVALUE>
    requires(MINVALUE >= 3)
[[nodiscard]] size_t getUsefulPrime(const size_t wIn)
{
    auto n = std::max(MINVALUE, wIn) | 1;
    if (wIn > 1'000'000'000)
    {
        return wIn;
    }
    // maximum tries 250, that is enough for correct primes until 387'096'133
    // after that however we still hit a lot of primes
    // a buffer primed to that huge number is actually pointless (>2 hours seconds periodlength at 48kHz)
    for (size_t m = 0; m < 250; ++m)
    {
        if (isPrimeNumber(n))
        {
            return n;
        }
        n += 2;
    }
    return n;
}

template <size_t MINVALUE, typename InputIterator>
inline auto returnOrderedPrimeTable(InputIterator source, InputIterator target, size_t numItems)
    -> std::enable_if_t<std::is_same_v<typename std::iterator_traits<InputIterator>::value_type, size_t>, void>
{
    std::copy(source, source + numItems, target);
    std::sort(target, target + numItems);
    size_t lastPrime = 0;

    for (size_t idx = 0; idx < numItems; ++idx, ++target)
    {
        auto val = *target;
        if (val <= lastPrime)
        {
            val = lastPrime;
        }
        val = getUsefulPrime<MINVALUE>(val);
        *target = val;
        lastPrime = val + 1;
    }
}


template <size_t MINVALUE, typename In, typename Out>
inline auto generateUniquePrimeSet(const In source, Out target, const size_t numItems)
    -> std::enable_if_t<std::is_unsigned_v<typename std::iterator_traits<In>::value_type>, void>
{
    if (numItems == 0)
    {
        throw(std::invalid_argument("prime set generator must produce at least one element"));
    }
    std::copy(source, source + numItems, target);
    std::sort(target, target + numItems);

    typename std::iterator_traits<In>::value_type successivePrime = 0u;
    for (size_t idx = 0; idx < numItems; ++idx, ++target)
    {
        const auto val = getUsefulPrime<MINVALUE>(std::max(*target, successivePrime));
        *target = val;
        successivePrime = val + 1;
    }
}

static void ensureUniqueDiscreteSize(size_t* discreteSize, unsigned int last)
{
    bool changes_made;
    do
    {
        changes_made = false;
        std::sort(discreteSize, discreteSize + last);

        for (size_t i = 1; i < last; ++i)
        {
            if (discreteSize[i] == discreteSize[i - 1])
            {
                discreteSize[i] = getUsefulPrime<3>(discreteSize[i] + 2);
                changes_made = true;
            }
        }
    } while (changes_made);
}
}
