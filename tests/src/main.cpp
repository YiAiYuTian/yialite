#include "utils/memory/allocator.h"

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
    return 0;
}
