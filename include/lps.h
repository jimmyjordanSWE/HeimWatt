#ifndef LPS_H
#define LPS_H

/**
 * @file lps.h
 * @brief Linear Programming Solver for Energy Optimization
 *
 * This module provides a specialized Dynamic Programming solver for the
 * HeimWatt energy optimization problem. It determines optimal battery
 * charging/discharging and grid import/export decisions to minimize cost
 * over a planning horizon (typically 48-72 hours).
 *
 * @note This solver is optimized for time-series energy problems and is
 *       not a general-purpose LP solver.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Type Definitions
 * ======================================================================== */

/**
 * @brief Opaque solver handle.
 *
 * Contains internal Dynamic Programming tables and state. Thread-safe
 * for concurrent solve operations.
 */
typedef struct lps_solver lps_solver;

/**
 * @brief Energy optimization problem definition.
 *
 * Defines the input parameters for the optimization problem including
 * forecasts, battery parameters, and grid constraints.
 */
typedef struct {
    size_t num_periods;                  // Number of time periods in planning horizon (e.g., 48)
    const float* solar_forecast_kwh;     // Solar production forecast (kWh)
    const float* price_sek_kwh;          // Electricity spot price per period (SEK/kWh)
    const float* demand_kwh;             // Household energy demand per period (kWh)
    float battery_capacity_kwh;          // Max battery storage capacity (kWh)
    float battery_initial_kwh;           // Initial battery charge level (kWh)
    float charge_rate_kw;                // Max battery charge rate (kW)
    float discharge_rate_kw;             // Max battery discharge rate (kW)
    float efficiency;                    // Round-trip efficiency (0.0-1.0)
    const float* min_battery_level_kwh;  // Min battery level per period (kWh, optional)
    float sell_price_ratio;              // Sell price as fraction of buy price (0.0-1.0)
} lps_problem;

/**
 * @brief Optimal energy plan solution.
 *
 * Contains the optimal decisions for each time period and aggregate metrics.
 * All arrays have length equal to num_periods.
 */
typedef struct {
    size_t num_periods;           // Number of time periods (matches input)
    float* buy_kwh;               // Energy purchased from grid (kWh)
    float* sell_kwh;              // Energy sold to grid (kWh)
    float* charge_kwh;            // Energy stored in battery (kWh)
    float* discharge_kwh;         // Energy drawn from battery (kWh)
    float* solar_direct_kwh;      // Solar energy used immediately (kWh)
    float* battery_level_kwh;     // Battery charge level at start of each period (kWh)
    float total_cost_sek;         // Total cost over planning horizon (SEK)
    float total_grid_import_kwh;  // Total energy purchased from grid (kWh)
    float total_grid_export_kwh;  // Total energy sold to grid (kWh)
    float total_solar_used_kwh;   // Total solar energy utilized (kWh)
} lps_solution;

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Creates a new LP solver instance.
 *
 * Allocates and initializes a solver with default parameters. The solver
 * can be reused for multiple solve operations.
 *
 * @return Solver handle on success, NULL on allocation failure.
 *
 * @note Must be freed with lps_solver_destroy() to avoid memory leaks.
 * @see lps_solver_destroy
 */
lps_solver* lps_solver_create(void);

/**
 * @brief Destroys solver and frees all resources.
 *
 * Releases all memory associated with the solver including internal DP
 * tables and synchronization primitives. Sets the pointer to NULL.
 *
 * @param solver Pointer to solver handle (will be set to NULL).
 *
 * @note Safe to call with NULL or already-freed pointer.
 */
void lps_solver_destroy(lps_solver** solver);

/**
 * @brief Allocates solution structure for given problem size.
 *
 * Pre-allocates all arrays needed to store the solution for a problem
 * with the specified number of time periods.
 *
 * @param num_periods Number of time periods in the problem.
 * @return Solution handle on success, NULL on allocation failure.
 *
 * @note Must be freed with lps_solution_destroy() to avoid memory leaks.
 * @see lps_solution_destroy
 */
lps_solution* lps_solution_create(size_t num_periods);

/**
 * @brief Frees solution and all internal arrays.
 *
 * Releases all memory associated with the solution including all action
 * arrays and metrics. Sets the pointer to NULL.
 *
 * @param solution Pointer to solution handle (will be set to NULL).
 *
 * @note Safe to call with NULL or already-freed pointer.
 */
void lps_solution_destroy(lps_solution** solution);

/* ========================================================================
 * Solving Functions
 * ======================================================================== */

/**
 * @brief Solves the energy optimization problem.
 *
 * Computes the optimal energy plan that minimizes cost while satisfying
 * all constraints (energy balance, battery limits, etc.). Uses Dynamic
 * Programming for guaranteed optimal solution.
 *
 * @param solver Solver instance (must not be NULL).
 * @param problem Problem definition (must not be NULL).
 * @param solution Pre-allocated solution structure (must not be NULL).
 * @return 0 on success, negative errno on failure:
 *         - -EINVAL: Invalid input (NULL pointers, invalid parameters)
 *         - -ENOMEM: Memory allocation failure during solve
 *         - -ERANGE: Problem too large or numerically infeasible
 *
 * @note The solution must be pre-allocated with lps_solution_create()
 *       and have num_periods matching the problem.
 * @note This function is thread-safe; multiple threads can solve
 *       different problems concurrently using the same solver.
 *
 * @see lps_problem_validate
 * @see lps_solution_verify
 */
int lps_solve(lps_solver* solver, const lps_problem* problem, lps_solution* solution);

/* ========================================================================
 * Validation Functions
 * ======================================================================== */

/**
 * @brief Validates problem definition.
 *
 * Checks that all problem parameters are valid before attempting to solve.
 * Validates:
 * - Non-NULL pointers
 * - Positive values where required
 * - Valid ranges (e.g., efficiency in [0,1])
 * - Consistent array lengths
 *
 * @param problem Problem to validate.
 * @return 0 if valid, negative errno if invalid:
 *         - -EINVAL: NULL pointers or invalid parameters
 *         - -ERANGE: Values out of acceptable range
 *
 * @note This is called automatically by lps_solve(), but can be used
 *       independently for early validation.
 */
int lps_problem_validate(const lps_problem* problem);

/**
 * @brief Verifies solution satisfies all constraints.
 *
 * Checks that a solution is feasible by verifying:
 * - Energy balance (demand met each period)
 * - Battery capacity limits
 * - Charge/discharge rate limits
 * - Solar production limits
 * - Non-negativity constraints
 *
 * @param problem Original problem definition.
 * @param solution Solution to verify.
 * @return 0 if valid, negative errno if constraint violated:
 *         - -EINVAL: NULL pointers
 *         - -EDOM: Constraint violation detected
 *
 * @note Useful for testing and debugging. Production code typically
 *       trusts the solver output.
 */
int lps_solution_verify(const lps_problem* problem, const lps_solution* solution);

#ifdef __cplusplus
}
#endif

#endif  // LPS_H
