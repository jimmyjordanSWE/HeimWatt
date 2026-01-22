/*
 * @file test_semantic_types.c
 * @brief Unit tests for semantic type system
 */

#include "semantic_types.h"

#include <string.h>

#include "libs/unity/unity.h"

// --- semantic_from_string tests ---

void test_semantic_from_string_valid(void) {
    semantic_type t = semantic_from_string("atmosphere.temperature");
    TEST_ASSERT_EQUAL_INT(SEM_ATMOSPHERE_TEMPERATURE, t);

    t = semantic_from_string("energy.price.spot");
    TEST_ASSERT_EQUAL_INT(SEM_ENERGY_PRICE_SPOT, t);

    t = semantic_from_string("storage.soc");
    TEST_ASSERT_EQUAL_INT(SEM_STORAGE_SOC, t);
}

void test_semantic_from_string_invalid(void) {
    semantic_type t = semantic_from_string("invalid.type.name");
    TEST_ASSERT_EQUAL_INT(SEM_UNKNOWN, t);

    t = semantic_from_string("");
    TEST_ASSERT_EQUAL_INT(SEM_UNKNOWN, t);

    t = semantic_from_string(NULL);
    TEST_ASSERT_EQUAL_INT(SEM_UNKNOWN, t);
}

// --- semantic_get_meta tests ---

void test_semantic_get_meta_valid(void) {
    const semantic_meta *meta = semantic_get_meta(SEM_ATMOSPHERE_TEMPERATURE);
    TEST_ASSERT_NOT_NULL(meta);
    TEST_ASSERT_EQUAL_INT(SEM_ATMOSPHERE_TEMPERATURE, meta->type);
    TEST_ASSERT_EQUAL_STRING("atmosphere.temperature", meta->id);
    TEST_ASSERT_EQUAL_STRING("celsius", meta->unit);
}

void test_semantic_get_meta_unknown(void) {
    const semantic_meta *meta = semantic_get_meta(SEM_UNKNOWN);
    // Should return NULL or a stub entry
    // Implementation-dependent, just check it doesn't crash
    (void) meta;
    TEST_PASS();
}

void test_semantic_type_count(void) {
    // SEM_TYPE_COUNT should be > 50 given the X-macro definitions
    TEST_ASSERT_TRUE(SEM_TYPE_COUNT > 50);
}
