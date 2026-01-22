#ifndef HEIMWATT_MEMORY_H
#define HEIMWATT_MEMORY_H

#include <stddef.h>
#include <unistd.h>  // for ssize_t

/* ============================================================
 * 1. UNIFIED ALLOCATOR API
 * ============================================================ */

/*
 * @brief Allocate zero-initialized memory.
 *
 * Replaces malloc/calloc.
 * GUARANTEE: Always returns memory set to 0.
 * GUARANTEE: Forces physical allocation (no optimistic overcommit).
 */
void* mem_alloc(size_t size);

/*
 * @brief Reallocate memory.
 *
 * WARNING: New memory is NOT zero-initialized.
 * User must handle initialization of the new section.
 * For auto-zeroing growable buffers, use HwBuffer.
 */
void* mem_realloc(void* ptr, size_t size);

/*
 * @brief Free memory allocated by mem_alloc/mem_realloc.
 */
void mem_free(void* ptr);
char* mem_strdup(const char* s);

/* ============================================================
 * 2. OBJECT POOL (Fixed-size, Fast)
 * ============================================================ */

typedef struct HwPool HwPool;

/*
 * @brief Create a new object pool.
 * @param item_size Size of each object in bytes.
 * @param count Number of objects to pre-allocate.
 */
HwPool* hw_pool_create(size_t item_size, size_t count);

/*
 * @brief Allocate an object from the pool.
 * GUARANTEE: Returned memory is zeroed.
 * @return Pointer to object, or NULL if pool is empty/exhausted.
 */
void* hw_pool_alloc(HwPool* pool);

/*
 * @brief Return an object to the pool.
 */
void hw_pool_free(HwPool* pool, void* ptr);

/*
 * @brief Destroy the pool and all its resources.
 */
void hw_pool_destroy(HwPool** pool);

/*
 * @brief Get the number of times pool allocation failed due to exhaustion.
 * @return Exhaust count (0 if pool is NULL).
 */
size_t hw_pool_get_exhaust_count(const HwPool* pool);

/* ============================================================
 * 3. LINEAR ARENA (Bump Pointer, Request-Scoped)
 * ============================================================ */

typedef struct HwArena HwArena;

/*
 * @brief Set the thread-local arena for scoped allocations.
 * When set, allocations go to this arena.
 */
void mem_set_scope_arena(HwArena* arena);

/*
 * @brief Create a new arena.
 * @param block_size Size of underlying memory blocks (pages).
 */
HwArena* hw_arena_create(size_t block_size);

/*
 * @brief Allocate memory from the arena.
 * GUARANTEE: Returned memory is zeroed.
 * @return Pointer to zeroed memory, or NULL on OOM.
 */
void* hw_arena_alloc(HwArena* arena, size_t size);

/*
 * @brief Reset the arena, freeing all allocations efficiently.
 * Does not free underlying blocks, keeps them for reuse.
 */
void hw_arena_reset(HwArena* arena);

/*
 * @brief Destroy the arena and release system memory.
 */
void hw_arena_destroy(HwArena** arena);

/* ============================================================
 * 4. DYNAMIC BUFFER (Strings, IPC, IO)
 * ============================================================ */

typedef struct HwBuffer {
    char* data;
    size_t len;
    size_t cap;
} HwBuffer;

void hw_buffer_init(HwBuffer* buf);
void hw_buffer_free(HwBuffer* buf);
void hw_buffer_clear(HwBuffer* buf);  // Reset len=0, keep cap

// Appends data, growing geometrically. New memory is zeroed.
int hw_buffer_append(HwBuffer* buf, const char* src, size_t n);

// Ensure capacity for generic usage
int hw_buffer_ensure_cap(HwBuffer* buf, size_t extra);

#endif  // HEIMWATT_MEMORY_H
