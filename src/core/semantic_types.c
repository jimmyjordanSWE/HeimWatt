#include "semantic_types.h"

#include <string.h>

// X-Macro expansion to populate the lookup table
// X-Macro expansion to populate the lookup table
#define X(suffix, id, unit, desc) [SEM_##suffix] = {SEM_##suffix, id, #suffix, unit, desc},

static const semantic_meta META_TABLE[SEM_TYPE_COUNT] = {HEIMWATT_SEMANTIC_TYPES(X)};
#undef X

const semantic_meta *semantic_get_meta(semantic_type type)
{
    if (type <= SEM_UNKNOWN || type >= SEM_TYPE_COUNT)
    {
        return NULL;
    }
    return &META_TABLE[type];
}

semantic_type semantic_from_string(const char *id)
{
    if (!id) return SEM_UNKNOWN;

    // Linear scan for now.
    // Optimization note: For ~100-200 items, O(N) is negligible (<< 1us).
    // If list grows to >1000, consider a gperf-generated hash or binary search.
    for (int i = 1; i < SEM_TYPE_COUNT; i++)
    {
        if (strcmp(META_TABLE[i].id, id) == 0)
        {
            return (semantic_type) i;
        }
        // Fallback: Check enum name (e.g. "ATMOSPHERE_TEMPERATURE")
        if (strcmp(META_TABLE[i].enum_name, id) == 0)
        {
            return (semantic_type) i;
        }
    }
    return SEM_UNKNOWN;
}
