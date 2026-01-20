#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/cJSON.h"
#include "log.h"
#include "memory.h"

struct config
{
    /* Legacy/Direct support */
    int csv_interval;

    /* Storage Configuration */
    struct
    {
        storage_backend_config backends[CONFIG_MAX_BACKENDS];
        size_t backend_count;
        int disk_write_interval_sec;
    } storage;

    /* Location Configuration */
    struct
    {
        char name[256];
        double lat;
        double lon;
        char area[16];
    } location;
};

int config_get_csv_interval(const config *cfg) { return cfg ? cfg->csv_interval : 60; }

const char *config_get_loc_name(const config *cfg) { return cfg ? cfg->location.name : "default"; }

double config_get_lat(const config *cfg) { return cfg ? cfg->location.lat : 0.0; }

double config_get_lon(const config *cfg) { return cfg ? cfg->location.lon : 0.0; }

const char *config_get_area(const config *cfg) { return cfg ? cfg->location.area : "SE3"; }

size_t config_get_backend_count(const config *cfg) { return cfg ? cfg->storage.backend_count : 0; }

const storage_backend_config *config_get_backend(const config *cfg, size_t idx)
{
    if (!cfg || idx >= cfg->storage.backend_count) return NULL;
    return &cfg->storage.backends[idx];
}

int config_get_disk_write_interval(const config *cfg)
{
    return cfg ? cfg->storage.disk_write_interval_sec : 60;
}

int config_add_backend(config *cfg, const char *type, const char *path, bool primary)
{
    if (!cfg || !type || !path) return -EINVAL;
    if (cfg->storage.backend_count >= CONFIG_MAX_BACKENDS) return -ENOSPC;

    storage_backend_config *be = &cfg->storage.backends[cfg->storage.backend_count];
    snprintf(be->type, sizeof(be->type), "%s", type);
    snprintf(be->path, sizeof(be->path), "%s", path);
    be->primary = primary;
    cfg->storage.backend_count++;
    return 0;
}

int config_load(config *cfg, const char *path)
{
    if (!cfg || !path) return -EINVAL;

    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        log_warn("[CONFIG] Could not open %s, using defaults", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = mem_alloc(len + 1);
    if (!data)
    {
        fclose(fp);
        return -ENOMEM;
    }
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    cJSON *json = cJSON_Parse(data);
    mem_free(data);

    if (!json)
    {
        log_error("[CONFIG] Failed to parse JSON in %s", path);
        return -1;
    }

    /* Legacy: csv_interval */
    cJSON *interval = cJSON_GetObjectItem(json, "csv_interval");
    if (interval)
    {
        cfg->csv_interval = interval->valueint;
    }

    /* Storage Configuration */
    cJSON *storage = cJSON_GetObjectItem(json, "storage");
    if (storage)
    {
        cJSON *backends = cJSON_GetObjectItem(storage, "backends");
        if (backends && cJSON_IsArray(backends))
        {
            int count = cJSON_GetArraySize(backends);
            if (count > CONFIG_MAX_BACKENDS) count = CONFIG_MAX_BACKENDS;

            cfg->storage.backend_count = 0;
            for (int i = 0; i < count; i++)
            {
                cJSON *item = cJSON_GetArrayItem(backends, i);
                cJSON *type = cJSON_GetObjectItem(item, "type");
                cJSON *path_val = cJSON_GetObjectItem(item, "path");
                cJSON *primary = cJSON_GetObjectItem(item, "primary");

                if (type && path_val)
                {
                    storage_backend_config *be = &cfg->storage.backends[cfg->storage.backend_count];
                    snprintf(be->type, sizeof(be->type), "%s", type->valuestring);
                    snprintf(be->path, sizeof(be->path), "%s", path_val->valuestring);
                    be->primary = primary ? cJSON_IsTrue(primary) : false;
                    cfg->storage.backend_count++;
                }
            }
        }

        cJSON *write_interval = cJSON_GetObjectItem(storage, "csv_disk_write_interval_sec");
        if (write_interval)
        {
            cfg->storage.disk_write_interval_sec = write_interval->valueint;
        }
        else
        {
            /* Fallback to legacy or default */
            cfg->storage.disk_write_interval_sec = cfg->csv_interval;
        }
    }

    /* Location Configuration */
    cJSON *loc = cJSON_GetObjectItem(json, "location");
    if (loc)
    {
        cJSON *name = cJSON_GetObjectItem(loc, "name");
        cJSON *lat = cJSON_GetObjectItem(loc, "lat");
        cJSON *lon = cJSON_GetObjectItem(loc, "lon");
        cJSON *area = cJSON_GetObjectItem(loc, "area");

        if (name && name->valuestring)
            snprintf(cfg->location.name, sizeof(cfg->location.name), "%s", name->valuestring);
        if (lat) cfg->location.lat = lat->valuedouble;
        if (lon) cfg->location.lon = lon->valuedouble;
        if (area && area->valuestring)
            snprintf(cfg->location.area, sizeof(cfg->location.area), "%s", area->valuestring);
    }

    cJSON_Delete(json);
    return 0;
}

void config_destroy(config **cfg)
{
    if (cfg && *cfg)
    {
        mem_free(*cfg);
        *cfg = NULL;
    }
}

config *config_create(void)
{
    config *cfg = mem_alloc(sizeof(*cfg));
    if (cfg)
    {
        config_init_defaults(cfg);
    }
    return cfg;
}

void config_init_defaults(config *cfg)
{
    if (cfg)
    {
        cfg->csv_interval = 60;
        cfg->storage.disk_write_interval_sec = 60;
        cfg->storage.backend_count = 0;

        snprintf(cfg->location.name, sizeof(cfg->location.name), "default");
        snprintf(cfg->location.area, sizeof(cfg->location.area), "SE3");
        cfg->location.lat = 0.0;
        cfg->location.lon = 0.0;
    }
}
