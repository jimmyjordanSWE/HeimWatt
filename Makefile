CC = gcc
# Common Flags
CFLAGS_COMMON = -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pthread -I libs -I include -I .

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
SRC = src/main.c src/utils.c src/config.c src/db.c src/pipeline.c src/server.c src/lps.c
# Convert src/%.c to build/obj/src/%.o
OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

# Libs Sources
LIBS_SRC = libs/cJSON.c libs/sqlite3.c
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
$(OBJ_DIR)/src/%.o: src/%.c | $(OBJ_DIR)/src
	$(CC) $(CFLAGS) -c $< -o $@

# Libs (Always compiled with safe flags)
$(OBJ_DIR)/libs/%.o: libs/%.c | $(OBJ_DIR)/libs
	$(CC) $(CFLAGS_LIBS) -c $< -o $@

# --- Testing ---

$(TEST_BIN): tests/test_lps.c $(OBJ_DIR)/src/lps.o | $(BIN_DIR)
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
