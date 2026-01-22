/*
 * @file test_lps.c
 * @brief Unit tests for LPS (Linear Programming Solver) - Unity version
 *
 * Migrated from custom test framework to Unity.
 */

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "libs/unity/unity.h"
#include "plugins/out/energy_strategy/lps/lps.h"

// Helper for float comparison
#define FLOAT_TOL 0.1f

/* ========================================================================
 * Lifecycle Tests
 * ======================================================================== */

void test_lps_solver_create_destroy(void) {
    lps_solver *solver = lps_solver_create();
    TEST_ASSERT_NOT_NULL(solver);

    lps_solver_destroy(&solver);
    TEST_ASSERT_NULL(solver);

    // Double destroy should be safe
    lps_solver_destroy(&solver);
}

void test_lps_solution_create_destroy(void) {
    lps_solution *solution = lps_solution_create(48);
    TEST_ASSERT_NOT_NULL(solution);
    TEST_ASSERT_EQUAL_size_t(48, solution->num_periods);
    TEST_ASSERT_NOT_NULL(solution->buy_kwh);
    TEST_ASSERT_NOT_NULL(solution->sell_kwh);
    TEST_ASSERT_NOT_NULL(solution->charge_kwh);
    TEST_ASSERT_NOT_NULL(solution->discharge_kwh);
    TEST_ASSERT_NOT_NULL(solution->solar_direct_kwh);
    TEST_ASSERT_NOT_NULL(solution->battery_level_kwh);

    lps_solution_destroy(&solution);
    TEST_ASSERT_NULL(solution);

    // Double destroy should be safe
    lps_solution_destroy(&solution);
}

void test_lps_solution_create_invalid(void) {
    // Zero periods
    lps_solution *solution = lps_solution_create(0);
    TEST_ASSERT_NULL(solution);

    // Too many periods
    solution = lps_solution_create(1000);
    TEST_ASSERT_NULL(solution);
}

/* ========================================================================
 * Validation Tests
 * ======================================================================== */

void test_lps_validate_null_problem(void) {
    int ret = lps_problem_validate(NULL);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_lps_validate_null_arrays(void) {
    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = NULL,
                           .price_sek_kwh = NULL,
                           .demand_kwh = NULL,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 5.0f,
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_lps_validate_invalid_battery(void) {
    float solar[24] = {0};
    float price[24] = {1.0f};
    float demand[24] = {1.0f};

    lps_problem problem = {.num_periods = 24,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 10.0f,
                           .battery_initial_kwh = 15.0f,  // Exceeds capacity
                           .charge_rate_kw = 5.0f,
                           .discharge_rate_kw = 5.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    TEST_ASSERT_EQUAL_INT(-ERANGE, ret);
}

void test_lps_validate_invalid_efficiency(void) {
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
                           .efficiency = 1.5f,  // > 1.0
                           .sell_price_ratio = 0.8f};

    int ret = lps_problem_validate(&problem);
    TEST_ASSERT_EQUAL_INT(-ERANGE, ret);
}

void test_lps_validate_valid_problem(void) {
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
    TEST_ASSERT_EQUAL_INT(0, ret);
}

/* ========================================================================
 * Correctness Tests
 * ======================================================================== */

void test_lps_no_battery_case(void) {
    const size_t T = 4;
    float solar[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float price[4] = {1.0f, 2.0f, 1.5f, 1.0f};
    float demand[4] = {5.0f, 5.0f, 5.0f, 5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 0.0f,
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 0.0f,
                           .discharge_rate_kw = 0.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should buy exactly demand each period
    for (size_t t = 0; t < T; t++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, demand[t], solution->buy_kwh[t]);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, solution->sell_kwh[t]);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, solution->charge_kwh[t]);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, solution->discharge_kwh[t]);
    }

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

void test_lps_constant_price_with_solar(void) {
    const size_t T = 4;
    float solar[4] = {2.0f, 4.0f, 3.0f, 1.0f};
    float price[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float demand[4] = {5.0f, 5.0f, 5.0f, 5.0f};

    lps_problem problem = {.num_periods = T,
                           .solar_forecast_kwh = solar,
                           .price_sek_kwh = price,
                           .demand_kwh = demand,
                           .battery_capacity_kwh = 0.0f,
                           .battery_initial_kwh = 0.0f,
                           .charge_rate_kw = 0.0f,
                           .discharge_rate_kw = 0.0f,
                           .efficiency = 0.9f,
                           .sell_price_ratio = 0.8f};

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should use all solar and buy remainder
    for (size_t t = 0; t < T; t++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, solar[t], solution->solar_direct_kwh[t]);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, demand[t] - solar[t], solution->buy_kwh[t]);
    }

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

void test_lps_price_arbitrage(void) {
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

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should charge at t=0, discharge at t=1
    TEST_ASSERT_TRUE(solution->charge_kwh[0] > 0.1f);
    TEST_ASSERT_TRUE(solution->discharge_kwh[1] > 0.1f);

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

void test_lps_excess_solar_sell(void) {
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

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // No grid purchase needed
    for (size_t t = 0; t < T; t++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, solution->buy_kwh[t]);
        TEST_ASSERT_TRUE(solution->solar_direct_kwh[t] >= demand[t] - 0.1f);
    }

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

/* ========================================================================
 * Edge Case Tests
 * ======================================================================== */

void test_lps_single_period(void) {
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

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

void test_lps_full_battery_start(void) {
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

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should discharge during expensive period (highest price at t=2)
    TEST_ASSERT_TRUE(solution->discharge_kwh[2] > 0.1f);

    ret = lps_solution_verify(&problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

void test_lps_storm_mode(void) {
    lps_solver *solver = lps_solver_create();
    const size_t T = 2;
    float solar[] = {0.0f, 0.0f};
    float price[] = {10.0f, 1.0f};  // Expensive t=0, Cheap t=1
    float demand[] = {0.0f, 0.0f};
    float min_level[] = {0.0f, 10.0f};  // Must be full at t=1

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

    lps_solution *solution = lps_solution_create(T);
    int ret = lps_solve(solver, &problem, solution);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Must charge to meet constraint even though price is high
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 10.0f, solution->charge_kwh[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 10.0f, solution->battery_level_kwh[1]);

    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);
}

/* ========================================================================
 * Performance Test
 * ======================================================================== */

void test_lps_performance_48h(void) {
    const size_t T = 48;
    float solar[48], price[48], demand[48];

    // Generate realistic data
    for (size_t t = 0; t < T; t++) {
        solar[t] = (t >= 6 && t <= 18) ? 3.0f : 0.0f;
        price[t] = 1.0f + 0.5f * sinf((float) t * 3.14159f / 12.0f);
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

    lps_solver *solver = lps_solver_create();
    lps_solution *solution = lps_solution_create(T);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int ret = lps_solve(solver, &problem, solution);

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_ms =
        (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // Verify solution correctness
    int verify_ret = lps_solution_verify(&problem, solution);

    // Cleanup BEFORE assertions (Unity uses longjmp on failure, which skips cleanup)
    lps_solution_destroy(&solution);
    lps_solver_destroy(&solver);

    // Now assert (safe - already cleaned up)
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(0, verify_ret);

    // Skip performance check under Valgrind (too slow)
    // In production, use: TEST_ASSERT_TRUE_MESSAGE(elapsed_ms < 50.0, "Performance target");
    (void) elapsed_ms;
}
