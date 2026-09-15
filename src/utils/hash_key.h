#ifndef YIALITE_HASH_KEY_H
#define YIALITE_HASH_KEY_H

#include "base_types.h"
#include "handle.h"
#include "string/yia_string.h"
#include "string/yia_string_view.h"

#include <array>
#include <bit>
#include <cstddef>
#include <type_traits>

namespace yialite
{

namespace detail
{
    inline constexpr std::size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    inline constexpr std::size_t FNV_PRIME        = 1099511628211ULL;

    template <typename Byte>
    [[nodiscard]] constexpr std::size_t fnv1a(const Byte* data, std::size_t count) noexcept
    {
        std::size_t hash = FNV_OFFSET_BASIS;
        for (std::size_t i = 0; i < count; ++i)
        {
            hash ^= static_cast<Uint8>(data[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    template <typename T>
    [[nodiscard]] constexpr std::size_t fnv1a_of(const T& value) noexcept
    {
        const auto bytes = std::bit_cast<std::array<Uint8, sizeof(T)>>(value);
        return fnv1a(bytes.data(), bytes.size());
    }

    template <typename...>
    inline constexpr bool always_false = false;
}

template <typename T>
struct HashKey
{
    [[nodiscard]] constexpr std::size_t operator()(const T& key) const noexcept
    {
        if constexpr (std::is_pointer_v<T>)
        {
            static_assert(sizeof(std::size_t) == sizeof(T),
                          "HashKey: pointers on this target are not size_t wide, so their bits do not fit in a size_t");
            return std::bit_cast<std::size_t>(key);
        }
        else if constexpr (std::is_enum_v<T>)
        {
            using Underlying = std::underlying_type_t<T>;
            return HashKey<Underlying>{}(static_cast<Underlying>(key));
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            return detail::fnv1a_of(key);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if constexpr (sizeof(T) <= sizeof(std::size_t))
            {
                return static_cast<std::size_t>(key);
            }
            else
            {
                return detail::fnv1a_of(key);
            }
        }
        else
        {
            static_assert(detail::always_false<T>,
                          "HashKey<T>: no hash is defined for this type. Add a HashKey<T> "
                          "specialisation next to the others in this header, and make it hash "
                          "exactly what T's operator== compares.");
            return 0;
        }
    }
};

template <>
struct HashKey<StringView>
{
    [[nodiscard]] constexpr std::size_t operator()(const StringView& str) const noexcept
    {
        return detail::fnv1a(str.data(), str.length());
    }
};

template <>
struct HashKey<String>
{
    [[nodiscard]] std::size_t operator()(const String& str) const noexcept
    {
        return HashKey<StringView>{}(StringView(str));
    }
};

template <typename T, typename Tag>
struct HashKey<Handle<T, Tag>>
{
    [[nodiscard]] constexpr std::size_t operator()(const Handle<T, Tag>& handle) const noexcept
    {
        return HashKey<T>{}(handle.id);
    }
};

} // namespace yialite

#endif // YIALITE_HASH_KEY_H
