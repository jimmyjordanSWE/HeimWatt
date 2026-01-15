/**
 * @file test_json.c
 * @brief Unit tests for JSON wrapper functions
 */

#include <stdlib.h>
#include <string.h>

#include "libs/unity/unity.h"
#include "net/json.h"

// --- json_parse tests ---

void test_json_parse_object(void)
{
    json_value *v = json_parse("{\"key\":\"value\"}");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE(json_is_object(v));

    const json_value *key = json_get(v, "key");
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_EQUAL_STRING("value", json_string_value(key));

    json_free(v);
}

void test_json_parse_array(void)
{
    json_value *v = json_parse("[1, 2, 3]");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE(json_is_array(v));
    TEST_ASSERT_EQUAL_INT(3, json_array_size(v));

    TEST_ASSERT_EQUAL_DOUBLE(1.0, json_number_value(json_array_get(v, 0)));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, json_number_value(json_array_get(v, 1)));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, json_number_value(json_array_get(v, 2)));

    json_free(v);
}

void test_json_parse_nested(void)
{
    json_value *v = json_parse("{\"outer\":{\"inner\":42}}");
    TEST_ASSERT_NOT_NULL(v);

    const json_value *outer = json_get(v, "outer");
    TEST_ASSERT_NOT_NULL(outer);
    TEST_ASSERT_TRUE(json_is_object(outer));

    const json_value *inner = json_get(outer, "inner");
    TEST_ASSERT_NOT_NULL(inner);
    TEST_ASSERT_EQUAL_INT(42, json_int_value(inner));

    json_free(v);
}

void test_json_parse_invalid(void)
{
    json_value *v = json_parse("{broken");
    TEST_ASSERT_NULL(v);

    v = json_parse(NULL);
    TEST_ASSERT_NULL(v);
}

// --- json_stringify tests ---

void test_json_stringify_roundtrip(void)
{
    json_value *obj = json_object_new();
    json_object_set_string(obj, "name", "test");
    json_object_set_number(obj, "count", 42);
    json_object_set_bool(obj, "active", true);

    char *str = json_stringify(obj);
    TEST_ASSERT_NOT_NULL(str);

    // Parse it back
    json_value *parsed = json_parse(str);
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_EQUAL_STRING("test", json_string_value(json_get(parsed, "name")));
    TEST_ASSERT_EQUAL_INT(42, json_int_value(json_get(parsed, "count")));
    TEST_ASSERT_TRUE(json_bool_value(json_get(parsed, "active")));

    free(str);
    json_free(obj);
    json_free(parsed);
}

void test_json_type_checks(void)
{
    json_value *str = json_string_new("hello");
    json_value *num = json_number_new(3.14);
    json_value *arr = json_array_new();
    json_value *obj = json_object_new();

    TEST_ASSERT_TRUE(json_is_string(str));
    TEST_ASSERT_FALSE(json_is_number(str));

    TEST_ASSERT_TRUE(json_is_number(num));
    TEST_ASSERT_FALSE(json_is_string(num));

    TEST_ASSERT_TRUE(json_is_array(arr));
    TEST_ASSERT_FALSE(json_is_object(arr));

    TEST_ASSERT_TRUE(json_is_object(obj));
    TEST_ASSERT_FALSE(json_is_array(obj));

    json_free(str);
    json_free(num);
    json_free(arr);
    json_free(obj);
}
