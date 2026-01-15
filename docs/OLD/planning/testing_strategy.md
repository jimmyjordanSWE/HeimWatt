# Testing & Fuzzing Strategy

> **Status**: Planning (decisions pending)  
> **Priority**: Before first stable release

---

## Overview

This document outlines the decisions we need to make for testing infrastructure.

---

## 1. Unit Testing Library

### Options

| Library | Pros | Cons |
|---------|------|------|
| **µnit (munit)** | Single header, TAP output, setup/teardown, parameterized tests | Less known |
| **Unity** | Very popular, CMock integration, embedded focus | Multiple files |
| **Check** | Fork isolation (each test in subprocess), mature | Heavier dependency |
| **cmocka** | Good mocking, Google uses it | Slightly more complex |
| **Greatest** | Single header, tiny, simple | Minimal features |
| **Criterion** | Modern, parallel, BDD style | Larger dependency |

### Current State
- Using custom test runner in `tests/test_lps.c`
- No framework dependency yet

### Decision Needed
- [ ] Choose primary unit test framework
- [ ] Decide on mocking approach (manual stubs vs framework)

---

## 2. Testing Strategy by Component

### Core Broker

| Area | Strategy |
|------|----------|
| Config parsing | Unit tests with valid/invalid JSON |
| Plugin manager | Integration tests (fork plugins, check IPC) |
| Data store | Unit tests against in-memory SQLite |
| Router | Unit tests for path matching |
| IPC protocol | Unit tests for message parsing |

### IN Plugins

| Area | Strategy |
|------|----------|
| API parsing | Unit tests with recorded API responses |
| Error handling | Test with malformed responses |
| Rate limiting | Mock timer, verify backoff |

### OUT Plugins

| Area | Strategy |
|------|----------|
| Query handling | Unit tests with mock data store |
| Endpoint handlers | Unit tests with mock request/response |
| Algorithm correctness | Property-based tests where applicable |

### Decision Needed
- [ ] How to mock external APIs (recorded responses vs fake server)
- [ ] Integration test harness design
- [ ] CI test matrix (what runs on every PR vs nightly)

---

## 3. Fuzzing Strategy

### Tools

| Tool | Description | Use Case |
|------|-------------|----------|
| **AFL++** | Coverage-guided fuzzer, industry standard | HTTP parsing, JSON parsing, IPC messages |
| **libFuzzer** | LLVM-based, in-process | Fast, good for parsing functions |
| **Honggfuzz** | Multi-threaded, crash dedup | Alternative to AFL++ |

### Targets to Fuzz

| Target | Priority | Notes |
|--------|----------|-------|
| HTTP request parser | High | Untrusted input from network |
| JSON parser | High | Plugin manifests, IPC messages |
| IPC message parser | High | Plugins are semi-trusted |
| Config parser | Medium | User-supplied config |
| Plugin manifest loader | Medium | From filesystem |

### Workflow Options

**Option A: Dedicated Fuzz Targets**
```
tests/fuzz/
├── fuzz_http_parser.c
├── fuzz_json_parser.c
└── fuzz_ipc_message.c
```
Each compiles to a standalone binary, run with AFL++/libFuzzer.

**Option B: Integrated Fuzz Mode**
```bash
make fuzz TARGET=http_parser
```
Single build system handles instrumentation.

### Decision Needed
- [ ] Choose primary fuzzer (AFL++ vs libFuzzer)
- [ ] Set up corpus directory structure
- [ ] Define minimum fuzzing duration for CI
- [ ] Crash triage workflow

---

## 4. Coverage

### Tools

| Tool | Notes |
|------|-------|
| **gcov/lcov** | Standard GCC coverage |
| **llvm-cov** | Clang coverage, nicer reports |

### Targets

- Overall: >80% line coverage
- Parsers: >95% branch coverage (security critical)

---

## 5. CI Integration

### Proposed Pipeline

```
PR Opened
    ├── Format check
    ├── Lint (clang-tidy)
    ├── Build (debug + release)
    ├── Unit tests (with ASAN)
    └── Coverage report

Nightly
    ├── Full test suite
    ├── Fuzz run (1 hour per target)
    └── Valgrind memcheck
```

---

## Next Steps

1. Choose unit test library
2. Write first batch of Core unit tests
3. Set up AFL++ with HTTP parser target
4. Add coverage reporting to CI
