#include "lps.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Constants and Configuration
 * ======================================================================== */

enum {
    /** Number of battery state discretization levels (0% to 100% in 1% steps) */
    BATTERY_STATES = 101,

    /** Number of action discretization steps per dimension */
    ACTION_STEPS = 20,

    /** Maximum number of time periods supported */
    MAX_PERIODS = 168,  // 1 week at hourly resolution
    MAX_ACTIONS_BUFFER = 100,
    ACTION_DIVISOR = 4
};

/** Epsilon for floating point comparisons */
static const float EPSILON = 1e-6F;

/* ========================================================================
 * Internal Structures
 * ======================================================================== */

/**
 * @brief Action representation for DP.
 *
 * Represents a feasible action at a given time period and battery state.
 */
typedef struct {
    float buy;          /**< Grid purchase (kWh) */
    float sell;         /**< Grid sale (kWh) */
    float charge;       /**< Battery charge (kWh) */
    float discharge;    /**< Battery discharge (kWh) */
    float solar_direct; /**< Solar used directly (kWh) */
} action;

/**
 * @brief DP table entry.
 *
 * Stores the optimal value and action for a given state.
 */
typedef struct {
    float value;        /**< Optimal cost-to-go from this state */
    action best_action; /**< Optimal action to take */
    int next_state;     /**< Next battery state index after action */
} dp_entry;

/**
 * @brief LP Solver internal state.
 */
struct lps_solver {
    /** DP value table: [time][battery_state] */
    dp_entry** dp_table;

    /** Number of allocated time periods */
    size_t allocated_periods;

    /** Mutex for thread safety */
    pthread_mutex_t lock;
};

/* ========================================================================
 * Helper Macros
 * ======================================================================== */

#define SAFE_FREE(ptr) \
    do {               \
        free(ptr);     \
        (ptr) = NULL;  \
    } while (0)

#define CHECK_ALLOC(ptr)     \
    do {                     \
        if ((ptr) == NULL) { \
            ret = -ENOMEM;   \
            goto cleanup;    \
        }                    \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

lps_solver* lps_solver_create(void) {
    int ret = 0;
    lps_solver* solver = NULL;

    solver = malloc(sizeof(*solver));
    CHECK_ALLOC(solver);
    memset(solver, 0, sizeof(*solver));

    // Initialize mutex
    if (pthread_mutex_init(&solver->lock, NULL) != 0) {
        goto cleanup;
    }

    solver->dp_table = NULL;
    solver->allocated_periods = 0;

    return solver;

cleanup:
    SAFE_FREE(solver);
    return NULL;
    (void)ret;  // Suppress unused warning
}

static void free_dp_table(lps_solver* solver) {
    if (solver->dp_table != NULL) {
        for (size_t period_idx = 0; period_idx < solver->allocated_periods; period_idx++) {
            SAFE_FREE(solver->dp_table[period_idx]);
        }
        SAFE_FREE(*(void**)&solver->dp_table);
        solver->allocated_periods = 0;
    }
}

void lps_solver_destroy(lps_solver** solver_ptr) {
    if (solver_ptr == NULL || *solver_ptr == NULL) {
        return;
    }

    lps_solver* solver = *solver_ptr;

    free_dp_table(solver);

    // Destroy mutex
    pthread_mutex_destroy(&solver->lock);

    SAFE_FREE(solver);
    *solver_ptr = NULL;
}

static int allocate_solution_arrays(lps_solution* solution, size_t num_periods) {
    int ret = 0;

    solution->buy_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->buy_kwh);

    solution->sell_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->sell_kwh);

    solution->charge_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->charge_kwh);

    solution->discharge_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->discharge_kwh);

    solution->solar_direct_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->solar_direct_kwh);

    solution->battery_level_kwh = malloc(num_periods * sizeof(float));
    CHECK_ALLOC(solution->battery_level_kwh);

    // Initialize to zero
    memset(solution->buy_kwh, 0, num_periods * sizeof(float));
    memset(solution->sell_kwh, 0, num_periods * sizeof(float));
    memset(solution->charge_kwh, 0, num_periods * sizeof(float));
    memset(solution->discharge_kwh, 0, num_periods * sizeof(float));
    memset(solution->solar_direct_kwh, 0, num_periods * sizeof(float));
    memset(solution->battery_level_kwh, 0, num_periods * sizeof(float));

    return 0;

cleanup:
    return ret;
}

lps_solution* lps_solution_create(size_t num_periods) {
    int ret = 0;
    lps_solution* solution = NULL;

    if (num_periods == 0 || num_periods > MAX_PERIODS) {
        return NULL;
    }

    solution = malloc(sizeof(*solution));
    CHECK_ALLOC(solution);
    memset(solution, 0, sizeof(*solution));

    solution->num_periods = num_periods;

    ret = allocate_solution_arrays(solution, num_periods);
    if (ret != 0) {
        goto cleanup;
    }

    return solution;

cleanup:
    lps_solution_destroy(&solution);
    return NULL;
}

void lps_solution_destroy(lps_solution** solution_ptr) {
    if (solution_ptr == NULL || *solution_ptr == NULL) {
        return;
    }

    lps_solution* solution = *solution_ptr;

    SAFE_FREE(solution->buy_kwh);
    SAFE_FREE(solution->sell_kwh);
    SAFE_FREE(solution->charge_kwh);
    SAFE_FREE(solution->discharge_kwh);
    SAFE_FREE(solution->solar_direct_kwh);
    SAFE_FREE(solution->battery_level_kwh);

    SAFE_FREE(solution);
    *solution_ptr = NULL;
}

/* ========================================================================
 * Validation Functions
 * ======================================================================== */

int lps_problem_validate(const lps_problem* problem) {
    if (problem == NULL) {
        return -EINVAL;
    }

    // Check num_periods
    if (problem->num_periods == 0 || problem->num_periods > MAX_PERIODS) {
        return -ERANGE;
    }

    // Check array pointers
    if (problem->solar_forecast_kwh == NULL || problem->price_sek_kwh == NULL ||
        problem->demand_kwh == NULL) {
        return -EINVAL;
    }

    // Check battery parameters
    if (problem->battery_capacity_kwh < 0.0F || problem->battery_initial_kwh < 0.0F ||
        problem->battery_initial_kwh > problem->battery_capacity_kwh) {
        return -ERANGE;
    }

    if (problem->charge_rate_kw < 0.0F || problem->discharge_rate_kw < 0.0F) {
        return -ERANGE;
    }

    if (problem->efficiency <= 0.0F || problem->efficiency > 1.0F) {
        return -ERANGE;
    }

    if (problem->sell_price_ratio < 0.0F || problem->sell_price_ratio > 1.0F) {
        return -ERANGE;
    }

    // Check array values
    for (size_t period_idx = 0; period_idx < problem->num_periods; period_idx++) {
        if (problem->solar_forecast_kwh[period_idx] < 0.0F ||
            problem->price_sek_kwh[period_idx] < 0.0F || problem->demand_kwh[period_idx] < 0.0F) {
            return -ERANGE;
        }

        if (problem->min_battery_level_kwh != NULL) {
            if (problem->min_battery_level_kwh[period_idx] < -EPSILON ||
                problem->min_battery_level_kwh[period_idx] >
                    problem->battery_capacity_kwh + EPSILON) {
                return -ERANGE;
            }
        }
    }

    return 0;
}

int lps_solution_verify(const lps_problem* problem, const lps_solution* solution) {
    if (problem == NULL || solution == NULL) {
        return -EINVAL;
    }

    if (problem->num_periods != solution->num_periods) {
        return -EINVAL;
    }

    const float tolerance = 0.01F;  // 10 Wh tolerance

    for (size_t period_idx = 0; period_idx < problem->num_periods; period_idx++) {
        // Check non-negativity
        if (solution->buy_kwh[period_idx] < -EPSILON || solution->sell_kwh[period_idx] < -EPSILON ||
            solution->charge_kwh[period_idx] < -EPSILON ||
            solution->discharge_kwh[period_idx] < -EPSILON ||
            solution->solar_direct_kwh[period_idx] < -EPSILON) {
            return -EDOM;
        }

        // Check energy balance: buy + solar_direct + discharge == demand + charge
        const float supply = solution->buy_kwh[period_idx] +
                             solution->solar_direct_kwh[period_idx] +
                             solution->discharge_kwh[period_idx];
        const float load = problem->demand_kwh[period_idx] + solution->charge_kwh[period_idx];

        if (fabsf(supply - load) > tolerance) {
            return -EDOM;
        }

        // Check solar production limit: solar_direct + sell <= solar_forecast
        const float solar_used =
            solution->solar_direct_kwh[period_idx] + solution->sell_kwh[period_idx];
        if (solar_used > problem->solar_forecast_kwh[period_idx] + tolerance) {
            return -EDOM;
        }

        // Check battery capacity
        if (solution->battery_level_kwh[period_idx] < -EPSILON ||
            solution->battery_level_kwh[period_idx] > problem->battery_capacity_kwh + tolerance) {
            return -EDOM;
        }

        // Check charge/discharge rates
        if (solution->charge_kwh[period_idx] > problem->charge_rate_kw + tolerance ||
            solution->discharge_kwh[period_idx] > problem->discharge_rate_kw + tolerance) {
            return -EDOM;
        }
    }

    return 0;
}

/* ========================================================================
 * DP Algorithm - Helper Functions
 * ======================================================================== */

/**
 * @brief Converts battery level (kWh) to state index.
 */
static inline int battery_to_state(float battery_kwh, float capacity_kwh) {
    if (capacity_kwh < EPSILON) {
        return 0;
    }

    const float fraction = battery_kwh / capacity_kwh;
    const int state = (int)(fraction * (float)(BATTERY_STATES - 1) + 0.5F);

    return MAX(0, MIN(BATTERY_STATES - 1, state));
}

/**
 * @brief Converts state index to battery level (kWh).
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static inline float state_to_battery(int state, float capacity_kwh) {
    const float fraction = (float)state / (float)(BATTERY_STATES - 1);
    return fraction * capacity_kwh;
}

/**
 * @brief Computes immediate cost of an action.
 */
static float compute_cost(const action* act, float price, float sell_ratio) {
    const float buy_cost = act->buy * price;
    const float sell_revenue = act->sell * price * sell_ratio;
    return buy_cost - sell_revenue;
}

/**
 * @brief Computes next battery state after action.
 */
static int compute_next_state(int current_state, const action* act, const lps_problem* problem) {
    const float current_battery = state_to_battery(current_state, problem->battery_capacity_kwh);
    const float next_battery =
        current_battery + (act->charge * problem->efficiency) - act->discharge;

    return battery_to_state(next_battery, problem->battery_capacity_kwh);
}

/**
 * @brief Checks if action is feasible.
 */
static bool is_action_feasible(const action* act, int state, const lps_problem* problem) {
    // Non-negativity
    if (act->buy < -EPSILON || act->sell < -EPSILON || act->charge < -EPSILON ||
        act->discharge < -EPSILON || act->solar_direct < -EPSILON) {
        return false;
    }

    // Can't charge and discharge simultaneously (simplified)
    if (act->charge > EPSILON && act->discharge > EPSILON) {
        return false;
    }

    // Battery state bounds after action
    const float current_battery = state_to_battery(state, problem->battery_capacity_kwh);
    const float next_battery =
        current_battery + (act->charge * problem->efficiency) - act->discharge;

    if (next_battery < -EPSILON || next_battery > problem->battery_capacity_kwh + EPSILON) {
        return false;
    }

    return true;
}

/**
 * @brief Generates feasible actions for a given state and time period.
 *
 * This is a simplified action enumeration that discretizes the action space.
 * For production, this could be optimized with constraint-based generation.
 */
/**
 * @brief Dispatch net load between solar and grid.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static void dispatch_energy(float demand, float solar, action* act) {
    const float net_load = demand + act->charge - act->discharge;

    if (net_load > 0.0F) {
        act->solar_direct = MIN(net_load, solar);
        act->buy = net_load - act->solar_direct;
        act->sell = solar - act->solar_direct;
    } else {
        act->solar_direct = 0.0F;
        act->buy = 0.0F;
        act->sell = solar;
    }

    // Ensure non-negative (FP precision safety)
    if (act->buy < 0.0F) {
        act->buy = 0.0F;
    }
    if (act->sell < 0.0F) {
        act->sell = 0.0F;
    }
}

/**
 * @brief Adds a feasible action to the buffer.
 */
static void add_feasible_action(const action* act, int state, const lps_problem* problem,
                                action* actions, size_t* num_actions) {
    if (*num_actions >= (size_t)MAX_ACTIONS_BUFFER) {
        return;
    }

    if (is_action_feasible(act, state, problem)) {
        actions[*num_actions] = *act;
        (*num_actions)++;
    }
}

/**
 * @brief Generates feasible actions for a given state and time period.
 */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static void enumerate_actions(int state, int time_idx, const lps_problem* problem, action* actions,
                              size_t* num_actions) {
    *num_actions = 0;

    const float demand = problem->demand_kwh[time_idx];
    const float solar = problem->solar_forecast_kwh[time_idx];
    const float current_battery = state_to_battery(state, problem->battery_capacity_kwh);

    for (int batt_action = -4; batt_action <= 4; batt_action++) {
        action act = {0};

        if (batt_action > 0) {
            const float max_charge =
                MIN(problem->charge_rate_kw, problem->battery_capacity_kwh - current_battery);
            act.charge = ((float)batt_action / (float)ACTION_DIVISOR) * max_charge;
        } else if (batt_action < 0) {
            const float max_discharge = MIN(problem->discharge_rate_kw, current_battery);
            const float safe_discharge = MIN(max_discharge, demand);
            act.discharge = (-(float)batt_action / (float)ACTION_DIVISOR) * safe_discharge;
        }

        dispatch_energy(demand, solar, &act);
        add_feasible_action(&act, state, problem, actions, num_actions);
    }
}

/* ========================================================================
 * DP Algorithm - Main Functions
 * ======================================================================== */

/**
 * @brief Allocates DP table for given problem size.
 */
static int allocate_dp_table(lps_solver* solver, size_t num_periods) {
    int ret = 0;

    // Free existing table if different size
    if (solver->dp_table != NULL && solver->allocated_periods != num_periods) {
        free_dp_table(solver);
    }

    // Allocate new table if needed
    if (solver->dp_table == NULL) {
        solver->dp_table = (dp_entry**)malloc(num_periods * sizeof(dp_entry*));
        CHECK_ALLOC(solver->dp_table);
        memset((void*)solver->dp_table, 0, num_periods * sizeof(dp_entry*));

        for (size_t period_idx = 0; period_idx < num_periods; period_idx++) {
            solver->dp_table[period_idx] = malloc(BATTERY_STATES * sizeof(dp_entry));
            CHECK_ALLOC(solver->dp_table[period_idx]);
        }

        solver->allocated_periods = num_periods;
    }

    return 0;

cleanup:
    return ret;
}

/**
 * @brief DP backward pass - computes optimal value function.
 */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static int dp_backward_pass(lps_solver* solver, const lps_problem* problem) {
    const size_t horizon = problem->num_periods;

    // Initialize terminal value function (t = horizon-1)
    for (int state_idx = 0; state_idx < BATTERY_STATES; state_idx++) {
        solver->dp_table[horizon - 1][state_idx].value = FLT_MAX;
    }

    // Enumerate actions for terminal period
    action actions[MAX_ACTIONS_BUFFER];
    size_t num_actions;

    for (int state_idx = 0; state_idx < BATTERY_STATES; state_idx++) {
        // Prune invalid states (violating min battery level)
        if (problem->min_battery_level_kwh != NULL) {
            const float bat = state_to_battery(state_idx, problem->battery_capacity_kwh);
            if (bat < problem->min_battery_level_kwh[horizon - 1] - EPSILON) {
                continue;
            }
        }

        enumerate_actions(state_idx, (int)horizon - 1, problem, actions, &num_actions);

        for (size_t act = 0; act < num_actions; act++) {
            const float cost = compute_cost(&actions[act], problem->price_sek_kwh[horizon - 1],
                                            problem->sell_price_ratio);

            if (cost < solver->dp_table[horizon - 1][state_idx].value) {
                solver->dp_table[horizon - 1][state_idx].value = cost;
                solver->dp_table[horizon - 1][state_idx].best_action = actions[act];
                solver->dp_table[horizon - 1][state_idx].next_state =
                    compute_next_state(state_idx, &actions[act], problem);
            }
        }
    }

    // Backward induction for t = horizon-2 down to 0
    for (int period_idx = (int)horizon - 2; period_idx >= 0; period_idx--) {
        for (int state_idx = 0; state_idx < BATTERY_STATES; state_idx++) {
            solver->dp_table[period_idx][state_idx].value = FLT_MAX;

            // Prune invalid states (violating min battery level)
            if (problem->min_battery_level_kwh != NULL) {
                const float bat = state_to_battery(state_idx, problem->battery_capacity_kwh);
                if (bat < problem->min_battery_level_kwh[period_idx] - EPSILON) {
                    continue;
                }
            }

            enumerate_actions(state_idx, period_idx, problem, actions, &num_actions);

            for (size_t act = 0; act < num_actions; act++) {
                const float immediate_cost = compute_cost(
                    &actions[act], problem->price_sek_kwh[period_idx], problem->sell_price_ratio);

                const int next_s = compute_next_state(state_idx, &actions[act], problem);

                const float future_cost = solver->dp_table[period_idx + 1][next_s].value;
                const float total_cost = immediate_cost + future_cost;

                if (total_cost < solver->dp_table[period_idx][state_idx].value) {
                    solver->dp_table[period_idx][state_idx].value = total_cost;
                    solver->dp_table[period_idx][state_idx].best_action = actions[act];
                    solver->dp_table[period_idx][state_idx].next_state = next_s;
                }
            }
        }
    }

    return 0;
}

/**
 * @brief DP forward pass - reconstructs optimal solution.
 */
static int dp_forward_pass(lps_solver* solver, const lps_problem* problem, lps_solution* solution) {
    const size_t horizon = problem->num_periods;

    // Start with initial battery state
    int state = battery_to_state(problem->battery_initial_kwh, problem->battery_capacity_kwh);

    // Initialize metrics
    solution->total_cost_sek = 0.0F;
    solution->total_grid_import_kwh = 0.0F;
    solution->total_grid_export_kwh = 0.0F;
    solution->total_solar_used_kwh = 0.0F;

    // Follow optimal policy
    for (size_t ts = 0; ts < horizon; ts++) {
        const dp_entry* entry = &solver->dp_table[ts][state];
        const action* act = &entry->best_action;

        // Store actions
        solution->buy_kwh[ts] = act->buy;
        solution->sell_kwh[ts] = act->sell;
        solution->charge_kwh[ts] = act->charge;
        solution->discharge_kwh[ts] = act->discharge;
        solution->solar_direct_kwh[ts] = act->solar_direct;
        solution->battery_level_kwh[ts] = state_to_battery(state, problem->battery_capacity_kwh);

        // Update metrics
        solution->total_cost_sek +=
            compute_cost(act, problem->price_sek_kwh[ts], problem->sell_price_ratio);
        solution->total_grid_import_kwh += act->buy;
        solution->total_grid_export_kwh += act->sell;
        solution->total_solar_used_kwh += act->solar_direct;

        // Transition to next state
        state = entry->next_state;
    }

    return 0;
}

/* ========================================================================
 * Public Solve Function
 * ======================================================================== */

int lps_solve(lps_solver* solver, const lps_problem* problem, lps_solution* solution) {
    int ret = 0;

    // Validate inputs
    if (solver == NULL || problem == NULL || solution == NULL) {
        return -EINVAL;
    }

    ret = lps_problem_validate(problem);
    if (ret != 0) {
        return ret;
    }

    if (solution->num_periods != problem->num_periods) {
        return -EINVAL;
    }

    // Lock for thread safety
    pthread_mutex_lock(&solver->lock);

    // Allocate DP table
    ret = allocate_dp_table(solver, problem->num_periods);
    if (ret != 0) {
        goto unlock;
    }

    // Run DP algorithm
    ret = dp_backward_pass(solver, problem);
    if (ret != 0) {
        goto unlock;
    }

    ret = dp_forward_pass(solver, problem, solution);
    if (ret != 0) {
        goto unlock;
    }

unlock:
    pthread_mutex_unlock(&solver->lock);
    return ret;
}
