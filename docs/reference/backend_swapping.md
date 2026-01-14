# Backend Swapping Tutorial

How to swap implementations (e.g., curl → hand-rolled HTTP) in C.

---

## TL;DR — Three Options

| Method | When to Use | Selection |
|--------|-------------|-----------|
| **Compile-time** (`#ifdef`) | Different builds for different targets | Makefile flags |
| **Link-time** (separate `.c` files) | Cleaner, one `.h` + multiple `.c` | Link different `.o` |
| **Runtime** (function pointers) | Plugin chooses backend | Config or API call |

---

## Option 1: Compile-Time (`#ifdef`)

**Best for**: Embedded vs desktop builds. Single source file.

```c
// http_client.c

#include "http_client.h"

#ifdef USE_CURL
#include <curl/curl.h>

int http_get(http_client *c, const char *url, char **out, size_t *len) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // ... curl implementation
    curl_easy_cleanup(curl);
    return status;
}

#else  // Hand-rolled with tcp_client + mbedTLS

#include "tcp_client.h"
#include <mbedtls/ssl.h>

int http_get(http_client *c, const char *url, char **out, size_t *len) {
    tcp_client_socket *sock;
    tcp_client_connect(&sock, host, 443);
    // ... TLS handshake, send HTTP request, parse response
    tcp_client_close(&sock);
    return status;
}

#endif
```

**Makefile**:
```makefile
# Desktop build (uses curl)
CFLAGS_DESKTOP = -DUSE_CURL
LDFLAGS_DESKTOP = -lcurl

# ESP32 build (hand-rolled)
CFLAGS_ESP32 = -DUSE_HANDROLLED
LDFLAGS_ESP32 = -lmbedtls

desktop: CFLAGS += $(CFLAGS_DESKTOP)
desktop: LDFLAGS += $(LDFLAGS_DESKTOP)
desktop: all

esp32: CFLAGS += $(CFLAGS_ESP32)
esp32: LDFLAGS += $(LDFLAGS_ESP32)
esp32: all
```

**Usage**:
```bash
make desktop   # Uses curl
make esp32     # Uses hand-rolled
```

---

## Option 2: Link-Time (Multiple `.c` Files)

**Best for**: Clean separation, no `#ifdef` spaghetti.

```
src/net/
├── http_client.h           # API (same for all)
├── http_client_curl.c      # Implementation using curl
└── http_client_raw.c       # Implementation using tcp_client
```

**http_client.h** — unchanged, just the API:
```c
int http_get(http_client *c, const char *url, char **out, size_t *len);
```

**http_client_curl.c**:
```c
#include "http_client.h"
#include <curl/curl.h>

int http_get(http_client *c, const char *url, char **out, size_t *len) {
    // curl implementation
}
```

**http_client_raw.c**:
```c
#include "http_client.h"
#include "tcp_client.h"

int http_get(http_client *c, const char *url, char **out, size_t *len) {
    // hand-rolled implementation
}
```

**Makefile**:
```makefile
# Choose ONE implementation to link
ifeq ($(HTTP_BACKEND),curl)
    HTTP_SRC = src/net/http_client_curl.c
else
    HTTP_SRC = src/net/http_client_raw.c
endif

$(TARGET): $(HTTP_SRC:.c=.o) ...
```

**Usage**:
```bash
make HTTP_BACKEND=curl      # Link curl version
make HTTP_BACKEND=raw       # Link hand-rolled
```

---

## Option 3: Runtime (Function Pointers)

**Best for**: Plugin chooses backend, or hot-swappable.

```c
// http_client.h

typedef struct http_backend {
    int (*get)(void *ctx, const char *url, char **out, size_t *len);
    int (*post)(void *ctx, const char *url, const char *body, char **out, size_t *len);
    void *ctx;  // Backend-specific context
} http_backend;

typedef struct http_client {
    http_backend *backend;
} http_client;

// Backend registration
void http_client_set_backend(http_client *c, http_backend *backend);

// Generic functions (dispatch to backend)
int http_get(http_client *c, const char *url, char **out, size_t *len);
```

**http_client.c**:
```c
int http_get(http_client *c, const char *url, char **out, size_t *len) {
    return c->backend->get(c->backend->ctx, url, out, len);
}
```

**Usage in plugin**:
```c
#include "http_client.h"

// Plugin can choose its backend
http_client *client;
http_client_init(&client);

#ifdef __XTENSA__  // ESP32
    http_client_set_backend(client, &raw_backend);
#else
    http_client_set_backend(client, &curl_backend);
#endif

http_get(client, "https://api.smhi.se/...", &body, &len);
```

---

## My Recommendation

For HeimWatt, use **Option 2 (Link-Time)** because:

1. ✅ Clean code — no `#ifdef` pollution
2. ✅ Compiler catches all errors for each backend
3. ✅ Simple — just swap which `.c` file you link
4. ✅ Works well with your plugin architecture

**Structure**:
```
src/net/
├── http_client.h              # API
├── impl/
│   ├── http_client_curl.c     # Desktop/server
│   └── http_client_mbedtls.c  # ESP32/embedded
```

Plugins don't choose — the **build target** chooses. You'd have:
- `make` → Linux server, uses curl
- `make TARGET=esp32` → ESP32, uses mbedTLS

---

## Quick Reference

| Question | Answer |
|----------|--------|
| Can plugins choose backend? | Yes (Option 3), but adds complexity |
| Simplest approach? | Option 2 (link-time) |
| Most flexible? | Option 3 (runtime pointers) |
| Best for ESP32 port? | Option 2 |
| Can I mix? | Yes — e.g., compile-time for target, runtime for plugin choice |
