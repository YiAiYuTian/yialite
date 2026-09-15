// A measurement tool, not a pass/fail test - deliberately not registered with
// CTest, so `ctest` stays fast. Build it, run it, read the numbers.
//
// Two rules keep the optimiser from deleting the work being measured:
//
//   * every value written comes from g_source, which is filled at runtime.
//     Nothing written is a compile-time constant, so the stores cannot be
//     folded away.
//   * the result of each batch is consumed by a noinline function that reads
//     it through a volatile pointer, and that happens *outside* the clock.
//
// Without this the numbers are garbage: an earlier version of this file
// reported a 68x win because GCC had removed the loop entirely.

#include "utils/containers/yia_list.h"
#include "utils/memory/allocator.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <vector>

#if defined(_MSC_VER)
#define YIA_BENCH_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define YIA_BENCH_NOINLINE __attribute__((noinline))
#else
#define YIA_BENCH_NOINLINE
#endif

namespace
{

using clock_type = std::chrono::steady_clock;

constexpr std::size_t kSourceCount = 1u << 16;

// Filled at startup from the clock, so no value in it is known to the
// compiler. Never read as volatile - one plain load per element is enough.
int g_source[kSourceCount];

std::uint64_t g_sink = 0;

void seed_source()
{
    volatile std::uint64_t seed =
        static_cast<std::uint64_t>(clock_type::now().time_since_epoch().count());

    std::uint64_t x = seed | 1u;
    for (std::size_t i = 0; i < kSourceCount; ++i)
    {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        g_source[i] = static_cast<int>(x & 0xFFFFu);
    }
}

YIA_BENCH_NOINLINE std::uint64_t checksum(const int* data, std::size_t count) noexcept
{
    const volatile int* view = data;
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < count; ++i) acc += static_cast<std::uint64_t>(view[i]);
    return acc;
}

std::uint64_t consume(const yialite::List<int>& v) noexcept { return checksum(v.data(), v.size()); }
std::uint64_t consume(const std::vector<int>& v)   noexcept { return checksum(v.data(), v.size()); }
std::uint64_t consume(std::uint64_t token)         noexcept { return token; }

// Runs `build` many times, timing only the build. The product is consumed
// after the clock stops, so verification never pollutes the measurement.
template <typename Build>
double time_build(int rounds, Build&& build)
{
    double total = 0.0;
    for (int r = 0; r < rounds; ++r)
    {
        const auto start = clock_type::now();
        auto product = build();
        const auto finish = clock_type::now();

        total += std::chrono::duration<double, std::micro>(finish - start).count();
        g_sink += consume(product);
    }
    return total / rounds;
}

void report(const char* what, double yia, double reference)
{
    std::printf("  %-30s yia %9.2f us   std %9.2f us   %5.2fx\n",
                what, yia, reference, reference / yia);
}

}

int main()
{
    seed_source();

    constexpr int kRounds   = 20;
    constexpr int kAllocOps = 20000;
    constexpr int kElements = 20000;
    constexpr int kChurn    = 2000;

    std::printf("\nyialite benchmark, Release\n");
#if defined(_MSC_VER)
    std::printf("  compiler MSVC %d\n", _MSC_VER);
#elif defined(__clang__)
    std::printf("  compiler Clang %d.%d\n", __clang_major__, __clang_minor__);
#else
    std::printf("  compiler GCC %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif

    // ------------------------------------------------------------------
    //  1. raw memory
    // ------------------------------------------------------------------
    std::printf("\n=== allocation, %d alloc+free pairs per batch ===\n", kAllocOps);

    {
        const double yia = time_build(kRounds, []
        {
            std::uint64_t acc = 0;
            for (int i = 0; i < kAllocOps; ++i)
            {
                void* p = yialite::alloc_raw(64);
                acc += reinterpret_cast<std::uintptr_t>(p);
                yialite::dealloc_raw(p);
            }
            return acc;
        });

        const double ref = time_build(kRounds, []
        {
            std::uint64_t acc = 0;
            for (int i = 0; i < kAllocOps; ++i)
            {
                void* p = ::operator new(64);
                acc += reinterpret_cast<std::uintptr_t>(p);
                ::operator delete(p);
            }
            return acc;
        });

        report("alloc 64B + free", yia, ref);
        std::printf("  %-30s yia %9.2f ns   std %9.2f ns\n",
                    "  per operation", yia * 1000.0 / kAllocOps, ref * 1000.0 / kAllocOps);
    }

    {
        static const std::size_t sizes[] = { 16, 24, 48, 96, 200, 600, 1500 };

        const double yia = time_build(kRounds, []
        {
            std::uint64_t acc = 0;
            for (int i = 0; i < kAllocOps; ++i)
            {
                void* p = yialite::alloc_raw(sizes[i % 7]);
                acc += reinterpret_cast<std::uintptr_t>(p);
                yialite::dealloc_raw(p);
            }
            return acc;
        });

        const double ref = time_build(kRounds, []
        {
            std::uint64_t acc = 0;
            for (int i = 0; i < kAllocOps; ++i)
            {
                void* p = ::operator new(sizes[i % 7]);
                acc += reinterpret_cast<std::uintptr_t>(p);
                ::operator delete(p);
            }
            return acc;
        });

        report("mixed sizes 16..1500", yia, ref);
    }

    // ------------------------------------------------------------------
    //  2. container growth
    // ------------------------------------------------------------------
    std::printf("\n=== container, %d elements per batch ===\n", kElements);

    {
        const double yia = time_build(kRounds, []
        {
            yialite::List<int> v;
            for (int i = 0; i < kElements; ++i) v.push_back(g_source[i]);
            return v;
        });

        const double ref = time_build(kRounds, []
        {
            std::vector<int> v;
            for (int i = 0; i < kElements; ++i) v.push_back(g_source[i]);
            return v;
        });

        report("push_back, no reserve", yia, ref);
    }

    {
        const double yia = time_build(kRounds, []
        {
            yialite::List<int> v;
            if (!v.try_reserve(kElements)) return v;
            for (int i = 0; i < kElements; ++i) v.push_back(g_source[i]);
            return v;
        });

        const double ref = time_build(kRounds, []
        {
            std::vector<int> v;
            v.reserve(kElements);
            for (int i = 0; i < kElements; ++i) v.push_back(g_source[i]);
            return v;
        });

        report("push_back, reserved", yia, ref);
    }

    {
        yialite::List<int> a;
        std::vector<int> b;
        if (a.try_reserve(kElements))
        {
            b.reserve(kElements);
            for (int i = 0; i < kElements; ++i) { a.push_back(g_source[i]); b.push_back(g_source[i]); }

            const double yia = time_build(kRounds, [&]
            {
                std::uint64_t acc = 0;
                for (int value : a) acc += static_cast<std::uint64_t>(value);
                return acc;
            });

            const double ref = time_build(kRounds, [&]
            {
                std::uint64_t acc = 0;
                for (int value : b) acc += static_cast<std::uint64_t>(value);
                return acc;
            });

            report("range-for over ints", yia, ref);
        }
    }

    // ------------------------------------------------------------------
    //  3. element churn
    // ------------------------------------------------------------------
    std::printf("\n=== element churn, %d rounds per batch ===\n", kChurn);

    {
        // Insert into the middle and drop the front, keeping a window of 512
        // elements alive. Erasing every iteration would return the size to
        // zero, leaving nothing observable - the compiler is then entitled to
        // delete the whole loop, which is exactly what happened before.
        constexpr std::size_t kWindow = 512;

        const double yia = time_build(kRounds, []
        {
            yialite::List<int> v;
            if (!v.try_reserve(kWindow)) return v;
            for (int i = 0; i < kChurn; ++i)
            {
                v.insert(v.begin() + (v.size() / 2), g_source[i]);
                if (v.size() > kWindow) v.erase(v.begin());
            }
            return v;
        });

        const double ref = time_build(kRounds, []
        {
            std::vector<int> v;
            v.reserve(kWindow);
            for (int i = 0; i < kChurn; ++i)
            {
                v.insert(v.begin() + (v.size() / 2), g_source[i]);
                if (v.size() > kWindow) v.erase(v.begin());
            }
            return v;
        });

        report("insert middle + erase front", yia, ref);
    }

    std::printf("\n(sink %llu - it only exists to defeat the optimiser)\n\n",
                static_cast<unsigned long long>(g_sink));
    return 0;
}
