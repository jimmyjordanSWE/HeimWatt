

#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "db.h"
#include "server_internal.h"
#include "utils.h"

// --- Internal Prototypes ---
static void handle_client_connection(server_ctx* ctx, int client_fd);
static void* client_thread_func(void* arg);
static void handle_api_events(server_ctx* ctx, int client_fd);
static void handle_api_state(server_ctx* ctx, int client_fd);
static void handle_api_plan(server_ctx* ctx, int client_fd);
static void handle_api_next_fetch(server_ctx* ctx, int client_fd);
static void handle_api_db(server_ctx* ctx, int client_fd, const char* buf);
static void handle_options(int client_fd);
static void handle_status(int client_fd);
static void handle_not_found(int client_fd);

// --- Lifecycle ---

server_ctx* server_create(void) {
    server_ctx* ctx = malloc(sizeof(*ctx));
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
        ctx->server_sock_fd = -1;  // Mark as uninitialized
    }
    return ctx;
}

void server_destroy(server_ctx** ctx_ptr) {
    if (!ctx_ptr || !*ctx_ptr) {
        return;
    }
    server_ctx* ctx = *ctx_ptr;
    server_fini(ctx);
    free(ctx);
    *ctx_ptr = NULL;
}

// --- Thread Args ---
typedef struct {
    server_ctx* ctx;
    int client_fd;
} client_thread_args;

// --- Write Helper ---
static int safe_write_all(int client_fd, const char* buf, size_t len) {
    while (len > 0) {
        ssize_t written = write(client_fd, buf, len);
        if (written <= 0) {
            return -1;
        }
        buf += written;
        len -= (size_t)written;
    }
    return 0;
}

int server_init(server_ctx* ctx) {
    bool mutex_init = false;
    bool cond_init = false;

    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        goto cleanup;
    }
    mutex_init = true;

    if (pthread_cond_init(&ctx->data_cond, NULL) != 0) {
        goto cleanup;
    }
    cond_init = true;

    // Open DB
    if (sqlite3_open("leop.db", &ctx->db) != SQLITE_OK) {
        log_msg(LOG_LEVEL_ERROR, "Cannot open DB: %s", sqlite3_errmsg(ctx->db));
        goto cleanup;
    }

    // Create socket
    ctx->server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_sock_fd < 0) {
        log_msg(LOG_LEVEL_ERROR, "socket() failed: %s", strerror(errno));
        goto cleanup;
    }

    // Reuse address
    int opt = 1;
    setsockopt(ctx->server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ctx->cfg.port);

    if (bind(ctx->server_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_msg(LOG_LEVEL_ERROR, "bind() failed: %s", strerror(errno));
        goto cleanup;
    }

    // Listen
    if (listen(ctx->server_sock_fd, BACKLOG_SIZE) < 0) {
        log_msg(LOG_LEVEL_ERROR, "listen() failed: %s", strerror(errno));
        goto cleanup;
    }
    return 0;

cleanup:
    if (ctx->server_sock_fd >= 0) {
        close(ctx->server_sock_fd);
        ctx->server_sock_fd = -1;
    }
    if (ctx->db) {
        sqlite3_close(ctx->db);
        ctx->db = NULL;
    }
    if (cond_init) {
        pthread_cond_destroy(&ctx->data_cond);
    }
    if (mutex_init) {
        pthread_mutex_destroy(&ctx->lock);
    }
    return -1;
}

void server_run(server_ctx* ctx) {
    log_msg(LOG_LEVEL_INFO, "Server loop running. Waiting for connections...");

    while (ctx->running && !is_shutdown_requested()) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_len = sizeof(client_addr);

        // Blocking accept
        int client_fd = accept(ctx->server_sock_fd, (struct sockaddr*)&client_addr, &client_len);

        if (is_shutdown_requested()) {
            break;
        }

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal
            }
            log_msg(LOG_LEVEL_WARN, "accept() failed: %s", strerror(errno));
            continue;
        }

        log_msg(LOG_LEVEL_INFO, "Accepted connection from %s:%d", inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));

        // Spawn thread for client
        client_thread_args* args = malloc(sizeof(client_thread_args));
        if (args) {
            args->ctx = ctx;
            args->client_fd = client_fd;
            pthread_t tid;
            if (pthread_create(&tid, NULL, client_thread_func, args) != 0) {
                log_msg(LOG_LEVEL_ERROR, "Failed to create client thread");
                close(client_fd);
                free(args);
            } else {
                pthread_detach(tid);
            }
        } else {
            log_msg(LOG_LEVEL_ERROR, "OOM: Failed to alloc client args");
            close(client_fd);
        }
    }
}

void server_fini(server_ctx* ctx) {
    if (ctx->server_sock_fd >= 0) {
        close(ctx->server_sock_fd);
        ctx->server_sock_fd = -1;
    }
    if (ctx->db) {
        sqlite3_close(ctx->db);
        ctx->db = NULL;
    }
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->data_cond);
}

static void* client_thread_func(void* arg) {
    client_thread_args* args = (client_thread_args*)arg;
    handle_client_connection(args->ctx, args->client_fd);
    free(args);
    return NULL;
}

static void handle_client_connection(server_ctx* ctx, int client_fd) {
    char buf[MAX_BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buf, sizeof(buf) - 1);

    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buf[bytes_read] = '\0';

    // Basic HTTP Handler
    if (strncmp(buf, "GET /api/events", strlen("GET /api/events")) == 0) {
        handle_api_events(ctx, client_fd);
    } else if (strncmp(buf, "GET /api/state", strlen("GET /api/state")) == 0) {
        handle_api_state(ctx, client_fd);
    } else if (strncmp(buf, "GET /api/plan", strlen("GET /api/plan")) == 0) {
        handle_api_plan(ctx, client_fd);
    } else if (strncmp(buf, "GET /api/next_fetch", strlen("GET /api/next_fetch")) == 0) {
        handle_api_next_fetch(ctx, client_fd);
    } else if (strncmp(buf, "GET /api/db/", strlen("GET /api/db/")) == 0) {
        handle_api_db(ctx, client_fd, buf);
    } else if (strncmp(buf, "OPTIONS", strlen("OPTIONS")) == 0) {
        handle_options(client_fd);
    } else if (strncasecmp(buf, "STATUS", strlen("STATUS")) == 0) {
        handle_status(client_fd);
    } else {
        handle_not_found(client_fd);
    }

    close(client_fd);
    log_msg(LOG_LEVEL_DEBUG, "Client request handled.");
}

static void handle_api_events(server_ctx* ctx, int client_fd) {
    // SSE Endpoint
    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";

    if (write(client_fd, headers, strlen(headers)) < 0) {
        return;  // caller will close
    }

    log_msg(LOG_LEVEL_DEBUG, "Client subscribed to SSE stream");

    pthread_mutex_lock(&ctx->lock);
    {
        weather_data weather = ctx->last_weather;
        spot_price price = ctx->last_price;
        int count_w = db_count_rows(ctx->db, "weather_log");
        int count_p = db_count_rows(ctx->db, "price_log");

        char json_resp[LARGE_BUF_SIZE];
        (void)snprintf(json_resp, sizeof(json_resp),
                       "{"
                       "\"timestamp\": %ld,"
                       "\"weather\": {\"temp_c\": %.2f, \"irradiance_wm2\": %.2f},"
                       "\"price\": {\"sek_kwh\": %.2f},"
                       "\"stats\": {\"weather_rows\": %d, \"price_rows\": %d},"
                       "\"status\": \"RUNNING\","
                       "\"next_weather_fetch\": %ld,"
                       "\"next_price_fetch\": %ld"
                       "}",
                       (long)time(NULL), weather.temp_c, weather.irradiance_wm2,
                       price.price_sek_kwh, count_w, count_p, (long)ctx->next_weather_fetch,
                       (long)ctx->next_price_fetch);

        char event_buf[EVENT_BUF_SIZE];
        int len = snprintf(event_buf, sizeof(event_buf), "data: %s\n\n", json_resp);

        pthread_mutex_unlock(&ctx->lock);

        if (write(client_fd, event_buf, len) < 0) {
            log_msg(LOG_LEVEL_DEBUG, "Client disconnected (initial SSE write error)");
            return;
        }

        log_msg(LOG_LEVEL_DEBUG, "Sent initial SSE event to client");
        pthread_mutex_lock(&ctx->lock);
    }

    while (ctx->running && !is_shutdown_requested()) {
        weather_data weather = ctx->last_weather;
        spot_price price = ctx->last_price;
        int count_w = db_count_rows(ctx->db, "weather_log");
        int count_p = db_count_rows(ctx->db, "price_log");

        char json_resp[LARGE_BUF_SIZE];
        (void)snprintf(json_resp, sizeof(json_resp),
                       "{"
                       "\"timestamp\": %ld,"
                       "\"weather\": {\"temp_c\": %.2f, \"irradiance_wm2\": %.2f},"
                       "\"price\": {\"sek_kwh\": %.2f},"
                       "\"stats\": {\"weather_rows\": %d, \"price_rows\": %d},"
                       "\"status\": \"RUNNING\","
                       "\"next_weather_fetch\": %ld,"
                       "\"next_price_fetch\": %ld"
                       "}",
                       (long)time(NULL), weather.temp_c, weather.irradiance_wm2,
                       price.price_sek_kwh, count_w, count_p, (long)ctx->next_weather_fetch,
                       (long)ctx->next_price_fetch);

        char event_buf[EVENT_BUF_SIZE];
        int len = snprintf(event_buf, sizeof(event_buf), "data: %s\n\n", json_resp);

        pthread_mutex_unlock(&ctx->lock);

        if (write(client_fd, event_buf, len) < 0) {
            log_msg(LOG_LEVEL_DEBUG, "Client disconnected (SSE write error)");
            return;
        }

        pthread_mutex_lock(&ctx->lock);

        struct timespec timeout_spec;
        clock_gettime(CLOCK_REALTIME, &timeout_spec);
        timeout_spec.tv_sec += TIMEOUT_SEC;

        int res = pthread_cond_timedwait(&ctx->data_cond, &ctx->lock, &timeout_spec);
        if (res == 0) {
            // Data updated
        } else if (res == ETIMEDOUT) {
            const char* keep_alive = ": keep-alive\n\n";
            if (write(client_fd, keep_alive, strlen(keep_alive)) < 0) {
                log_msg(LOG_LEVEL_DEBUG, "Client disconnected (keep-alive failed).");
                pthread_mutex_unlock(&ctx->lock);
                return;
            }
        } else {
            log_msg(LOG_LEVEL_ERROR, "pthread_cond_timedwait failed: %s", strerror(res));
            pthread_mutex_unlock(&ctx->lock);
            return;
        }
    }
    pthread_mutex_unlock(&ctx->lock);
    log_msg(LOG_LEVEL_DEBUG, "SSE client disconnected.");
}

static void handle_api_state(server_ctx* ctx, int client_fd) {
    weather_data weather;
    spot_price price;

    pthread_mutex_lock(&ctx->lock);
    weather = ctx->last_weather;
    price = ctx->last_price;
    int count_w = db_count_rows(ctx->db, "weather_log");
    int count_p = db_count_rows(ctx->db, "price_log");
    pthread_mutex_unlock(&ctx->lock);

    char json_resp[LARGE_BUF_SIZE];
    (void)snprintf(json_resp, sizeof(json_resp),
                   "{"
                   "\"timestamp\": %ld,"
                   "\"weather\": {\"temp_c\": %.2f, \"irradiance_wm2\": %.2f},"
                   "\"price\": {\"sek_kwh\": %.2f},"
                   "\"stats\": {\"weather_rows\": %d, \"price_rows\": %d},"
                   "\"status\": \"RUNNING\","
                   "\"next_weather_fetch\": %ld,"
                   "\"next_price_fetch\": %ld"
                   "}",
                   (long)time(NULL), weather.temp_c, weather.irradiance_wm2, price.price_sek_kwh,
                   count_w, count_p, (long)ctx->next_weather_fetch, (long)ctx->next_price_fetch);

    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n";

    (void)safe_write_all(client_fd, headers, strlen(headers));
    (void)safe_write_all(client_fd, json_resp, strlen(json_resp));
}

static void handle_api_plan(server_ctx* ctx, int client_fd) {
    pthread_mutex_lock(&ctx->lock);
    energy_plan plan = ctx->last_plan;
    const int history_hours = HISTORY_HOURS;
    float hist_prices[HISTORY_HOURS];
    float hist_solar[HISTORY_HOURS];
    time_t now = time(NULL);
    time_t history_start_ts = now - ((time_t)history_hours * SECS_PER_HOUR);
    history_start_ts = history_start_ts - (history_start_ts % SECS_PER_HOUR);

    // Using db.c header function
    db_get_history(ctx->db, history_start_ts, history_hours, hist_prices, hist_solar);
    pthread_mutex_unlock(&ctx->lock);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "generated_at", (double)plan.generated_at);
    cJSON_AddNumberToObject(root, "start_hour", plan.start_hour);
    cJSON_AddNumberToObject(root, "horizon", plan.horizon);
    cJSON_AddNumberToObject(root, "total_cost_sek", plan.total_cost_sek);
    cJSON_AddNumberToObject(root, "history_hours", history_hours);
    cJSON_AddNumberToObject(root, "history_start", (double)history_start_ts);

    cJSON* prices_arr =
        cJSON_CreateFloatArray(plan.prices, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);
    cJSON* solar_arr =
        cJSON_CreateFloatArray(plan.solar, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);
    cJSON* demand_arr =
        cJSON_CreateFloatArray(plan.demand, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);
    cJSON* battery_arr =
        cJSON_CreateFloatArray(plan.battery, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);
    cJSON* buy_arr =
        cJSON_CreateFloatArray(plan.buy, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);
    cJSON* sell_arr =
        cJSON_CreateFloatArray(plan.sell, plan.horizon > 0 ? plan.horizon : DEFAULT_HORIZON);

    cJSON_AddItemToObject(root, "prices", prices_arr);
    cJSON_AddItemToObject(root, "solar", solar_arr);
    cJSON_AddItemToObject(root, "demand", demand_arr);
    cJSON_AddItemToObject(root, "battery", battery_arr);
    cJSON_AddItemToObject(root, "buy", buy_arr);
    cJSON_AddItemToObject(root, "sell", sell_arr);

    cJSON* hist_prices_arr = cJSON_CreateFloatArray(hist_prices, history_hours);
    cJSON* hist_solar_arr = cJSON_CreateFloatArray(hist_solar, history_hours);
    cJSON_AddItemToObject(root, "history_prices", hist_prices_arr);
    cJSON_AddItemToObject(root, "history_solar", hist_solar_arr);

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n";

    (void)safe_write_all(client_fd, headers, strlen(headers));
    if (json_str) {
        (void)safe_write_all(client_fd, json_str, strlen(json_str));
        free(json_str);
    }
}

static void handle_api_next_fetch(server_ctx* ctx, int client_fd) {
    pthread_mutex_lock(&ctx->lock);
    time_t next_weather = ctx->next_weather_fetch;
    time_t next_price = ctx->next_price_fetch;
    pthread_mutex_unlock(&ctx->lock);

    char weather_buf[SMALL_BUF_SIZE];
    char price_buf[SMALL_BUF_SIZE];

    get_time_str(next_weather, weather_buf, sizeof(weather_buf));
    get_time_str(next_price, price_buf, sizeof(price_buf));

    char json_resp[JSON_RESP_SIZE];
    (void)snprintf(json_resp, sizeof(json_resp),
                   "{"
                   "\"next_weather_fetch\": %ld,"
                   "\"next_weather_human\": \"%s\","
                   "\"next_price_fetch\": %ld,"
                   "\"next_price_human\": \"%s\""
                   "}",
                   (long)next_weather, weather_buf, (long)next_price, price_buf);

    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n";

    (void)safe_write_all(client_fd, headers, strlen(headers));
    (void)safe_write_all(client_fd, json_resp, strlen(json_resp));
}

static void handle_api_db(server_ctx* ctx, int client_fd, const char* buf) {
    char table[SMALL_BUF_SIZE] = {0};
    int limit = DEFAULT_LIMIT;
    int offset = 0;

    const char* path_start = buf + strlen("GET /api/db/");
    const char* path_end = strchr(path_start, '?');
    if (!path_end) {
        path_end = strchr(path_start, ' ');
    }
    if (!path_end) {
        path_end = path_start + strlen(path_start);
    }

    size_t table_len = (size_t)(path_end - path_start);
    if (table_len > 0 && table_len < sizeof(table)) {
        strncpy(table, path_start, table_len);
        table[table_len] = '\0';
    }

    const char* query = strchr(path_start, '?');
    if (query) {
        const char* lim_str = strstr(query, "limit=");
        const char* off_str = strstr(query, "offset=");
        if (lim_str) {
            limit = (int)strtol(lim_str + strlen("limit="), NULL, BASE_10);
        }
        if (off_str) {
            offset = (int)strtol(off_str + strlen("offset="), NULL, BASE_10);
        }
    }

    if (limit < 1) {
        limit = 1;
    }
    if (limit > MAX_LIMIT) {
        limit = MAX_LIMIT;
    }
    if (offset < 0) {
        offset = 0;
    }

    pthread_mutex_lock(&ctx->lock);
    char* json_resp = db_query_table(ctx->db, table, limit, offset);
    pthread_mutex_unlock(&ctx->lock);

    if (json_resp) {
        const char* headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n";
        (void)safe_write_all(client_fd, headers, strlen(headers));
        (void)safe_write_all(client_fd, json_resp, strlen(json_resp));
        free(json_resp);
    } else {
        const char* err =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"error\": \"Invalid table name. Use weather_log or price_log.\"}";
        (void)safe_write_all(client_fd, err, strlen(err));
    }
}

static void handle_options(int client_fd) {
    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Connection: close\r\n"
        "\r\n";
    (void)safe_write_all(client_fd, headers, strlen(headers));
}

static void handle_status(int client_fd) {
    const char* resp = "STATUS: OK. Pipeline Running.\n> ";
    (void)safe_write_all(client_fd, resp, strlen(resp));
}

static void handle_not_found(int client_fd) {
    const char* msg = "HTTP/1.1 404 Not Found\r\n\r\nNot Found";
    (void)safe_write_all(client_fd, msg, strlen(msg));
}

// --- Thread-Safe Updates (API) ---

void server_update_weather(server_ctx* ctx, const weather_data* weather) {
    if (!ctx || !weather) {
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->last_weather = *weather;
    pthread_cond_broadcast(&ctx->data_cond);
    pthread_mutex_unlock(&ctx->lock);
}

void server_update_price(server_ctx* ctx, const spot_price* price, const float* forecast_24h) {
    if (!ctx || !price) {
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->last_price = *price;
    if (forecast_24h) {
        memcpy(ctx->last_forecast_prices, forecast_24h, sizeof(ctx->last_forecast_prices));
    }
    pthread_cond_broadcast(&ctx->data_cond);
    pthread_mutex_unlock(&ctx->lock);
}

void server_update_plan(server_ctx* ctx, const energy_plan* production_plan) {
    if (!ctx || !production_plan) {
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->last_plan = *production_plan;
    pthread_cond_broadcast(&ctx->data_cond);
    pthread_mutex_unlock(&ctx->lock);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void server_set_next_fetch(server_ctx* ctx, time_t next_weather, time_t next_price) {
    if (!ctx) {
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->next_weather_fetch = next_weather;
    ctx->next_price_fetch = next_price;
    pthread_cond_broadcast(&ctx->data_cond);
    pthread_mutex_unlock(&ctx->lock);
}

// --- Accessors (API) ---

bool server_is_running(server_ctx* ctx) {
    if (!ctx) {
        return false;
    }
    // running is volatile, but for reading we can just return it.
    // If strict thread safety is needed for this bool, we could lock,
    // but typically reading a volatile bool is fine for loop checks.
    // However, for consistency with opaque pattern:
    return ctx->running && !is_shutdown_requested();
}

sqlite3* server_get_db(server_ctx* ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->db;
}

const config* server_get_config(server_ctx* ctx) {
    if (!ctx) {
        return NULL;
    }
    return &ctx->cfg;
}

config* server_get_config_mutable(server_ctx* ctx) {
    if (!ctx) {
        return NULL;
    }
    return &ctx->cfg;
}

pthread_t* server_get_pipeline_thread(server_ctx* ctx) {
    if (!ctx) {
        return NULL;
    }
    return &ctx->pipeline_thread;
}

void server_set_running(server_ctx* ctx, bool running) {
    if (!ctx) {
        return;
    }
    ctx->running = running;
}
