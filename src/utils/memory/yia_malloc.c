#define _DEFAULT_SOURCE
#include "yia_malloc.h"

#include <stdio.h>

void *volatile g_arena = NULL;
YiaSlot g_slot_table[YIA_SLOT_COUNT] = {0};
static YiaPool g_pool_table[YIA_SLOT_COUNT] = {0};
YIA_TLS YiaPool *g_pool = NULL;

static volatile uint32_t g_slot_free_map = 0xFFFFFFFFu; // 1(free), 0(busy)
static volatile int32_t g_slots_busy = 0;
// TODO: 保留：g_os_live / g_os_live_bytes(泄漏检测)

// os malloc tools
static void *yia_os_reserve(size_t size)
{
#ifdef _WIN32
    void *p = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
#else
    void *p = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
#endif
    yia_os_track(p, size, 1);
    return p;
}

static bool yia_os_commit(void *p, size_t size)
{
#ifdef _WIN32
    return VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    return mprotect(p, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

static void yia_os_decommit(void *p, size_t size)
{
#ifdef _WIN32
    VirtualFree(p, size, MEM_DECOMMIT);
#else
    mprotect(p, size, PROT_NONE);
    madvise(p, size, MADV_DONTNEED);
#endif
}

static void yia_os_release(void *p, size_t size)
{
    yia_os_track(p, size, -1);
#ifdef _WIN32
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, size);
#endif
}

// page cleanup
static void yia_lifecycle_unregister(void);

#ifdef _WIN32
static DWORD g_fls_index = FLS_OUT_OF_INDEXES;

static void CALLBACK yia_fls_dtor(PVOID data)
{
    yia_lifecycle_unregister();
    int idx = *(int *)data;
    if (idx == YIA_SLOT_INVALID_INDEX) return;

    YiaSlot *slot = &g_slot_table[idx];
    YiaPool *pool = &g_pool_table[idx];

    // pool_drain
    size_t n = 0;
    YiaFreeNode *chain = (YiaFreeNode *)yia_atomic_exchange_ptr((void *volatile *)&slot->retqueue, NULL);
    while (chain != NULL)
    {
        YiaFreeNode *next = chain->next;
        ++n;
        chain = next;
    }
    pool->outstanding -= n;

    // large_flush
    for (size_t i = 0; i < YIA_LARGE_BUCKET_COUNT; ++i)
    {
        YiaFreeNode *node = pool->large_cache.buckets[i];
        while (node != NULL)
        {
            YiaFreeNode *next = node->next;
            yia_os_free(node, (YIA_MEDIUM_CUTOFF << (i + 1)));
            node = next;
        }
        pool->large_cache.buckets[i] = NULL;
        pool->large_cache.count[i] = 0;
    }
    pool->large_cache.total_bytes = 0;

    if (pool->outstanding == 0)
    {
        pool->inited = false;
        pool->slot_index = YIA_SLOT_INVALID_INDEX;
        pool->slot_used = 0;
        yia_slot_return(idx);
    }
    else
    {
        yia_atomic_store_s32(&slot->outstanding, (int32_t)pool->outstanding);
        yia_atomic_store_s32(&slot->orphaned, 1);
    }
}

static bool yia_lifecycle_register(void)
{
    if (g_fls_index == FLS_OUT_OF_INDEXES)
    {
        DWORD idx = FlsAlloc(yia_fls_dtor);
        if (idx == FLS_OUT_OF_INDEXES) return false;
        DWORD expected = FLS_OUT_OF_INDEXES;
        if (InterlockedCompareExchange((volatile LONG *)&g_fls_index, (LONG)idx, (LONG)expected) != (LONG)expected)
        {
            FlsFree(idx);
        }
    }
    return FlsSetValue(g_fls_index, (void *)&g_pool->slot_index) != 0;
}

static void yia_lifecycle_unregister(void)
{
    if (g_fls_index == FLS_OUT_OF_INDEXES) return;
    FlsSetValue(g_fls_index, NULL);
}
#else
pthread_once_t g_key_once = PTHREAD_ONCE_INIT;
pthread_key_t  g_lifecycle_key = 0;

static void yia_lifecycle_dtor(void *data)
{
    yia_lifecycle_unregister();
    int idx = *(int *)data;
    if (idx == YIA_SLOT_INVALID_INDEX) return;

    YiaSlot *slot = &g_slot_table[idx];
    YiaPool *pool = &g_pool_table[idx];

    // pool_drain
    size_t n = 0;
    YiaFreeNode *chain = (YiaFreeNode *)yia_atomic_exchange_ptr((void *volatile *)&slot->retqueue, NULL);
    while (chain != NULL)
    {
        YiaFreeNode *next = chain->next;
        ++n;
        chain = next;
    }
    pool->outstanding -= n;

    // large_flush
    for (size_t i = 0; i < YIA_LARGE_BUCKET_COUNT; ++i)
    {
        YiaFreeNode *node = pool->large_cache.buckets[i];
        while (node != NULL)
        {
            YiaFreeNode *next = node->next;
            yia_os_free(node, (YIA_MEDIUM_CUTOFF << (i + 1)));
            node = next;
        }
        pool->large_cache.buckets[i] = NULL;
        pool->large_cache.count[i] = 0;
    }
    pool->large_cache.total_bytes = 0;

    if (pool->outstanding == 0)
    {
        pool->inited = false;
        pool->slot_index = YIA_SLOT_INVALID_INDEX;
        pool->slot_used = 0;
        yia_slot_return(idx);
    }
    else
    {
        yia_atomic_store_s32(&slot->outstanding, (int32_t)pool->outstanding);
        yia_atomic_store_s32(&slot->orphaned, 1);
    }
}

static void yia_lifecycle_key_init(void)
{
    pthread_key_create(&g_lifecycle_key, yia_lifecycle_dtor);
}

static bool yia_lifecycle_register(void)
{
    pthread_once(&g_key_once, yia_lifecycle_key_init);
    return pthread_setspecific(g_lifecycle_key, (void *)&g_pool->slot_index) == 0;
}

static void yia_lifecycle_unregister(void)
{
    pthread_setspecific(g_lifecycle_key, NULL);
}
#endif

// page tools
static bool yia_reclaim_orphan(void)
{
    for (int i = 0; i < YIA_SLOT_COUNT; ++i)
    {
        YiaSlot *s = &g_slot_table[i];
        if (yia_atomic_load_s32(&s->orphaned) == 0) continue;
        if (yia_atomic_cas_s32(&s->orphaned, 1, 0) != 1) continue;

        YiaFreeNode *chain = (YiaFreeNode *)yia_atomic_exchange_ptr((void *volatile *)&s->retqueue, NULL);
        int32_t n = 0;
        for (YiaFreeNode *c = chain; c != NULL; c = c->next) ++n;
        
        if (yia_atomic_load_s32(&s->outstanding) <= (int32_t)n)
        {
            YiaPool *pool = &g_pool_table[i];
            pool->inited = false;
            pool->outstanding = 0;
            pool->slot_index = YIA_SLOT_INVALID_INDEX;
            pool->slot_used = 0;

            yia_slot_return(i);
            return true;
        }

        yia_atomic_dec_s32(&s->outstanding, n);
        yia_atomic_store_s32(&s->orphaned, 1);
    }
    return false;
}

bool yia_arena_ensure(void)
{
    if (YIA_LIKELY(g_arena != NULL)) return true;

    void *arena = yia_os_reserve(YIA_ARENA_SIZE);
    if (YIA_UNLIKELY(arena == YIA_OS_MALLOC_ERROR)) return false;

    if (yia_atomic_cas_ptr((void *volatile *)&g_arena, NULL, arena) != NULL)
        yia_os_release(arena, YIA_ARENA_SIZE);

    return true;
}

void yia_arena_release(void)
{
    void *arena = yia_atomic_exchange_ptr(&g_arena, NULL);
    if (arena == NULL || arena == YIA_OS_MALLOC_ERROR) return;
    yia_os_release(arena, YIA_ARENA_SIZE);
}

int yia_slot_take(void)
{
    for (;;)
    {
        uint32_t old = g_slot_free_map;
        if (YIA_UNLIKELY(old == 0))
        {
            if (!yia_reclaim_orphan()) return YIA_SLOT_INVALID_INDEX;
            continue;
        }

        int idx = yia_ctz_u32(old);
        uint32_t new = old & ~(1u << (uint32_t)idx);
        if (yia_atomic_cas_u32(&g_slot_free_map, old, new) == old)
        {
            yia_atomic_inc_s32(&g_slots_busy, 1);
            return idx;
        }
    }
}

bool yia_slot_commit(int idx)
{
    return yia_os_commit((unsigned char *)g_arena + (size_t)idx * YIA_SLOT_SIZE, YIA_SLOT_SIZE);
}

void yia_slot_return(int idx)
{
    YiaSlot *slot = &g_slot_table[idx];
    slot->retqueue = NULL;
    slot->owner = 0;
    yia_atomic_store_s32(&slot->outstanding, 0);
    yia_atomic_store_s32(&slot->orphaned, 0);

    yia_os_decommit((unsigned char *)g_arena + (size_t)idx * YIA_SLOT_SIZE, YIA_SLOT_SIZE);

    for (;;)
    {
        uint32_t old = g_slot_free_map;
        uint32_t new = old | (1u << (uint32_t)idx);
        if (yia_atomic_cas_u32(&g_slot_free_map, old, new) == old) break;
    }
    yia_atomic_dec_s32(&g_slots_busy, 1);
}

int yia_slot_of(const void *p)
{
    uintptr_t addr_a = (uintptr_t)g_arena;
    if (YIA_UNLIKELY(addr_a == 0)) return YIA_SLOT_INVALID_INDEX;

    uintptr_t addr_p = (uintptr_t)p;
    if (addr_p < addr_a) return YIA_SLOT_INVALID_INDEX;

    uintptr_t offset = addr_p - addr_a;
    if (offset >= YIA_ARENA_SIZE) return YIA_SLOT_INVALID_INDEX;

    return (int)(offset >> YIA_SLOT_SHIFT);
}

bool yia_page_carve(YiaPage *page)
{
    size_t bk_count = page->block_count;
    size_t seg_size = bk_count * page->block_size;
    if (g_pool->slot_used + seg_size > YIA_SLOT_SIZE) return false;

    unsigned char *seg = (unsigned char *)g_arena +
                         (size_t)g_pool->slot_index * YIA_SLOT_SIZE +
                         g_pool->slot_used;
    g_pool->slot_used += seg_size;

    YiaFreeNode *first = (YiaFreeNode *)seg;
    YiaFreeNode *prev = first;
    for (size_t i = 1; i < bk_count; ++i)
    {
        YiaFreeNode *node = (YiaFreeNode *)(seg + i * page->block_size);
        prev->next = node;
        prev = node;
    }
    prev->next = page->free_list;
    page->free_list = first;

    ++page->segment_count;
    return true;
}

bool yia_pool_init(void)
{
    if (!yia_arena_ensure()) return false;
    if (g_pool != NULL && g_pool->inited) return true;

    int idx = yia_slot_take();
    if (YIA_UNLIKELY(idx == YIA_SLOT_INVALID_INDEX)) return false;
    if (YIA_UNLIKELY(!yia_slot_commit(idx)))
    {
        yia_slot_return(idx);
        return false;
    }
    g_pool = &g_pool_table[idx];

    g_pool->slot_index = idx;
    g_pool->slot_used = 0;
    g_pool->outstanding = 0;

    YiaSlot *slot = &g_slot_table[idx];
    slot->retqueue = NULL;
    slot->orphaned = 0;
    slot->outstanding = 0;
#ifdef _WIN32
    slot->owner = (uint64_t)GetCurrentThreadId();
#else
    slot->owner = (uint64_t)pthread_self();
#endif

    /* // init page (16(PAGE_STRIDE)...256(PAGE_STRIDE * YIA_SMALL_PAGE_COUNT)) */
    /* for (size_t i = 0; i < YIA_SMALL_PAGE_COUNT; ++i) */
    /* { */
    /*     YiaPage *p = &g_pool->pages[i]; */
    /*     p->free_list = NULL; */
    /*     p->block_size = YIA_SMALL_PAGE_STRIDE * (i + 1); */
    /*     p->block_count = 256; */
    /*     p->segment_count = 0; */
    /*     if (!yia_page_carve(p)) */
    /*     { */
    /*         yia_slot_return(idx); */
    /*         g_pool->slot_index = YIA_SLOT_INVALID_INDEX; */
    /*         return false; */
    /*     } */
    /* } */

    /* // init page (512...8192 (256 << 5)) */
    /* size_t medium_size = YIA_SMALL_CUTOFF * 2; */
    /* for (size_t i = YIA_SMALL_PAGE_COUNT; i < YIA_SMALL_PAGE_COUNT + YIA_MEDIUM_PAGE_COUNT; ++i) */
    /* { */
    /*     YiaPage *p = &g_pool->pages[i]; */
    /*     p->free_list = NULL; */
    /*     p->block_size = medium_size; */
    /*     p->block_count = medium_size <= 2048 ? 256 : 256 * 1024 / medium_size; */
    /*     p->segment_count = 0; */
    /*     if (!yia_page_carve(p)) */
    /*     { */
    /*         yia_slot_return(idx); */
    /*         g_pool->slot_index = YIA_SLOT_INVALID_INDEX; */
    /*         return false; */
    /*     } */
    /*     medium_size *= 2; */
    /* } */

    /* // init large cache */
    /* for (size_t i = 0; i < YIA_LARGE_BUCKET_COUNT; ++i) */
    /* { */
    /*     g_pool->large_cache.buckets[i] = NULL; */
    /*     g_pool->large_cache.count[i] = 0; */
    /*     g_pool->large_cache.total_bytes = 0; */
    /* }       */

    for (size_t i = 0; i < YIA_SMALL_PAGE_COUNT; ++i)
    {
        YiaPage *p = &g_pool->pages[i];
        p->free_list     = NULL;
        p->block_size    = YIA_SMALL_PAGE_STRIDE * (i + 1);
        p->block_count   = 256;
        p->segment_count = 0;
    }

    // init page (512...8192 (256 << 5))
    size_t medium_size = YIA_SMALL_CUTOFF * 2;
    for (size_t i = YIA_SMALL_PAGE_COUNT; i < YIA_SMALL_PAGE_COUNT + YIA_MEDIUM_PAGE_COUNT; ++i)
    {
        YiaPage *p = &g_pool->pages[i];
        p->free_list     = NULL;
        p->block_size    = medium_size;
        p->block_count   = medium_size <= 2048 ? 256 : 256 * 1024 / medium_size;
        p->segment_count = 0;
        medium_size *= 2;
    }

    // init large cache
    for (size_t i = 0; i < YIA_LARGE_BUCKET_COUNT; ++i)
    {
        g_pool->large_cache.buckets[i] = NULL;
        g_pool->large_cache.count[i] = 0;
        g_pool->large_cache.total_bytes = 0;
    }

    if (!yia_lifecycle_register())
    {
        yia_slot_return(idx);
        g_pool->slot_index = YIA_SLOT_INVALID_INDEX;
        return false;
    }
 
    g_pool->inited = true;
    return true;
}

void yia_pool_destroy(void)
{
    if (g_pool == NULL || !g_pool->inited) return;

    int idx = g_pool->slot_index;
    yia_pool_drain();
    if (g_pool->outstanding != 0)
    {
        fprintf(stderr, "[yia] destroy: slot %d outstanding=%zu, keep slot\n", idx, g_pool->outstanding);
        return;
    }

    yia_large_flush();
    yia_slot_return(idx);
    yia_lifecycle_unregister();
    g_pool->slot_index = YIA_SLOT_INVALID_INDEX;
    g_pool->slot_used = 0;
    g_pool->inited = false;
}

void yia_retq_push(YiaSlot *slot, YiaFreeNode *node, size_t size)
{
    // storage size
    *(size_t *)((unsigned char *)node + 8) = size;

    YiaFreeNode *head = slot->retqueue;
    for (;;)
    {
        node->next = head;
        if (yia_atomic_cas_ptr((void *volatile *)&slot->retqueue, head, node) == head)
            break;
        head = slot->retqueue;
    }
}

void yia_pool_drain(void)
{
    size_t n = 0;
    YiaSlot *slot = &g_slot_table[g_pool->slot_index];
    YiaFreeNode *chain = (YiaFreeNode *)yia_atomic_exchange_ptr((void *volatile *)&slot->retqueue, NULL);
    while (chain != NULL)
    {
        YiaFreeNode *next = chain->next;
        size_t size = *(size_t *)((unsigned char *)chain + 8);
        YiaPage *page = yia_page_of_size(size, NULL);
        if (page != NULL)
        {
            chain->next = page->free_list;
            page->free_list = chain;
            ++n;
        }
        chain = next;
    }
    g_pool->outstanding -= n;
}

void *yia_large_try_alloc(size_t round_up_size)
{
    YiaLargeCache *c = &g_pool->large_cache;
    int idx = yia_ctz_u32((uint32_t)round_up_size) - YIA_LARGE_BUCKET_SHIFT;
    YiaFreeNode *b = c->buckets[idx];
    if (b == NULL) return NULL;

    c->buckets[idx] = b->next;
    c->total_bytes -= round_up_size;
    --c->count[idx];
    return (void *)b;
}

void yia_large_free(void *p, size_t round_up_size)
{
    YiaLargeCache *c = &g_pool->large_cache;
    int idx = yia_ctz_u32((uint32_t)round_up_size) - YIA_LARGE_BUCKET_SHIFT;
    YiaFreeNode *node = (YiaFreeNode *)p;

    node->next = c->buckets[idx];
    c->buckets[idx] = node;
    c->total_bytes += round_up_size;
    ++c->count[idx];

    if (c->total_bytes >= YIA_LARGE_MAX_SIZE) yia_large_flush();
}

void yia_large_flush(void)
{
    for (size_t i = 0; i < YIA_LARGE_BUCKET_COUNT; ++i)
    {
        YiaFreeNode *node = g_pool->large_cache.buckets[i];
        while (node != NULL)
        {
            YiaFreeNode *next = node->next;
            yia_os_free(node, (YIA_MEDIUM_CUTOFF << (i + 1)));
            node = next;
        }
        g_pool->large_cache.buckets[i] = NULL;
        g_pool->large_cache.count[i] = 0;
    }
    g_pool->large_cache.total_bytes = 0;
}

YiaFreeNode *yia_malloc_slow(size_t page_idx)
{
    yia_pool_drain();
    YiaPage *page = &g_pool->pages[page_idx];
    if (page->free_list == NULL && !yia_page_carve(page)) return NULL;

    YiaFreeNode *node = page->free_list;
    page->free_list = node->next;
    return node;
}
