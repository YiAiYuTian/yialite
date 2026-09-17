#ifndef YIALITE_PAIR_H
#define YIALITE_PAIR_H

#include <compare>
#include <type_traits>
#include <utility>

namespace yialite
{

namespace detail
{
    struct PiecewiseConstructTag {};
} // namespace detail

template <typename T1, typename T2>
struct Pair
{
    using first_type  = T1;
    using second_type = T2;

    T1 first{};
    T2 second{};

    constexpr Pair() = default;
    constexpr Pair(const Pair &) = default;
    constexpr Pair(Pair &&) = default;

    constexpr Pair(const T1 &a, const T2 &b) noexcept(std::is_nothrow_copy_constructible_v<T1> && std::is_nothrow_copy_constructible_v<T2>)
        : first(a), second(b)
    {
    }

    // HashMap use
    template <typename K, typename... Args>
    constexpr Pair(detail::PiecewiseConstructTag, K &&k, Args &&...args)
        : first(std::forward<K>(k)), second(std::forward<Args>(args)...)
    {
    }

    template <typename U1 = T1, typename U2 = T2>
    requires (std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>)
    constexpr explicit(!std::is_convertible_v<U1, T1> || !std::is_convertible_v<U2, T2>)
    Pair(U1 &&a, U2 &&b) noexcept(std::is_nothrow_constructible_v<T1, U1> && std::is_nothrow_constructible_v<T2, U2>)
        : first(std::forward<U1>(a)), second(std::forward<U2>(b))
    {
    }

    template <typename U1, typename U2>
    requires (std::is_constructible_v<T1, const U1&> && std::is_constructible_v<T2, const U2&>)
    constexpr explicit(!std::is_convertible_v<const U1&, T1> || !std::is_convertible_v<const U2&, T2>)
    Pair(const Pair<U1, U2> &other) noexcept(std::is_nothrow_constructible_v<T1, const U1&> && std::is_nothrow_constructible_v<T2, const U2&>)
        : first(other.first), second(other.second)
    {
    }

    template <typename U1, typename U2>
    requires (std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>)
    constexpr explicit(!std::is_convertible_v<U1, T1> || !std::is_convertible_v<U2, T2>)
    Pair(Pair<U1, U2> &&other) noexcept(std::is_nothrow_constructible_v<T1, U1> && std::is_nothrow_constructible_v<T2, U2>)
        : first(std::forward<U1>(other.first)), second(std::forward<U2>(other.second))
    {
    }

    constexpr Pair &operator=(const Pair& other)
    noexcept(std::is_nothrow_assignable_v<T1&, const T1&> && std::is_nothrow_assignable_v<T2&, const T2&>)
    requires (std::is_assignable_v<T1&, const T1&> && std::is_assignable_v<T2&, const T2&>)
    {
        first  = other.first;
        second = other.second;
        return *this;
    }

    constexpr Pair& operator=(Pair&&) = default;

    template <typename U1, typename U2>
    requires (std::is_assignable_v<T1&, const U1&> && std::is_assignable_v<T2&, const U2&>)
    constexpr Pair &operator=(const Pair<U1, U2> &other)  
        noexcept(std::is_nothrow_assignable_v<T1&, const U1&> && std::is_nothrow_assignable_v<T2&, const U2&>)
    {
        first  = other.first;
        second = other.second;
        return *this;
    }

    template <typename U1, typename U2>
    requires (std::is_assignable_v<T1&, U1> && std::is_assignable_v<T2&, U2>)
    constexpr Pair &operator=(Pair<U1, U2> &&other) noexcept(std::is_nothrow_assignable_v<T1&, U1> && std::is_nothrow_assignable_v<T2&, U2>)
    {
        first  = std::forward<U1>(other.first);
        second = std::forward<U2>(other.second);
        return *this;
    }

    constexpr void swap(Pair &other) noexcept(std::is_nothrow_swappable_v<T1> && std::is_nothrow_swappable_v<T2>)
    {
        std::swap(first, other.first);
        std::swap(second, other.second);
    }

    [[nodiscard]] friend constexpr bool operator==(const Pair&, const Pair&) = default;
    [[nodiscard]] friend constexpr auto operator<=>(const Pair&, const Pair&) = default;
};

template <typename T1, typename T2>
constexpr void swap(Pair<T1, T2> &a, Pair<T1, T2> &b) noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

template <typename T1, typename T2>
[[nodiscard]] constexpr Pair<std::decay_t<T1>, std::decay_t<T2>> make_pair(T1 &&a, T2 &&b)
    noexcept(std::is_nothrow_constructible_v<std::decay_t<T1>, T1> && std::is_nothrow_constructible_v<std::decay_t<T2>, T2>)
{
    return Pair<std::decay_t<T1>, std::decay_t<T2>>(std::forward<T1>(a), std::forward<T2>(b));
}

template <typename T1, typename T2>
Pair(T1, T2) -> Pair<T1, T2>;

} // namespace yialite

#endif // YIALITE_PAIR_H
