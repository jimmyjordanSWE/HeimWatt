# Contributing to HeimWatt

Thank you for your interest in contributing! This guide covers everything you need to know.

---

## Quick Start

```bash
# Clone and build
git clone <repo-url>
cd heimwatt
make

# Run tests
make test

# Check your code
make format lint
```

---

## Development Setup

### Prerequisites

- **Clang** (compiler + tools)
- **libcurl** (system library)
- **Python 3** (for analysis scripts)

```bash
# Ubuntu/Debian
sudo apt install clang clang-format clang-tidy libcurl4-openssl-dev python3

# Arch
sudo pacman -S clang curl python
```

### Build Targets

| Command | Description |
|---------|-------------|
| `make` | Debug build with ASAN |
| `make release` | Optimized release build |
| `make test` | Run unit tests |
| `make format` | Auto-format code |
| `make lint` | Run clang-tidy |
| `make docs` | Generate Doxygen docs |
| `make analyze` | Run analysis scripts |

---

## Code Style

All code must pass `clang-format` and `clang-tidy` before merge.

**Full standards**: [docs/standards/coding.md](docs/standards/coding.md)

### Key Points

1. **C99** standard, POSIX compliant
2. **Naming**: `module_verb_noun()`, lowercase with underscores
3. **Lifecycle pairs**: `create`/`destroy`, `init`/`fini`, `open`/`close`
4. **Error handling**: Return negative errno (`-EINVAL`, `-ENOMEM`)
5. **Memory**: Use `goto cleanup` pattern for multi-resource functions

---

## Git Workflow

### Branches

| Branch | Purpose |
|--------|---------|
| `main` | Stable, production-ready |
| `dev` | Development integration |
| `feature/*` | New features |
| `fix/*` | Bug fixes |

### Commit Messages

```
<type>: <short description>

[optional body]
```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`

**Examples**:
```
feat: add SMHI weather plugin
fix: prevent double-free in cache cleanup
docs: add API documentation for data_store
```

### Pull Request Process

1. Branch from `dev`
2. Make changes, ensure `make format lint test` passes
3. Open PR to `dev`
4. Get review, address feedback
5. Squash merge

---

## Documentation

### Where to Write Docs

| Type | Location |
|------|----------|
| Architecture/Design | `docs/architecture/` |
| API reference | Doxygen comments in headers |
| Quick references | `docs/reference/` |
| Decisions | `docs/adr/` (ADR format) |

### Doxygen Comments

Public APIs in header files use Doxygen format:

```c
/**
 * @brief Load configuration from file.
 *
 * @param path   Path to JSON config file.
 * @param out    Output pointer for config object.
 * @return 0 on success, negative errno on failure.
 */
int config_load(const char *path, config **out);
```

### Generating Docs

```bash
# Generate HTML documentation
make docs

# View in browser
open build/docs/html/index.html
```

### Architecture Decision Records (ADRs)

For significant architectural decisions, create an ADR:

```
docs/adr/NNN-short-title.md
```

**Template**:
```markdown
# ADR-NNN: Title

## Status
Accepted | Proposed | Deprecated

## Context
Why is this decision needed?

## Decision
What did we decide?

## Consequences
What are the trade-offs?
```

---

## Testing

### Unit Tests

Tests live in `tests/` with naming `test_<module>.c`.

```bash
# Run all tests
make test

# Run with verbose output
./build/bin/test_lps
```

### Memory Checking

All tests run with AddressSanitizer (ASAN) by default in debug builds.

```bash
# Manual Valgrind check
valgrind --leak-check=full ./build/bin/test_lps
```

---

## Automation

### Analysis Scripts

Located in `scripts/`, these generate useful analysis files:

| Script | Output | Purpose |
|--------|--------|---------|
| `structure.py` | `scripts/out/structure.txt` | API surface map |
| `call_chains.py` | `scripts/out/call_chains.txt` | Function call graph |
| `token_count.py` | `scripts/out/token_count.txt` | Code size metrics |

Run all with:
```bash
make analyze
```

### CI Pipeline

Every PR runs:

1. **Format check** — `clang-format`
2. **Lint** — `clang-tidy`
3. **Build** — Debug + Release
4. **Test** — Unit tests with ASAN

---

## Questions?

- Check [docs/README.md](docs/README.md) for documentation index
- Review [docs/architecture/overview.md](docs/architecture/overview.md) for system design
- Open an issue for questions
