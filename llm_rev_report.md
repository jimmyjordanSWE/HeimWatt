# Documentation & Architecture Review

> **Scope**: High-level documentation, organization, and architectural coherence  
> **Method**: Per llm_review workflow — teleological analysis, single source of truth

---

## Executive Summary

**Verdict**: The architecture is **sound** and well-documented. The main issue is **documentation sprawl** — good content scattered across too many files.

| Area | Status |
|------|--------|
| Architecture Design | ✅ Coherent, modular, well-reasoned |
| Module APIs | ✅ Complete, consistent naming |
| README | ✅ Accurate, up-to-date |
| Documentation Organization | ⚠️ Fragmented, needs consolidation |

---

## Architectural Analysis

### What's Working

1. **Clear Separation of Concerns**
   - Core is a pure broker (no domain logic)
   - Plugins handle all external I/O and business logic
   - Wrapper pattern for swappable backends

2. **Consistent API Design**
   - Symmetric lifecycle pairs (`create`/`destroy`, `init`/`fini`)
   - Opaque pointer pattern used correctly
   - `const` correctness on inputs

3. **Well-Defined Data Flow**
   - IN plugins → Core (REPORT)
   - Core → OUT plugins (HTTP_REQUEST)
   - IPC protocol documented with message schemas

### Minor Code Observations

| File | Issue | Severity |
|------|-------|----------|
| `include/` vs `src/core/` | Two `config.h` files with different APIs | Low — intentional (old vs new) |
| `src/server.c` | Legacy monolith, will be replaced by modular Core | Low — expected |
| `src/pipeline.c` | Uses curl directly, should use `http_client.h` in future | Low — legacy |

**These are not violations** — you're in a transition from legacy to modular architecture.

---

## Documentation Audit

### Current File Tree (20 markdown files)

```
Uppgift/           ← Swedish assignment docs (external to project)
├── Planering.md
└── specc.md

docs/
├── cheat_sheets/           ← API quick references
│   ├── api_open_meteo.md
│   ├── api_prices.md
│   ├── api_smhi.md
│   ├── backend_swapping.md
│   └── lib_reference.md
├── design/                 ← Architecture & module specs
│   ├── architecture.md
│   └── modules/
│       ├── core/design.md
│       ├── db/design.md
│       ├── net/design.md
│       ├── plugins/
│       │   ├── design.md
│       │   ├── generated_semantic_types.md
│       │   └── semantic_types_reference.md
│       └── sdk/design.md
├── coding_standards.md     ← Project standards (excellent)
├── dev_workflow.md         ← Git/CI workflow
├── hardware_deploy_options.md  ← Deployment planning
└── tech_stack.md           ← Libraries, tools

readme.md                   ← Project entry point (good)
```

---

## Problems Identified

### 1. **Fragmented Reference Material**

Files like `tech_stack.md`, `hardware_deploy_options.md`, and `dev_workflow.md` are isolated and hard to discover.

### 2. **`Uppgift/` Pollutes Project**

Swedish assignment documents (`Planering.md`, `specc.md`) are project-external but live in the repo. They should be:
- Moved to a personal notes folder, OR
- Added to `.gitignore`

### 3. **No Single Entry Point for Contributors**

A new developer has to guess where to start. There's no `docs/README.md` or `CONTRIBUTING.md`.

### 4. **Redundant `config.h`**

Two config headers exist:
- `include/config.h` — legacy (simple CLI parsing)
- `src/core/config.h` — new (full config struct)

This is fine for now but should be reconciled when Core is implemented.

---

## Recommended Reorganization

### New Structure

```
docs/
├── README.md               ← NEW: Documentation index
├── CONTRIBUTING.md         ← NEW: How to contribute
│
├── architecture/           ← Renamed from design/
│   ├── overview.md         ← Renamed from architecture.md
│   └── modules/            ← Keep as-is
│
├── standards/              ← NEW: Group standards together
│   ├── coding.md           ← Renamed from coding_standards.md
│   └── workflow.md         ← Renamed from dev_workflow.md
│
├── reference/              ← NEW: Quick-reference material
│   ├── tech_stack.md
│   ├── hardware.md         ← Renamed from hardware_deploy_options.md
│   ├── backend_swapping.md ← Moved from cheat_sheets/
│   └── external_apis/      ← Moved from cheat_sheets/
│       ├── smhi.md
│       ├── open_meteo.md
│       └── prices.md
│
└── plugins/                ← Plugin-specific docs
    └── semantic_types.md   ← Consolidate the semantic type docs
```

### Key Changes

| Change | Rationale |
|--------|-----------|
| Add `docs/README.md` | Single entry point for documentation |
| Rename `design/` → `architecture/` | Clearer purpose |
| Create `standards/` | Group coding + workflow standards |
| Create `reference/` | Consolidate cheat sheets + tech stack |
| Move `Uppgift/` out | Not project documentation |
| Consolidate semantic types | 3 files → 1 file |

---

## Professional Documentation Practices

### How Production Projects Handle This

1. **Single Source of Truth**
   - Architecture decisions in **ADRs** (Architecture Decision Records)
   - One file per decision: `docs/adr/001-plugin-ipc.md`
   
2. **Documentation Hierarchy**
   ```
   README.md           → "What is this?"
   docs/README.md      → "How do I learn more?"
   docs/architecture/  → "How does it work?"
   docs/reference/     → "Quick lookups"
   CONTRIBUTING.md     → "How do I contribute?"
   ```

3. **Living Documents**
   - `architecture.md` has version and status at top ✅ (you do this)
   - Mark sections as `[DRAFT]`, `[STABLE]`, `[DEPRECATED]`

4. **Automation**
   - Generate API docs from Doxygen comments
   - Link to generated docs from markdown

---

## Immediate Actions

### High Priority

1. **Create `docs/README.md`** — Index of all documentation
2. **Move `Uppgift/` to `.notes/`** — Keep it tracked but separate
3. **Consolidate semantic type docs** — 3 files → 1

### Medium Priority

4. Rename folders per new structure
5. Add `CONTRIBUTING.md`

### Low Priority

6. Set up ADR format for future decisions
7. Add Doxygen generation to Makefile

---

## Conclusion

**Architecture**: ✅ Sound, coherent, well-designed wrapper pattern.

**Code**: ✅ Legacy code exists but is being properly replaced by modular design.

**Documentation**: ⚠️ Good content, poor organization. Fix with consolidation.

> *"The architecture exhibits structural integrity. The documentation exhibits entropy.  
> The former is ready for implementation; the latter needs housekeeping."*
