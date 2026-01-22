#include "memory.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 1. UNIFIED ALLOCATOR API
 * ============================================================ */

static __thread HwArena* tls_arena = NULL;

void mem_set_scope_arena(HwArena* arena) {
    tls_arena = arena;
}

void* mem_alloc(size_t size) {
    if (size == 0)
        return NULL;

    if (tls_arena) {
        return hw_arena_alloc(tls_arena, size);
    }

    void* ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* mem_realloc(void* ptr, size_t size) {
    if (size == 0) {
        mem_free(ptr);
        return NULL;
    }

    if (tls_arena) {
        if (!ptr)
            return hw_arena_alloc(tls_arena, size);
        return NULL;  // Realloc not supported in arena mode for existing pointers without size info
    }

    return realloc(ptr, size);
}

void mem_free(void* ptr) {
    if (!ptr)
        return;
    if (tls_arena)
        return;  // No-op in arena mode
    free(ptr);
}

char* mem_strdup(const char* s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char* dup = mem_alloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

/* ============================================================
 * 2. OBJECT POOL (Fixed-size, Fast)
 * ============================================================ */

struct HwPool {
    size_t item_size;
    size_t count;
    void* block;      // The big contiguous block
    void* free_head;  // Head of free list

    // Observability metrics
    atomic_size_t exhaust_count;  // Times pool was empty on alloc
};

// Internal node for free list
typedef struct PoolNode {
    struct PoolNode* next;
} PoolNode;

HwPool* hw_pool_create(size_t item_size, size_t count) {
    if (item_size < sizeof(PoolNode))
        item_size = sizeof(PoolNode);  // Enforce min size
    if (count == 0)
        return NULL;

    HwPool* pool = mem_alloc(sizeof(HwPool));
    if (!pool)
        return NULL;

    pool->item_size = item_size;
    pool->count = count;

    // Allocate one big block
    // We use mem_alloc which zeroes it, though not strictly necessary for the pool structure itself
    // since we link it up immediately.
    pool->block = mem_alloc(count * item_size);
    if (!pool->block) {
        mem_free(pool);
        return NULL;
    }

    // Initialize free list
    char* ptr = (char*) pool->block;
    for (size_t i = 0; i < count - 1; i++) {
        PoolNode* node = (PoolNode*) (ptr + i * item_size);
        node->next = (PoolNode*) (ptr + (i + 1) * item_size);
    }
    // Terminate last node
    PoolNode* last = (PoolNode*) (ptr + (count - 1) * item_size);
    last->next = NULL;

    pool->free_head = (void*) ptr;
    return pool;
}

void* hw_pool_alloc(HwPool* pool) {
    if (!pool)
        return NULL;

    if (!pool->free_head) {
        atomic_fetch_add(&pool->exhaust_count, 1);
        return NULL;
    }

    PoolNode* node = (PoolNode*) pool->free_head;
    pool->free_head = node->next;

    // Zero out the memory before giving it to user
    memset(node, 0, pool->item_size);
    return node;
}

void hw_pool_free(HwPool* pool, void* ptr) {
    if (!pool || !ptr)
        return;

    // Range check could be done here if we wanted strict safety,
    // but for now we trust the pointer (fast path).

    PoolNode* node = (PoolNode*) ptr;
    node->next = (PoolNode*) pool->free_head;
    pool->free_head = node;
}

void hw_pool_destroy(HwPool** pool_ptr) {
    if (!pool_ptr || !*pool_ptr)
        return;
    HwPool* pool = *pool_ptr;

    mem_free(pool->block);
    mem_free(pool);
    *pool_ptr = NULL;
}

size_t hw_pool_get_exhaust_count(const HwPool* pool) {
    if (!pool)
        return 0;
    return atomic_load(&pool->exhaust_count);
}

/* ============================================================
 * 3. LINEAR ARENA (Bump Pointer)
 * ============================================================ */

typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t cap;
    size_t used;
    char data[];  // Flexible array
} ArenaBlock;

struct HwArena {
    size_t block_size;
    ArenaBlock* head;
    ArenaBlock* current;
};

static ArenaBlock* arena_new_block(size_t size) {
    // Allocate block header + data
    ArenaBlock* b = mem_alloc(sizeof(ArenaBlock) + size);
    if (b) {
        b->cap = size;
        b->used = 0;
        b->next = NULL;
    }
    return b;
}

HwArena* hw_arena_create(size_t block_size) {
    if (block_size < 1024)
        block_size = 1024;  // Min size

    HwArena* arena = mem_alloc(sizeof(HwArena));
    if (!arena)
        return NULL;

    arena->block_size = block_size;
    arena->head = arena_new_block(block_size);
    if (!arena->head) {
        mem_free(arena);
        return NULL;
    }
    arena->current = arena->head;
    return arena;
}

void* hw_arena_alloc(HwArena* arena, size_t size) {
    if (!arena || size == 0)
        return NULL;

    // Align size to 8 bytes
    size_t aligned = (size + 7) & ~7;

    if (aligned > arena->block_size) {
        // Too big for standard block -> Allocate oversize block
        // Insert it *after* current to avoid disrupting the flow?
        // Or simpler: just allocate distinct block and link it.
        // For simplicity in this version, we make a dedicated block
        // and link it at the head (or next).

        ArenaBlock* big = arena_new_block(aligned);
        if (!big)
            return NULL;

        // Push to head to ensure cleanup finds it
        big->next = arena->head;
        arena->head = big;

        // Mark used
        big->used = aligned;
        return big->data;
    }

    if (arena->current->used + aligned > arena->current->cap) {
        // Need new block. Check if next exists (reuse)
        if (arena->current->next) {
            arena->current = arena->current->next;
            arena->current->used = 0;  // Reset reused block
        } else {
            // Allocate new
            ArenaBlock* nb = arena_new_block(arena->block_size);
            if (!nb)
                return NULL;
            arena->current->next = nb;
            arena->current = nb;
        }
    }

    void* ptr = arena->current->data + arena->current->used;
    arena->current->used += aligned;

    // mem_alloc already zeroed the block on creation,
    // but if we reuse blocks (reset), we must re-zero.
    // However, since we track `used`, the memory *after* used is dirty.
    // So we should memset HERE to be safe.
    memset(ptr, 0, aligned);

    return ptr;
}

void hw_arena_reset(HwArena* arena) {
    if (!arena)
        return;
    ArenaBlock* b = arena->head;
    while (b) {
        b->used = 0;
        b = b->next;
    }
    arena->current = arena->head;
}

void hw_arena_destroy(HwArena** arena_ptr) {
    if (!arena_ptr || !*arena_ptr)
        return;
    HwArena* arena = *arena_ptr;

    ArenaBlock* b = arena->head;
    while (b) {
        ArenaBlock* next = b->next;
        mem_free(b);
        b = next;
    }

    mem_free(arena);
    *arena_ptr = NULL;
}

/* ============================================================
 * 4. DYNAMIC BUFFER
 * ============================================================ */

#define BUFFER_INITIAL_CAP 256

void hw_buffer_init(HwBuffer* buf) {
    if (buf) {
        memset(buf, 0, sizeof(HwBuffer));
    }
}

void hw_buffer_free(HwBuffer* buf) {
    if (buf) {
        mem_free(buf->data);
        memset(buf, 0, sizeof(HwBuffer));
    }
}

void hw_buffer_clear(HwBuffer* buf) {
    if (buf) {
        buf->len = 0;
        // Keep capacity
    }
}

int hw_buffer_ensure_cap(HwBuffer* buf, size_t extra) {
    if (!buf)
        return -EINVAL;

    size_t needed = buf->len + extra + 1;  // +1 sentinel
    if (needed <= buf->cap)
        return 0;

    size_t new_cap = buf->cap == 0 ? BUFFER_INITIAL_CAP : buf->cap;
    while (new_cap < needed) {
        new_cap *= 2;
        if (new_cap < buf->cap)
            return -ENOMEM;  // Overflow
    }

    // Use mem_realloc (Does NOT zero)
    char* new_data = mem_realloc(buf->data, new_cap);
    if (!new_data)
        return -ENOMEM;

    // Manual zeroing of new space
    if (new_cap > buf->cap) {
        size_t old_cap = buf->cap;
        memset(new_data + old_cap, 0, new_cap - old_cap);
    }

    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

int hw_buffer_append(HwBuffer* buf, const char* src, size_t n) {
    if (!buf || !src)
        return -EINVAL;
    if (n == 0)
        return 0;

    int ret = hw_buffer_ensure_cap(buf, n);
    if (ret != 0)
        return ret;

    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    buf->data[buf->len] = '\0';  // Sentinel

    return 0;
}
