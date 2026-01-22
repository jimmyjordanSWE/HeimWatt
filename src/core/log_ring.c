#include "log_ring.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/cJSON.h"

// Internal State
static struct {
    log_entry* buffer;
    size_t capacity;
    size_t head;  // Points to the next write position
    size_t count;
    pthread_mutex_t lock;
    int initialized;
} ring = {0};

void log_ring_init(size_t capacity) {
    if (ring.initialized)
        return;

    ring.buffer = calloc(capacity, sizeof(log_entry));
    if (!ring.buffer)
        return;  // Should handle OOM better in real/embedded, but std here

    ring.capacity = capacity;
    ring.head = 0;
    ring.count = 0;
    pthread_mutex_init(&ring.lock, NULL);
    ring.initialized = 1;
}

void log_ring_push(const log_entry* entry) {
    if (!ring.initialized || !entry)
        return;

    pthread_mutex_lock(&ring.lock);

    // Copy data
    // Note: We do struct copy which is safe for fixed-size arrays inside struct
    ring.buffer[ring.head] = *entry;

    // Advance head
    ring.head = (ring.head + 1) % ring.capacity;

    // Update count
    if (ring.count < ring.capacity) {
        ring.count++;
    }

    pthread_mutex_unlock(&ring.lock);
}

size_t log_ring_get_recent(log_entry* out, size_t max_count) {
    if (!ring.initialized || !out || max_count == 0)
        return 0;

    pthread_mutex_lock(&ring.lock);

    size_t count_to_copy = ring.count < max_count ? ring.count : max_count;
    size_t copied = 0;

    // We want to return entries in chronological order (oldest to newest)
    // OR reverse chronological order (newest to oldest)?
    // Usually logs are displayed top-down or bottom-up.
    // Let's provide chronological order (oldest first in the returned array).

    // The "oldest" valid entry is at (head - count + capacity) % capacity
    size_t start_idx = (ring.head + ring.capacity - ring.count) % ring.capacity;

    // If we request fewer than available, skip the very oldest
    if (max_count < ring.count) {
        size_t skip = ring.count - max_count;
        start_idx = (start_idx + skip) % ring.capacity;
    }

    for (size_t i = 0; i < count_to_copy; i++) {
        size_t idx = (start_idx + i) % ring.capacity;
        out[i] = ring.buffer[idx];
        copied++;
    }

    pthread_mutex_unlock(&ring.lock);
    return copied;
}

char* log_ring_to_json(size_t max_count) {
    if (!ring.initialized)
        return NULL;

    // Allocate temp buffer on stack for retrieval to avoid holding lock during JSON gen
    // But max_count could be large.
    // Ideally we should limit max_count for stack usage or alloc.
    // Let's assume max_count <= 100 which is reasonable for WebUI.
    // If large, malloc.

    log_entry* entries = malloc(sizeof(log_entry) * max_count);
    if (!entries)
        return NULL;

    size_t count = log_ring_get_recent(entries, max_count);

    cJSON* root = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "logs", arr);

    // Mapping for levels
    const char* level_names[] = {"trace", "debug", "info", "warn", "error", "fatal"};

    for (size_t i = 0; i < count; i++) {
        log_entry* e = &entries[i];
        cJSON* item = cJSON_CreateObject();

        cJSON_AddNumberToObject(item, "timestamp", (double) e->timestamp);

        const char* lvl = "info";
        if (e->level >= 0 && e->level < 6)
            lvl = level_names[e->level];
        cJSON_AddStringToObject(item, "level", lvl);

        cJSON_AddStringToObject(item, "category", e->category);
        cJSON_AddStringToObject(item, "event", e->event);
        cJSON_AddStringToObject(item, "message", e->message);

        cJSON_AddItemToArray(arr, item);
    }

    free(entries);

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}
