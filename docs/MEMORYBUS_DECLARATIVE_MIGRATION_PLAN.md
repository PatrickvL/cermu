# MemoryBus Declarative Configuration Migration Plan

## Goal

Move cermu to a compact, declaration-driven system configuration model while preserving the fastest possible runtime access paths in the emulation hot loop.

This plan treats MemoryBus and Board as a two-layer architecture:

1. Declaration/Resolve layer (startup or reconfiguration only)
2. Runtime/Banking layer (hot path, pre-resolved tables only)

The guiding rule is strict:

- No runtime slowdown in read/write/resolve/service/tick hot paths.
- Any flexibility cost is paid during declaration parsing and resolve compilation.

---

## Non-Goals

- No replacement of current cycle-level tick sequencing.
- No dynamic schema interpretation during CPU/video ticks.
- No additional virtual dispatch in hot MemoryBus paths.
- No mandatory migration of every system in one step.

---

## Current State Summary

Existing code already provides most required primitives:

- `ChipManifest` for static slot declaration.
- `BusMap<Spec>` as a resolve-like layer that programs tables.
- `MemoryBus<Spec>` as the runtime dispatch engine with page tables, sentinels, sub-tables, MMIO handlers, and snapshots.
- `ChipRegistry` for runtime string-to-factory resolution (external configuration bridge).

Main gaps to close:

- Declaration data is split between `CoreChips` and manifest slot lists.
- Resolve phase is implicit inside `apply()` and not explicitly represented as an artifact.
- Signal wiring and memory-view declarations are not first-class declarative entities.
- `bus.hpp` contains duplicated read/write helper logic that can be deduplicated while preserving inlining.

---

## Target Architecture

### Phase Boundaries

1. Declaration phase
- Build a compact `BoardDeclaration` from C++ constexpr data or external TOML/LJON.
- Declare slots, role tags, signal bindings, and view modes.

2. Resolve phase
- Compile declaration into a `ResolvedBoardPlan`.
- Validate all references and constraints once.
- Precompute page-table mappings, sub-table definitions, handler indices, and snapshots.

3. Runtime phase
- Install `ResolvedBoardPlan` into `MemoryBus`.
- Runtime tick paths perform only fixed table lookups and pre-bound callback calls.

4. Banking phase
- Switch views by loading precomputed snapshots and changing pre-resolved mode selectors.
- Avoid rebuilding decode structures during emulation.

### Canonical Source Rule

Use a single canonical declaration model for chip inventory and mapping. Typed convenience accessors are generated from that model, rather than duplicated manually in `CoreChips` and manifest definitions.

---

## Migration Phases

## Phase 0 (Immediate): Maximal `bus.hpp` Dedup With Forced Inlining

Purpose: Reduce maintenance duplication in the hot path while retaining generated code quality.

### Scope

Refactor duplicated read/write code blocks into small `FORCE_INLINE` helpers with compile-time specialization.

Candidate duplicate areas:

- `read_flat_mem()` vs `read_buffer_no_cs()`
- `write_flat_mem()` vs `write_buffer_no_cs()`
- CS side-effect variants
- Partial-bus mask handling branches

### Refactor Pattern

Use template boolean parameters so the compiler emits separate optimized versions without runtime branches:

- `read_buffer_impl<SetCs>()`
- `write_buffer_impl<SetCs>()`
- `read_sentinel_impl<HasSubTables, HasMmio>()` only if useful after assembly checks

Rules:

- Keep public API unchanged (`read`, `write`, `resolve`, `service_*`, `tick`).
- Keep storage layout unchanged.
- Keep sentinel semantics unchanged.
- Prefer helper extraction only where object code remains equal or better.

### Validation and Acceptance Criteria

1. Functional parity
- Existing test suites pass.
- No behavior changes in MMIO, sub-table resolution, open-bus behavior, or partial bus masks.

2. Performance parity or win
- Compare before/after on representative systems (at least C64-like and a masked-subtable-heavy system).
- No regression in cycles per emulated frame under release build.

3. Code quality
- Reduced duplicated logic blocks in `bus.hpp`.
- Helper names map to hardware intent (buffer read/write, sentinel resolution, MMIO dispatch).

---

## Phase 1: Introduce Explicit Declaration Types

Purpose: Create a compact declaration model that can be authored in C++ and externally.

### New Core Types

- `BoardDeclaration`
- `SlotDeclaration`
- `SignalBindingDeclaration`
- `ViewDeclaration`

Minimal required fields:

- Slot identity, chip type key, role tag, address/range/mask, bank properties, overlay group, optional condition.
- Signal mappings (chip input pin groups to bus lines/bitfields).
- Named views or mode bitsets that describe banked visibility.

### Backward-Compatibility Adapter

- Add adapter from existing `ChipManifest` to `BoardDeclaration`.
- Keep existing systems compiling unchanged while new declarations are introduced.

Acceptance:

- `BoardDeclaration` can represent at least one currently migrated system without loss.

---

## Phase 2: Resolve Artifact Extraction

Purpose: Separate declaration parsing/validation from runtime install.

### Add `ResolvedBoardPlan`

A precomputed artifact containing:

- Slot-to-chip-id layout
- Page-map write plans
- MMIO registration plan
- Indexed/masked sub-table plans
- Overlay/view snapshots and mode index metadata

### API Shape

- `resolve_board_plan(const BoardDeclaration&, const ResolveContext&) -> ResolvedBoardPlan`
- `Board::apply_plan(const ResolvedBoardPlan&, MemoryBus<Spec>&, size_t viewer)`

Keep current `apply()` as a compatibility wrapper:

- `apply()` internally builds or retrieves the plan and calls `apply_plan()`.

Acceptance:

- Resolve output is deterministic.
- `apply_plan()` is side-effect equivalent to current `apply()` for migrated systems.

---

## Phase 3: Compact Typed Access Without Duplication

Purpose: Remove `CoreChips`/manifest duplication while preserving ergonomic typed access.

### Approach

- Keep declaration as canonical chip inventory.
- Generate typed handles/accessors by role tag and slot id from declaration metadata.

Examples:

- `board.cpu()`
- `board.video()`
- `board.slot_as<MOS6526>(slot_id)`
- `board.find_role(RoleTag::Sound)`

Avoid manual duplicate chip lists in system headers.

Acceptance:

- At least one system removes duplicated chip inventory declarations.
- No additional runtime lookup in hot paths.

---

## Phase 4: External Configuration Bridge (Optional at Runtime)

Purpose: Enable file-driven declarations with zero runtime hot-loop penalty.

### Flow

1. Parse TOML/LJON into `BoardDeclaration`.
2. Resolve to `ResolvedBoardPlan`.
3. Instantiate chips via `ChipRegistry` factories.
4. Apply plan to bus.

### Caching

Optional resolved-plan cache keyed by declaration hash + build/version signature.

Acceptance:

- Same runtime dispatch path as C++ constexpr declarations.
- External declarations can configure at least one pilot system.

---

## Phase 5: Signal Wiring as First-Class Declarations

Purpose: Make chip/bus signal connections explicit and compact.

### Declarative Coverage

- Address/data line ownership and masks
- IRQ/NMI/READY/clock pin bindings
- Shared lines and wired-AND semantics where required

### Runtime Model

- Resolve signal bindings to compact bit operations and callback hooks.
- No per-access dynamic interpretation.

Acceptance:

- At least one complex system migrates explicit wiring to declarations with no runtime regression.

---

## Phase 6: System Rollout Strategy

Recommended migration order (risk-balanced):

1. VIC-20 or Apple 1 (already close to declarative model)
2. One Z80 + sub-table-heavy system
3. C64 (high-value, includes views/overlays and strict perf requirements)
4. Remaining systems in batches by architecture family

For each migrated system:

- Keep old path behind a compile-time or runtime switch during validation.
- Compare correctness (tests + known ROM boot behavior).
- Compare performance in release builds.

---

## Performance Guardrails

These constraints apply to all phases:

- MemoryBus hot methods (`resolve`, `read`, `write`, `tick`, `service_*`) remain branch-minimal and allocation-free.
- No string lookups in runtime read/write path.
- No variant/virtual dispatch newly introduced inside hot read/write helpers.
- Sub-table and MMIO logic remains pre-registered and table-driven.

Measurement requirements:

- Record baseline and post-change metrics for at least two systems and one synthetic memory stress scenario.
- Track frame time, CPU utilization, and optional instruction/cycle estimates from profiling tools.

---

## Risks and Mitigations

1. Risk: Helper refactor in `bus.hpp` changes codegen
- Mitigation: inspect generated assembly for hot methods before and after; keep fallback branch.

2. Risk: Over-generalized declaration schema slows migration
- Mitigation: start with minimal schema that exactly covers current semantics; extend only after pilot systems.

3. Risk: Signal binding schema mismatch across chip families
- Mitigation: introduce role/pin vocabulary in layers (core required fields + per-family extension blocks).

4. Risk: Resolve-time complexity growth
- Mitigation: keep `ResolvedBoardPlan` POD-like and cacheable; validate once, execute many.

---

## Deliverables Checklist

Phase 0 deliverables:

- `bus.hpp` helper dedup patch with preserved API and verified perf parity.
- Bench notes in commit message or linked perf note.

Phase 1-2 deliverables:

- `BoardDeclaration` and `ResolvedBoardPlan` core headers.
- Adapter from `ChipManifest` to declaration.
- `apply_plan()` path with parity tests.

Phase 3-4 deliverables:

- Typed role accessors generated from declaration metadata.
- External declaration parser bridge and at least one pilot system.

Phase 5-6 deliverables:

- Signal declaration support.
- Multi-system rollout with correctness/perf sign-off per system.

---

## Suggested First Execution Slice (1-2 PRs)

PR 1: `bus.hpp` dedup-and-inline optimization (Phase 0)
- Extract read/write helper templates with compile-time flags for CS side-effects.
- Preserve public API and behavior.
- Add focused tests for sentinel/MMIO/sub-table/partial-mask behavior.

PR 2: Resolve artifact extraction scaffolding (Phase 1-2 start)
- Introduce `BoardDeclaration` and `ResolvedBoardPlan` shells.
- Implement compatibility adapter from existing manifest.
- Route `apply()` through internal `apply_plan()` without behavior changes.

This sequence provides immediate maintenance and clarity gains while keeping runtime speed constraints explicit and testable.
