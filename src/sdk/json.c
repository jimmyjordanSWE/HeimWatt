#include <heimwatt_sdk.h>
#include <net/json.h>
#include <stdlib.h>

json_value *sdk_json_parse(const char *str) { return json_parse(str); }

void sdk_json_free(json_value *v) { json_free(v); }

const json_value *sdk_json_get(const json_value *obj, const char *key)
{
    return json_get(obj, key);
}

size_t sdk_json_array_size(const json_value *arr) { return json_array_size(arr); }

const json_value *sdk_json_array_get(const json_value *arr, size_t idx)
{
    return json_array_get(arr, idx);
}

const char *sdk_json_string(const json_value *v) { return json_string_value(v); }

double sdk_json_number(const json_value *v) { return json_number_value(v); }

int64_t sdk_json_int(const json_value *v) { return json_int_value(v); }

bool sdk_json_bool(const json_value *v) { return json_bool_value(v); }

json_value *sdk_json_new_object(void) { return json_object_new(); }

void sdk_json_set_number(json_value *obj, const char *key, double val)
{
    json_object_set_number(obj, key, val);
}

char *sdk_json_stringify(const json_value *v) { return json_stringify(v); }
