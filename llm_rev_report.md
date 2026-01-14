# HEIMWATT LLM Code Review Report

## Summary
The codebase generally follows the requested architecture but exhibits several violations of the project's specific coding standards (`docs/standards/coding.md`). The most significant issues are related to naming conventions, lifecycle naming mismacks, and resource logic (error return codes). Additionally, a major architectural gap exists where the public SDK API is defined but lacks implementation in the `src/` directory.

## Semantic & Architectural Violations

### 1. Naming Convention Violations
- **File**: [heimwatt_sdk.h](file:///home/jimmy/HeimWatt/include/heimwatt_sdk.h)
- **Violation**: Use of `_t` suffix for `sdk_metric_t` and `sdk_data_point_t`.
- **Standards Reference**: `docs/standards/coding.md` Section 2 ("Naming & Style"): `// Structs: lowercase_with_underscores (No _t suffix)`.

### 2. Lifecycle Naming Mismatch
- **File**: [heimwatt_sdk.h](file:///home/jimmy/HeimWatt/include/heimwatt_sdk.h)
- **Violation**: `sdk_init` and `sdk_fini` are used for heap allocation/deallocation of the context.
- **Standards Reference**: `docs/standards/coding.md` Section 2 ("Lifecycle Naming Pairs"): `create/destroy` should be used for object allocation/deallocation. `init/fini` is reserved for in-place initialization.

### 3. Resource Logic (Error Codes)
- **Files**: 
    - [server.c](file:///home/jimmy/HeimWatt/src/server.c) (`server_init`)
    - [config.c](file:///home/jimmy/HeimWatt/src/config.c) (`config_parse_args`)
- **Violation**: Functions return `-1` on failure.
- **Standards Reference**: `docs/standards/coding.md` Section 4 ("Return Value Conventions"): `FAILURE: Negative errno value (e.g., -EINVAL, -ENOMEM)`.

### 4. Architectural Integrity (Missing Implementation)
- **Violation**: The public API defined in `include/heimwatt_sdk.h` has no corresponding implementation in `src/`. The `src/sdk` directory contains only internal headers.
- **Analysis**: This violates the principle of **Structural Integrity**. An API that earns its existence in public headers must be backed by an implementation that serves the stated purpose.

### 5. Type Consistency
- **Violation**: Multiple modules define their own log levels or duplicate concept names.
- **Analysis**: While `heimwatt_sdk.h` isolation is good, the naming inconsistency with `types.h`'s `log_level` creates accidental complexity for developers using both or moving between layers.

## Conclusion
The core server implementation (`server.c`, `lps.c`) is robust and correctly uses patterns like `goto cleanup`. However, the SDK layer needs a naming and lifecycle refactor to adhere to the project's standards. The missing implementation of the SDK is a critical blocking item for the platform's extensible goal.
