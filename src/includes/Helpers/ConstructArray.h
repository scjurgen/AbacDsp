#pragma once

#include <array>
#include <cstddef>
#include <utility>


namespace AbacDsp
{

namespace impl
{
template <typename T, std::size_t N, std::size_t... Ns, typename... Args>
auto constructArrayImplementation(const std::index_sequence<Ns...>&, Args&&... args)
{
    static_assert(std::conjunction_v<std::is_copy_constructible<Args>...>, "Ctor arguments must be copy constructible");
    const auto create = [&args...](std::size_t) { return T(args...); };
    return std::array<T, N>{create(Ns)...};
}
}

template <typename T, std::size_t N, typename... Args>
auto constructArray(Args&&... args)
{
    return impl::constructArrayImplementation<T, N>(std::make_index_sequence<N>(), std::forward<Args>(args)...);
}
}
