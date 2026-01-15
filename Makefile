CC = gcc
# Common Flags
CFLAGS_COMMON = -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pthread -I libs -I include -I src -I .

# Debug Flags (Default): ASAN, UBSAN, Debug Info
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g -fsanitize=address,undefined -fno-omit-frame-pointer

# Release Flags: Optimization, No Debug Info
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -DNDEBUG

# Libs Flags: Supress warnings
CFLAGS_LIBS = -std=c99 -D_GNU_SOURCE -pthread -g -w

LDFLAGS_COMMON = -lcurl -ldl -lm
LDFLAGS_DEBUG = $(LDFLAGS_COMMON) -fsanitize=address,undefined

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Sources
SRC = src/main.c plugins/out/lps/lps.c src/core/semantic_types.c
# Convert src/%.c to build/obj/src/%.o
OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

# Libs Sources
LIBS_SRC = libs/cJSON.c libs/sqlite3.c libs/log.c
# Convert libs/%.c to build/obj/libs/%.o
LIBS_OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIBS_SRC))

# All Objects
ALL_OBJ = $(OBJ) $(LIBS_OBJ)

# Targets
TARGET_BIN = $(BIN_DIR)/server_prototype
TEST_BIN = $(BIN_DIR)/test_lps

.PHONY: all debug release test format lint clean asan clean_objs analyze analyze-structure docs

# Default -> Debug -> Test
all: debug

# --- Analysis Scripts ---
SCRIPTS_DIR = scripts
SCRIPTS_OUT = $(SCRIPTS_DIR)/out
VENV_PYTHON = .venv/bin/python3

# Run core analysis scripts (fast, ~2s total)
analyze: analyze-structure
	@cd $(SCRIPTS_DIR) && ../$(VENV_PYTHON) call_chains.py
	@cd $(SCRIPTS_DIR) && ../$(VENV_PYTHON) token_count.py

# Just structure (minimum for agent workflows)
analyze-structure:
	@echo ">> Running structure analysis..."
	@mkdir -p $(SCRIPTS_OUT)
	@cd $(SCRIPTS_DIR) && ../$(VENV_PYTHON) structure.py

# Generate Doxygen documentation
docs:
	@echo ">> Generating API documentation..."
	@mkdir -p build/docs
	doxygen Doxyfile
	@echo ">> Docs generated at build/docs/html/index.html"

# --- Workflow Targets ---

# Debug Build (The "Song" / ASAN build)
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: analyze format lint $(TARGET_BIN) test
	@echo ">> DEBUG Build Complete (with ASAN)."

# Alias for debug
asan: debug

# Release Build
release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_COMMON)
release: analyze format lint clean_objs $(TARGET_BIN) test
	@echo ">> RELEASE Build Complete."

# --- Code Quality ---

format:
	@echo ">> Formatting..."
	clang-format -i src/*.c include/*.h

lint:
	@echo ">> Linting..."
	clang-tidy src/*.c -- $(CFLAGS_COMMON)

# --- Compilation Rules ---

# Ensure directories exist
$(OBJ_DIR)/src:
	@mkdir -p $(OBJ_DIR)/src

$(OBJ_DIR)/libs:
	@mkdir -p $(OBJ_DIR)/libs

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Linking the Server
$(TARGET_BIN): $(ALL_OBJ) | $(BIN_DIR)
	@echo ">> Linking $(TARGET_BIN)..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

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

# --- Testing ---

$(TEST_BIN): tests/test_lps.c $(OBJ_DIR)/plugins/out/lps/lps.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test: CFLAGS = $(CFLAGS_DEBUG)
test: LDFLAGS = $(LDFLAGS_DEBUG)
test: $(TEST_BIN)
	@echo ">> Running Tests..."
	$(TEST_BIN)

# --- Cleanup ---

clean_objs:
	rm -rf $(OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR) leop.db

# --- SDK Build ---

SDK_SRC = src/sdk/lifecycle.c src/sdk/scheduler.c src/sdk/ipc.c \
          src/sdk/report.c src/sdk/config.c src/sdk/http.c \
          src/sdk/json.c src/sdk/state.c \
          src/net/http_client.c src/net/json.c \
          libs/cJSON.c

SDK_OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SDK_SRC))

sdk: CFLAGS = $(CFLAGS_DEBUG)
sdk: $(SDK_OBJ)
	@echo ">> Creating SDK Static Library..."
	@mkdir -p $(BUILD_DIR)/lib
	ar rcs $(BUILD_DIR)/lib/libheimwatt_sdk.a $(SDK_OBJ)
	@echo ">> SDK Built at $(BUILD_DIR)/lib/libheimwatt_sdk.a"

# --- Plugins Build ---

SMHI_BIN = $(BIN_DIR)/plugins/smhi_weather

smhi: CFLAGS = $(CFLAGS_DEBUG)
smhi: LDFLAGS = $(LDFLAGS_DEBUG)
smhi: sdk $(SMHI_BIN)
	@echo ">> SMHI Plugin Built at $(SMHI_BIN)"

$(SMHI_BIN): plugins/in/smhi_weather/main.c $(BUILD_DIR)/lib/libheimwatt_sdk.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD_DIR)/lib -lheimwatt_sdk $(LDFLAGS)
