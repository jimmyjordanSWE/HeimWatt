# HEIMWATT Coding Standards (Linux C99)

> **Version**: 1.0 (2026-01-13)
> **Target Platform**: Linux (POSIX compliant). Windows development requires WSL or Docker.


## Table of Contents
1.  [Core Principles](#core-principles)
2.  [Naming & Style](#naming--style)
3.  [Language Usage & Idioms](#language-usage--idioms)
4.  [Function Design & Control Flow](#function-design--control-flow)
5.  [Resource Management & Safety](#resource-management--safety)
6.  [Architecture Patterns](#architecture-patterns)
7.  [Enforcement & Tooling](#enforcement--tooling)

## Core Principles

1.  **Explicit over Implicit**: Code should be obvious; avoid "magic" behavior or hidden state.
2.  **Fail Fast**: Detect and report errors immediately at the source.
3.  **Single Responsibility**: Each function and module should have exactly one purpose.
4.  **Zero Trust**: Validate all inputs at public API boundaries.
5.  **Defensive Programming**: Assume the caller might misuse your API; handle it safely.
6.  **Parameter Limit**: Max 5 parameters per function; use a configuration struct for more.

## Naming & Style

### Types

```c
// Structs: lowercase_with_underscores (No _t suffix)
typedef struct {
    int value;
} weather_data;

// Enums: lowercase_with_underscores
typedef enum {
    ACTION_BUY,        // UPPERCASE for enum values
    ACTION_SELL
} action;

// Opaque types: same pattern
typedef struct config config;  // forward declaration
```

### Functions

```c
// module_verb_noun pattern
int config_load(const char* path, config** out);
void config_destroy(config** cfg);
int cache_put(cache* c, const char* key, const void* data, size_t len);
void* cache_get(cache* c, const char* key, size_t* out_len);

// Predicates: is_, has_, can_
bool cache_is_expired(const cache_entry* entry);
bool config_has_key(const config* cfg, const char* key);

// Getters: module_get_property
int config_get_port(const config* cfg);

// Non-Blocking: use _try suffix
int mutex_trylock(mutex* m);  // Returns -EBUSY if locked
void* pool_try_get(pool* p);   // Returns NULL if empty
```

### Lifecycle Naming Pairs

Naming conveys memory and resource ownership. We use three primary pairs to distinguish between allocation and initialization:

| Start | End | Use For |
|-------|-----|---------|
| `create` | `destroy` | **Heap Allocation**: Caller owns the memory. Returns `T*` or takes `T**`. |
| `init` | `fini` | **In-place Initialization**: Caller provided memory (stack/struct). Takes `T*`. |
| `open` | `close` | **External Resources**: Files, Sockets, Database connections. |
| `start` | `stop` | **Execution Units**: Threads, Processes, Services. |

**Rules**:
- **Heap vs. Stack**: Use `create/destroy` if the function calls `malloc`. Use `init/fini` if the function only populates a pointer provided by the caller.
- **Symmetry**: Never mix pairs (e.g., no `create` with `free`).
- **Opaque Pointers**: Destructors for heap objects (`destroy`) should take a double pointer `T**` and set it to `NULL` to prevent use-after-free.

```c
// HEAP: symmetric Pairs
cache* cache_create(size_t capacity);
void   cache_destroy(cache** c);

// STACK: in-place
int    server_init(server* s, const config* cfg);
void   server_fini(server* s);

// SYSTEM: resources
int    db_open(db* d, const char* path);
void   db_close(db* d);
```

### Variables

```c
// Local: lowercase_with_underscores
int connection_count = 0;
char* response_body = NULL;

// Loop counters: i, j, k or descriptive
for (size_t slot_idx = 0; slot_idx < slot_count; slot_idx++) { }

// Pointers: suffix with _ptr only if ambiguous
weather_data* forecast = NULL;  // clear from type
void* raw_ptr = buffer;         // clarify when void*

// Boolean: use positive names
bool is_connected = true;       // GOOD
bool is_not_connected = false;  // BAD - double negative
```

### Project Structure & File Organization

#### Directory Layout
```
/
├── src/                # Implementation files (*.c)
├── include/            # Public header files (*.h)
├── tests/              # Test files (test_*.c)
├── build/              # Build artifacts (do not commit)
└── docs/               # Documentation
```

#### File Naming
```
module_name.c           # Implementation
module_name.h           # Public API
module_name_internal.h  # Internal helpers (private to module)
test_module_name.c      # Unit tests
```

### Documentation & Comments

#### Philosophy
**Code should be self-commenting.**
We prioritize clear naming (variables, functions, types) over comments.

#### Rules
1.  **Minimum Comments**: 0 comments is widely accepted if the code is clear.
2.  **Explain the "Why", not the "What"**: Only comment on unintuitive logic, complex algorithms, or "gotchas".
3.  **Use Single-line Comments**: For brief explanations inside functions.

#### Public API (Intellisense)
For public interfaces (`.h` files), use **Doxygen-style** comments to ensure documentation shows up in IDE Intellisense properties.

```c
/*
 * @brief  Loads the configuration from the specified path.
 * @param  path Path to the JSON config file.
 * @param  out  Output pointer for the config object.
 * @return 0 on success, negative error code on failure.
 */
int config_load(const char* path, config** out);
```

### Scope & Linkage

#### Rule 1: Static by Default
All functions and global variables **MUST** be marked `static` unless they are explicitly part of the public API (declared in the header file) or are a required plugin export (e.g., `const storage_ops my_plugin = ...`).
**Why?** Prevents namespace pollution and allows the compiler to optimize better (inlining).

```c
// INTERNAL HELPER: Must be static
static int validate_input(const char* input) {
    if (!input) return -EINVAL;
    return 0;
}

// PUBLIC API: No static
int server_start(void) {
    if (validate_input("test") < 0) return -EINVAL;
    // ...
}
```

#### Rule 2: Module-Internal Headers
If a function needs to be shared between multiple `.c` files within the *same* module but not exposed to the world, declare it in a `module_internal.h`. NEVER put it in the public `module.h`.



## Language Usage & Idioms

### Constants & Enums

#### 1. Integer Constants: Prefer `enum`
For integer constants (array sizes, state machines, flags), use `enum`.
**Why?**
1.  **True Constants**: Enums are distinct compile-time constants. `static const int` is just a *read-only variable* in C, meaning it cannot be used for global array sizes or `switch` cases.
2.  **Debuggers**: Tools show the enum name (e.g., `STATE_INIT`) instead of a raw number `0`.

```c
// GOOD
enum {
    MAX_CONNECTIONS = 100,
    DEFAULT_TIMEOUT_MS = 5000
};

// BAD
#define MAX_CONNECTIONS 100
```

#### 2. Typed Constants: Prefer `static const`
For floats, strings, or complex types, use `static const`.
**Why?** Type safety. The compiler validates usage. `#define` is a blind text substitution.

```c
// GOOD
static const float PI = 3.14159f;
static const char* SERVER_NAME = "HeimWatt/1.0";

// BAD
#define PI 3.14159f
```

#### 3. Constants vs. Configuration
Do NOT use constants for values that might change between deployments (e.g., ports, hostnames, credentials).

*   **Internal Invariants (`static const`/`enum`)**: Values that *never* change (e.g., `MAX_INT`, physics constants).
*   **Runtime Configuration (`struct config`)**: Values that *might* change. Load these at startup.

```c
// WRONG: Hardcoded configuration
#define SERVER_PORT 8080  // What if port 8080 is taken?

// RIGHT: Configuration Struct
typedef struct {
    int port;
    int timeout_ms;
} server_config;

// Load from file/env at runtime
server_config cfg = {0};
config_load("config.json", &cfg);
```

### Macro Guidelines

#### Philosophy: Control Flow vs. Logic
*   **Good**: Macros that reduce boilerplate that functions *cannot* handle (e.g., file/line capture).
*   **Bad**: Macros that hide logic, math, or constants that *could* be functions or enums.

#### ✅ Allowed Macros (Non-exhaustive)
*These are common examples, not an exclusive list.*

| Category | Example | Reason |
|----------|---------|--------|
| Header guards | `#ifndef FOO_H` | Standard practice |
| Build toggles | `#ifdef DEBUG` | Build-time config |
| Location capture | `LOG_ERR(__FILE__, __LINE__)` | Can't get this from functions |

| **Safety/Boilerplate** | `SAFE_FREE(ptr)` | Nulls pointer after free (avoids double-free) |
| Attribute wrappers | `#define UNUSED __attribute__((unused))` | Compiler portability |
| Array length | `#define ARRAY_LEN(arr) ...` | Classic idiom |

#### ❌ Discouraged Macros

| Anti-pattern | Problem | Alternative |
|--------------|---------|-------------|
| Logic Macros | `MAX(x, y)` | `static inline` function (Type-safe) |
| Constants via `#define` | No type safety | `static const` or `enum` (See Naming) |
| Multi-statement | Breaks in `if` blocks | `do { } while(0)` wrapper |
| Side Effects | `SQUARE(x++)` | **Dangerous** double evaluation |

#### C99 Preferences

```c
// PREFER: typed constants
static const size_t MAX_BUFFER = 4096;
enum { CACHE_TTL_SEC = 3600 };

// PREFER: inline functions over function-like macros
static inline int min_int(int a, int b) { return a < b ? a : b; }
```

### Const Correctness

#### Rule 1: Read-Only Inputs MUST be `const`
If a function reads from a pointer but does not modify the data, that pointer **must** be `const`.

```c
// BAD: Implies modification
int calculate_crc(uint8_t* data, size_t len);

// GOOD: Guarantees immutability
int calculate_crc(const uint8_t* data, size_t len);
```

#### Rule 2: Return `const` for Internal State
**Critical for Opaque Pointers.** Even if the struct is opaque, returning a non-const pointer to a field (like a string/buffer) allows the caller to corrupt internal state. Always return `const` to enforce read-only access.

```c
// BAD: Breaks encapsulation - caller can overwrite internal memory
char* user_get_name(user* u);

// GOOD: Preserves encapsulation - caller gets read-only view
const char* user_get_name(const user* u);
```

#### Rule 3: `const` Local Variables
Declare local variables as `const` if they are not intended to be modified after initialization. This helps reason about data flow.

```c
const int max_limit = config_get_limit(cfg);
const size_t len = strlen(input);
```

#### Rule 4: Immutable Struct Members
Use `const` for struct members that should never change after creation (e.g., IDs, creation timestamps).

```c
typedef struct {
    const int id;          // Set at creation, never changes
    const time_t created_at;
    int status;            // Mutable
} session;
```

### C Idioms

#### Idiom 1: Initialize to Zero/NULL

```c
// ALWAYS initialize variables
int result = 0;
char* buffer = NULL;
weather_data data = {0};  // Zero-initialize struct
```

#### Idiom 2: sizeof on Variable, Not Type

```c
// RULE: ALways apply sizeof to the VARIABLE, never the TYPE.
// Why: If you change the variable's type later (e.g., float* to double*),
//      the malloc size automatically updates. Hardcoding type leads to insufficient allocation bugs.

// GOOD: Robust against refactoring
weather_data* data = malloc(sizeof(*data));
if (data) memset(data, 0, sizeof(*data));

// BAD: Brittle - requires manual update if type changes
weather_data* data = malloc(sizeof(weather_data));
```

#### Idiom 3: Compound Literal for Struct Init

```c
// Initialize struct with named fields
weather_data data = {
    .timestamp = time(NULL),
    .temperature_c = 20.0f,
    .cloud_cover_pct = 0.0f
};

// Pass struct to function inline
process_data(&(weather_data){
    .timestamp = time(NULL),
    .temperature_c = temp
});
```

#### Idiom 4: Array Length Macro

```c
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

int numbers[] = {1, 2, 3, 4, 5};
for (size_t i = 0; i < ARRAY_LEN(numbers); i++) {
    // ...
}
```

#### Idiom 5: Static Inline for Small Functions

```c
// In header - avoids function call overhead
static inline int min_int(int a, int b) {
    return a < b ? a : b;
}

static inline bool is_power_of_two(size_t n) {
    return n && !(n & (n - 1));
}
```

#### Idiom 6: Designated Initializers for Arrays

Use this pattern to create safe, O(1) lookup tables for enums or error codes.

**Why?**
*   **O(1) Speed**: Direct array access is faster than `switch` or `if/else` chains.
*   **Safety**: Handles sparse arrays (missing values are NULL) and out-of-bounds checks.
*   **Readability**: Keeps the mapping of "Code -> String" in one clean table.

```c
// 1. The Setup (Internal)
static const char* error_names[] = {
    [0]         = "success",
    [ENOMEM]    = "out of memory",
    [EINVAL]    = "invalid argument",
    [ETIMEDOUT] = "timeout"
};

// 2. The Accessor (Public)
const char* error_str(int err) {
    // Handle negative error codes convention (e.g. return -ENOMEM)
    int code = (err < 0) ? -err : err;

    // Bounds check AND null check (in case of sparse array)
    if (code >= ARRAY_LEN(error_names) || !error_names[code]) {
        return "unknown error";
    }
    return error_names[code];
}

// 3. Usage at Call Site
void example_usage(void) {
    int ret = do_something(); // Returns -ENOMEM
    if (ret != 0) {
        // Logs: "Operation failed: out of memory"
        fprintf(stderr, "Operation failed: %s\n", error_str(ret));
    }
}
```


#### Idiom 7: Safe Free & Allocation Macros

```c
#define SAFE_FREE(ptr) \
    do { \
        free(ptr); \
        (ptr) = NULL; \
    } while (0)

#define SAFE_ALLOC(ptr) \
    do { \
        (ptr) = malloc(sizeof(*(ptr))); \
        if ((ptr)) memset((ptr), 0, sizeof(*(ptr))); \
    } while (0)

// Usage:
weather_data* data = NULL;
SAFE_ALLOC(data);      // allocates 1 struct
if (data == NULL) return -ENOMEM;

SAFE_FREE(data);
// pointers are now NULL
```

## Function Design & Control Flow

### Convention: Function Signature

Focus on predictable mechanics:

```c
int module_action(
    const input_type* input_param,  // [IN]  Read-only
    inout_type* inout_param,        // [I/O] Modified in-place
    output_type** output_param      // [OUT] Application allocation
);
```

### Standard Includes

#### Include Order
1.  **Corresponding Header**: `module.h` (to ensure it's self-contained)
2.  **System Headers**: `<stdlib.h>`, `<stdio.h>`
3.  **Project Headers**: `"other_module.h"`

```c
#include "my_module.h"  // 1. Verify self-containment

#include <stdlib.h>     // 2. System headers
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "utils.h"      // 3. Other project headers
```

### Return Value Conventions

```c
// 1. Process Exit Codes (main function)
// Use standard macros from <stdlib.h>
return EXIT_SUCCESS;
return EXIT_FAILURE;

// 2. Internal Function Status
// SUCCESS: 0
// FAILURE: Negative errno value (e.g., -EINVAL, -ENOMEM)

int perform_task(void) {
    if (error) return -EINVAL;
    return 0;
}

// 3. Pointer Returns
// SUCCESS: Valid pointer
// FAILURE: NULL (sets errno)
```

> **Why Negative Errno?**
> This pattern is borrowed from the **Linux Kernel**. It provides a universal error vocabulary (`-ENOMEM`, `-ETIMEDOUT`) that developers instantly recognize and allows errors to bubble up through layers without translation.
>
> **Need more detail?**
> If standard codes like `-EINVAL` are too generic for a specific failure, **log the specific details** (e.g., "Port 9999 out of range") before returning the standard error code. Do not invent custom enums for this.

### Error Handling

#### Idiom: GOTO-Cleanup
In C, we lack "Exceptions" or "Destructors" (RAII) found in other languages. If a function allocates multiple resources (e.g., file, memory, socket) and fails halfway through, you must manually release everything allocated so far.
*   **Without GOTO**: You get deeply nested `if` statements ("Arrow Code") or copy-pasted cleanup code.
*   **With GOTO**: You have **one linear flow** for success and **one centralized place** for cleanup. This is how the Linux Kernel and OpenSSL manage complexity.



> **Note: When to Use GOTO-Cleanup**
> This pattern is for functions that **manage multiple resources**. Simple utility functions (pure computations, single-allocation helpers, or leaf functions that return immediately on error) may use direct `return` statements for clarity.

```c
int process_request(const char* input, response** out) {
    int ret = 0;
    buffer* buf = NULL;
    parsed_data* data = NULL;
    response* resp = NULL;

    // Validate inputs
    if (!input || !out) {
        ret = -EINVAL;
        goto cleanup;
    }

    // Allocate resources
    buf = buffer_create(1024);
    if (!buf) {
        ret = -ENOMEM;
        goto cleanup;
    }

    data = parse_input(input);
    if (!data) {
        ret = -EBADMSG; // specific error
        goto cleanup; 
    }

    resp = response_create();
    if (!resp) {
        ret = -ENOMEM;
        goto cleanup;
    }

    // Do actual work (propagate error code)
    ret = build_response(data, resp);
    if (ret != 0) {
        goto cleanup;
    }

    // Transfer ownership to caller
    *out = resp;
    resp = NULL;  // Prevent cleanup from freeing it

cleanup:
    // Safe to call even if NULL (because valid destructors check for NULL)
    buffer_destroy(&buf);
    parsed_data_destroy(&data);
    response_destroy(&resp);
    return ret;
}
```

> **Note**: To handle specific error codes (like `-EBADMSG` above), use a standard `if` block.

## Resource Management & Safety

### Memory Management

#### Ownership Rules

> **Key Ownership Rules**
> 1.  **Allocator Deallocates**: The module that allocates memory is responsible for freeing it.
> 2.  **Explicit Transfer**: Ownership transfer must be explicit (e.g., specific out-parameters).
> 3.  **Const by Default**: Borrowed pointers (non-owning) must be `const`.
> 4.  **Safe Destructors**: Public destructors must take a double pointer (`T**`) to nullify the handle.
> 5.  **Internal Safety**: Use `SAFE_FREE` for internal raw memory management.

#### Hybrid Deallocation Strategy

We use a **Hybrid Approach** for safety:

1.  **Public Objects (`destroy(T** ptr)`)**: Pass address of pointer. Function frees memory AND sets caller's pointer to `NULL`.
2.  **Internal/Raw (`SAFE_FREE(ptr)`)**: Use macro for raw buffers inside functions.

```c
// Pattern: Clear pointer after free (Internal Use)
// See "Idiom 7: Safe Free Macro" for definition.

// Pattern: Constructor returns pointer, destructor takes DOUBLE pointer (Public API)
weather_data* weather_data_create(void) {
    int ret = 0;
    weather_data* data = NULL;

    // RULE: Use malloc + memset instead of calloc
    // Why: Forces OS to touch memory immediately, preventing lazy allocation (overcommit) surprises later.
    data = malloc(sizeof(*data));
    CHECK_ALLOC(data);
    memset(data, 0, sizeof(*data));

    // Initialize fields
    return data;

cleanup:
    SAFE_FREE(data);
    return NULL;
    (void)ret;  // Suppress unused warning in simple case
}

// Destructor neutralizes the pointer at the call site
void weather_data_destroy(weather_data** data_ptr) {
    // 1. Check if pointer exists and is not already NULL
    if (data_ptr == NULL || *data_ptr == NULL) {
        return;
    }
    
    // 2. Dereference to local variable for easy access
    weather_data* data = *data_ptr;
    
    // 3. Free resources using SAFE_FREE for raw parts
    SAFE_FREE(data->forecast_json); // Internal macro usage
    SAFE_FREE(data);                // Free self
    
    // 4. NULL the caller's pointer to prevent use-after-free
    *data_ptr = NULL;
}
```

#### Array Allocation

```c
// Use malloc+memset for strict memory guarantees
weather_data* slots = malloc(count * sizeof(*slots));
if (slots) memset(slots, 0, count * sizeof(*slots));

// Flexible array members
typedef struct {
    size_t count;
    plan_entry entries[];  // C99 flexible array
} energy_plan;

energy_plan* energy_plan_create(size_t count) {
    int ret = 0;
    energy_plan* plan = NULL;

    // Total size: header + flex array
    const size_t total_size = sizeof(*plan) + count * sizeof(plan->entries[0]);
    
    plan = malloc(total_size);
    CHECK_ALLOC(plan);
    memset(plan, 0, total_size);
    plan->count = count;

    return plan;

cleanup:
    SAFE_FREE(plan);
    return NULL;
    (void)ret;  // Suppress unused warning in simple case
}
```

### Safe vs Banned Functions

#### BANNED FUNCTIONS (Never Use)

| Banned | Reason | Safe Alternative |
|--------|--------|------------------|
| `gets()` | No bounds checking | `fgets()` |
| `sprintf()` | Buffer overflow | `snprintf()` |
| `strcpy()` | Buffer overflow | `strncpy()` or `strlcpy()` |
| `strcat()` | Buffer overflow | `strncat()` or `strlcat()` |
| `strncpy()` | No null-termination | `snprintf()` or manual |
| `scanf("%s")` | Buffer overflow | `scanf("%99s")` with width |
| `atoi()`, `atof()` | No error detection | `strtol()`, `strtod()` |
| `strtok()` | Not thread-safe | `strtok_r()` |
| `gmtime()`, `localtime()` | Not thread-safe | `gmtime_r()`, `localtime_r()` |
| `ctime()`, `asctime()` | Not thread-safe | `strftime()` |
| `rand()` | Poor randomness, not thread-safe | `arc4random()` or `/dev/urandom` |
| `alloca()` | Stack overflow | `malloc()` |
| `system()` | Command injection | `exec*()` family |

#### ✅ SAFE ALTERNATIVES

```c
// String copying
size_t safe_strcpy(char* dst, size_t dst_size, const char* src){
    if (dst_size == 0) return 0;
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return len;
}

// String formatting
// String formatting
int result = snprintf(buffer, ARRAY_LEN(buffer), "Value: %d", value);
if (result < 0 || (size_t)result >= ARRAY_LEN(buffer)) {
    // Handle truncation or error
}

// Number parsing (returns 0 on success, -EINVAL on failure)
int parse_long(const char* str, long* out){
    if (!str || !out) return -EINVAL;
    
    char* endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    if (errno != 0 || endptr == str || *endptr != '\0') {
        return -EINVAL;
    }
    *out = val;
    return 0;
}

// Time functions
struct tm result;
gmtime_r(&timestamp, &result);  // Thread-safe (POSIX)
// Random numbers
#include <sys/random.h>
#include <stdlib.h>
uint32_t secure_random(void) {
    uint32_t val;
    if (getrandom(&val, sizeof(val), 0) != sizeof(val)) {
        abort();  // Should never happen
    }
    return val;
}
```

#### Build-Time Enforcement

```makefile
# Compiler warnings to catch unsafe functions
CFLAGS += -Wformat=2
CFLAGS += -Wformat-security
CFLAGS += -Wformat-truncation=2
CFLAGS += -Wstringop-overflow=4

# Define _FORTIFY_SOURCE for runtime checks
CFLAGS += -D_FORTIFY_SOURCE=2 -O2
```

### Thread Safety

#### Rules

> **Thread Safety Rules**
> 1.  **Lock Data, Not Code**: Protect shared mutable state with mutexes.
> 2.  **Consistent Ordering**: Always lock resources in the same global order to prevent deadlocks.
> 3.  **Minimize Critical Sections**: Hold locks for the minimum time necessary.
> 4.  **Prefer Immutability**: Immutable data is inherently thread-safe.
> 5.  **Thread Local**: Use thread-local storage where possible to avoid contention.

#### Concurrency Pattern: Thread-Safe Singleton

```c
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static config* global_config = NULL;
static bool init_failed = false;

static void init_config(void) {
    global_config = config_create();
    if (global_config == NULL) {
        init_failed = true;
        fprintf(stderr, "FATAL: Failed to create global config\n");
    }
}

config* config_get_global(void) {
    pthread_once(&init_once, init_config);
    // FAIL FAST: Return NULL if initialization failed
    return init_failed ? NULL : global_config;
}
```

#### Concurrency Pattern: Mutex Wrapper

```c
#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    // ... protected data
} thread_safe_cache;

int cache_get(thread_safe_cache* c, const char* key, void* out) {
    int ret = 0;
    
    pthread_mutex_lock(&c->lock);
    ret = cache_get_internal(c, key, out);
    pthread_mutex_unlock(&c->lock);
    
    return ret;
}
```

## Architecture Patterns

### Design Pattern 1: Opaque Pointer (The Standard)
This is the **REQUIRED** pattern for all stateful modules (e.g., `server`, `config`, `cache`).
**Rule**: NEVER expose struct internals in a public `.h` file. Use a forward declaration and define the struct in the `.c` file.

```c
// config.h - public API
typedef struct config config;  // Opaque - users can't see inside

config* config_create(void);
void config_destroy(config** cfg);
int config_get_port(const config* cfg);

// config.c - implementation
struct config {
    int port;
    char* hostname;
    // ... private fields
};
```

### Design Pattern 2: Interface (Plugin System)

```c
// storage.h
typedef struct {
    const char* name;
    int  (*open)(const char* path, void** ctx);
    int  (*read)(void* ctx, const char* key, void* buf, size_t* len);
    int  (*write)(void* ctx, const char* key, const void* buf, size_t len);
    void (*close)(void* ctx);
} storage_ops;

// sqlite_storage.c
static int sqlite_open(const char* path, void** ctx) { /* ... */ }
static int sqlite_read(void* ctx, const char* key, void* buf, size_t* len) { /* ... */ }
static int sqlite_write(void* ctx, const char* key, const void* buf, size_t len) { /* ... */ }
static void sqlite_close(void* ctx) { /* ... */ }

// NOTE: Intentionally NOT static - this is the public "plugin" export.
// Other modules reference this symbol to use the sqlite backend.
const storage_ops sqlite_storage = {
    .name = "sqlite",
    .open = sqlite_open,
    .read = sqlite_read,
    .write = sqlite_write,
    .close = sqlite_close
};
```

### Design Pattern 3: Builder (Complex Object Construction)

```c
// Example: builds a HTTP response object

// request_builder.h
typedef struct request_builder request_builder;

// Build the object incrementally 
request_builder* request_builder_create(void);
void request_builder_method(request_builder* b, const char* method);
void request_builder_url(request_builder* b, const char* url);
void request_builder_header(request_builder* b, const char* k, const char* v);
void request_builder_timeout(request_builder* b, int ms);

// Finalize, Get the built object and consume (free) the builder
http_request* request_builder_build(request_builder** b);  // Finalizes, frees builder
void request_builder_destroy(request_builder** b);  // If not calling build

// Usage (Defensive: check builder before all operations)
request_builder* builder = request_builder_create();
if (!builder) {
    // Handle allocation failure
    return -ENOMEM;
}
request_builder_method(builder, "GET");
request_builder_url(builder, "https://api.smhi.se/forecast");
request_builder_header(builder, "Accept", "application/json");
request_builder_timeout(builder, 5000);
http_request* req = request_builder_build(&builder); // Frees builder
```

### Design Pattern 4: Object Pool (Reuse Expensive Objects)
**Why Use This?**
*   **Performance**: Avoids thousands of `malloc`/`free` calls per second.
*   **Throttling**: Naturally limits resource usage (e.g., max 50 DB connections).
*   **Predictability**: Reduces memory fragmentation.

```c
typedef struct {
    void** resources;      // Generic stack of pointers (void*)
    size_t capacity;       // Max number of objects (e.g. 50)
    size_t available;      // Current count of free objects
    pthread_mutex_t lock;  // Ensures thread safety
} resource_pool;

// Get an object from the pool
// (Implementation choice: Block or return NULL if available = 0)
void* pool_get(resource_pool* p);

// Return an object to the pool (Reset state before returning!)
void pool_put(resource_pool* p, void* resource);
```

### Design Pattern 5: Observer/Callback
**Why Use This?**
*   **Decoupling**: The data source needs no knowledge of who consumes the data.
*   **Extensibility**: Add new listeners (Logger, UI, Network) without changing the source code.
*   **Closure Emulation**: `userdata` allows C functions to carry context/state.

```c
// Function pointer type: Recieves data + context
typedef void (*data_callback)(const weather_data* data, void* userdata);

typedef struct {
    data_callback callback; // The function to call (CODE)
    void* userdata;         // The context for that function (DATA)
} subscriber;

// Register a callback: "Call 'cb' with 'userdata' when you have data"
void databus_subscribe(databus* bus, data_callback cb, void* userdata);

// Broadcast: Loops through all subscribers and invokes their callbacks
void databus_publish(databus* bus, const weather_data* data);
```



## Testing & Verification

### Framework
*   **Unit Tests**: Use a simple, standalone test runner (like `Unity` or a custom `test_assert` macro) in `tests/`.
*   **Integration Tests**: Scripted tests running the binary against mock inputs.

### Test Standards
1.  **Separate Files**: One test file per source module (e.g., `tests/test_parser.c` tests `src/parser.c`).
2.  **No Internals**: Tests should primarily use the **Public API** (`.h`) to verify behavior, not private `static` functions (unless `#include "module.c"` is used for white-box testing, which is discouraged but sometimes necessary).
3.  **Fail Fast**: Test should exit/abort on the first failure.

### Example Test
```c
#include "module.h"
#include <assert.h>

void test_creation(void) {
    module_t* m = module_create();
    assert(m != NULL);
    module_destroy(&m);
    assert(m == NULL);
}

int main(void) {
    test_creation();
    printf("All tests passed.\n");
    return 0;
}
```

## Enforcement & Tooling

We employ a 3-layer defense strategy to help ensure code quality:

### 1. Formatting (`clang-format`)
**Automated Style Enforcement.**
`clang-format` to maintain a consistent codebase. Handles indentations, spacing, and brace placement.

### 2. Static Analysis (`clang-tidy`)
**Automated Safety & Linting.**
`clang-tidy` to catch bugs, unsafe patterns, and naming violations. Enforces "Safe Functions", variable naming conventions, and C99 best practices.

### 3. LLM Review (Optional)
**Semantic & Architectural Verification.**
For complex changes, use an LLM Code Review to catch what tools miss.
Use `/llm_review` in a LLM chat to trigger the workflow. (recommended models Gemini 3 pro or claude opus 4.5)
