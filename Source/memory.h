#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "browser.h"
#include <psl1ght/lv2/memory.h>

// ═══════════════════════════════════════════════════════════════
//  ثوابت إدارة الذاكرة
// ═══════════════════════════════════════════════════════════════
#define MEMORY_ALIGNMENT        16
#define MEMORY_GUARD_SIZE       4
#define MEMORY_GUARD_PATTERN    0xDEADBEEF
#define MEMORY_FREE_PATTERN     0xFEEEFEEE
#define MEMORY_ALLOC_PATTERN    0xCDCDCDCD
#define MEMORY_MAX_BLOCKS       2048
#define MEMORY_MAX_POOLS        8

// ═══════════════════════════════════════════════════════════════
//  هيكل كتلة الذاكرة
// ═══════════════════════════════════════════════════════════════
typedef struct MemoryBlock MemoryBlock;
struct MemoryBlock {
    size_t          size;
    size_t          requested_size;
    int             free;
    u32             magic;
    const char*     file;
    int             line;
    const char*     tag;
    MemoryBlock*    next;
    MemoryBlock*    prev;
    u64             alloc_time;
    u32             alloc_id;
};

// ═══════════════════════════════════════════════════════════════
//  هيكل مجمع الذاكرة (Memory Pool)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    // الذاكرة الأساسية
    u8*             base;
    size_t          capacity;
    size_t          used;
    size_t          peak_used;
    size_t          wasted;

    // قائمة الكتل
    MemoryBlock*    first_block;
    MemoryBlock*    last_block;
    MemoryBlock*    free_list;
    int             block_count;
    int             free_block_count;

    // إحصائيات
    u32             total_allocs;
    u32             total_frees;
    u32             failed_allocs;
    u32             current_allocs;
    size_t          total_allocated;
    size_t          total_freed;

    // الإعدادات
    int             debug_mode;
    int             allow_growth;
    const char*     name;
    int             id;

    // المزامنة
    int             locked;

} MemoryPool;

// ═══════════════════════════════════════════════════════════════
//  هيكل مخصص الإطار (Frame Allocator) - لتخصيصات مؤقتة سريعة
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u8*     base;
    size_t  capacity;
    size_t  offset;
    size_t  saved_offset;
    int     frame_count;
} FrameAllocator;

// ═══════════════════════════════════════════════════════════════
//  هيكل مجمع الأحجام الثابتة (Fixed-Size Pool)
// ═══════════════════════════════════════════════════════════════
typedef struct {
    u8*     base;
    size_t  block_size;
    int     block_count;
    int     free_count;
    u8*     free_list[MEMORY_MAX_BLOCKS];
    int     free_list_count;
} FixedPool;

// ═══════════════════════════════════════════════════════════════
//  واجهة برمجة الذاكرة
// ═══════════════════════════════════════════════════════════════
#ifdef __cplusplus
extern "C" {
#endif

// مجمعات الذاكرة
MemoryPool*  memory_pool_create      (size_t capacity);
void         memory_pool_destroy     (MemoryPool* pool);
void         memory_pool_reset       (MemoryPool* pool);
void*        memory_pool_alloc       (MemoryPool* pool, size_t size);
void*        memory_pool_alloc_zero  (MemoryPool* pool, size_t size);
void*        memory_pool_realloc     (MemoryPool* pool, void* ptr, size_t new_size);
void         memory_pool_free        (MemoryPool* pool, void* ptr);
void         memory_pool_defrag      (MemoryPool* pool);
size_t       memory_pool_available   (MemoryPool* pool);
size_t       memory_pool_used        (MemoryPool* pool);
f32          memory_pool_usage_pct   (MemoryPool* pool);
void         memory_pool_print_stats (MemoryPool* pool);
int          memory_pool_contains    (MemoryPool* pool, void* ptr);
void         memory_pool_set_name    (MemoryPool* pool, const char* name);

// مخصص الإطار
FrameAllocator* frame_alloc_create   (size_t capacity);
void            frame_alloc_destroy  (FrameAllocator* fa);
void*           frame_alloc          (FrameAllocator* fa, size_t size);
void*           frame_alloc_zero     (FrameAllocator* fa, size_t size);
void            frame_alloc_reset    (FrameAllocator* fa);
void            frame_alloc_save     (FrameAllocator* fa);
void            frame_alloc_restore  (FrameAllocator* fa);

// مجمعات الأحجام الثابتة
FixedPool*   fixed_pool_create       (size_t block_size, int count);
void         fixed_pool_destroy      (FixedPool* pool);
void*        fixed_pool_alloc        (FixedPool* pool);
void         fixed_pool_free         (FixedPool* pool, void* ptr);
int          fixed_pool_available    (FixedPool* pool);

// أدوات الذاكرة
void*        mem_align               (void* ptr, size_t alignment);
size_t       mem_align_size          (size_t size, size_t alignment);
void         mem_zero                (void* ptr, size_t size);
void         mem_copy                (void* dst, const void* src, size_t size);
void         mem_move                (void* dst, const void* src, size_t size);
int          mem_compare             (const void* a, const void* b, size_t size);
void         mem_set                 (void* ptr, u8 val, size_t size);
void         mem_copy_u32            (u32* dst, const u32* src, size_t count);
void         mem_set_u32             (u32* dst, u32 val, size_t count);

// إحصائيات الذاكرة الكلية للنظام
size_t       system_memory_total     (void);
size_t       system_memory_free      (void);
size_t       system_memory_used      (void);
f32          system_memory_usage_pct (void);
void         system_memory_print     (void);

// ماكرو مساعدة للتخصيص مع تتبع الموقع
#define MEM_ALLOC(pool, size)       memory_pool_alloc((pool), (size))
#define MEM_ALLOC_Z(pool, size)     memory_pool_alloc_zero((pool), (size))
#define MEM_FREE(pool, ptr)         memory_pool_free((pool), (ptr))
#define MEM_REALLOC(pool, ptr, sz)  memory_pool_realloc((pool), (ptr), (sz))
#define MEM_NEW(pool, type)         ((type*)memory_pool_alloc_zero((pool), sizeof(type)))
#define MEM_NEW_ARR(pool, type, n)  ((type*)memory_pool_alloc_zero((pool), sizeof(type)*(n)))

#ifdef __cplusplus
}
#endif

#endif // MEMORY_H
