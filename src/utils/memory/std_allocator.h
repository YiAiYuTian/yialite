#ifndef YIALITE_STD_ALLOCATOR_H
#define YIALITE_STD_ALLOCATOR_H

#include "allocator.h"

#include <cstddef>
#include <limits>
#include <type_traits>

namespace yialite
{

template <typename T>
class Allocator
{
    static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>, "Allocator<T>: T must be a non-const object type");
    static_assert(alignof(T) <= ALLOC_ALIGNMENT, "Allocator<T>: T requires alignment >16, yia_malloc is only 16-byte aligned");

public:
    using value_type = T;
    using is_always_equal                        = std::true_type;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap            = std::false_type;

    Allocator() noexcept = default;

    template <typename U>
    Allocator(const Allocator<U> &) noexcept {}

    [[nodiscard]] T *allocate(std::size_t count)
    {
        if (count > max_size()) detail::out_of_memory();

        T *raw = static_cast<T *>(try_alloc_raw_sized(count * sizeof(T)));
        if (!raw && count != 0) detail::out_of_memory();

        return raw;
    }

    void deallocate(T *p, std::size_t count) noexcept
    {
        if (!p) return;
        dealloc_raw_sized(p, count * sizeof(T));
    }

    [[nodiscard]] constexpr std::size_t max_size() const noexcept
    {
        return std::numeric_limits<std::size_t>::max() / sizeof(T);
    }

    template <typename U>
    [[nodiscard]] constexpr bool operator==(const Allocator<U> &) const noexcept { return true; }

    template <typename U>
    [[nodiscard]] constexpr bool operator!=(const Allocator<U> &) const noexcept { return false; }
};

} // namespace yialite

#endif // !YIALITE_STD_ALLOCATOR_H
