#ifndef YIALITE_TEST_UTIL_H
#define YIALITE_TEST_UTIL_H

#include <cstdio>

namespace yia_test
{

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
