# HEIMWATT Development Workflow

Git conventions, CI pipeline, and documentation standards for the team.

---

## Git Workflow

### Branch Naming

| Type | Format | Example |
|------|--------|---------|
| Feature | `feature/<short-name>` | `feature/weather-parser` |
| Bugfix | `fix/<issue-or-name>` | `fix/memory-leak-cache` |
| Hotfix | `hotfix/<name>` | `hotfix/critical-crash` |
| Refactor | `refactor/<name>` | `refactor/fetcher-interface` |

### Commit Messages

```
<type>: <short description>

[optional body]

[optional footer]
```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

**Examples**:
```
feat: add SMHI weather fetcher

fix: prevent double-free in cache cleanup

refactor: extract parser router from main

docs: add Doxygen comments to public API

test: add unit tests for price calculation
```

### Branch Rules

- `main`: **Solid / Production**. The strict, "really solid" branch. Always ready for release. Protected; requires PR + CI pass.
- `dev`: **Development**. Supposed to work, but allowed to be "sloppier" than main. Heavy development integration. Protected.
- Feature branches: **Volatile / "Super Soft"**. Experimental workspace. Merge via PR (squash commits) to `dev`.
- Delete branch after merge

---

## CI Pipeline

Runs on every pull request. All checks must pass before merge.

### Stage 1: Format & Lint

```bash
# Check formatting (fails if unformatted)
clang-format --dry-run --Werror src/*/*.c src/*/*.h

# Static analysis
clang-tidy src/*/*.c -- -I./include
```

### Stage 2: Build

```bash
# Debug build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

### Stage 3: Test

```bash
# Unit tests
ctest --test-dir build-debug --output-on-failure

# ASAN test run
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan
ctest --test-dir build-asan
```

### Stage 4: Documentation

See `coding_standards.md` for documentation guidelines.

### CI Summary

| Stage | Tool | Fail Condition |
|-------|------|----------------|
| Format | clang-format | Any unformatted file |
| Lint | clang-tidy | Any warning |
| Build | CMake | Compile error |
| Test | ctest | Any test failure |
| ASAN | ctest + ASAN | Memory error detected |

---
