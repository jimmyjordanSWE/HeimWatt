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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libs/cJSON.h"
#include "log.h"

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
    char resource[MAX_ID_LEN];
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

    char *content = malloc((size_t) len + 1);
    if (!content)
    {
        fclose(f);
        return -1;
    }

    size_t read_len = fread(content, 1, (size_t) len, f);
    content[read_len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(content);
    free(content);

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

    cJSON_Delete(json);
    return 0;
}

/* ============================================================
 * MANAGER LIFECYCLE
 * ============================================================ */

int plugin_mgr_init(plugin_mgr **mgr, const char *plugins_dir, const char *ipc_sock)
{
    if (!mgr || !plugins_dir || !ipc_sock) return -1;

    plugin_mgr *m = calloc(1, sizeof(*m));
    if (!m) return -1;

    (void) snprintf(m->plugins_dir, sizeof(m->plugins_dir), "%s", plugins_dir);
    (void) snprintf(m->ipc_sock, sizeof(m->ipc_sock), "%s", ipc_sock);

    *mgr = m;
    return 0;
}

void plugin_mgr_destroy(plugin_mgr **mgr)
{
    if (!mgr || !*mgr) return;
    free(*mgr);
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
