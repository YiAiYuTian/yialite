#ifndef YIALITE_TEST_UTIL_H
#define YIALITE_TEST_UTIL_H

#include <cstddef>
#include <cstdio>
#include <iterator>

namespace yia_test
{

// Satisfies std::input_iterator but NOT std::forward_iterator, so passing it
// to a range overload forces the slow path. Used to check that the fast and
// slow overloads of a container agree.
template <typename T>
struct InputOnlyIt
{
    using iterator_category = std::input_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T *;
    using reference         = const T &;

    const T *p = nullptr;

    reference operator*() const { return *p; }
    pointer operator->() const { return p; }

    InputOnlyIt &operator++()
    {
        ++p;
        return *this;
    }

    void operator++(int) { ++p; }

    friend bool operator==(const InputOnlyIt &, const InputOnlyIt &) = default;
};

static_assert(std::input_iterator<InputOnlyIt<int>>);
static_assert(!std::forward_iterator<InputOnlyIt<int>>);

inline int g_failures = 0;

inline void section(const char *name)
{
    std::printf("\n=== %s ===\n", name);
}

inline void check(bool ok, const char *what)
{
    if (ok)
    {
        std::printf("  [ ok ] %s\n", what);
        return;
    }

    std::printf("  [FAIL] %s\n", what);
    ++g_failures;
}

inline int report()
{
    if (g_failures == 0)
    {
        std::printf("\n[ ok ] all checks passed\n");
        return 0;
    }

    std::printf("\n[FAIL] %d check(s) failed\n", g_failures);
    return 1;
}

} // namespace yia_test

#endif // YIALITE_TEST_UTIL_H
