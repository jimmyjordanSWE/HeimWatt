# HEIMWATT Tech Stack

C99 server libraries and APIs. Priority: single-header, minimal dependencies.

## Build Environment

| Component | Choice |
|-----------|--------|
| Language | C99 |
| Compiler | Clang — best clang-tidy/format integration |
| Build System | CMake — industry standard, generates `compile_commands.json` |
| Debug Flags | `-std=c99 -Wall -Wextra -Werror -pedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -g3 -O0` |
| Release Flags | `-std=c99 -Wall -Wextra -Werror -O2 -DNDEBUG -march=native` |

## Libraries

| Library | Purpose | Type |
|---------|---------|------|
| [cJSON](https://github.com/DaveGamble/cJSON) | JSON parsing | 2 files |
| [libcurl](https://curl.se/libcurl/) | HTTP client | System lib (note: we get HTTPS for free) |
| [SQLite](https://sqlite.org/) | Database | Amalgamation |
| [log.c](https://github.com/rxi/log.c) | Logging | Single header |
| [stb_ds.h](https://github.com/nothings/stb) | Hash maps, dynamic arrays | Single header |
| [µnit](https://github.com/nemequ/munit) | Unit testing | Single header |

## External APIs

| API | Data | Format |
|-----|------|--------|
| [SMHI Open Data](https://opendata.smhi.se/) | Sweden weather | JSON |
| [Open-Meteo](https://open-meteo.com/) | Global weather + solar | JSON |
| [Elpriset Just Nu](https://www.elprisetjustnu.se/api) | Swedish spot prices | JSON |
| [ENTSO-E](https://transparency.entsoe.eu/) | EU prices | XML |
| [PVGIS](https://re.jrc.ec.europa.eu/pvg_tools/) | EU solar irradiance | JSON |

## Dev Tools

| Tool | Purpose |
|------|---------|
| clang-format | Code formatting |
| clang-tidy | Static analysis, linting |
| AddressSanitizer (ASAN) | Memory errors, leaks |
| UndefinedBehaviorSanitizer (UBSAN) | Undefined behavior |
| Valgrind | Memory debugging, profiling |
| AFL++ | Fuzz testing |
| perf | Linux profiling, sampling |
| [FlameGraph](https://github.com/brendangregg/FlameGraph) | Flame graph visualization |

## SIMD Intrinsics

| Header | Instruction Set |
|--------|-----------------|
| `<immintrin.h>` | SSE, AVX, AVX2, AVX-512 (all-in-one) |
| `<xmmintrin.h>` | SSE only |
| `<emmintrin.h>` | SSE2 only |

**Reference**: [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)

**Compile flags**: `-march=native` (use host CPU features) or `-mavx2` (explicit)

### Verifying Vectorization

Before writing intrinsics, check if the compiler is auto-vectorizing your loops:

```bash
# Disassemble and look for SIMD instructions (vmov*, vpadd*, etc.)
objdump -d -M intel build/server | grep -E 'vmov|vpadd|vmul|vadd'

# Clang optimization report — shows what got vectorized
clang -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize src/calc.c
```

If loops show scalar instructions (`mov`, `add`) instead of vector (`vmov`, `vpadd`), then write explicit intrinsics.

## Dev Tools Guide

### clang-format

Enforces consistent code style automatically. Eliminates style debates in code review.

```bash
clang-format -i src/*.c src/*.h          # Format in-place
clang-format --dry-run --Werror src/*.c  # CI check (fail if unformatted)
```

---

### clang-tidy

Catches bugs, modernization issues, and style problems at compile time. Finds what the compiler misses.

```bash
clang-tidy src/*.c -- -I./include        # Run checks
clang-tidy --fix src/*.c -- -I./include  # Auto-fix issues
```

---

### AddressSanitizer (ASAN)

Detects memory bugs at runtime: buffer overflows, use-after-free, double-free, leaks. Fast enough for testing.

```bash
gcc -fsanitize=address -g -o server src/*.c
./server  # Crashes with detailed report on memory error
```

---

### UndefinedBehaviorSanitizer (UBSAN)

Catches undefined behavior: integer overflow, null pointer dereference, misaligned access. UB causes silent corruption.

```bash
gcc -fsanitize=undefined -g -o server src/*.c
./server  # Reports UB at runtime
```

---

### Valgrind

Deep memory analysis when ASAN isn't enough. Cache profiling, thread error detection. Slower but more thorough.

```bash
valgrind --leak-check=full ./server       # Memory leaks
valgrind --tool=cachegrind ./server       # Cache performance
valgrind --tool=helgrind ./server         # Thread errors
```

---

### AFL++

Finds crashes and bugs by generating mutated inputs automatically. Essential for parsing code security.

```bash
afl-gcc -o server-fuzz src/*.c            # Compile with AFL instrumentation
mkdir in out && echo "seed" > in/seed
afl-fuzz -i in -o out -- ./server-fuzz    # Start fuzzing
```

---

### perf

Low-overhead CPU profiling. Shows where time is spent. Required for SIMD optimization work.

```bash
perf record -g ./server                   # Record with call graph
perf report                               # Interactive viewer
perf stat ./server                        # Quick stats (cycles, cache misses)
```

---

### FlameGraph

Visualizes profiling data as interactive SVG. Hot paths jump out immediately. Essential for optimization.

```bash
perf record -g ./server
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
# Open flame.svg in browser
```
