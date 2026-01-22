/*
 * @file json.h
 * @brief JSON utilities
 *
 * JSON parsing and serialization. Thin wrapper (uses cJSON internally).
 */

#ifndef HEIMWATT_JSON_H
#define HEIMWATT_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct json_value json_value;

/* ============================================================
 * PARSING
 * ============================================================ */

/*
 * Parse JSON string.
 *
 * @param str JSON string
 * @return Parsed value or NULL on error
 */
json_value* json_parse(const char* str);

/*
 * @brief Parse JSON string using an arena allocator.
 *
 * Uses the provided arena for all internal allocations.
 * WARNING: Do NOT call json_free() on the result. destroying the arena frees the memory.
 *
 * @param str JSON string
 * @param arena The arena to use
 * @return Parsed value or NULL on error
 */
#include "memory.h"  // For HwArena
json_value* json_parse_arena(const char* str, HwArena* arena);

/*
 * Free parsed value (and all children).
 *
 * @param v Value to free
 */
void json_free(json_value* v);

/* ============================================================
 * TYPE CHECKS
 * ============================================================ */

bool json_is_object(const json_value* v);
bool json_is_array(const json_value* v);
bool json_is_string(const json_value* v);
bool json_is_number(const json_value* v);
bool json_is_bool(const json_value* v);
bool json_is_null(const json_value* v);

/* ============================================================
 * OBJECT ACCESS
 * ============================================================ */

/*
 * Get value by key.
 *
 * @param obj Object
 * @param key Key name
 * @return Value or NULL if not found
 */
const json_value* json_get(const json_value* obj, const char* key);

/*
 * Get number of keys in object.
 */
size_t json_object_size(const json_value* obj);

/*
 * Iterate object keys.
 *
 * @param obj   Object
 * @param index Key index
 * @param value Output value pointer
 * @return Key name or NULL if out of bounds
 */
const char* json_object_iter(const json_value* obj, size_t index, const json_value** value);

/* ============================================================
 * ARRAY ACCESS
 * ============================================================ */

/*
 * Get array size.
 */
size_t json_array_size(const json_value* arr);

/*
 * Get array element.
 *
 * @param arr Array
 * @param idx Index
 * @return Element or NULL if out of bounds
 */
const json_value* json_array_get(const json_value* arr, size_t idx);

/* ============================================================
 * VALUE EXTRACTION
 * ============================================================ */

const char* json_string_value(const json_value* v); /*< NULL if not string */
double json_number_value(const json_value* v);      /*< 0.0 if not number */
bool json_bool_value(const json_value* v);          /*< false if not bool */
int64_t json_int_value(const json_value* v);        /*< Cast from number */

/* ============================================================
 * BUILDING
 * ============================================================ */

json_value* json_object_new(void);
json_value* json_array_new(void);
json_value* json_string_new(const char* s);
json_value* json_number_new(double n);
json_value* json_bool_new(bool b);
json_value* json_null_new(void);

void json_object_set(json_value* obj, const char* key, json_value* val);
void json_object_set_string(json_value* obj, const char* key, const char* val);
void json_object_set_number(json_value* obj, const char* key, double val);
void json_object_set_bool(json_value* obj, const char* key, bool val);

void json_array_append(json_value* arr, json_value* val);

/* ============================================================
 * SERIALIZATION
 * ============================================================ */

/*
 * Serialize to compact string. Caller frees.
 */
char* json_stringify(const json_value* v);

/*
 * Serialize to pretty-printed string. Caller frees.
 */
char* json_stringify_pretty(const json_value* v);

#endif /* HEIMWATT_JSON_H */
