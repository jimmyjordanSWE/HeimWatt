#include <libs/cJSON.h>
#include <net/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Typedef casting? cJSON* is implementation detail.
// internal: json_value is cJSON

json_value *json_parse(const char *str) { return (json_value *) cJSON_Parse(str); }

void json_free(json_value *v) { cJSON_Delete((cJSON *) v); }

const json_value *json_get(const json_value *obj, const char *key)
{
    return (const json_value *) cJSON_GetObjectItemCaseSensitive((const cJSON *) obj, key);
}

size_t json_object_size(const json_value *obj)
{
    return (size_t) cJSON_GetArraySize((const cJSON *) obj);
}

const char *json_object_iter(const json_value *obj, size_t index, const json_value **value)
{
    cJSON *item = cJSON_GetArrayItem((const cJSON *) obj, (int) index);
    if (!item) return NULL;
    if (value) *value = (const json_value *) item;
    return item->string;
}

size_t json_array_size(const json_value *arr)
{
    return (size_t) cJSON_GetArraySize((const cJSON *) arr);
}

const json_value *json_array_get(const json_value *arr, size_t idx)
{
    return (const json_value *) cJSON_GetArrayItem((const cJSON *) arr, (int) idx);
}

const char *json_string_value(const json_value *v)
{
    if (!cJSON_IsString((const cJSON *) v)) return NULL;
    return ((const cJSON *) v)->valuestring;
}

double json_number_value(const json_value *v)
{
    if (!cJSON_IsNumber((const cJSON *) v)) return 0.0;
    return ((const cJSON *) v)->valuedouble;
}

bool json_bool_value(const json_value *v) { return cJSON_IsTrue((const cJSON *) v); }

int64_t json_int_value(const json_value *v)
{
    if (!cJSON_IsNumber((const cJSON *) v)) return 0;
    return (int64_t) ((const cJSON *) v)->valuedouble;
}

json_value *json_object_new(void) { return (json_value *) cJSON_CreateObject(); }

json_value *json_array_new(void) { return (json_value *) cJSON_CreateArray(); }

json_value *json_string_new(const char *s) { return (json_value *) cJSON_CreateString(s); }

json_value *json_number_new(double n) { return (json_value *) cJSON_CreateNumber(n); }

json_value *json_bool_new(bool b) { return (json_value *) cJSON_CreateBool(b); }

json_value *json_null_new(void) { return (json_value *) cJSON_CreateNull(); }

void json_object_set(json_value *obj, const char *key, json_value *val)
{
    cJSON_AddItemToObject((cJSON *) obj, key, (cJSON *) val);
}

void json_object_set_string(json_value *obj, const char *key, const char *val)
{
    cJSON_AddStringToObject((cJSON *) obj, key, val);
}

void json_object_set_number(json_value *obj, const char *key, double val)
{
    cJSON_AddNumberToObject((cJSON *) obj, key, val);
}

void json_object_set_bool(json_value *obj, const char *key, bool val)
{
    cJSON_AddBoolToObject((cJSON *) obj, key, val);
}

void json_array_append(json_value *arr, json_value *val)
{
    cJSON_AddItemToArray((cJSON *) arr, (cJSON *) val);
}

char *json_stringify(const json_value *v) { return cJSON_PrintUnformatted((const cJSON *) v); }

char *json_stringify_pretty(const json_value *v) { return cJSON_Print((const cJSON *) v); }

bool json_is_array(const json_value *v) { return cJSON_IsArray((const cJSON *) v); }
bool json_is_object(const json_value *v) { return cJSON_IsObject((const cJSON *) v); }
bool json_is_string(const json_value *v) { return cJSON_IsString((const cJSON *) v); }
bool json_is_number(const json_value *v) { return cJSON_IsNumber((const cJSON *) v); }
