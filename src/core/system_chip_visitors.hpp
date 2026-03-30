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
//   1.  ctx        — system-specific context (board variable name, etc.)
//   2.  type       — chip type (e.g., MOS6510, mos6581_t)
//   3.  chip       — field/tag name (e.g., cpu, sid)
//   4.  base       — base address (may be 0 for chips with no mapped range)
//   5.  size       — flat-memory buffer size in bytes (e.g. 0x10000 for 64 KB).
//                    Non-zero for memory chips (RAMChip, ROMChip, MOS2114, …).
//                    Zero for MMIO-only chips (CPUs, sound, I/O controllers).
//   6.  mask       — address decode mask.
//                    For memory chips this is normally 0, meaning "use size-1".
//                    For MMIO-only chips: register address mask
//                    (e.g. 0x3F → 64 register addresses).
//   7.  overlay    — overlay_group (0 for non-overlaid, 1+ for PLA-switched)
//   8.  label      — display label (manifest, PLA debug GUI, chip display name)
//   9.  rom_files  — optional ROM filename patterns (nullptr if N/A)
//
// Buffer vs MMIO distinction is explicit via the size column:
//   size > 0  → buffer chip (flat-memory allocation)
//   size == 0 → MMIO-only (register handlers)
// Board::bind_chip() auto-calls on_bind_buffer() for buffer chips.
//
// ALL chips are value-typed fields in the chipset struct.  There is no
// factory/pointer distinction at the visitor level.
//
// =============================================================================

// Helper: strip leading '?' from optional ROM filename strings.
// Returns {filenames, optional} with the prefix removed.
inline constexpr RomFileInfo parse_rom_spec(const char* spec) noexcept {
    if (!spec) return {nullptr, false};
    if (spec[0] == '?') return {spec + 1, true};
    return {spec, false};
}

// ── Manifest Row Visitor ─────────────────────────────────────────────────────
//
// Emits a ChipSlot{...} initializer for aggregate initialization of
// ChipManifest<N>.  Includes the resolved factory function pointer.
// Trailing comma is safe inside aggregate init.
//
// Buffer/MMIO routing is explicit via the size column:
//   size > 0  → size_bytes = size,  addr_mask = mask (0 means "use size-1")
//   size == 0 → size_bytes = 0,     addr_mask = mask
//
#define CERMU_CHIP_VISITOR_MANIFEST_ROW(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ChipSlot{base, (size_t)(size), (uint32_t)(mask),                                                  \
             0, 0, overlay, resolve_slot_factory<type>(), label, 0, parse_rom_spec(rom_files)},

// ── Chipset Field Declaration Visitor ────────────────────────────────────────
//
// Declares a value-typed field in the chipset struct.
// Emitted for every chip — all chips are value members.
//
#define CERMU_CHIP_VISITOR_DECLARE_FIELD(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    type chip;

// ── Sequential Binding Visitor ────────────────────────────────────────────────
//
// Emits a ctx.bind_chip(slot_idx_++, &ctx.chip) call.
// Requires a `size_t slot_idx_ = 0;` variable in scope before expansion.
// Slot order matches manifest array order (both generated from the same macro).
//
#define CERMU_CHIP_VISITOR_BIND_SEQUENTIAL(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ctx.bind_chip(slot_idx_++, &ctx.chip);

// ── Component Registration Visitor ───────────────────────────────────────────
//
// Emits a ctx.register_component(&ctx.chip) call.
//
#define CERMU_CHIP_VISITOR_REGISTER_COMPONENT(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ctx.register_component(&ctx.chip);

// ── Info Row Visitor (for chip info tables) ──────────────────────────────────
//
// Emits a row in a chip info lookup table (used for PLA debug GUI and metadata).
// Uses size directly when non-zero; falls back to mask+1 for MMIO chips.
//
#define CERMU_CHIP_VISITOR_INFO_ROW(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    {base, (size) > 0 ? (size_t)(size) : (size_t)(mask) + 1, label},

// ── Convenience-Pointer Declaration Visitor ──────────────────────────────────
//
// Declares a convenience pointer (type* chip = nullptr) in the system class.
// These are non-owning pointers into the chipset's value-typed fields.
//
#define CERMU_CHIP_VISITOR_DECLARE_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    type* chip = nullptr;

// ── Convenience-Pointer Assignment Visitor ───────────────────────────────────
//
// Assigns each convenience pointer from ctx.chip, where ctx is
// the Board reference.  Expand inside a member function (uses `this`).
//
#define CERMU_CHIP_VISITOR_ASSIGN_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    this->chip = &ctx.chip;

// ── Convenience-Pointer Null Visitor ─────────────────────────────────────────
//
// Nulls out each convenience pointer during shutdown.
//
#define CERMU_CHIP_VISITOR_NULL_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    this->chip = nullptr;

// ── Enum Value Visitor ────────────────────────────────────────────────────────
//
// Emits `chip,` — use inside an enum class body to auto-declare one value
// per chip with the same name as the chip field.  The resulting enum values
// are sequential and match the manifest slot indices.
//
#define CERMU_CHIP_VISITOR_ENUM_VALUE(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    chip,

// ── Count Visitor ────────────────────────────────────────────────────────────
//
// Expands to +1 for each chip.  Use as: `0 FOREACH(COUNT_ONE, unused)`
//
#define CERMU_CHIP_VISITOR_COUNT_ONE(ctx, type, chip, base, size, mask, overlay, label, rom_files) +1
