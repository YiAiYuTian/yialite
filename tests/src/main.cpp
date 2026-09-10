#include "yialite/utils/memory/allocator.h"

#include <cstdio>

namespace
{

int g_failures = 0;

void check(bool condition, const char* what)
{
    if (condition)
    {
        std::printf("[ ok ] %s\n", what);
        return;
    }
    std::printf("[FAIL] %s\n", what);
    ++g_failures;
}

}

int main()
{
    yialite::Allocator::init();

    const auto baseline = yialite::Allocator::get_alloc_size();

    void* block = ALLOCATE_SIZED(256);
    check(block != nullptr, "allocator hands out memory");

    if (block != nullptr)
    {
        static_cast<unsigned char*>(block)[0] = 0xABu;
        static_cast<unsigned char*>(block)[255] = 0xCDu;
        check(static_cast<unsigned char*>(block)[0] == 0xABu &&
              static_cast<unsigned char*>(block)[255] == 0xCDu,
              "allocated memory is writable");
        DEALLOCATE_SIZED(block);
    }

    check(yialite::Allocator::get_alloc_size() == baseline,
          "the allocation is fully reclaimed");

    yialite::Allocator::shutdown();

    if (g_failures == 0)
    {
        std::printf("[ ok ] core smoke\n");
        return 0;
    }
    std::printf("[FAIL] %d check(s) failed\n", g_failures);
    return 1;
}
