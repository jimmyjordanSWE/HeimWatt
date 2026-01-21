# HeimWatt Development Guide for AI Agents

## Project Overview
HeimWatt is an extensible data platform for energy optimization with a hybrid C99/React architecture:
- **Backend**: C99 server with plugin system for data collection/processing
- **Frontend**: React + TypeScript + Vite web interface  
- **Database**: DuckDB (primary) with CSV fallback
- **Testing**: Unity framework for C, ESLint for TypeScript

## Build Commands

### C99 Backend
```bash
# Main builds
make debug      # Default: ASAN + debug + tests
make release    # Optimized production build
make clean      # Remove build artifacts

# Development workflow
make format     # clang-format code formatting
make lint       # clang-tidy static analysis
make analyze    # Run Python analysis scripts

# Testing
make unit-test  # Build and run all Unity tests
make valgrind-test  # Memory leak detection
make fuzz-build # AFL++ fuzzing targets

# SDK & Plugins
make sdk        # Build static SDK library
make plugins    # Auto-discover and build all plugins
```

### React Frontend
```bash
cd webui
npm run dev     # Development server with HMR
npm run build   # Production build
npm run lint    # ESLint + TypeScript checking
npm run preview # Preview production build
```

## Running Single Tests

### C99 Unit Tests
Tests are located in `tests/unit/test_*.c`. To run a specific test:
1. Build the test binary: `make unit-test`
2. Run individual tests by modifying the test runner or using Unity's test selection

### Frontend Tests
Use standard Jest/Vitest patterns (check webui/ for test setup)

## Code Style Guidelines

### C99 Standards (Primary)

#### Naming Conventions
- **Functions**: `module_verb_noun` pattern (e.g., `config_load`, `cache_put`)
- **Variables**: `snake_case` for all variables
- **Types**: `snake_case` for structs/enums (NO `_t` suffix)
- **Constants**: `UPPER_CASE` for enum values, `static const` for typed constants
- **Files**: `module_name.c` for implementation, `module_name.h` for public API

#### Function Patterns
```c
// Lifecycle pairs (MUST be symmetric)
config* config_create();           // Heap allocation
void config_destroy(config** cfg);  // Double pointer, sets NULL

int server_init(server* s);         // Stack initialization
void server_fini(server* s);

int db_open(db* d, const char* path);  // External resources
void db_close(db* d);
```

#### Error Handling
- **Return values**: 0 for success, negative errno for failure (`-EINVAL`, `-ENOMEM`)
- **Resource cleanup**: Use goto-cleanup pattern for multi-resource functions
- **Input validation**: Always validate public API inputs, return `-EINVAL`

#### Memory Management
- **Allocation**: Use `mem_alloc(sizeof(*ptr))` - NEVER raw `malloc`
- **Destructors**: Take double pointer, set to NULL
- **Arrays**: Use flexible array members for dynamic structs
- **Buffers**: Use `HwBuffer` for variable-length data

#### Include Order
1. Corresponding header (`module.h`)
2. System headers (`<stdlib.h>`, `<stdio.h>`)
3. Project headers (`"other_module.h"`)

#### Const Correctness
- Input parameters: `const` if read-only
- Return values: `const` for internal state access
- Local variables: `const` if not modified

### TypeScript/React Standards

#### Naming
- **Components**: `PascalCase` (e.g., `WeatherDashboard`)
- **Functions**: `camelCase` (e.g., `fetchWeatherData`)
- **Variables**: `camelCase`
- **Constants**: `UPPER_SNAKE_CASE`

#### Code Style
- Use Tailwind CSS for styling
- Prefer functional components with hooks
- Use Radix UI for accessible components
- Follow ESLint configuration in `webui/eslint.config.js`

## Architecture Patterns

### Opaque Pointer Pattern (Required)
```c
// config.h - Public API
typedef struct config config;  // Forward declaration only

// config.c - Implementation
struct config {
    int port;
    char* hostname;
    // Private fields
};
```

### Plugin Interface Pattern
```c
typedef struct {
    const char* name;
    int (*open)(const char* path, void** ctx);
    int (*read)(void* ctx, const char* key, void* buf, size_t* len);
    void (*close)(void* ctx);
} storage_ops;
```

## Safety Rules

### Banned Functions (Never Use)
- `gets()`, `sprintf()`, `strcpy()`, `strcat()`
- `atoi()`, `atof()` - use `strtol()`, `strtod()`
- `malloc()`, `free()` - use `mem_alloc()`, `mem_free()`
- `strtok()` - use `strtok_r()`

### Safe Alternatives
- String copying: `snprintf()` or custom safe functions
- Number parsing: `strtol()` with error checking
- Time functions: `gmtime_r()`, `localtime_r()`

## Testing Guidelines

### C99 Unit Tests
- Use Unity framework in `tests/unit/`
- Test public APIs, not private static functions
- One test file per source module
- Use setup/teardown for resource management

### Frontend Tests
- Follow React Testing Library patterns
- Test user behavior, not implementation details

## Development Workflow

### Git Workflow
- `main` (stable), `dev` (integration), feature branches
- Conventional commits: `feat:`, `fix:`, `refactor:`
- CI pipeline: format → lint → build → test → ASAN validation

### Code Quality Pipeline
1. `make format` - Automated formatting
2. `make lint` - Static analysis and safety checks  
3. `make unit-test` - Unity framework validation
4. ASAN/UBSAN - Runtime error detection
5. Fuzzing - Security testing for parsers

## Key Files to Reference
- `docs/standards/coding.md` - Comprehensive C99 standards
- `.clang-format` - Code formatting rules
- `Makefile` - Build system and targets
- `webui/package.json` - Frontend dependencies and scripts