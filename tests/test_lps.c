#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lps.h"

/* ========================================================================
 * Test Utilities
 * ======================================================================== */

#define TEST_PASS() printf("[PASS] %s\n", __func__)
#define TEST_FAIL(msg)                            \
    do {                                          \
        printf("[FAIL] %s: %s\n", __func__, msg); \
        exit(EXIT_FAILURE);                       \
    } while (0)

#define ASSERT_EQ(a, b)                                                                            \
    do {                                                                                           \
        if ((a) != (b)) {                                                                          \
            printf("[FAIL] %s:%d: Expected %d, got %d\n", __func__, __LINE__, (int)(b), (int)(a)); \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

#define ASSERT_FLOAT_EQ(a, b, tol)                                                                 \
    do {                                                                                           \
        if (fabsf((a) - (b)) > (tol)) {                                                            \
            printf("[FAIL] %s:%d: Expected %.4f, got %.4f (diff %.4f)\n", __func__, __LINE__, (b), \
                   (a), fabsf((a) - (b)));                                                         \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

#define ASSERT_TRUE(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[FAIL] %s:%d: Condition failed\n", __func__, __LINE__); \
            exit(EXIT_FAILURE);                                             \
        }                                                                   \
    } while (0)

/* ========================================================================
 * Lifecycle Tests
 * ======================================================================== */

void test_solver_create_destroy(void) {
    lps_solver* solver = lps_solver_create();
    ASSERT_TRUE(solver != NULL);

    lps_solver_destroy(&solver);
    ASSERT_TRUE(solver == NULL);

    // Double destroy should be safe
    lps_solver_destroy(&solver);

    TEST_PASS();
}

void test_solution_create_destroy(void) {
    lps_solution* solution = lps_solution_create(48);
    ASSERT_TRUE(solution != NULL);
    ASSERT_EQ(solution->num_periods, 48);
    ASSERT_TRUE(solution->buy_kwh != NULL);
    ASSERT_TRUE(solution->sell_kwh != NULL);
    ASSERT_TRUE(solution->charge_kwh != NULL);
    ASSERT_TRUE(solution->discharge_kwh != NULL);
    ASSERT_TRUE(solution->solar_direct_kwh != NULL);
    ASSERT_TRUE(solution->battery_level_kwh != NULL);

    lps_solution_destroy(&solution);
    ASSERT_TRUE(solution == NULL);

    // Double destroy should be safe
    lps_solution_destroy(&solution);

    TEST_PASS();
}

void test_solution_create_invalid(void) {
    // Zero periods
    lps_solution* solution = lps_solution_create(0);
    ASSERT_TRUE(solution == NULL);

    // Too many periods
    solution = lps_solution_create(1000);
    ASSERT_TRUE(solution == NULL);

    TEST_PASS();
}

/* ========================================================================
 * Validation Tests
 * ======================================================================== */

void test_validate_null_problem(void) {
    int ret = lps_problem_validate(NULL);
    ASSERT_EQ(ret, -EINVAL);
    TEST_PASS();
}

void test_validate_null_arrays(void) {
    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = NULL,  // Invalid
                           .price_sek_kwh = NULL,
                           .demand_kwh = NULL,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    ASSERT_EQ(ret, -EINVAL);
    TEST_PASS();
}

void test_validate_invalid_battery(void) {
    float solar[24] = {0};
    float price[24] = {1.0f};
    float demand[24] = {1.0f};

    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 15.0f,  // Invalid: exceeds capacity
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    ASSERT_EQ(ret, -ERANGE);
    TEST_PASS();
}

void test_validate_invalid_efficiency(void) {
    float solar[24] = {0};
    float price[24] = {1.0f};
    float demand[24] = {1.0f};

    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 1.5f,  // Invalid: > 1.0
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    ASSERT_EQ(ret, -ERANGE);
    TEST_PASS();
}

void test_validate_valid_problem(void) {
    float solar[24] = {0};
    float price[24] = {1.0f};
    float demand[24] = {1.0f};

    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    ASSERT_EQ(ret, 0);
    TEST_PASS();
}

/* ========================================================================
 * Correctness Tests
 * ======================================================================== */

void test_no_battery_case(void) {
    // Problem: No battery, constant demand, no solar
    // Expected: Buy exactly demand each period

    const size_t T = 4;
    float solar[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float price[4] = {1.0f, 2.0f, 1.5f, 1.0f};
    float demand[4] = {5.0f, 5.0f, 5.0f, 5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 0.0f,  // No battery
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 0.0f,
                           .discharge_rate_kw = 0.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Verify: Should buy exactly demand each period
    for (size_t t = 0; t < T; t++) {
        ASSERT_FLOAT_EQ(solution->buy_kwh[t], demand[t], 0.1f);
        ASSERT_FLOAT_EQ(solution->sell_kwh[t], 0.0f, 0.01f);
        ASSERT_FLOAT_EQ(solution->charge_kwh[t], 0.0f, 0.01f);
        ASSERT_FLOAT_EQ(solution->discharge_kwh[t], 0.0f, 0.01f);
    }

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

void test_constant_price_with_solar(void) {
    // Problem: Constant price, variable solar, no battery
    // Expected: Use all available solar, buy remainder

    const size_t T = 4;
    float solar[4] = {2.0f, 4.0f, 3.0f, 1.0f};
    float price[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float demand[4] = {5.0f, 5.0f, 5.0f, 5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 0.0f,  // No battery
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 0.0f,
                           .discharge_rate_kw = 0.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Verify: Should use all solar and buy remainder
    for (size_t t = 0; t < T; t++) {
        ASSERT_FLOAT_EQ(solution->solar_direct_kwh[t], solar[t], 0.1f);
        ASSERT_FLOAT_EQ(solution->buy_kwh[t], demand[t] - solar[t], 0.1f);
    }

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

void test_price_arbitrage(void) {
    // Problem: Price varies, battery available, no solar
    // Expected: Charge when cheap, discharge when expensive

    const size_t T = 2;
    float solar[2] = {0.0f, 0.0f};
    float price[2] = {1.0f, 3.0f};  // Cheap then expensive
    float demand[2] = {5.0f, 5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 10.0f,
                           .discharge_rate_kw = 10.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Verify: Should charge at t=0, discharge at t=1
    // At t=0: Buy for demand + charge battery
    ASSERT_TRUE(solution->charge_kwh[0] > 0.1f);

    // At t=1: Discharge battery to reduce expensive purchase
    ASSERT_TRUE(solution->discharge_kwh[1] > 0.1f);

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

void test_excess_solar_sell(void) {
    // Problem: Solar exceeds demand, high price
    // Expected: Use solar for demand, sell excess

    const size_t T = 2;
    float solar[2] = {10.0f, 10.0f};  // Excess solar
    float price[2] = {2.0f, 2.0f};
    float demand[2] = {3.0f, 3.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 5.0f,
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Verify: Should use solar for demand and sell/store excess
    for (size_t t = 0; t < T; t++) {
        ASSERT_FLOAT_EQ(solution->buy_kwh[t], 0.0f, 0.1f);               // No grid purchase needed
        ASSERT_TRUE(solution->solar_direct_kwh[t] >= demand[t] - 0.1f);  // At least demand
    }

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

/* ========================================================================
 * Edge Case Tests
 * ======================================================================== */

void test_single_period(void) {
    const size_t T = 1;
    float solar[1] = {2.0f};
    float price[1] = {1.5f};
    float demand[1] = {5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

void test_full_battery_start(void) {
    const size_t T = 4;
    float solar[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float price[4] = {1.0f, 2.0f, 3.0f, 1.0f};
    float demand[4] = {2.0f, 2.0f, 2.0f, 2.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 10.0f,  // Full battery
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // Should discharge during expensive periods
    ASSERT_TRUE(solution->discharge_kwh[2] > 0.1f);  // Highest price period

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

/* ========================================================================
 * Performance Tests
 * ======================================================================== */

void test_performance_48h(void) {
    const size_t T = 48;
    float solar[48], price[48], demand[48];

    // Generate realistic data
    for (size_t t = 0; t < T; t++) {
        solar[t] = (t >= 6 && t <= 18) ? 3.0f : 0.0f;         // Solar during day
        price[t] = 1.0f + 0.5f * sinf(t * 3.14159f / 12.0f);  // Variable price
        demand[t] = 2.0f;
    }

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver* solver = lps_solver_create();
    lps_solution* solution = lps_solution_create(T);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int ret = lps_solve(solver, &problem, solution);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_ms =
        (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    printf("  48-hour solve time: %.2f ms\n", elapsed_ms);

    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(elapsed_ms < 50.0);  // Relaxed target for initial implementation

    // Verify solution
    ret = lps_solution_verify(&problem, solution);
    ASSERT_EQ(ret, 0);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

static void test_storm_mode(void) {
    lps_solver* solver = lps_solver_create();
    const size_t T = 2;
    float solar[] = {0.0f, 0.0f};
    float price[] = {10.0f, 1.0f};  // Expensive t=0, Cheap t=1
    float demand[] = {0.0f, 0.0f};

    // Constraint: Must be full (10.0) at start of t=1
    float min_level[] = {0.0f, 10.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 10.0f,
                           .discharge_rate_kw = 10.0f,
                           .efficiency = 1.0f,
                           .sell_price_ratio = 0.5f,
                           .min_battery_level_kwh = min_level};

    lps_solution* solution = lps_solution_create(T);
    int ret = lps_solve(solver, &problem, solution);
    ASSERT_EQ(ret, 0);

    // To meet min_level[1]=10.0, we MUST charge 10.0 at t=0
    // Even though price is 10.0 (Expensive)!
    ASSERT_FLOAT_EQ(solution->charge_kwh[0], 10.0f, 0.1f);
    ASSERT_FLOAT_EQ(solution->battery_level_kwh[1], 10.0f, 0.1f);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
    TEST_PASS();
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(void) {
    printf("=== LPS Unit Tests ===\n\n");

    printf("Lifecycle Tests:\n");
    test_solver_create_destroy();
    test_solution_create_destroy();
    test_solution_create_invalid();

    printf("\nValidation Tests:\n");
    test_validate_null_problem();
    test_validate_null_arrays();
    test_validate_invalid_battery();
    test_validate_invalid_efficiency();
    test_validate_valid_problem();

    printf("\nCorrectness Tests:\n");
    test_no_battery_case();
    test_constant_price_with_solar();
    test_price_arbitrage();
    test_excess_solar_sell();
    test_storm_mode();

    printf("\nEdge Case Tests:\n");
    test_single_period();
    test_full_battery_start();

    printf("\nPerformance Tests:\n");
    test_performance_48h();

    printf("\n=== All Tests Passed ===\n");
    return EXIT_SUCCESS;
}
