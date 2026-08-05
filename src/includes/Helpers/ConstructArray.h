#pragma once

#include <array>
#include <cstddef>
#include <utility>


/**
 * @file
 * @ingroup helpers
 * @brief Builds a std::array whose element type has no default constructor.
 *
 * std::array value-initialises its elements, so it cannot hold a type that
 * requires constructor arguments, which every stateful processor here does
 * because they all take a sample rate. Expanding an index sequence into the
 * aggregate initialiser constructs each element in place with the same
 * arguments, keeping the array inline and allocation-free.
 */

namespace AbacDsp
{

namespace impl
{
template <typename T, std::size_t N, std::size_t... Ns, typename... Args>
[[nodiscard]] auto constructArrayImplementation(const std::index_sequence<Ns...>&, Args&&... args)
{
    static_assert(std::conjunction_v<std::is_copy_constructible<Args>...>, "Ctor arguments must be copy constructible");
    const auto create = [&args...](std::size_t) { return T(args...); };
    return std::array<T, N>{create(Ns)...};
}
}

template <typename T, std::size_t N, typename... Args>
[[nodiscard]] auto constructArray(Args&&... args)
{
    return impl::constructArrayImplementation<T, N>(std::make_index_sequence<N>(), std::forward<Args>(args)...);
}
}
