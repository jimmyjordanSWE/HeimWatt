/**
 * @file fuzz_lps_solver.c
 * @brief AFL++ fuzz harness for LPS Solver
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugins/out/energy_strategy/lps/lps.h"

// Fixed size header for scalar params
typedef struct __attribute__((packed))
{
    float battery_capacity;
    float battery_initial;
    float charge_rate;
    float discharge_rate;
    float efficiency;
    float sell_price_ratio;
} fuzz_header;

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

int main(void)
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000))
    {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
#else
    char buf[16384];  // 16KB max for non-AFL
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return 0;
    size_t len = (size_t) n;
#endif

        // 1. Min size check
        if (len <= sizeof(fuzz_header))
        {
#ifdef __AFL_HAVE_MANUAL_CONTROL
            continue;
#else
        return 0;
#endif
        }

        // 2. Parse header
        const fuzz_header *hdr = (const fuzz_header *) buf;
        size_t remaining = len - sizeof(fuzz_header);

        // 3. Determine num_periods (3 floats per period: solar, price, demand)
        size_t bytes_per_period = 3 * sizeof(float);
        size_t num_periods = remaining / bytes_per_period;

        if (num_periods == 0)
        {
#ifdef __AFL_HAVE_MANUAL_CONTROL
            continue;
#else
        return 0;
#endif
        }

        // Limit to reasonble size to prevent timeout/OOM during fuzzing
        if (num_periods > 100) num_periods = 100;

        // 4. Allocate arrays
        float *solar = malloc(num_periods * sizeof(float));
        float *price = malloc(num_periods * sizeof(float));
        float *demand = malloc(num_periods * sizeof(float));

        // Fill arrays from buffer
        const float *data_ptr = (const float *) (buf + sizeof(fuzz_header));
        for (size_t i = 0; i < num_periods; i++)
        {
            solar[i] = data_ptr[i * 3 + 0];
            price[i] = data_ptr[i * 3 + 1];
            demand[i] = data_ptr[i * 3 + 2];
        }

        // 5. Construct problem
        lps_problem problem = {.num_periods = num_periods,
                               .solar_forecast_kwh = solar,
                               .price_sek_kwh = price,
                               .demand_kwh = demand,
                               .battery_capacity_kwh = hdr->battery_capacity,
                               .battery_initial_kwh = hdr->battery_initial,
                               .charge_rate_kw = hdr->charge_rate,
                               .discharge_rate_kw = hdr->discharge_rate,
                               .efficiency = hdr->efficiency,
                               .sell_price_ratio = hdr->sell_price_ratio,
                               .min_battery_level_kwh = NULL};

        // 6. Run Solver
        lps_solver *solver = lps_solver_create();
        lps_solution *solution = lps_solution_create(num_periods);

        // We don't check return value - we want to find CRASHES
        if (solver && solution)
        {
            lps_solve(solver, &problem, solution);
        }

        // 7. Cleanup
        lps_solution_destroy(&solution);
        lps_solver_destroy(&solver);
        free(solar);
        free(price);
        free(demand);

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    return 0;
}
