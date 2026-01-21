/**
 * @file plugin_mgr.c
 * @brief Plugin lifecycle management implementation
 *
 * Discovers, forks, and supervises plugin processes.
 */

#include "plugin_mgr.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libs/cJSON.h"
#include "libs/log.h"
#include "memory.h"
#include "semantic_types.h"

enum
{
    MAX_PLUGINS = 32,
    MAX_PATH_LEN = 4096,
    MAX_ID_LEN = 256
};

struct plugin_handle
{
    char id[MAX_ID_LEN];
    char exe_path[MAX_PATH_LEN];
    char manifest_path[MAX_PATH_LEN];
    plugin_type type;
    plugin_state state;
    pid_t pid;
    int interval;
    time_t last_run;
    time_t next_run;  // Next aligned run time
    char resource[MAX_ID_LEN];
    cJSON *config_json;       // Store entire config object
    cJSON *provides_json;     // Store provides object for dynamic queries
    cJSON *requires_json;     // Store requires list for dependency checking
    cJSON *devices_json;      // Store devices definition
    cJSON *credentials_json;  // Store credentials definition
    uint32_t capabilities;    // Bitmask of plugin_capability
};

struct plugin_mgr
{
    plugin_handle plugins[MAX_PLUGINS];
    int count;
    char plugins_dir[MAX_PATH_LEN];
    char ipc_sock[MAX_PATH_LEN];
};

int plugin_mgr_count(const plugin_mgr *mgr) { return mgr ? mgr->count : 0; }

/* ============================================================
 * HELPERS
 * ============================================================ */

// Forward declaration
static time_t calculate_next_aligned_time(int interval_sec);

/**
 * Convert plugin ID to executable name.
 * Example: "se.smhi.weather" -> "smhi_weather"
 * Takes the last two segments and joins with underscore.
 */
static void id_to_exe_name(const char *id, char *out, size_t out_len)
{
    // Find segments by splitting on '.'
    const char *segments[8];
    int seg_count = 0;
    char buf[MAX_ID_LEN];
    (void) snprintf(buf, sizeof(buf), "%s", id);

    char *saveptr;
    char *tok = strtok_r(buf, ".", &saveptr);
    while (tok && seg_count < 8)
    {
        segments[seg_count++] = tok;
        tok = strtok_r(NULL, ".", &saveptr);
    }

    if (seg_count >= 2)
    {
        // Use last two segments: "smhi.weather" -> "smhi_weather"
        (void) snprintf(out, out_len, "%s_%s", segments[seg_count - 2], segments[seg_count - 1]);
    }
    else if (seg_count == 1)
    {
        (void) snprintf(out, out_len, "%s", segments[0]);
    }
    else
    {
        (void) snprintf(out, out_len, "unknown");
    }
}

/**
 * Load and parse manifest.json, extract plugin info.
 */
static int load_manifest(const char *manifest_path, plugin_handle *h)
{
    FILE *f = fopen(manifest_path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = mem_alloc((size_t) len + 1);
    if (!content)
    {
        fclose(f);
        return -1;
    }

    size_t read_len = fread(content, 1, (size_t) len, f);
    content[read_len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(content);
    mem_free(content);

    if (!json) return -1;

    // Extract "id"
    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    if (id_item && id_item->valuestring)
    {
        (void) snprintf(h->id, sizeof(h->id), "%s", id_item->valuestring);
    }
    else
    {
        cJSON_Delete(json);
        return -1;
    }

    // Extract "type"
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (type_item && type_item->valuestring)
    {
        if (strcmp(type_item->valuestring, "in") == 0)
            h->type = PLUGIN_TYPE_IN;
        else
            h->type = PLUGIN_TYPE_OUT;
    }

    // Extract "resource"
    cJSON *resource_item = cJSON_GetObjectItem(json, "resource");
    if (resource_item && resource_item->valuestring)
    {
        (void) snprintf(h->resource, sizeof(h->resource), "%s", resource_item->valuestring);
    }
    else
    {
        (void) snprintf(h->resource, sizeof(h->resource), "External Source");
    }

    cJSON *schedule = cJSON_GetObjectItem(json, "schedule");
    if (schedule)
    {
        cJSON *interval = cJSON_GetObjectItem(schedule, "interval_seconds");
        if (interval)
        {
            h->interval = interval->valueint;
        }
        else
        {
            h->interval = 60;  // Default
        }
    }
    else
    {
        h->interval = 60;  // Default
    }

    // Extract "config" object
    cJSON *config_item = cJSON_GetObjectItem(json, "config");
    if (config_item)
    {
        h->config_json = cJSON_Duplicate(config_item, 1);
    }

    // Extract "provides" object for dynamic metadata queries
    cJSON *provides_item = cJSON_GetObjectItem(json, "provides");
    if (provides_item)
    {
        h->provides_json = cJSON_Duplicate(provides_item, 1);
    }

    // Extract "requires" array for dependency checking
    cJSON *requires_item = cJSON_GetObjectItem(json, "requires");
    if (requires_item)
    {
        h->requires_json = cJSON_Duplicate(requires_item, 1);
    }

    // Extract "devices"
    cJSON *devices = cJSON_GetObjectItem(json, "devices");
    if (devices)
    {
        h->devices_json = cJSON_Duplicate(devices, 1);
    }

    // Extract "credentials"
    cJSON *creds = cJSON_GetObjectItem(json, "credentials");
    if (creds)
    {
        h->credentials_json = cJSON_Duplicate(creds, 1);
    }

    // Extract "capabilities"
    cJSON *caps = cJSON_GetObjectItem(json, "capabilities");
    h->capabilities = CAP_NONE;
    if (caps && cJSON_IsArray(caps))
    {
        cJSON *cap = NULL;
        cJSON_ArrayForEach(cap, caps)
        {
            if (cJSON_IsString(cap) && cap->valuestring)
            {
                if (strcmp(cap->valuestring, "report") == 0)
                    h->capabilities |= CAP_REPORT;
                else if (strcmp(cap->valuestring, "query") == 0)
                    h->capabilities |= CAP_QUERY;
                else if (strcmp(cap->valuestring, "actuate") == 0)
                    h->capabilities |= CAP_ACTUATE;
                else if (strcmp(cap->valuestring, "constrain") == 0)
                    h->capabilities |= CAP_CONSTRAIN;
                else if (strcmp(cap->valuestring, "sense") == 0)
                    h->capabilities |= CAP_SENSE;
            }
        }
    }
    else
    {
        // Default: If no caps specified, assume legacy "report" + "query"?
        // Or strictly NONE?
        // For safety, let's default to CAP_REPORT if type is IN, generic default NONE
        // But plan said default to NONE. Let's stick to NONE.
        h->capabilities = CAP_NONE;
    }

    cJSON_Delete(json);
    return 0;
}

/* ============================================================
 * MANAGER LIFECYCLE
 * ============================================================ */

int plugin_mgr_init(plugin_mgr **mgr, const char *plugins_dir, const char *ipc_sock)
{
    if (!mgr || !plugins_dir || !ipc_sock) return -1;

    plugin_mgr *m = mem_alloc(sizeof(*m));
    if (!m) return -1;

    (void) snprintf(m->plugins_dir, sizeof(m->plugins_dir), "%s", plugins_dir);
    (void) snprintf(m->ipc_sock, sizeof(m->ipc_sock), "%s", ipc_sock);

    *mgr = m;
    return 0;
}

void plugin_mgr_destroy(plugin_mgr **mgr)
{
    if (!mgr || !*mgr) return;

    // Free config_json and provides_json for each plugin to prevent memory leaks
    for (int i = 0; i < (*mgr)->count; i++)
    {
        if ((*mgr)->plugins[i].config_json)
        {
            cJSON_Delete((*mgr)->plugins[i].config_json);
            (*mgr)->plugins[i].config_json = NULL;
        }
        if ((*mgr)->plugins[i].provides_json)
        {
            cJSON_Delete((*mgr)->plugins[i].provides_json);
            (*mgr)->plugins[i].provides_json = NULL;
        }
        if ((*mgr)->plugins[i].requires_json)
        {
            cJSON_Delete((*mgr)->plugins[i].requires_json);
            (*mgr)->plugins[i].requires_json = NULL;
        }
        if ((*mgr)->plugins[i].devices_json)
        {
            cJSON_Delete((*mgr)->plugins[i].devices_json);
            (*mgr)->plugins[i].devices_json = NULL;
        }
        if ((*mgr)->plugins[i].credentials_json)
        {
            cJSON_Delete((*mgr)->plugins[i].credentials_json);
            (*mgr)->plugins[i].credentials_json = NULL;
        }
    }

    mem_free(*mgr);
    *mgr = NULL;
}

/* ============================================================
 * DISCOVERY & LOADING
 * ============================================================ */

int plugin_mgr_scan(plugin_mgr *mgr)
{
    if (!mgr) return -1;

    const char *subdirs[] = {"in", "out"};
    int found = 0;

    for (int s = 0; s < 2; s++)
    {
        char subdir_path[MAX_PATH_LEN];
        (void) snprintf(subdir_path, sizeof(subdir_path), "%.3000s/%s", mgr->plugins_dir,
                        subdirs[s]);

        DIR *dir = opendir(subdir_path);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && mgr->count < MAX_PLUGINS)
        {
            if (entry->d_type != DT_DIR) continue;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char manifest_path[MAX_PATH_LEN];
            (void) snprintf(manifest_path, sizeof(manifest_path), "%.3000s/%s/manifest.json",
                            subdir_path, entry->d_name);

            if (access(manifest_path, F_OK) != 0) continue;

            plugin_handle *h = &mgr->plugins[mgr->count];
            memset(h, 0, sizeof(*h));
            (void) snprintf(h->manifest_path, sizeof(h->manifest_path), "%s", manifest_path);

            if (load_manifest(manifest_path, h) < 0)
            {
                log_warn("[PLUGIN] Failed to parse: %s", manifest_path);
                continue;
            }

            // Determine executable path
            char exe_name[128];
            id_to_exe_name(h->id, exe_name, sizeof(exe_name));
            (void) snprintf(h->exe_path, sizeof(h->exe_path), "build/bin/plugins/%s", exe_name);

            h->state = PLUGIN_STATE_STOPPED;
            h->next_run = calculate_next_aligned_time(h->interval);  // Set initial aligned time
            mgr->count++;
            found++;

            log_info("[PLUGIN] Discovered: %s (%s)", h->id, subdirs[s]);
        }
        closedir(dir);
    }

    log_info("[PLUGIN] Scan complete: %d plugin(s) found", found);
    return found;
}

int plugin_mgr_validate(plugin_mgr *mgr)
{
    if (!mgr) return -1;

    int errors = 0;
    for (int i = 0; i < mgr->count; i++)
    {
        plugin_handle *h = &mgr->plugins[i];
        if (access(h->exe_path, X_OK) != 0)
        {
            log_warn("[PLUGIN] Executable not found: %s (for %s)", h->exe_path, h->id);
            h->state = PLUGIN_STATE_FAILED;
            errors++;
        }
    }
    return errors > 0 ? -1 : 0;
}

/* ============================================================
 * PROCESS CONTROL
 * ============================================================ */

static int start_plugin(plugin_handle *h, const char *ipc_sock)
{
    if (h->state == PLUGIN_STATE_RUNNING) return 0;

    if (access(h->exe_path, X_OK) != 0)
    {
        log_warn("[PLUGIN] Cannot start %s: executable not found at %s", h->id, h->exe_path);
        h->state = PLUGIN_STATE_FAILED;
        return -1;
    }

    h->state = PLUGIN_STATE_STARTING;

    pid_t pid = fork();
    if (pid < 0)
    {
        log_error("[PLUGIN] Fork failed for %s: %s", h->id, strerror(errno));
        h->state = PLUGIN_STATE_FAILED;
        return -1;
    }

    if (pid == 0)
    {
        // Child process
        // Pass socket path and plugin ID as expected by SDK
        execl(h->exe_path, h->exe_path, "--socket", ipc_sock, "--id", h->id, NULL);
        // If we get here, exec failed
        fprintf(stderr, "[PLUGIN] Exec failed for %s: %s\n", h->id, strerror(errno));
        _exit(127);
    }

    // Parent
    h->pid = pid;
    h->state = PLUGIN_STATE_RUNNING;
    log_info("[PLUGIN] Started: %s (PID %d)", h->id, pid);
    return 0;
}

int plugin_mgr_start_all(plugin_mgr *mgr)
{
    if (!mgr) return -1;

    int started = 0;
    for (int i = 0; i < mgr->count; i++)
    {
        plugin_handle *h = &mgr->plugins[i];
        if (start_plugin(h, mgr->ipc_sock) == 0)
        {
            started++;
        }
    }

    log_info("[PLUGIN] Started %d plugin(s)", started);
    return started;
}

static void stop_plugin(plugin_handle *h)
{
    if (h->state != PLUGIN_STATE_RUNNING || h->pid <= 0) return;

    log_info("[PLUGIN] Stopping: %s (PID %d)", h->id, h->pid);

    // Mark as stopping. The health check loop will reap it.
    h->state = PLUGIN_STATE_STOPPING;

    // Send SIGTERM
    if (kill(h->pid, SIGTERM) != 0)
    {
        // If kill fails (e.g. process gone), mark stopped immediately
        log_warn("[PLUGIN] Kill failed for %s: %s", h->id, strerror(errno));
        h->state = PLUGIN_STATE_STOPPED;
        h->pid = 0;
    }
}

void plugin_mgr_stop_all(plugin_mgr *mgr)
{
    if (!mgr) return;

    log_info("[PLUGIN] Stopping all plugins...");
    for (int i = 0; i < mgr->count; i++)
    {
        stop_plugin(&mgr->plugins[i]);
    }
}

/* ============================================================
 * INDIVIDUAL PLUGIN CONTROL
 * ============================================================ */

int plugin_mgr_start(plugin_mgr *mgr, const char *plugin_id)
{
    plugin_handle *h = plugin_mgr_get(mgr, plugin_id);
    if (!h) return -1;
    return start_plugin(h, mgr->ipc_sock);
}

int plugin_mgr_stop(plugin_mgr *mgr, const char *plugin_id)
{
    plugin_handle *h = plugin_mgr_get(mgr, plugin_id);
    if (!h) return -1;
    stop_plugin(h);
    return 0;
}

int plugin_mgr_restart(plugin_mgr *mgr, const char *plugin_id)
{
    plugin_handle *h = plugin_mgr_get(mgr, plugin_id);
    if (!h) return -1;

    // If it's running, we stop it and mark as RESTARTING so check_health knows to start it again
    if (h->state == PLUGIN_STATE_RUNNING)
    {
        stop_plugin(h);
        h->state = PLUGIN_STATE_RESTARTING;  // Override STOPPING
        return 0;
    }

    // If already stopped, just start
    return start_plugin(h, mgr->ipc_sock);
}

/* ============================================================
 * QUERY
 * ============================================================ */

plugin_handle *plugin_mgr_get(plugin_mgr *mgr, const char *plugin_id)
{
    if (!mgr || !plugin_id) return NULL;

    for (int i = 0; i < mgr->count; i++)
    {
        if (strcmp(mgr->plugins[i].id, plugin_id) == 0)
        {
            return &mgr->plugins[i];
        }
    }
    return NULL;
}

plugin_state plugin_handle_state(const plugin_handle *h)
{
    return h ? h->state : PLUGIN_STATE_STOPPED;
}

plugin_type plugin_handle_type(const plugin_handle *h) { return h ? h->type : PLUGIN_TYPE_IN; }

bool plugin_handle_has_capability(const plugin_handle *h, plugin_capability cap)
{
    if (!h) return false;
    return (h->capabilities & cap) != 0;
}

const char *plugin_handle_id(const plugin_handle *h) { return h ? h->id : NULL; }

pid_t plugin_handle_pid(const plugin_handle *h) { return h ? h->pid : 0; }

int plugin_handle_interval(const plugin_handle *h) { return h ? h->interval : 0; }

time_t plugin_handle_last_run(const plugin_handle *h) { return h ? h->last_run : 0; }

void plugin_mgr_set_last_run(plugin_mgr *mgr, const char *plugin_id, time_t ts)
{
    plugin_handle *h = plugin_mgr_get(mgr, plugin_id);
    if (h)
    {
        h->last_run = ts;
    }
}

const char *plugin_handle_resource(const plugin_handle *h) { return h ? h->resource : "Unknown"; }

const char *plugin_mgr_get_config(plugin_mgr *mgr, const char *plugin_id, const char *key)
{
    plugin_handle *h = plugin_mgr_get(mgr, plugin_id);
    if (!h || !h->config_json) return NULL;

    cJSON *item = cJSON_GetObjectItem(h->config_json, key);
    if (item && item->valuestring)
    {
        return item->valuestring;
    }
    return NULL;
}

/* ============================================================
 * ITERATION
 * ============================================================ */

void plugin_mgr_foreach(plugin_mgr *mgr, plugin_iter_fn fn, void *userdata)
{
    if (!mgr || !fn) return;

    for (int i = 0; i < mgr->count; i++)
    {
        fn(&mgr->plugins[i], userdata);
    }
}

/* ============================================================
 * SUPERVISION
 * ============================================================ */

int plugin_mgr_check_health(plugin_mgr *mgr)
{
    if (!mgr) return -1;

    int restarted = 0;

    for (int i = 0; i < mgr->count; i++)
    {
        plugin_handle *h = &mgr->plugins[i];

        // Skip if already stopped
        if (h->state == PLUGIN_STATE_STOPPED || h->state == PLUGIN_STATE_FAILED) continue;

        // Use non-blocking wait
        int status;
        pid_t ret = waitpid(h->pid, &status, WNOHANG);

        if (ret > 0)
        {
            // Process exited
            if (h->state == PLUGIN_STATE_STOPPING)
            {
                log_info("[PLUGIN] %s stopped gracefully", h->id);
                h->state = PLUGIN_STATE_STOPPED;
                h->pid = 0;
            }
            else if (h->state == PLUGIN_STATE_RESTARTING)
            {
                log_info("[PLUGIN] %s stopped for restart", h->id);
                h->state = PLUGIN_STATE_STOPPED;
                h->pid = 0;
                // Immediately start it back up
                if (start_plugin(h, mgr->ipc_sock) == 0)
                {
                    restarted++;
                }
            }
            else if (h->state == PLUGIN_STATE_RUNNING || h->state == PLUGIN_STATE_STARTING)
            {
                // Crash / Unexpected exit
                if (WIFEXITED(status))
                    log_warn("[PLUGIN] %s exited with code %d", h->id, WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    log_warn("[PLUGIN] %s killed by signal %d", h->id, WTERMSIG(status));

                h->state = PLUGIN_STATE_STOPPED;
                h->pid = 0;

                // Auto-restart if it was running (and not manually stopped)
                log_info("[PLUGIN] Auto-restarting: %s", h->id);
                if (start_plugin(h, mgr->ipc_sock) == 0)
                {
                    restarted++;
                }
            }
        }
        else if (ret == 0)
        {
            // Still running check constraints?
            // If stopping and taking too long, maybe force kill?
            // (TODO: Implementation of timeout for stopping/restarting)
        }
    }

    return restarted;
}
/* ============================================================
 * METADATA ACCESSORS
 * ============================================================ */

/**
 * Get list of semantic types provided by this plugin.
 * Returns count of types written to caller-owned buffer.
 */
int plugin_get_provided_types(const plugin_handle *h, const char **out, int max_count,
                              int *out_count)
{
    int count = 0;

    if (!out || max_count <= 0)
    {
        if (out_count) *out_count = 0;
        return -1;
    }

    if (!h || !h->provides_json)
    {
        if (out_count) *out_count = 0;
        return 0;
    }

    cJSON *known = cJSON_GetObjectItem(h->provides_json, "known");
    if (!known || !cJSON_IsArray(known))
    {
        if (out_count) *out_count = 0;
        return 0;
    }

    int arr_size = cJSON_GetArraySize(known);
    for (int i = 0; i < arr_size && count < max_count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(known, i);
        if (cJSON_IsString(item))
        {
            // Manifest has uppercase enum names like "ATMOSPHERE_TEMPERATURE"
            // Convert to semantic ID like "atmosphere.temperature"
            semantic_type sem_type = semantic_from_string(item->valuestring);
            if (sem_type != SEM_UNKNOWN)
            {
                const semantic_meta *meta = semantic_get_meta(sem_type);
                if (meta)
                {
                    out[count++] = meta->id;
                    continue;
                }
            }
            // Fallback: use as-is if conversion fails
            out[count++] = item->valuestring;
        }
    }

    if (out_count) *out_count = count;
    return 0;
}

/**
 * Find all providers for a given semantic type.
 * Writes provider IDs to caller-owned buffer.
 */
int find_providers_for_type(const plugin_mgr *mgr, const char *semantic_type, const char **out,
                            int max_count, int *out_count)
{
    int count = 0;

    if (!out || max_count <= 0)
    {
        if (out_count) *out_count = 0;
        return -1;
    }

    if (!mgr || !semantic_type)
    {
        if (out_count) *out_count = 0;
        return 0;
    }

    for (int i = 0; i < mgr->count && count < max_count; i++)
    {
        const plugin_handle *h = &mgr->plugins[i];

        // Use local buffer for provided types
        const char *types[64];
        int type_count = 0;
        plugin_get_provided_types(h, types, 64, &type_count);

        for (int j = 0; j < type_count; j++)
        {
            if (strcmp(types[j], semantic_type) == 0)
            {
                out[count++] = h->id;
                break;
            }
        }
    }

    if (out_count) *out_count = count;
    // TODO: Sort by priority (for now, discovery order = priority)
    return 0;
}
/* ============================================================
 * ALIGNED SCHEDULING
 * ============================================================ */

/**
 * Calculate next aligned run time for a plugin.
 * Aligns to clock boundaries (e.g., :00, :15, :30, :45 for 15-min intervals)
 *
 * @param interval_sec Plugin interval in seconds
 * @return Next aligned timestamp
 */
static time_t calculate_next_aligned_time(int interval_sec)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    if (interval_sec < 60)
    {
        // Sub-minute intervals: align to next minute boundary
        tm.tm_sec = 0;
        tm.tm_min++;
        return mktime(&tm);
    }
    else if (interval_sec < 3600)
    {
        // Sub-hour intervals: align to minute boundaries
        int interval_min = interval_sec / 60;
        int current_min = tm.tm_min;
        int next_min = ((current_min / interval_min) + 1) * interval_min;

        if (next_min >= 60)
        {
            next_min = 0;
            tm.tm_hour++;
        }
        tm.tm_min = next_min;
        tm.tm_sec = 0;
        return mktime(&tm);
    }
    else
    {
        // Hourly+: align to top of hour
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_hour++;
        return mktime(&tm);
    }
}

/**
 * Validate dependencies for all plugins.
 * Prints reports to stdout about available providers and missing dependencies.
 */
int plugin_mgr_validate_dependencies(const plugin_mgr *mgr, const char *report_path,
                                     bool auto_bootstrap)
{
    if (!mgr) return -1;

    int missing_deps_count = 0;

    // 1. Report Available Providers
    log_info("[DEP] --- Available Data Providers ---");
    for (int i = 0; i < mgr->count; i++)
    {
        const char *provides[64];
        int provides_count = 0;
        plugin_get_provided_types(&mgr->plugins[i], provides, 64, &provides_count);

        if (provides_count > 0)
        {
            log_info("[DEP] %s provides:", mgr->plugins[i].id);
            for (int j = 0; j < provides_count; j++)
            {
                log_info("[DEP]   - %s", provides[j]);
            }
        }
    }

    // 2. Check Plugin Dependencies
    log_info("[DEP] --- Checking Dependencies ---");
    for (int i = 0; i < mgr->count; i++)
    {
        const plugin_handle *h = &mgr->plugins[i];
        if (!h->requires_json || !cJSON_IsArray(h->requires_json)) continue;

        cJSON *item = NULL;
        cJSON_ArrayForEach(item, h->requires_json)
        {
            if (cJSON_IsString(item))
            {
                char sem_id[MAX_ID_LEN];
                semantic_type type = semantic_from_string(item->valuestring);
                const semantic_meta *meta = (type != SEM_UNKNOWN) ? semantic_get_meta(type) : NULL;

                if (meta)
                    snprintf(sem_id, sizeof(sem_id), "%s", meta->id);
                else
                    snprintf(sem_id, sizeof(sem_id), "%s", item->valuestring);

                const char *providers[32];
                int provider_count = 0;
                find_providers_for_type(mgr, sem_id, providers, 32, &provider_count);

                if (provider_count == 0)
                {
                    log_warn("[DEP] \033[31mMISSING\033[0m: %s requires %s (No provider found)",
                             h->id, sem_id);
                    missing_deps_count++;
                }
                else
                {
                    log_info("[DEP] \033[32mSATISFIED\033[0m: %s requires %s -> Provided by %s",
                             h->id, sem_id, providers[0]);

                    if (auto_bootstrap)
                    {
                        log_info("[BOOTSTRAP] Attempting to fetch %s from %s...", sem_id,
                                 providers[0]);
                    }
                }
            }
        }
    }

    // 3. Global Coverage Report (User Requested)
    if (report_path)
    {
        FILE *f = fopen(report_path, "w");
        if (f)
        {
            int total_missing = 0;
            int total_found = 0;

            fprintf(f, "# Missing Providers Report\n");
            fprintf(f, "# Generated: %ld\n\n", time(NULL));

            for (int i = 1; i < SEM_TYPE_COUNT; i++)
            {
                const semantic_meta *meta = semantic_get_meta((semantic_type) i);
                if (!meta) continue;

                const char *provs[32];
                int prov_count = 0;
                find_providers_for_type(mgr, meta->id, provs, 32, &prov_count);

                if (prov_count == 0)
                {
                    fprintf(f, "MISSING: %s (%s)\n", meta->id, meta->description);
                    total_missing++;
                }
                else
                {
                    total_found++;
                }
            }
            fclose(f);

            if (total_missing > 0)
            {
                log_warn("[DEP] %d semantic types missing providers. See %s for details.",
                         total_missing, report_path);
            }
            else
            {
                log_info("[DEP] All %d known semantic types have providers!", total_found);
            }
        }
        else
        {
            log_warn("[DEP] Failed to write missing provider report to %s", report_path);
        }
    }

    return missing_deps_count;
}
