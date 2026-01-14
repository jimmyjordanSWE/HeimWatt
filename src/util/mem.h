/**
 * @file mem.h
 * @brief Memory allocation helpers
 *
 * Safe allocation wrappers and utilities.
 */

#ifndef HEIMWATT_MEM_H
#define HEIMWATT_MEM_H

#include <stddef.h>

/**
 * Allocate memory, exits on failure.
 * Use for allocations that must not fail.
 *
 * @param size Bytes to allocate
 * @return Allocated memory (never NULL)
 */
void* mem_alloc(size_t size);

/**
 * Allocate zeroed memory, exits on failure.
 *
 * @param count Number of elements
 * @param size  Size of each element
 * @return Allocated memory (never NULL)
 */
void* mem_calloc(size_t count, size_t size);

/**
 * Reallocate memory, exits on failure.
 *
 * @param ptr  Existing allocation (or NULL)
 * @param size New size
 * @return Reallocated memory (never NULL)
 */
void* mem_realloc(void* ptr, size_t size);

/**
 * Duplicate a string, exits on failure.
 *
 * @param str String to duplicate
 * @return Duplicated string (never NULL)
 */
char* mem_strdup(const char* str);

/**
 * Free memory (safe with NULL).
 *
 * @param ptr Memory to free (can be NULL)
 */
void mem_free(void* ptr);

#endif /* HEIMWATT_MEM_H */
