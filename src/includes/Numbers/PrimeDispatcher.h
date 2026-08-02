#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace AbacDsp
{
/*
 * prime number are needed for delay buffer chains (without modulation)
 * to avoid phase cancellation effects
 *
 */
[[nodiscard]] inline bool isPrimeNumber(const size_t n)
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

template <size_t MINVALUE>
    requires(MINVALUE >= 3)
[[nodiscard]] inline size_t getUsefulPrime(const size_t wIn)
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
    size_t lastPrime{0};

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

// Like generateUniquePrimeSet, but keeps output index i tied to input index i instead of
// sorting by value first. Needed by callers where the index itself carries meaning (e.g. a
// per-element delay chain nudged by a signed offset per element): sorting there can swap which
// element ends up with which length once the offsets flip two neighbors' relative order. An
// out-of-order input is simply nudged up to the next available prime past its predecessor's,
// same as generateUniquePrimeSet does for duplicate values.
template <size_t MINVALUE, typename In, typename Out>
inline auto generateUniquePrimeSequence(In source, Out target, const size_t numItems)
    -> std::enable_if_t<std::is_unsigned_v<typename std::iterator_traits<In>::value_type>, void>
{
    if (numItems == 0)
    {
        throw(std::invalid_argument("prime sequence generator must produce at least one element"));
    }
    typename std::iterator_traits<In>::value_type successivePrime = 0u;
    for (size_t idx = 0; idx < numItems; ++idx, ++source, ++target)
    {
        const auto val = getUsefulPrime<MINVALUE>(std::max(*source, successivePrime));
        *target = val;
        successivePrime = val + 1;
    }
}

inline void ensureUniqueDiscreteSize(size_t* discreteSize, const unsigned int last)
{
    bool changes_made{false};
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
