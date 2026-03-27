// ═══════════════════════════════════════════════════════════════
//  PS3 UltraBrowser - memory.cpp
//  نظام إدارة الذاكرة المتقدم
// ═══════════════════════════════════════════════════════════════

#include "memory.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// ═══════════════════════════════════════════════════════════════
//  دوال مساعدة داخلية
// ═══════════════════════════════════════════════════════════════

static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline int is_aligned(void* ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

static inline MemoryBlock* ptr_to_block(void* ptr) {
    return (MemoryBlock*)((u8*)ptr - sizeof(MemoryBlock));
}

static inline void* block_to_ptr(MemoryBlock* block) {
    return (void*)((u8*)block + sizeof(MemoryBlock));
}

static inline size_t block_total_size(size_t data_size) {
    return align_up(sizeof(MemoryBlock) + data_size + MEMORY_GUARD_SIZE,
                    MEMORY_ALIGNMENT);
}

// ═══════════════════════════════════════════════════════════════
//  إنشاء مجمع الذاكرة
// ═══════════════════════════════════════════════════════════════
MemoryPool* memory_pool_create(size_t capacity) {
    // تخصيص الذاكرة الأساسية مع محاذاة
    u8* raw = (u8*)memalign(MEMORY_ALIGNMENT, sizeof(MemoryPool) + capacity);
    if (!raw) {
        printf("[ذاكرة] فشل تخصيص %zu بايت\n", capacity);
        return NULL;
    }

    MemoryPool* pool = (MemoryPool*)raw;
    memset(pool, 0, sizeof(MemoryPool));

    pool->base        = raw + sizeof(MemoryPool);
    pool->capacity    = capacity;
    pool->used        = 0;
    pool->peak_used   = 0;
    pool->wasted      = 0;
    pool->first_block = NULL;
    pool->last_block  = NULL;
    pool->free_list   = NULL;
    pool->block_count = 0;
    pool->debug_mode  = 0;
    pool->allow_growth= 0;
    pool->name        = "unnamed";

    // تهيئة الكتلة الأولى الفارغة
    MemoryBlock* initial = (MemoryBlock*)pool->base;
    initial->size          = capacity - sizeof(MemoryBlock);
    initial->requested_size= 0;
    initial->free          = 1;
    initial->magic         = MEMORY_FREE_PATTERN;
    initial->next          = NULL;
    initial->prev          = NULL;
    initial->file          = NULL;
    initial->line          = 0;
    initial->tag           = NULL;
    initial->alloc_time    = 0;
    initial->alloc_id      = 0;

    pool->first_block = initial;
    pool->last_block  = initial;
    pool->free_list   = initial;

    printf("[ذاكرة] تم إنشاء مجمع '%s' بحجم %zu KB\n",
           pool->name, capacity / 1024);
    return pool;
}

// ═══════════════════════════════════════════════════════════════
//  تدمير مجمع الذاكرة
// ═══════════════════════════════════════════════════════════════
void memory_pool_destroy(MemoryPool* pool) {
    if (!pool) return;
    if (pool->current_allocs > 0) {
        printf("[تحذير ذاكرة] تدمير مجمع '%s' مع %u تخصيص نشط!\n",
               pool->name, pool->current_allocs);
    }
    printf("[ذاكرة] تدمير مجمع '%s' - استخدم %zu/%zu بايت (%.1f%%)\n",
           pool->name, pool->peak_used, pool->capacity,
           (f32)pool->peak_used * 100.0f / (f32)pool->capacity);
    free(pool);
}

// ═══════════════════════════════════════════════════════════════
//  إعادة تعيين مجمع الذاكرة
// ═══════════════════════════════════════════════════════════════
void memory_pool_reset(MemoryPool* pool) {
    if (!pool) return;
    memset(pool->base, 0, pool->capacity);
    pool->used          = 0;
    pool->current_allocs= 0;
    pool->total_frees  += pool->current_allocs;
    pool->wasted        = 0;

    MemoryBlock* initial = (MemoryBlock*)pool->base;
    initial->size  = pool->capacity - sizeof(MemoryBlock);
    initial->free  = 1;
    initial->magic = MEMORY_FREE_PATTERN;
    initial->next  = NULL;
    initial->prev  = NULL;

    pool->first_block = initial;
    pool->last_block  = initial;
    pool->free_list   = initial;
    pool->block_count = 1;
}

// ═══════════════════════════════════════════════════════════════
//  تخصيص ذاكرة من المجمع
// ═══════════════════════════════════════════════════════════════
void* memory_pool_alloc(MemoryPool* pool, size_t size) {
    if (!pool || size == 0) return NULL;

    // محاذاة الحجم
    size_t aligned = align_up(size, MEMORY_ALIGNMENT);
    size_t needed  = sizeof(MemoryBlock) + aligned + MEMORY_GUARD_SIZE;

    // البحث عن كتلة حرة مناسبة (first-fit)
    MemoryBlock* block = pool->free_list;
    while (block) {
        if (block->free && block->size >= needed) break;
        block = block->next;
    }

    // إذا لم نجد كتلة
    if (!block) {
        pool->failed_allocs++;
        printf("[خطأ ذاكرة] لا توجد ذاكرة كافية في مجمع '%s' "
               "(طلب: %zu، متاح: %zu)\n",
               pool->name, size, memory_pool_available(pool));
        return NULL;
    }

    // تقسيم الكتلة إذا كانت أكبر من اللازم
    size_t leftover = block->size - needed;
    if (leftover > sizeof(MemoryBlock) + MEMORY_ALIGNMENT * 4) {
        MemoryBlock* split = (MemoryBlock*)((u8*)block_to_ptr(block) + aligned + MEMORY_GUARD_SIZE);
        split->size  = leftover - sizeof(MemoryBlock);
        split->free  = 1;
        split->magic = MEMORY_FREE_PATTERN;
        split->next  = block->next;
        split->prev  = block;
        split->file  = NULL;
        split->line  = 0;
        split->tag   = NULL;

        if (block->next) block->next->prev = split;
        else             pool->last_block  = split;
        block->next = split;
        block->size = needed - sizeof(MemoryBlock);
        pool->block_count++;
    }

    // تحديد الكتلة كمستخدمة
    block->free           = 0;
    block->magic          = MEMORY_ALLOC_PATTERN;
    block->requested_size = size;

    // كلمة حراسة في النهاية
    u32* guard = (u32*)((u8*)block_to_ptr(block) + aligned);
    *guard = MEMORY_GUARD_PATTERN;

    // تحديث الإحصائيات
    pool->used          += needed;
    pool->total_allocs++;
    pool->current_allocs++;
    pool->total_allocated += size;
    if (pool->used > pool->peak_used) pool->peak_used = pool->used;

    return block_to_ptr(block);
}

// ═══════════════════════════════════════════════════════════════
//  تخصيص ذاكرة مُصفَّرة
// ═══════════════════════════════════════════════════════════════
void* memory_pool_alloc_zero(MemoryPool* pool, size_t size) {
    void* ptr = memory_pool_alloc(pool, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

// ═══════════════════════════════════════════════════════════════
//  إعادة تخصيص ذاكرة
// ═══════════════════════════════════════════════════════════════
void* memory_pool_realloc(MemoryPool* pool, void* ptr, size_t new_size) {
    if (!ptr)     return memory_pool_alloc(pool, new_size);
    if (!new_size){ memory_pool_free(pool, ptr); return NULL; }

    MemoryBlock* block = ptr_to_block(ptr);
    if (block->magic != MEMORY_ALLOC_PATTERN) {
        printf("[خطأ ذاكرة] realloc على مؤشر غير صالح!\n");
        return NULL;
    }

    // إذا كان الحجم الجديد أصغر أو يناسب الكتلة الحالية
    if (new_size <= block->requested_size) return ptr;

    // تخصيص جديد ونسخ
    void* new_ptr = memory_pool_alloc(pool, new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->requested_size);
    memory_pool_free(pool, ptr);
    return new_ptr;
}

// ═══════════════════════════════════════════════════════════════
//  تحرير ذاكرة
// ═══════════════════════════════════════════════════════════════
void memory_pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) return;

    MemoryBlock* block = ptr_to_block(ptr);

    // فحص صحة الكتلة
    if (block->magic != MEMORY_ALLOC_PATTERN) {
        printf("[خطأ ذاكرة] تحرير مؤشر غير صالح أو محرر مسبقاً!\n");
        return;
    }

    // فحص كلمة الحراسة
    size_t aligned = align_up(block->requested_size, MEMORY_ALIGNMENT);
    u32*   guard   = (u32*)((u8*)ptr + aligned);
    if (*guard != MEMORY_GUARD_PATTERN) {
        printf("[خطأ ذاكرة] تجاوز حدود الذاكرة في كتلة!\n");
    }

    // تحديد الكتلة كحرة
    block->free  = 1;
    block->magic = MEMORY_FREE_PATTERN;
    memset(ptr, MEMORY_FREE_PATTERN & 0xFF, block->requested_size);

    // تحديث الإحصائيات
    size_t needed = sizeof(MemoryBlock) + align_up(block->requested_size, MEMORY_ALIGNMENT) + MEMORY_GUARD_SIZE;
    if (pool->used >= needed) pool->used -= needed;
    pool->total_frees++;
    pool->current_allocs--;
    pool->total_freed += block->requested_size;

    // دمج الكتل المجاورة الحرة (coalescing)
    // دمج مع الكتلة التالية
    if (block->next && block->next->free) {
        MemoryBlock* next = block->next;
        block->size += sizeof(MemoryBlock) + next->size;
        block->next  = next->next;
        if (next->next) next->next->prev = block;
        else            pool->last_block = block;
        pool->block_count--;
    }

    // دمج مع الكتلة السابقة
    if (block->prev && block->prev->free) {
        MemoryBlock* prev = block->prev;
        prev->size += sizeof(MemoryBlock) + block->size;
        prev->next  = block->next;
        if (block->next) block->next->prev = prev;
        else             pool->last_block  = prev;
        pool->block_count--;
    }

    // تحديث قائمة الكتل الحرة
    pool->free_list = pool->first_block;
    while (pool->free_list && !pool->free_list->free)
        pool->free_list = pool->free_list->next;
}

// ═══════════════════════════════════════════════════════════════
//  إزالة التشتت (Defragmentation)
// ═══════════════════════════════════════════════════════════════
void memory_pool_defrag(MemoryPool* pool) {
    if (!pool) return;
    MemoryBlock* block = pool->first_block;
    int merged = 0;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += sizeof(MemoryBlock) + block->next->size;
            block->next  = block->next->next;
            if (block->next) block->next->prev = block;
            else             pool->last_block  = block;
            pool->block_count--;
            merged++;
        } else {
            block = block->next;
        }
    }
    if (merged > 0)
        printf("[ذاكرة] دمج %d كتلة في مجمع '%s'\n", merged, pool->name);
}

// ═══════════════════════════════════════════════════════════════
//  معلومات المجمع
// ═══════════════════════════════════════════════════════════════
size_t memory_pool_available(MemoryPool* pool) {
    if (!pool) return 0;
    return pool->capacity > pool->used ? pool->capacity - pool->used : 0;
}

size_t memory_pool_used(MemoryPool* pool) {
    return pool ? pool->used : 0;
}

f32 memory_pool_usage_pct(MemoryPool* pool) {
    if (!pool || pool->capacity == 0) return 0.0f;
    return (f32)pool->used * 100.0f / (f32)pool->capacity;
}

void memory_pool_set_name(MemoryPool* pool, const char* name) {
    if (pool) pool->name = name;
}

int memory_pool_contains(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) return 0;
    u8* p = (u8*)ptr;
    return p >= pool->base && p < pool->base + pool->capacity;
}

void memory_pool_print_stats(MemoryPool* pool) {
    if (!pool) return;
    printf("╔══════════════════════════════════════╗\n");
    printf("║ مجمع الذاكرة: %-22s ║\n", pool->name);
    printf("╠══════════════════════════════════════╣\n");
    printf("║ الحجم الكلي:    %8zu KB          ║\n", pool->capacity / 1024);
    printf("║ المستخدم:       %8zu KB (%.1f%%)  ║\n",
           pool->used / 1024, memory_pool_usage_pct(pool));
    printf("║ الذروة:         %8zu KB          ║\n", pool->peak_used / 1024);
    printf("║ التخصيصات:      %8u              ║\n", pool->total_allocs);
    printf("║ التحريرات:      %8u              ║\n", pool->total_frees);
    printf("║ النشطة:         %8u              ║\n", pool->current_allocs);
    printf("║ الفاشلة:        %8u              ║\n", pool->failed_allocs);
    printf("║ عدد الكتل:      %8d              ║\n", pool->block_count);
    printf("╚══════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════
//  مخصص الإطار
// ═══════════════════════════════════════════════════════════════
FrameAllocator* frame_alloc_create(size_t capacity) {
    FrameAllocator* fa = (FrameAllocator*)memalign(MEMORY_ALIGNMENT,
                          sizeof(FrameAllocator) + capacity);
    if (!fa) return NULL;
    fa->base         = (u8*)fa + sizeof(FrameAllocator);
    fa->capacity     = capacity;
    fa->offset       = 0;
    fa->saved_offset = 0;
    fa->frame_count  = 0;
    return fa;
}

void frame_alloc_destroy(FrameAllocator* fa) {
    if (fa) free(fa);
}

void* frame_alloc(FrameAllocator* fa, size_t size) {
    if (!fa || size == 0) return NULL;
    size_t aligned = align_up(size, MEMORY_ALIGNMENT);
    if (fa->offset + aligned > fa->capacity) return NULL;
    void* ptr   = fa->base + fa->offset;
    fa->offset += aligned;
    return ptr;
}

void* frame_alloc_zero(FrameAllocator* fa, size_t size) {
    void* ptr = frame_alloc(fa, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void frame_alloc_reset(FrameAllocator* fa) {
    if (fa) { fa->offset = 0; fa->frame_count++; }
}

void frame_alloc_save(FrameAllocator* fa) {
    if (fa) fa->saved_offset = fa->offset;
}

void frame_alloc_restore(FrameAllocator* fa) {
    if (fa) fa->offset = fa->saved_offset;
}

// ═══════════════════════════════════════════════════════════════
//  مجمع الأحجام الثابتة
// ═══════════════════════════════════════════════════════════════
FixedPool* fixed_pool_create(size_t block_size, int count) {
    size_t aligned_block = align_up(block_size, MEMORY_ALIGNMENT);
    FixedPool* pool = (FixedPool*)memalign(MEMORY_ALIGNMENT,
                       sizeof(FixedPool) + aligned_block * count);
    if (!pool) return NULL;

    pool->base        = (u8*)pool + sizeof(FixedPool);
    pool->block_size  = aligned_block;
    pool->block_count = count;
    pool->free_count  = count;
    pool->free_list_count = 0;

    // تهيئة قائمة الكتل الحرة
    for (int i = 0; i < count && i < MEMORY_MAX_BLOCKS; i++) {
        pool->free_list[pool->free_list_count++] = pool->base + i * aligned_block;
    }
    return pool;
}

void fixed_pool_destroy(FixedPool* pool) {
    if (pool) free(pool);
}

void* fixed_pool_alloc(FixedPool* pool) {
    if (!pool || pool->free_list_count == 0) return NULL;
    void* ptr = pool->free_list[--pool->free_list_count];
    pool->free_count--;
    memset(ptr, 0, pool->block_size);
    return ptr;
}

void fixed_pool_free(FixedPool* pool, void* ptr) {
    if (!pool || !ptr) return;
    if (pool->free_list_count < MEMORY_MAX_BLOCKS) {
        pool->free_list[pool->free_list_count++] = (u8*)ptr;
        pool->free_count++;
    }
}

int fixed_pool_available(FixedPool* pool) {
    return pool ? pool->free_count : 0;
}

// ═══════════════════════════════════════════════════════════════
//  أدوات الذاكرة
// ═══════════════════════════════════════════════════════════════
void* mem_align(void* ptr, size_t alignment) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t a = (uintptr_t)alignment;
    return (void*)((p + a - 1) & ~(a - 1));
}

size_t mem_align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void mem_zero(void* ptr, size_t size) {
    if (ptr && size) memset(ptr, 0, size);
}

void mem_copy(void* dst, const void* src, size_t size) {
    if (dst && src && size) memcpy(dst, src, size);
}

void mem_move(void* dst, const void* src, size_t size) {
    if (dst && src && size) memmove(dst, src, size);
}

int mem_compare(const void* a, const void* b, size_t size) {
    if (!a || !b) return -1;
    return memcmp(a, b, size);
}

void mem_set(void* ptr, u8 val, size_t size) {
    if (ptr && size) memset(ptr, val, size);
}

void mem_copy_u32(u32* dst, const u32* src, size_t count) {
    if (!dst || !src || !count) return;
    // نسخ محسّن بـ 4 كلمات في كل مرة
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        dst[i+0] = src[i+0];
        dst[i+1] = src[i+1];
        dst[i+2] = src[i+2];
        dst[i+3] = src[i+3];
    }
    for (; i < count; i++) dst[i] = src[i];
}

void mem_set_u32(u32* dst, u32 val, size_t count) {
    if (!dst || !count) return;
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        dst[i+0] = val;
        dst[i+1] = val;
        dst[i+2] = val;
        dst[i+3] = val;
    }
    for (; i < count; i++) dst[i] = val;
}

// ═══════════════════════════════════════════════════════════════
//  إحصائيات ذاكرة النظام
// ═══════════════════════════════════════════════════════════════
size_t system_memory_total(void) {
    return 256 * 1024 * 1024; // 256 ميغابايت للبلايستيشن 3 سليم
}

size_t system_memory_free(void) {
    // قراءة من نظام البلايستيشن 3
    sys_memory_info_t info;
    if (sysMemoryGetInfo(&info) == 0) {
        return (size_t)info.available_memory;
    }
    return 0;
}

size_t system_memory_used(void) {
    return system_memory_total() - system_memory_free();
}

f32 system_memory_usage_pct(void) {
    return (f32)system_memory_used() * 100.0f / (f32)system_memory_total();
}

void system_memory_print(void) {
    size_t total = system_memory_total();
    size_t used  = system_memory_used();
    size_t free_ = system_memory_free();
    printf("[نظام] الذاكرة: مستخدم %zu KB / الكلي %zu KB (حر: %zu KB)\n",
           used / 1024, total / 1024, free_ / 1024);
}
