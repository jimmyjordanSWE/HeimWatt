#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/cJSON.h"
#include "log.h"

struct config
{
    int csv_interval;
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

    char *data = malloc(len + 1);
    if (!data)
    {
        fclose(fp);
        return -ENOMEM;
    }
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    cJSON *json = cJSON_Parse(data);
    free(data);

    if (!json)
    {
        log_error("[CONFIG] Failed to parse JSON in %s", path);
        return -1;
    }

    cJSON *interval = cJSON_GetObjectItem(json, "csv_interval");
    if (interval)
    {
        cfg->csv_interval = interval->valueint;
    }

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
        free(*cfg);
        *cfg = NULL;
    }
}

config *config_create(void)
{
    config *cfg = calloc(1, sizeof(config));
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
        snprintf(cfg->location.name, sizeof(cfg->location.name), "default");
        snprintf(cfg->location.area, sizeof(cfg->location.area), "SE3");
        cfg->location.lat = 0.0;
        cfg->location.lon = 0.0;
    }
}
