#pragma once
// =============================================================================
// system_chip_visitors.hpp — Shared visitor macros for system chip declarations
// =============================================================================
//
// These macros are invoked by system-specific FOR_EACH_SYSTEM_CHIP lists to
// generate manifest entries, chipset struct fields, getters, binding logic,
// component registration, and info tables from a single authoritative chip
// row list.
//
// Every visitor receives arguments in this canonical order:
//   1. ctx        — system-specific context (board variable name, etc.)
//   2. type       — chip type (e.g., MOS6510, mos6581_t)
//   3. chip       — field/tag name (e.g., cpu, sid)
//   4. base       — base address (may be 0 for chips with no mapped range)
//   5. size       — size in bytes (0 for chips with no flat-memory buffer)
//   6. overlay    — overlay_group (0 for non-overlaid, 1+ for PLA-switched)
//   7. label      — display label for manifest
//   8. info_label — descriptive label for info tables / PLA debug GUI
//   9. rom_files  — optional ROM filename patterns (nullptr if N/A)
//
// Buffer vs MMIO distinction is implicit:  chips with size > 0 get a flat-
// memory buffer; chips with size == 0 are MMIO-only.  Board::bind_chip()
// auto-calls on_bind_buffer() for buffer chips (MemoryChipBase overrides).
//
// ALL chips are value-typed fields in the chipset struct.  There is no
// factory/pointer distinction at the visitor level.
//
// =============================================================================

// ── Manifest Row Visitor ─────────────────────────────────────────────────────
//
// Emits a ChipSlot{...} initializer for aggregate initialization of
// ChipManifest<N>.  Includes the resolved factory function pointer.
// Trailing comma is safe inside aggregate init.
//
#define CERMU_CHIP_VISITOR_MANIFEST_ROW(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    ChipSlot{base, size, 0, (size > 0 ? size : 0), 0, overlay, resolve_slot_factory<type>(), label, 0, {rom_files}},

// ── Chipset Field Declaration Visitor ────────────────────────────────────────
//
// Declares a value-typed field in the chipset struct.
// Emitted for every chip — all chips are value members.
//
#define CERMU_CHIP_VISITOR_DECLARE_FIELD(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    type chip;

// ── Getter Declaration Visitor (mutable + const) ─────────────────────────────
//
// Returns a reference to the chip field in ctx.chips().
// Both mutable and const overloads are emitted.
//
#define CERMU_CHIP_VISITOR_DECLARE_GETTER(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    type& get_##chip() { return ctx.chips().chip; }                              \
    const type& get_##chip() const { return ctx.chips().chip; }

// ── Sequential Binding Visitor ────────────────────────────────────────────────
//
// Emits a ctx.bind_chip(slot_idx_++, &ctx.chips().chip) call.
// Requires a `size_t slot_idx_ = 0;` variable in scope before expansion.
// Slot order matches manifest array order (both generated from the same macro).
//
#define CERMU_CHIP_VISITOR_BIND_SEQUENTIAL(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    ctx.bind_chip(slot_idx_++, &ctx.chips().chip);

// ── Component Registration Visitor ───────────────────────────────────────────
//
// Emits a ctx.register_component(&ctx.chips().chip) call.
//
#define CERMU_CHIP_VISITOR_REGISTER_COMPONENT(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    ctx.register_component(&ctx.chips().chip);

// ── Info Row Visitor (for chip info tables) ──────────────────────────────────
//
// Emits a row in a chip info lookup table (used for PLA debug GUI and metadata).
//
#define CERMU_CHIP_VISITOR_INFO_ROW(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    {base, size, info_label},

// ── Convenience-Pointer Declaration Visitor ──────────────────────────────────
//
// Declares a convenience pointer (type* chip = nullptr) in the system class.
// These are non-owning pointers into the chipset's value-typed fields.
//
#define CERMU_CHIP_VISITOR_DECLARE_POINTER(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    type* chip = nullptr;

// ── Convenience-Pointer Assignment Visitor ───────────────────────────────────
//
// Assigns each convenience pointer from ctx.chips().chip, where ctx is
// the Board reference.  Expand inside a member function (uses `this`).
//
#define CERMU_CHIP_VISITOR_ASSIGN_POINTER(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    this->chip = &ctx.chips().chip;

// ── Convenience-Pointer Null Visitor ─────────────────────────────────────────
//
// Nulls out each convenience pointer during shutdown.
//
#define CERMU_CHIP_VISITOR_NULL_POINTER(ctx, type, chip, base, size, overlay, label, info_label, rom_files) \
    this->chip = nullptr;

// ── Count Visitor ────────────────────────────────────────────────────────────
//
// Expands to +1 for each chip.  Use as: `0 FOREACH(COUNT_ONE, unused)`
//
#define CERMU_CHIP_VISITOR_COUNT_ONE(ctx, type, chip, base, size, overlay, label, info_label, rom_files) +1
