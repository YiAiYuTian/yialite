#ifndef YIA_MALLOC_H
#define YIA_MALLOC_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif

// some definitions
#define YIA_INVALID_INDEX -1

#if defined(_MSC_VER)
    #define YIA_TLS __declspec(thread)
    #define YIA_LIKELY(x)   (x)
    #define YIA_UNLIKELY(x) (x)
#else
    #define YIA_TLS __thread
    #define YIA_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define YIA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

// header (yia_malloc)
#define YIA_HEADER_SIZE  16
typedef struct YiaHeader
{
    size_t size;
    uint64_t route; // low 32 bit(page_idx), high 32 bit (slot_idx)
} YiaHeader;

#ifdef __cplusplus
static_assert(sizeof(YiaHeader) == YIA_HEADER_SIZE, "YiaHeader must be YIA_HEADER_SIZEB");
#else
_Static_assert(sizeof(YiaHeader) == YIA_HEADER_SIZE, "YiaHeader must be YIA_HEADER_SIZEB");
#endif

// page type
typedef struct YiaFreeNode
{
    struct YiaFreeNode *next;
} YiaFreeNode;

#define YIA_PAGE_INVALID_INDEX YIA_INVALID_INDEX
#define YIA_SMALL_PAGE_COUNT  16
#define YIA_SMALL_PAGE_STRIDE 16
#define YIA_MEDIUM_PAGE_COUNT 5 // 512 1024 2048 4096 8192
typedef struct YiaPage
{
    YiaFreeNode *free_list;
    size_t block_size;
    size_t block_count;
    size_t segment_count;
} YiaPage;

#define YIA_SLOT_INVALID_INDEX YIA_INVALID_INDEX
#define YIA_SLOT_SHIFT   22
#define YIA_SLOT_SIZE    (1U << YIA_SLOT_SHIFT)  // 4MB
#define YIA_SLOT_COUNT   32                      // thread count
typedef struct YiaSlot
{
    YiaFreeNode *volatile retqueue;
    int32_t volatile orphaned;
    int32_t volatile outstanding;
    uint64_t owner;
    uint8_t padding[40];
} YiaSlot;

#ifdef __cplusplus
static_assert(sizeof(YiaSlot) == 64, "YiaSlot must be 64B");
#else
_Static_assert(sizeof(YiaSlot) == 64, "YiaSlot must be 64B");
#endif

#define YIA_LARGE_BUCKET_COUNT 8 // 16KB 32KB 64KB 128KB 256KB 512KB 1M 2M
#define YIA_LARGE_BUCKET_SHIFT 14 // 16KB
#define YIA_LARGE_MAX_SIZE (8U * 1024 * 1024) // 8MB
typedef struct YiaLargeCache
{
    YiaFreeNode *buckets[YIA_LARGE_BUCKET_COUNT];
    uint16_t count[YIA_LARGE_BUCKET_COUNT];
    size_t total_bytes;
} YiaLargeCache;

#define YIA_ARENA_SIZE  (YIA_SLOT_SIZE * YIA_SLOT_COUNT)
typedef struct YiaPool
{
    YiaPage pages[YIA_SMALL_PAGE_COUNT + YIA_MEDIUM_PAGE_COUNT];
    YiaLargeCache large_cache;
    int slot_index;
    size_t slot_used;
    size_t outstanding;
    bool inited;
} YiaPool;

#define YIA_SMALL_CUTOFF   (YIA_SMALL_PAGE_COUNT * YIA_SMALL_PAGE_STRIDE) // 256
#define YIA_MEDIUM_CUTOFF  (YIA_SMALL_CUTOFF << YIA_MEDIUM_PAGE_COUNT)    // 8192 (256 << 5)
#define YIA_LARGE_CUTOFF   (1U << 21)                                     // 2MB

// global var in yia_malloc.c
extern void *volatile g_arena;
extern YiaSlot g_slot_table[YIA_SLOT_COUNT];
extern YIA_TLS YiaPool *g_pool;

// tools
static inline int yia_ctz_u32(uint32_t v)
{
#ifdef _MSC_VER
    unsigned long idx;
    _BitScanForward(&idx, v);
    return (int)idx;
#else
    return __builtin_ctz(v);
#endif
}

static inline size_t yia_round_up_pow2(size_t x)
{
    if (x == 0) return 1;

    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    x |= x >> 32;
#endif
    return x + 1;
}

#ifdef _WIN32
static inline uint32_t yia_atomic_cas_u32(volatile uint32_t *p, uint32_t old_v, uint32_t new_v)
{
    return (uint32_t)InterlockedCompareExchange((volatile LONG *)p, (LONG)new_v, (LONG)old_v);
}

static inline int32_t yia_atomic_cas_s32(volatile int32_t *p, int32_t old_v, int32_t new_v)
{
    return (int32_t)InterlockedCompareExchange((volatile LONG *)p, (LONG)new_v, (LONG)old_v);
}

static inline int32_t yia_atomic_inc_s32(volatile int32_t *p, int32_t v)
{
    return (int32_t)InterlockedExchangeAdd((volatile LONG *)p, (LONG)v) + v;
}

static inline int32_t yia_atomic_dec_s32(volatile int32_t *p, int32_t v)
{
    return (int32_t)InterlockedExchangeAdd((volatile LONG *)p, -(LONG)v) - v;
}

static inline int32_t yia_atomic_load_s32(volatile int32_t *p)
{
    return (int32_t)InterlockedCompareExchange((volatile LONG *)p, 0, 0);
}

static inline uint32_t yia_atomic_load_u32(volatile uint32_t *p)
{
    return (uint32_t)InterlockedCompareExchange((volatile LONG *)p, 0, 0);
}

static inline void yia_atomic_store_s32(volatile int32_t *p, int32_t v)
{
    InterlockedExchange((volatile LONG *)p, (LONG)v);
}

static inline void *yia_atomic_cas_ptr(void *volatile *p, void *old_v, void *new_v)
{
    return InterlockedCompareExchangePointer(p, new_v, old_v);
}

static inline void *yia_atomic_exchange_ptr(void *volatile *p, void *new_v)
{
    return InterlockedExchangePointer(p, new_v);
}
#else
static inline uint32_t yia_atomic_cas_u32(volatile uint32_t *p, uint32_t old_v, uint32_t new_v)
{
    __atomic_compare_exchange_n(p, &old_v, new_v, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return old_v;
}

static inline int32_t yia_atomic_cas_s32(volatile int32_t *p, int32_t old_v, int32_t new_v)
{
    __atomic_compare_exchange_n(p, &old_v, new_v, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return old_v;
}

static inline int32_t yia_atomic_inc_s32(volatile int32_t *p, int32_t v)
{
    return __atomic_add_fetch(p, v, __ATOMIC_ACQ_REL);
}

static inline int32_t yia_atomic_dec_s32(volatile int32_t *p, int32_t v)
{
    return __atomic_sub_fetch(p, v, __ATOMIC_ACQ_REL);
}

static inline int32_t yia_atomic_load_s32(volatile int32_t *p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static inline uint32_t yia_atomic_load_u32(volatile uint32_t *p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static inline void yia_atomic_store_s32(volatile int32_t *p, int32_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

static inline void *yia_atomic_cas_ptr(void *volatile *p, void *old_v, void *new_v)
{
    __atomic_compare_exchange_n(p, &old_v, new_v, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return old_v;
}

static inline void *yia_atomic_exchange_ptr(void *volatile *p, void *new_v)
{
    return __atomic_exchange_n(p, new_v, __ATOMIC_ACQ_REL);
}
#endif // _WIN32

// os malloc tools
#ifdef _WIN32
#define YIA_OS_MALLOC_ERROR NULL
#else
#define YIA_OS_MALLOC_ERROR MAP_FAILED
#endif

static inline void yia_os_track(void *p, size_t size, long delta)
{
    (void)size, (void)delta;
    if (YIA_UNLIKELY(p == YIA_OS_MALLOC_ERROR)) return;
#ifdef _WIN32
    /* InterlockedExchangeAdd((volatile LONG *)&g_os_live, (LONG)delta); */
    /* InterlockedExchangeAdd64((volatile LONGLONG *)&g_os_live_bytes, */
    /*                          (LONGLONG)((LONGLONG)size * (LONGLONG)delta)); */
#else
    /* __atomic_add_fetch(&g_os_live, delta, __ATOMIC_RELAXED); */
    /* __atomic_add_fetch(&g_os_live_bytes, (long long)size * (long long)delta, */
    /*                    __ATOMIC_RELAXED); */
#endif
}

static inline void *yia_os_malloc(size_t size)
{
#ifdef _WIN32
    void *p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
     yia_os_track(p, size, 1);
     return p;
}

static inline void yia_os_free(void *p, size_t size)
{
    yia_os_track(p, size, -1);
#ifdef _WIN32
    (void)size;
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, size);
#endif
}

// arena
bool yia_arena_ensure(void);
void yia_arena_release(void);

// slot
int  yia_slot_take(void);
bool yia_slot_commit(int idx);
void yia_slot_return(int idx);
int  yia_slot_of(const void *p);
void yia_retq_push(YiaSlot *slot, YiaFreeNode *node, size_t size);

// pool
bool yia_page_carve(YiaPage *page);
bool yia_pool_init(void);
void yia_pool_destroy(void);
void yia_pool_drain(void);

// large cache
void *yia_large_try_alloc(size_t round_up_size);
void yia_large_free(void *p, size_t round_up_size);
void yia_large_flush(void);

// malloc and free
YiaFreeNode*  yia_malloc_slow(size_t page_idx);

static inline YiaPage *yia_page_of_size(size_t size, int *idx)
{
    if (size == 0) return NULL;
    
    int page_idx = (size - 1) / YIA_SMALL_PAGE_STRIDE;

    if (page_idx < YIA_SMALL_PAGE_COUNT)
    {
        if (idx != NULL) *idx = page_idx;
        return &g_pool->pages[page_idx];
    }

    if (size > YIA_MEDIUM_CUTOFF)
    {
        if (idx != NULL) *idx = YIA_PAGE_INVALID_INDEX;
        return NULL;
    }

    size_t size_pow2 = yia_round_up_pow2(size);
    page_idx = yia_ctz_u32((uint32_t)size_pow2) - 9 + YIA_SMALL_PAGE_COUNT;

    if (idx != NULL) *idx = page_idx;
    return &g_pool->pages[page_idx];
}

static inline void *yia_malloc_sized_impl(size_t *size, int *page_idx, int *slot_idx)
{
    if (YIA_UNLIKELY(g_pool == NULL || !g_pool->inited) && YIA_UNLIKELY(!yia_pool_init())) return NULL;

    int idx   = YIA_PAGE_INVALID_INDEX;
    if(page_idx != NULL) *page_idx = YIA_PAGE_INVALID_INDEX;
    if(slot_idx != NULL) *slot_idx = YIA_SLOT_INVALID_INDEX;

    YiaPage *p = yia_page_of_size(*size, &idx);
    if (p == NULL)
    {
        void *raw = NULL;
        size_t round_up_size = yia_round_up_pow2(*size);
        *size = round_up_size;
        if (round_up_size <= YIA_LARGE_CUTOFF) raw = yia_large_try_alloc(round_up_size);

        if (raw == NULL)
            raw = yia_os_malloc(round_up_size); // !small pool && !medium pool -> use yia_os_malloc
        else
            return raw;

        if (raw == YIA_OS_MALLOC_ERROR) return NULL;
        return raw;
    }

    if (page_idx != NULL) *page_idx = idx;

    YiaFreeNode *node = p->free_list;
    if (node == NULL)
    {
        node = yia_malloc_slow(idx);
        if (node == NULL) return malloc(*size); // 2MB slot exhausted -> use malloc
    }
    else p->free_list = node->next;

    if (slot_idx != NULL) *slot_idx = g_pool->slot_index;

    ++g_pool->outstanding;
    return (void *)node;
}

static inline void *yia_malloc_sized(size_t size)
{
    if (size == 0) return NULL;
    return yia_malloc_sized_impl(&size, NULL, NULL);
}

static inline void yia_free_sized(void *p, size_t size)
{
    if (p == NULL || size == 0) return;

    if (size > YIA_MEDIUM_CUTOFF)
    {
        size_t round_up_size = yia_round_up_pow2(size);
        if (g_pool != NULL && g_pool->inited && round_up_size <= YIA_LARGE_CUTOFF)
        {
            yia_large_free(p, round_up_size);
            return;
        }
        yia_os_free(p, round_up_size);
        return;
    }

    int idx = yia_slot_of(p);
    if (g_pool != NULL && g_pool->inited && idx == g_pool->slot_index)
    {
        YiaPage *page = yia_page_of_size(size, NULL);
        YiaFreeNode *node = (YiaFreeNode *)p;
        node->next = page->free_list;
        page->free_list = node;
        --g_pool->outstanding;
        return;
    }

    if (idx >= 0)
    {
        yia_retq_push(&g_slot_table[idx], (YiaFreeNode *)p, size);
        return;
    }
    free(p);
}

static inline void *yia_malloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX - YIA_HEADER_SIZE) return NULL;

    size_t total = size + YIA_HEADER_SIZE;
    int page_idx = YIA_PAGE_INVALID_INDEX;
    int slot_idx = YIA_SLOT_INVALID_INDEX;
    void *block = yia_malloc_sized_impl(&total, &page_idx, &slot_idx);  // use malloc -> slot_idx = INVALID_INDEX
    if (block == NULL) return NULL;

    *(size_t *)block = total;
    *(uint64_t *)((unsigned char *)block + 8) = ((uint64_t)(uint32_t)slot_idx << 32) | (uint32_t)page_idx;
    return (unsigned char *)block + YIA_HEADER_SIZE;
}

static inline void yia_free(void *p)
{
    if (p == NULL) return;

    void *block = (unsigned char *)p - YIA_HEADER_SIZE;
    size_t size = *(size_t *)block;
    uint64_t route = *(uint64_t *)((unsigned char *)block + 8);
    int page_idx = (int32_t)(uint32_t)route;
    int slot_idx = (int32_t)(uint32_t)(route >> 32);

    if (page_idx != YIA_PAGE_INVALID_INDEX && slot_idx == YIA_SLOT_INVALID_INDEX)
    {
        free(block);
        return;
    }
    yia_free_sized(block, size);
}

static inline void *yia_calloc(size_t n, size_t size)
{
    if (n != 0 && size > SIZE_MAX / n) return NULL;

    size_t total = n * size;
    void *p = yia_malloc(total);
    if (p != NULL) memset(p, 0, total);
    return p;
}

static inline void *yia_realloc(void *p, size_t size)
{
    if (p == NULL) return yia_malloc(size);
    if (size == 0)
    {
        yia_free(p);
        return NULL;
    }
    if (size > SIZE_MAX - YIA_HEADER_SIZE) return NULL;

    void *block = (unsigned char *)p - YIA_HEADER_SIZE;
    size_t old_size = *(size_t *)block;
    size_t new_size = size + YIA_HEADER_SIZE;
    uint64_t route = *(uint64_t *)((unsigned char *)block + 8);
    int page_idx = (int32_t)(uint32_t)route;
    int slot_idx = (int32_t)(uint32_t)(route >> 32);
    size_t copy_len = (old_size < new_size ? old_size : new_size) - YIA_HEADER_SIZE;

    if (page_idx == YIA_PAGE_INVALID_INDEX) // large
    {
        new_size = yia_round_up_pow2(size + YIA_HEADER_SIZE);
        if (new_size <= old_size) return p;

        const bool cached = (g_pool != NULL && g_pool->inited);

        void *nb = NULL;
        if (cached && new_size <= YIA_LARGE_CUTOFF) nb = yia_large_try_alloc(new_size);
        if (nb == NULL) nb = yia_os_malloc(new_size);
        if (nb == YIA_OS_MALLOC_ERROR) return NULL;

        *(size_t *)nb = new_size;
        *(uint64_t *)((unsigned char *)nb + 8) = ((uint64_t)(uint32_t)YIA_SLOT_INVALID_INDEX << 32) | (uint32_t)YIA_PAGE_INVALID_INDEX;

        void *np = (void *)((unsigned char *)nb + YIA_HEADER_SIZE);
        memcpy(np, p, old_size - YIA_HEADER_SIZE);

        if (cached && old_size <= YIA_LARGE_CUTOFF) yia_large_free(block, old_size);
        else yia_os_free(block, old_size);

        return np;
    }
    else if (slot_idx == YIA_SLOT_INVALID_INDEX) // realloc
    {
        void *nb = realloc(block, new_size);
        if (nb == NULL) return NULL;

        *(size_t *)nb = new_size;
        return (unsigned char *)nb + YIA_HEADER_SIZE;
    }

    // yia_malloc
    // new_size <= page size
    size_t cap = 0;
    if (page_idx < YIA_SMALL_PAGE_COUNT) cap = (page_idx + 1) * YIA_SMALL_PAGE_STRIDE;
    else cap = (YIA_SMALL_CUTOFF * 2) << (page_idx - YIA_SMALL_PAGE_COUNT);

    if (new_size <= cap)
    {
        *(size_t *)block = new_size;
        return p;
    }

    // new_size > page size
    void *np = yia_malloc(size);
    if (np == NULL) return NULL;
    memcpy(np, p, copy_len);
    yia_free(p);
    return np;
}

#ifdef __cplusplus
}
#endif

#endif // !YIA_MALLOC_H
