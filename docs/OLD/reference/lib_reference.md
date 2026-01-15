# C Library Reference (Cheat Sheet)

## 1. cJSON (JSON Parsing)
**Header**: `#include "cJSON.h"`

### Parsing Pattern
```c
char* json_str = "..."; // from network

cJSON* root = cJSON_Parse(json_str);
if (!root) {
    const char* error_ptr = cJSON_GetErrorPtr();
    // Handle error
    return;
}

// Access Object Item
cJSON* time_series = cJSON_GetObjectItemCaseSensitive(root, "timeSeries");
if (cJSON_IsArray(time_series)) {
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, time_series) {
        cJSON* valid_time = cJSON_GetObjectItem(item, "validTime");
        if (cJSON_IsString(valid_time) && (valid_time->valuestring != NULL)) {
             // Use valid_time->valuestring
        }
    }
}

// Cleanup (Crucial!)
cJSON_Delete(root);
```

---

## 2. libcurl (HTTP Client)
**Header**: `#include <curl/curl.h>`

### Memory Struct Pattern (Dynamic Buffer)
```c
struct memory_struct {
    char* memory;
    size_t size;
};

// Callback to write data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct memory_struct* mem = (struct memory_struct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0; // Out of memory

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // Null-terminate

    return realsize;
}

// Perform Request
void fetch_url(const char* url) {
    CURL* curl = curl_easy_init();
    struct memory_struct chunk = {0};
    chunk.memory = malloc(1); 
    chunk.size = 0;

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
        
        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
             fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
             // Use chunk.memory
        }

        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
}
```

---

## 3. SQLite3 (Database)
**Header**: `#include <sqlite3.h>`

### Query Loop Pattern
```c
sqlite3* db;
sqlite3_stmt* stmt;
int rc;

rc = sqlite3_open("leop.db", &db);
if (rc) return; // Error

const char* sql = "SELECT price, timestamp FROM prices WHERE timestamp > ?;";
rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

if (rc == SQLITE_OK) {
    // Bind parameters (1-based index)
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));

    // Loop results
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double price = sqlite3_column_double(stmt, 0);
        time_t ts = (time_t)sqlite3_column_int64(stmt, 1);
        // Process row...
    }
}

// Finalize (Crucial!)
sqlite3_finalize(stmt);
sqlite3_close(db);
```

### Transaction Pattern (Speed)
Always wrap batch inserts in a transaction!
```c
char* err_msg = NULL;
sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &err_msg);

// ... perform 1000 inserts ...

sqlite3_exec(db, "COMMIT;", NULL, NULL, &err_msg);
```
