# Makefile for HeimWatt
#
# Targets:
#   debug       - Build with -g, -O0, ASAN (default)
#   release     - Build with -O2, no ASAN
#   analyze     - Run structure analysis script
#   lint        - Run clang-tidy
#   format      - Run clang-format
#   clean       - Remove build artifacts
#   sdk         - Build SDK static library
#   smhi        - Build SMHI Weather plugin
#   stats       - Build Avg/Sum Stats plugin

# Compiler Settings
CC = gcc
CFLAGS_COMMON = -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DLOG_USE_COLOR -Wall -Wextra -pthread -I libs -I include -I src -I . -I libs/duckdb
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O2 -DNDEBUG

# Filter out flags incompatible with clang-tidy
LINT_FLAGS = $(filter-out -fsanitize%, $(CFLAGS_DEBUG))

LDFLAGS_COMMON = -lcurl -ldl -lm -Llibs/duckdb -lduckdb -Wl,-rpath,'$$ORIGIN/../../libs/duckdb'
LDFLAGS_DEBUG = $(LDFLAGS_COMMON) -fsanitize=address,undefined

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Sources
SRC = src/main.c src/server.c src/core/ipc.c src/core/semantic_types.c \
      src/core/plugin_mgr.c src/core/config.c src/core/memory.c src/db/db.c src/db/csv_backend.c \
      src/db/duckdb_backend.c \
      src/net/tcp_server.c src/net/http_parse.c src/net/http_server.c
# Convert src/%.c to build/obj/src/%.o
OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

# Libs
LIBS_SRC = libs/cJSON.c libs/log.c
LIBS_OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIBS_SRC))
CFLAGS_LIBS = -std=c99 -D_GNU_SOURCE -DLOG_USE_COLOR -pthread -g -w

# Output Binary
TARGET_BIN = $(BIN_DIR)/server_prototype

.PHONY: all debug release clean analyze lint format sdk smhi stats

all: debug

# --- Build Rules ---

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Main Sources
$(OBJ_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Plugins
$(OBJ_DIR)/plugins/%.o: plugins/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Libs (Always compiled with safe flags)
$(OBJ_DIR)/libs/%.o: libs/%.c | $(OBJ_DIR)/libs
	$(CC) $(CFLAGS_LIBS) -c $< -o $@

# --- Cleanup ---

clean_objs:
	rm -rf $(OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR) leop.db

# --- Structure Analysis ---

analyze: compile_commands
	@echo ">> Running structure analysis..."
	@mkdir -p scripts/out
	@# Script invocation if needed
	
# --- Linting/Formatting ---

lint: compile_commands
	@echo ">> Linting..."
	find src plugins -name "*.c" | xargs clang-tidy --quiet -p . 2>&1 | grep -v "warnings generated" || true

compile_commands:
	@echo ">> Generating compile_commands.json..."
	python3 scripts/gen_compile_commands.py

format: compile_commands
	@echo ">> Formatting..."
	find src include plugins -name "*.c" -o -name "*.h" | xargs clang-format -i

# --- Unity Testing ---

UNITY_SRC = libs/unity/unity.c
UNITY_OBJ = $(OBJ_DIR)/libs/unity/unity.o

# Compile Unity with safe flags (enable double precision)
$(OBJ_DIR)/libs/unity/%.o: libs/unity/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -g -w -I. -DUNITY_INCLUDE_DOUBLE -c $< -o $@

# Test sources
UNIT_TESTS = tests/unit/test_http_parse.c \
             tests/unit/test_json.c \
             tests/unit/test_semantic_types.c \
             tests/unit/test_file_backend.c \
             tests/unit/test_ipc.c \
             tests/unit/test_plugin_mgr.c \
             tests/unit/test_http_server.c \
             tests/unit/test_duckdb_backend.c \
             tests/unit/test_lps.c

TEST_RUNNER_SRC = tests/test_runner.c

# Objects needed for testing (module implementations)
TEST_DEPS_OBJ = $(OBJ_DIR)/src/net/http_parse.o \
                $(OBJ_DIR)/src/net/json.o \
                $(OBJ_DIR)/src/core/semantic_types.o \
                $(OBJ_DIR)/src/core/config.o \
                $(OBJ_DIR)/src/core/memory.o \
                $(OBJ_DIR)/src/db/db.o \
                $(OBJ_DIR)/src/db/csv_backend.o \
                $(OBJ_DIR)/src/db/duckdb_backend.o \
                $(OBJ_DIR)/src/core/ipc.o \
                $(OBJ_DIR)/src/core/plugin_mgr.o \
                $(OBJ_DIR)/src/net/http_server.o \
                $(OBJ_DIR)/src/net/tcp_server.o \
                $(OBJ_DIR)/plugins/out/energy_strategy/lps/lps.o

# Build and run unit tests
unit-test: CFLAGS = $(CFLAGS_DEBUG)
unit-test: LDFLAGS = $(LDFLAGS_DEBUG)
unit-test: $(UNITY_OBJ) $(TEST_DEPS_OBJ) $(LIBS_OBJ) | $(BIN_DIR)
	@echo ">> Building unit tests..."
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -o $(BIN_DIR)/unit_tests \
		$(UNITY_OBJ) $(UNIT_TESTS) $(TEST_RUNNER_SRC) \
		$(TEST_DEPS_OBJ) $(LIBS_OBJ) $(LDFLAGS)
	@echo ">> Running unit tests..."
	@$(BIN_DIR)/unit_tests

.PHONY: unit-test

# --- Targets ---

$(BIN_DIR):
	@mkdir -p $@

$(OBJ_DIR)/libs:
	@mkdir -p $@

# Debug Build (The "Song" / ASAN build)
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: analyze format lint $(TARGET_BIN) plugins unit-test
	@echo ">> DEBUG Build Complete (with ASAN, tests passed)."

# Alias for debug
release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_COMMON)
release: analyze format lint clean_objs $(TARGET_BIN)
	@echo ">> RELEASE Build Complete."

$(TARGET_BIN): $(OBJ) $(LIBS_OBJ) | $(BIN_DIR)
	@echo ">> Linking $@..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# --- SDK Build ---

SDK_SRC = src/sdk/lifecycle.c src/sdk/scheduler.c src/sdk/ipc.c \
          src/sdk/report.c src/sdk/config.c src/sdk/http.c \
          src/sdk/json.c src/sdk/state.c src/sdk/query.c \
          src/sdk/api.c src/core/semantic_types.c \
          src/net/http_client.c src/net/json.c \
          src/core/memory.c \
          libs/cJSON.c

SDK_OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SDK_SRC))

$(BUILD_DIR)/lib/libheimwatt_sdk.a: $(SDK_OBJ)
	@echo ">> Creating SDK Static Library..."
	@mkdir -p $(BUILD_DIR)/lib
	ar rcs $@ $(SDK_OBJ)
	@echo ">> SDK Built at $@"

sdk: CFLAGS = $(CFLAGS_DEBUG)
sdk: $(BUILD_DIR)/lib/libheimwatt_sdk.a

# --- Run ---

run: debug
	@echo ">> Running $(TARGET_BIN) (verbose mode)..."
	$(TARGET_BIN) -v

# --- Plugins Build (Auto-Discovery) ---

# Find all plugin source files (directories containing main.c)
PLUGIN_SRCS := $(wildcard plugins/in/*/main.c) $(wildcard plugins/out/*/main.c)

# Convert source paths to binary paths: plugins/in/smhi_weather/main.c -> build/bin/plugins/smhi_weather
PLUGIN_BINS := $(patsubst plugins/%/main.c,$(BIN_DIR)/plugins/%,$(subst /in/,/,$(subst /out/,/,$(PLUGIN_SRCS))))

# Build all plugins
plugins: CFLAGS = $(CFLAGS_DEBUG)
plugins: LDFLAGS = $(LDFLAGS_DEBUG)
plugins: sdk $(PLUGIN_BINS)
	@echo ">> Built $(words $(PLUGIN_BINS)) plugin(s)"

# Pattern rule for building any plugin
$(BIN_DIR)/plugins/%: plugins/in/%/main.c $(BUILD_DIR)/lib/libheimwatt_sdk.a
	@mkdir -p $(dir $@)
	@echo ">> Building plugin: $*"
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD_DIR)/lib -lheimwatt_sdk $(LDFLAGS) -Wl,-rpath,'$$ORIGIN/../../../libs/duckdb' || echo ">> WARN: Plugin $* failed to build (skipped)"

$(BIN_DIR)/plugins/%: plugins/out/%/main.c $(BUILD_DIR)/lib/libheimwatt_sdk.a
	@mkdir -p $(dir $@)
	@echo ">> Building plugin: $*"
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD_DIR)/lib -lheimwatt_sdk $(LDFLAGS) -Wl,-rpath,'$$ORIGIN/../../../libs/duckdb' || echo ">> WARN: Plugin $* failed to build (skipped)"

# List discovered plugins (for debugging)
list-plugins:
	@echo "Discovered plugins:"
	@for p in $(PLUGIN_BINS); do echo "  $$p"; done

# --- Fuzzing (AFL++) ---

FUZZ_DIR = tests/fuzz
FUZZ_BIN_DIR = $(BUILD_DIR)/fuzz

FUZZ_HTTP_SRC = $(FUZZ_DIR)/fuzz_http_parser.c
FUZZ_JSON_SRC = $(FUZZ_DIR)/fuzz_json_parser.c

# Build fuzz targets with AFL++ instrumentation
# Ensure that even if built individually, they use the correct compiler
$(FUZZ_BIN_DIR)/%: CC = afl-gcc
$(FUZZ_BIN_DIR)/%: CFLAGS = $(CFLAGS_COMMON) -g

fuzz-build: $(FUZZ_BIN_DIR)/fuzz_http_parser $(FUZZ_BIN_DIR)/fuzz_json_parser $(FUZZ_BIN_DIR)/fuzz_lps_solver $(FUZZ_BIN_DIR)/fuzz_sdk_config $(FUZZ_BIN_DIR)/fuzz_semantic
	@echo ">> Fuzz targets built in $(FUZZ_BIN_DIR)"

$(FUZZ_BIN_DIR)/fuzz_http_parser: $(FUZZ_HTTP_SRC) src/net/http_parse.c
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -I include -I src -I .

$(FUZZ_BIN_DIR)/fuzz_json_parser: $(FUZZ_JSON_SRC) src/net/json.c libs/cJSON.c
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -I include -I src -I libs -I .

# Run HTTP fuzzer
fuzz-http: $(FUZZ_BIN_DIR)/fuzz_http_parser
	@echo ">> Starting HTTP parser fuzzer (Ctrl+C to stop)..."
	AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 afl-fuzz -i $(FUZZ_DIR)/corpus/http -o $(FUZZ_DIR)/out/http -x $(FUZZ_DIR)/dictionaries/http.dict $(AFL_EXTRA_ARGS) -- $<

# Run JSON fuzzer
fuzz-json: $(FUZZ_BIN_DIR)/fuzz_json_parser
	@echo ">> Starting JSON parser fuzzer (Ctrl+C to stop)..."
	AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 afl-fuzz -i $(FUZZ_DIR)/corpus/json -o $(FUZZ_DIR)/out/json -x $(FUZZ_DIR)/dictionaries/json.dict $(AFL_EXTRA_ARGS) -- $<

# Run LPS solver fuzzer
fuzz-lps: $(FUZZ_BIN_DIR)/fuzz_lps_solver
	@echo ">> Starting LPS solver fuzzer (Ctrl+C to stop)..."
	AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 afl-fuzz -i $(FUZZ_DIR)/corpus/lps -o $(FUZZ_DIR)/out/lps $(AFL_EXTRA_ARGS) -- $<

# Run SDK Config fuzzer
fuzz-sdk-config: $(FUZZ_BIN_DIR)/fuzz_sdk_config
	@echo ">> Starting SDK config fuzzer (Ctrl+C to stop)..."
	AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 afl-fuzz -i $(FUZZ_DIR)/corpus/sdk_config -o $(FUZZ_DIR)/out/sdk_config $(AFL_EXTRA_ARGS) -- $<

# Run Semantic Type fuzzer
fuzz-semantic: $(FUZZ_BIN_DIR)/fuzz_semantic
	@echo ">> Starting Semantic Type fuzzer (Ctrl+C to stop)..."
	AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 afl-fuzz -i $(FUZZ_DIR)/corpus/semantic -o $(FUZZ_DIR)/out/semantic $(AFL_EXTRA_ARGS) -- $<

# Run all fuzzers in a cycle (infinite loop)
fuzz-cycle: fuzz-build
	@echo ">> Starting Fuzzing Cycle (Ctrl+C to stop)..."
	@./tests/fuzz/fuzz_cycle.sh

$(FUZZ_BIN_DIR)/fuzz_lps_solver: $(FUZZ_DIR)/fuzz_lps_solver.c plugins/out/energy_strategy/lps/lps.c
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -I include -I src -I libs -I . -lm

$(FUZZ_BIN_DIR)/fuzz_sdk_config: $(FUZZ_DIR)/fuzz_sdk_config.c src/sdk/config.c
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -I include -I src -I .

$(FUZZ_BIN_DIR)/fuzz_semantic: $(FUZZ_DIR)/fuzz_semantic.c src/core/semantic_types.c
	@mkdir -p $(FUZZ_BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -I include -I src -I .

.PHONY: fuzz-build fuzz-http fuzz-json fuzz-lps fuzz-sdk-config fuzz-semantic fuzz-cycle

# --- Valgrind (Deep Memory Analysis) ---
# Note: Valgrind cannot run with ASAN, so we build without sanitizers

VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes \
                 --error-exitcode=1 --errors-for-leak-kinds=definite

# Build without sanitizers for Valgrind
valgrind-build: CFLAGS = $(CFLAGS_COMMON) -g -O0
valgrind-build: LDFLAGS = $(LDFLAGS_COMMON)
valgrind-build: clean_objs $(UNITY_OBJ) $(TEST_DEPS_OBJ) $(LIBS_OBJ) | $(BIN_DIR)
	@echo ">> Building for Valgrind (no sanitizers)..."
	$(CC) $(CFLAGS) -DUNITY_INCLUDE_DOUBLE -o $(BIN_DIR)/unit_tests_valgrind \
		$(UNITY_OBJ) $(UNIT_TESTS) $(TEST_RUNNER_SRC) \
		$(TEST_DEPS_OBJ) $(LIBS_OBJ) $(LDFLAGS)
	@echo ">> Built $(BIN_DIR)/unit_tests_valgrind"

# Run unit tests under Valgrind
valgrind-test: valgrind-build
	@echo ">> Running unit tests under Valgrind (this may take a while)..."
	valgrind $(VALGRIND_FLAGS) $(BIN_DIR)/unit_tests_valgrind

# Quick Valgrind check (fewer leak details)
valgrind-quick: valgrind-build
	@echo ">> Quick Valgrind check..."
	valgrind --leak-check=summary --error-exitcode=1 $(BIN_DIR)/unit_tests_valgrind

.PHONY: valgrind-build valgrind-test valgrind-quick
