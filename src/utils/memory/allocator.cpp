#include "pch.h"
#include "allocator.h"
#include "yia_malloc.h"

#include <atomic>
#include <cstdio>

namespace yialite
{

namespace
{
std::atomic<uint64_t> g_alloc_total{ 0 };
std::atomic<uint64_t> g_alloc_live{ 0 };
std::atomic<uint64_t> g_alloc_peak{ 0 };
std::atomic<uint64_t> g_alloc_size{ 0 };  
}

// 底层全部由 yia_malloc 提供：不再使用 yialite 自己的内存池和 AllocationHeader。
// 全局 operator new/delete 不被接管；只有显式走 ALLOCATE_* 的引擎内存进 yia。

void* Allocator::allocate(size_t size, const char*, int)
{
    void* p = yia_malloc(size);
    if (p != nullptr)
    {
        uint64_t live = ++g_alloc_live;
        uint64_t peak = g_alloc_peak.load(std::memory_order_relaxed);
        while (live > peak &&
               !g_alloc_peak.compare_exchange_weak(peak, live, std::memory_order_relaxed))
        {
        }
        ++g_alloc_total;
        size_t new_size = *(size_t *)((unsigned char *)p - YIA_HEADER_SIZE);
        g_alloc_size += new_size;
    }
    return p;
}

void Allocator::deallocate(void* ptr)
{
    if (ptr != nullptr)
    {
        --g_alloc_live;
        size_t size = *(size_t *)((unsigned char *)ptr - YIA_HEADER_SIZE);
        g_alloc_size -= size;
    }
    yia_free(ptr);
}

void* Allocator::reallocate(void* ptr, size_t new_size, const char*, int)
{
    if (ptr == nullptr) return allocate(new_size, nullptr, 0);

    size_t old_size = *(size_t *)((unsigned char *)ptr - YIA_HEADER_SIZE);
    void* np = yia_realloc(ptr, new_size);
    if (np != nullptr)
    {
        size_t now_size = *(size_t *)((unsigned char *)np - YIA_HEADER_SIZE);
        g_alloc_size += (uint64_t)now_size - (uint64_t)old_size;
    }
    return np;
}

MemoryInfo Allocator::find_memory_info(void*)
{
    return MemoryInfo{};
}

void Allocator::print_all_memory_info()
{
}

size_t Allocator::get_alloc_size()
{
    return 0;
}

size_t Allocator::get_alloc_requested_size()
{
    return 0;
}

void Allocator::init()
{
}

void Allocator::shutdown()
{
}

void Allocator::print_stats()
{
    printf("[yia] total_allocs=%llu live=%llu peak_live=%llu bytes=%llu\n",
           (unsigned long long)g_alloc_total.load(),
           (unsigned long long)g_alloc_live.load(),
           (unsigned long long)g_alloc_peak.load(),
           (unsigned long long)g_alloc_size.load());
    fflush(stdout);
}

}
