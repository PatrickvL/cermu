/*
 * chip_debug_registry.h — Semantic debug field registry for emulated chips
 *
 * Chips register WHAT their data is (semantic kind), not how to display it.
 * The renderer decides presentation based on the data kind, and can evolve
 * independently — e.g. when the renderer learns to draw color swatches,
 * every chip that registered a Color field benefits automatically.
 *
 * Design principles:
 *   1. Registrations describe data semantics, never visual presentation.
 *   2. Renderers dispatch on DataKind and upgrade independently.
 *   3. The escape hatch (Custom callback) keeps nothing blocked.
 *   4. Composite kinds (Port, Timer, AudioChannel) carry structured
 *      metadata so the renderer can present them as rich widgets.
 *   5. All string literals must have static storage duration.
 */

#pragma once

#include "core/cermu.h"          // CERMU_HAS_VA_OPT, CERMU_PP_OVERLOAD_

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

class ChipBase;  // forward — callbacks receive the owning chip at render time

// ============================================================================
// SEMANTIC DATA KINDS — what the field IS, NOT how to draw it
// ============================================================================
//
// Rules for adding kinds:
//   - A new kind may be added at any time without changing registrations.
//   - Kinds without a renderer handler fall back to a generic text display.
//   - Never add a kind that describes presentation (no "ProgressBar" kind).

enum class DataKind : uint8_t {
    // ---- Atomic scalar values ----
    Value,              // Generic numeric value ($XX, decimal, etc.)
    Flag,               // Boolean on/off state
    State,              // Enumerated named state from a fixed set
    Address,            // Memory address (may become clickable)
    Counter,            // Value within a known range (enables range viz)
    Level,              // Normalized 0.0–1.0 (volume, amplitude, duty)
    Color,              // Color value (index or direct RGB) — swatch-ready
    Frequency,          // Frequency in some unit (Hz, period reload)
    SignedValue,        // Signed integer (e.g. motion registers)

    // ---- Structured / composite data ----
    Bitfield,           // Per-bit labeled breakdown of a byte
    Port,               // I/O port: data + direction per pin
    Timer,              // Counter + latch + running + mode
    AudioChannel,       // Freq + vol + waveform + enable
    Palette,            // Array of color entries
    Memory,             // Byte array for inspection (RAM, register banks)
    PatternTile,        // Sprite/character tile pixel data
    WaveformBuffer,     // Ring buffer of audio samples
    RasterPosition,     // Scanline + cycle within frame bounds
    FlagString,         // Processor status flags as string (NVUBDIZc)

    // ---- Escape hatch ----
    Custom,             // Chip-provided rendering callback
};

// ============================================================================
// REGISTER ENTRY — label (from X-macro #symbol) + description string
// ============================================================================

struct RegEntry {
    const char* label;                      // Short technical label (stringified symbol name)
    const char* desc;                       // Human-readable description
    DataKind    kind = DataKind::Value;     // Register-level semantic kind (for pre-masked regs)
    uint8_t     value_bits = 0;             // Meaningful bits (0 = full register width)
};

// ============================================================================
// CHIP REGISTER TRAITS — per-chip register bank metadata (NTTP)
// ============================================================================
//
// Each chip declares one constexpr ChipRegTraits instance.  This struct is
// used as an NTTP (non-type template parameter) to derive types and generate
// infrastructure with zero per-chip boilerplate.
//
// Example:
//   inline constexpr ChipRegTraits vicii_reg_traits = {
//       .num_registers  = 66,
//       .register_width = 1,
//       .base_address   = 0xD000,
//   };

struct ChipRegTraits {
    uint16_t num_registers  = 0;     // Total register count
    uint8_t  register_width = 1;     // Bytes per register (1, 2, 4)
    uint16_t base_address   = 0;     // I/O base address for debug display
};

// Derive the offset type from the register count — chips with ≤256 registers
// use uint8_t (zero overhead for 8-bit era chips), larger chips get uint16_t
// or uint32_t automatically.
template<const ChipRegTraits& T>
using reg_offset_t = std::conditional_t<(T.num_registers <= 256), uint8_t,
                     std::conditional_t<(T.num_registers <= 65536), uint16_t,
                     uint32_t>>;

// ============================================================================
// BITFIELD ACCESSOR MACROS — NV-style hi:lo ternary trick
// ============================================================================
//
// The token "7:0" is NOT valid C++ alone, but IS valid in a ternary:
//   (1 ? 7:0) = 7 (hi)    (0 ? 7:0) = 0 (lo)
// This lets a single hi:lo token produce both bounds at compile time.
// Used by FLD X-macro extractors to turn field declarations into constants.

#define BF_HI(hilo)     (1 ? hilo)
#define BF_LO(hilo)     (0 ? hilo)
#define BF_WIDTH(hilo)  (BF_HI(hilo) - BF_LO(hilo) + 1)
#define BF_MASK(hilo)   (((1u << BF_WIDTH(hilo)) - 1u) << BF_LO(hilo))

// Extract bitfield from a register value
#define BF_GET(val, hilo) \
    (((val) & BF_MASK(hilo)) >> BF_LO(hilo))

// Set bitfield in a register value (returns modified value)
#define BF_SET(val, hilo, fval) \
    (((val) & ~BF_MASK(hilo)) | (((fval) << BF_LO(hilo)) & BF_MASK(hilo)))

// ---- DECL row-type swallower macros ----
// Use these as no-op callbacks when extracting specific row types from a
// unified CHIP_DECL(REG, FLD, CMP) table.  Each swallows its row's args.
//   REG(offset, symbol, description [, kind, hi:lo])  — optional kind+bits
//   FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
//   CMP(symbol, description, kind, total_bits, display_shift, display_scale, reg1, hilo1, dst1, reg2, hilo2, dst2)
#define DECL_REG_NOP(a, s, d, ...)
#define DECL_FLD_NOP(r, f, hilo, d, k, ds, dm)
#define DECL_CMP_NOP(s, d, k, b, ds, dm, r1, h1, d1, r2, h2, d2)

// ---- Shared DECL extraction callbacks ----
// Pre-defined callbacks for X-macro walks.  Each chip invokes these
// directly instead of defining per-chip variants.

// Helper macros to split the optional kind+hilo suffix of a REG row.
#define REG_KIND_(kind, hilo) kind
#define REG_HILO_(kind, hilo) hilo

// Register constant extractor — use inside a namespace block
#define DECL_X_CONST_(a, s, l, ...)  constexpr uint8_t s = a;

// Register info extractor — produces RegEntry initializers.
// Optional trailing args (kind, hi:lo) populate the kind and value_bits
// fields; plain REG rows get the defaults (Value, 0).
#ifdef CERMU_HAS_VA_OPT
// GCC, Clang, MSVC /Zc:preprocessor — use __VA_OPT__.
#define DECL_X_REG_INFO_(a, s, l, ...)  \
    { #s, l __VA_OPT__(, DataKind::REG_KIND_(__VA_ARGS__), BF_WIDTH(REG_HILO_(__VA_ARGS__))) },
#else
// MSVC traditional preprocessor — arg-count dispatch via CERMU_PP_OVERLOAD_.
#define DECL_X_REG_INFO_3(a, s, l)          { #s, l },
#define DECL_X_REG_INFO_5(a, s, l, k, hilo) { #s, l, DataKind::k, BF_WIDTH(hilo) },
#define DECL_X_REG_INFO_(...)  CERMU_PP_OVERLOAD_(DECL_X_REG_INFO_, __VA_ARGS__)
#endif

// Field info extractor — produces FieldEntry initializers with reg_index=0
// (resolved later by assign_field_reg_indices from the DECL order)
#define DECL_X_FLD_INFO_(reg, fld, hilo, desc, kind, ds, dm) \
    { #fld, desc, 0, BF_LO(hilo), BF_WIDTH(hilo), DataKind::kind, (uint8_t)(ds), (uint16_t)(dm) },

// Declaration order extractors
#define DECL_X_ORD_REG_(a, s, l, ...)                                 { DeclRowType::Reg, (uint16_t)(a) },
#define DECL_X_ORD_FLD_(r, s, hilo, d, k, ds, dm)                    { DeclRowType::Field, 0 },
#define DECL_X_ORD_CMP_(s, d, k, b, ds, dm, r1, h1, d1, r2, h2, d2)  { DeclRowType::Compound, 0 },

// ============================================================================
// FIELD ENTRY — bitfield within a register (from FLD X-macro)
// ============================================================================
//
// Produced by a FLD extractor macro.  Describes a named bit range inside a
// single register, with a semantic DataKind for the debug renderer.
//
// Example FLD row:  FLD(CONTROL1, INTERLACE, 7:7, "Interlace", Flag, 0, 0)
//   → FieldEntry { "INTERLACE", "Interlace", 17, 7, 1, DataKind::Flag, 0, 0 }

struct FieldEntry {
    const char* label;       // Field symbol name (stringified)
    const char* desc;        // Human-readable description
    uint16_t    reg_index;   // Register index in the bank (for renderer association)
    uint8_t     shift;       // LSB position within the register
    uint8_t     width;       // Bit count (1..32)
    DataKind    kind;        // Semantic type
    uint8_t     display_shift = 0;  // Left-shift applied to extracted value for display
    uint16_t    display_scale = 0;  // Multiplier for display (0 = 1, no scaling)
};

// ============================================================================
// COMPOUND VALUE — assembled from bit ranges across multiple registers
// ============================================================================
//
// For values like VIC-II sprite X (9-bit: 8 bits from M0X + 1 bit from MX8),
// where the logical value is scattered across multiple physical registers.
// The fragment list describes how to assemble the result from pieces.
//
// Template parameter T (ChipRegTraits NTTP) determines the offset type, so
// 8-bit era chips pay zero storage overhead while future chips with large
// register files get wider offsets automatically.

static constexpr size_t COMPOUND_MAX_FRAGMENTS = 4;

template<const ChipRegTraits& T>
struct CompoundFragment {
    reg_offset_t<T> reg_index;    // Register index in the bank (NOT byte offset)
    uint8_t         src_hi;       // High bit position in source register
    uint8_t         src_lo;       // Low bit position in source register
    uint8_t         dst_lo;       // Destination bit position in assembled result
};

template<const ChipRegTraits& T>
struct CompoundEntry {
    const char*           label;
    const char*           desc;
    DataKind              kind;            // Semantic type of the assembled value
    uint8_t               total_bits;      // Width of assembled result (e.g. 9)
    uint8_t               fragment_count;
    CompoundFragment<T>   fragments[COMPOUND_MAX_FRAGMENTS];
};

// Generic extraction — assembles a compound value from register fragments.
// Handles multi-byte registers via T.register_width.
template<const ChipRegTraits& T>
static inline uint32_t compound_get(const uint8_t* regs,
                                    const CompoundEntry<T>& c) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < c.fragment_count; i++) {
        const auto& f = c.fragments[i];
        // Read the full register value (1–4 bytes, little-endian)
        uint32_t reg_val = 0;
        if constexpr (T.register_width == 1) {
            reg_val = regs[f.reg_index];
        } else {
            size_t byte_idx = static_cast<size_t>(f.reg_index) * T.register_width;
            for (uint8_t b = 0; b < T.register_width; b++)
                reg_val |= static_cast<uint32_t>(regs[byte_idx + b]) << (b * 8);
        }
        // Extract the bitfield and place it at the destination position
        uint8_t  width = f.src_hi - f.src_lo + 1;
        uint32_t mask  = ((1u << width) - 1u) << f.src_lo;
        result |= ((reg_val & mask) >> f.src_lo) << f.dst_lo;
    }
    return result;
}

// ============================================================================
// DECLARATION ORDER — type-erased walk for renderers
// ============================================================================
//
// The DECL(REG, FLD, CMP) table interleaves three row types.  Each chip
// extracts a DeclOrderEntry[] that preserves the declaration order, allowing
// the renderer to walk registers, their bitfields, and compound values in
// the exact order the hardware programmer defined them.
//
// The order array pairs each entry with an index into the typed array for
// that row type (REG_INFO, FLD_INFO, COMPOUND_INFO).

enum class DeclRowType : uint8_t { Reg, Field, Compound };

struct DeclOrderEntry {
    DeclRowType type;
    uint16_t    index;   // REG: register offset, FLD: into FLD_INFO[], CMP: into COMPOUND_INFO[]
};

// Non-templated compound metadata — enough for the renderer to display
// a compound value without knowing the ChipRegTraits template parameter.
struct CompoundInfo {
    const char* label;
    const char* desc;
    DataKind    kind;
    uint8_t     total_bits;
    uint8_t     display_shift = 0;  // Left-shift applied to assembled value for display
    uint16_t    display_scale = 0;  // Multiplier for display (0 = 1, no scaling)
};

// Type-erased compound value reader.  The chip provides a function that
// calls compound_get<T>() internally.
using CompoundReadFn = uint32_t (*)(const uint8_t* regs, uint16_t compound_index);

// constexpr helper: assign sequential FLD/CMP indices to a raw order array.
// REG rows keep their register offset (set by the macro); FLD and CMP rows
// get placeholder 0 in the macro expansion and are sequentially numbered here.
template<size_t N>
constexpr std::array<DeclOrderEntry, N> assign_decl_indices(const DeclOrderEntry (&raw)[N]) {
    std::array<DeclOrderEntry, N> result{};
    uint16_t fld_idx = 0, cmp_idx = 0;
    for (size_t i = 0; i < N; ++i) {
        result[i] = raw[i];
        switch (raw[i].type) {
            case DeclRowType::Reg:      break;
            case DeclRowType::Field:    result[i].index = fld_idx++; break;
            case DeclRowType::Compound: result[i].index = cmp_idx++; break;
        }
    }
    return result;
}

// constexpr helper: assign reg_index to FieldEntry items based on the
// declaration order.  FLD rows produced by DECL_X_FLD_INFO_ have reg_index=0;
// this function resolves them from the preceding REG row's register offset.
template<size_t NO, size_t NF>
constexpr std::array<FieldEntry, NF> assign_field_reg_indices(
    const DeclOrderEntry (&order)[NO],
    const FieldEntry (&raw)[NF])
{
    std::array<FieldEntry, NF> result{};
    for (size_t i = 0; i < NF; ++i) result[i] = raw[i];
    uint16_t cur_reg = 0;
    size_t fld_idx = 0;
    for (size_t i = 0; i < NO; ++i) {
        if (order[i].type == DeclRowType::Reg)
            cur_reg = order[i].index;
        else if (order[i].type == DeclRowType::Field && fld_idx < NF)
            result[fld_idx++].reg_index = cur_reg;
    }
    return result;
}

// ============================================================================
// DECL_EXTRACT_ALL — one-macro extraction for chip headers
// ============================================================================
//
// Usage (chip header):
//   #define CHIP_DECL(REG, FLD, CMP) ...   // unique DECL table
//   DECL_EXTRACT_ALL(CHIP, CHIP_DECL)
//
// Generates (all static constexpr, header-safe):
//   CHIP_REG_INFO[]     — RegEntry array
//   CHIP_NUM_REGS       — uint16_t register count
//   CHIP_FLD_INFO       — const FieldEntry* (nullptr if no fields)
//   CHIP_NUM_FIELDS     — uint16_t field count
//   CHIP_DECL_ORDER     — std::array<DeclOrderEntry, N>
//
// Constants namespace and backward-compat REG_TABLE are NOT generated;
// define those per-chip as needed.
//
// For chips with CMP (compound) entries, add the CMP extraction manually
// after DECL_EXTRACT_ALL — the compound read function is chip-specific.
//
// Requires ≥1 FLD entry in the DECL table.  For REG-only chips (no FLD),
// use DECL_EXTRACT_REGS_ONLY instead.

#define DECL_EXTRACT_ALL(PREFIX, DECL)                                         \
    static constexpr RegEntry PREFIX##_REG_INFO[] =                            \
        { DECL(DECL_X_REG_INFO_, DECL_FLD_NOP, DECL_CMP_NOP) };              \
    constexpr uint16_t PREFIX##_NUM_REGS =                                     \
        sizeof(PREFIX##_REG_INFO) / sizeof(PREFIX##_REG_INFO[0]);              \
    static constexpr DeclOrderEntry PREFIX##_DECL_ORD_RAW_[] =                 \
        { DECL(DECL_X_ORD_REG_, DECL_X_ORD_FLD_, DECL_X_ORD_CMP_) };        \
    static constexpr auto PREFIX##_DECL_ORDER =                                \
        assign_decl_indices(PREFIX##_DECL_ORD_RAW_);                           \
    static constexpr FieldEntry PREFIX##_FLD_RAW_[] =                          \
        { DECL(DECL_REG_NOP, DECL_X_FLD_INFO_, DECL_CMP_NOP) };              \
    static constexpr auto PREFIX##_FLD_ARR_ =                                  \
        assign_field_reg_indices(PREFIX##_DECL_ORD_RAW_, PREFIX##_FLD_RAW_);   \
    static constexpr const FieldEntry* PREFIX##_FLD_INFO =                     \
        PREFIX##_FLD_ARR_.data();                                              \
    constexpr uint16_t PREFIX##_NUM_FIELDS =                                   \
        (uint16_t)PREFIX##_FLD_ARR_.size();

// Variant for chips with only REG entries (no FLD or CMP).
#define DECL_EXTRACT_REGS_ONLY(PREFIX, DECL)                                   \
    static constexpr RegEntry PREFIX##_REG_INFO[] =                            \
        { DECL(DECL_X_REG_INFO_, DECL_FLD_NOP, DECL_CMP_NOP) };              \
    constexpr uint16_t PREFIX##_NUM_REGS =                                     \
        sizeof(PREFIX##_REG_INFO) / sizeof(PREFIX##_REG_INFO[0]);              \
    static constexpr DeclOrderEntry PREFIX##_DECL_ORD_RAW_[] =                 \
        { DECL(DECL_X_ORD_REG_, DECL_X_ORD_FLD_, DECL_X_ORD_CMP_) };        \
    static constexpr auto PREFIX##_DECL_ORDER =                                \
        assign_decl_indices(PREFIX##_DECL_ORD_RAW_);                           \
    static constexpr const FieldEntry* PREFIX##_FLD_INFO = nullptr;            \
    constexpr uint16_t PREFIX##_NUM_FIELDS = 0;

/// Read from a contiguous register array:
///   byte_offset  = byte index into the array
///   bit_offset   = LSB position within the extracted value
///   bit_count    = number of bits to extract (1..32)
///   byte_count   = 1 or 2 (for 16-bit registers, lo byte first)
struct RegSource {
    uint16_t byte_offset = 0;
    uint8_t  bit_offset  = 0;
    uint8_t  bit_count   = 8;
    uint8_t  byte_count  = 1;
};

/// Function pointer types — callbacks receive the owning chip instance at render time.
/// The chip pointer is supplied by the renderer at paint time, never stored per-field.
using UIntFn      = uint32_t (*)(const ChipBase*);
using FloatFn     = float (*)(const ChipBase*);
using StringFn    = const char* (*)(const ChipBase*);
using VoidFn      = void (*)(const ChipBase*);
using ByteDataFn  = std::pair<const uint8_t*, size_t> (*)(const ChipBase*);
using FloatDataFn = std::pair<const float*, size_t> (*)(const ChipBase*);

/// Value source: either a compile-time register reference or a runtime callback.
using UIntSource  = std::variant<RegSource, UIntFn>;

// ============================================================================
// KIND-SPECIFIC METADATA — structured payload per DataKind
// ============================================================================

/// Metadata for DataKind::State — named options.
struct StateMeta {
    const char* const* names = nullptr;
    uint8_t            count = 0;
};

/// Metadata for DataKind::Counter — max value (static or dynamic).
struct CounterMeta {
    std::variant<uint32_t, UIntFn> max;
};

/// Metadata for DataKind::Bitfield — per-bit label array.
struct BitfieldMeta {
    const char* const* labels   = nullptr;  // [bit_count] labels, MSB-first
    uint8_t            bit_count = 8;
};

/// Metadata for DataKind::Port — data + direction sources.
struct PortMeta {
    UIntSource data_src;
    UIntSource ddr_src;
    uint8_t    pin_count = 8;
    const char* const* pin_labels = nullptr;  // optional: "PA0".."PA7"
};

/// Metadata for DataKind::Timer — all sub-fields.
struct TimerMeta {
    UIntSource counter_src;
    UIntSource latch_src;
    UIntSource running_src;     // bool: nonzero = running
    const char* mode_label = nullptr;   // optional: e.g. "Free-running"
    StringFn mode_fn = nullptr;  // dynamic mode string
};

/// Metadata for DataKind::AudioChannel — per-channel sub-fields.
struct AudioChannelMeta {
    UIntSource  enabled_src;
    UIntSource  frequency_src;
    UIntSource  volume_src;
    const char* const* waveform_names = nullptr;
    uint8_t     waveform_count = 0;
    UIntSource  waveform_src;         // index into waveform_names
    // Optional extra sub-fields (envelope, sweep, etc.) via custom callback
    VoidFn extra_render_fn = nullptr;
};

/// Metadata for DataKind::Palette — color array.
struct PaletteMeta {
    ByteDataFn data_fn = nullptr;
    uint16_t entries      = 0;
    uint8_t  bits_per_entry = 8;   // 8 = indexed, 24 = RGB, 32 = RGBA
    // Optional: system palette lookup.  The renderer uses this to show
    // actual colors for indexed entries.  If null, shows raw hex.
    const uint32_t* system_palette     = nullptr;
    uint16_t        system_palette_size = 0;
};

/// Metadata for DataKind::Memory — byte array.
struct MemoryMeta {
    ByteDataFn data_fn = nullptr;
    uint16_t base_address = 0;
    size_t   max_display  = 256;
};

/// Metadata for DataKind::PatternTile — sprite/character pixel data.
struct PatternTileMeta {
    ByteDataFn data_fn = nullptr;
    uint8_t  width       = 8;
    uint8_t  height      = 8;
    uint8_t  bpp         = 1;
    const uint32_t* palette       = nullptr;
    uint16_t        palette_size  = 0;
};

/// Metadata for DataKind::WaveformBuffer — audio sample ring buffer.
struct WaveformBufferMeta {
    FloatDataFn data_fn = nullptr;
    uint32_t sample_rate = 44100;
};

/// Metadata for DataKind::RasterPosition — position within frame.
struct RasterPositionMeta {
    UIntSource scanline_src;
    UIntSource cycle_src;
    std::variant<uint32_t, UIntFn> total_lines;
    std::variant<uint32_t, UIntFn> total_cycles;
};

/// Metadata for DataKind::FlagString — processor status flags.
struct FlagStringMeta {
    const char* flag_chars_when_set   = nullptr;  // e.g. "NV-BDIZC"
    const char* flag_chars_when_clear = nullptr;  // e.g. "nv-bdizc"
    uint8_t     bit_count = 8;
};

/// Metadata for DataKind::Color — single color value.
struct ColorMeta {
    const uint32_t* system_palette     = nullptr;
    uint16_t        system_palette_size = 0;
};

/// Custom rendering callback — the escape hatch.
struct CustomMeta {
    VoidFn render_fn = nullptr;
};

// ============================================================================
// VARIANT OF ALL METADATA TYPES
// ============================================================================

using KindMeta = std::variant<
    std::monostate,         // No metadata (Value, Flag, Address, SignedValue, Frequency, Level)
    StateMeta,
    CounterMeta,
    BitfieldMeta,
    PortMeta,
    TimerMeta,
    AudioChannelMeta,
    PaletteMeta,
    MemoryMeta,
    PatternTileMeta,
    WaveformBufferMeta,
    RasterPositionMeta,
    FlagStringMeta,
    ColorMeta,
    CustomMeta
>;

// ============================================================================
// DEBUG FIELD — a single entry in the registry
// ============================================================================

struct DebugField {
    const char* label  = nullptr;
    DataKind    kind   = DataKind::Value;
    uint8_t     indent = 0;       // nesting depth (0 = top-level)

    // Primary value source — used for atomic scalar kinds
    UIntSource  uint_src;
    FloatFn     float_src = nullptr;
    StringFn    string_src = nullptr;  // For dynamic label text

    // Kind-specific metadata
    KindMeta    meta;

    // Number of bits to display (hint for Value/Address formatting)
    uint8_t     display_bits = 8;  // 8, 16, 24, 32
};

// ============================================================================
// DEBUG CATEGORY — a named collapsible group of fields
// ============================================================================

struct DebugCategory {
    std::string name;                   // Stable heap copy (names may be snprintf'd)
    bool        default_open = true;
    std::vector<DebugField> fields;
};

// ============================================================================
// CHIP DEBUG REGISTRY — the main container, owned by ChipBase
// ============================================================================
//
// Chips populate this at construction time via a builder-pattern API.
// The rendering layer reads it at paint time.
//
// Non-GUI builds carry the registry (it's lightweight), but the render()
// method is a no-op stub compiled with the GUI_STUB_SOURCES.

class ChipDebugRegistry {
public:
    // ---- Register backing store ----
    // Chips with a flat register array call this once.
    // Fields using RegSource read from this pointer.
    // Optional: pass per-register info (label + description) and a mapped base address.
    void set_registers(const uint8_t* data, size_t size,
                       const RegEntry* info = nullptr,
                       uint16_t base_address = 0) {
        reg_base_address_ = base_address;
        reg_size_ = size;
        reg_data_ = data;
        reg_info_ = info;
    }

    uint16_t            reg_base_address() const { return reg_base_address_; }
    size_t              reg_size()         const { return reg_size_; }
    const uint8_t*      reg_data()         const { return reg_data_; }
    const RegEntry*     reg_info()         const { return reg_info_; }

    // ---- Query ----
    bool empty() const { return categories_.empty(); }
    const std::vector<DebugCategory>& categories() const { return categories_; }

    // ---- Builder API ----
    // Each method returns *this for chaining.

    /// Start a new category (collapsible section).
    ChipDebugRegistry& category(const char* name, bool default_open = true);

    // -- Atomic scalar kinds --

    /// Generic numeric value — registers.
    ChipDebugRegistry& value(const char* label, uint16_t reg_offset, uint8_t bits = 8);
    /// Generic numeric value — callback source.
    ChipDebugRegistry& value(const char* label, UIntFn fn, uint8_t bits = 8);
    /// Generic numeric value — callback source, explicit display bits.
    ChipDebugRegistry& value(const char* label, UIntFn fn, uint8_t bits, uint8_t indent);

    /// Boolean flag — register bit.
    ChipDebugRegistry& flag(const char* label, uint16_t reg_offset, uint8_t bit);
    /// Boolean flag — callback source.
    ChipDebugRegistry& flag(const char* label, UIntFn fn);

    /// Enumerated state — register bits.
    ChipDebugRegistry& state(const char* label, uint16_t reg_offset,
                             uint8_t bit_count, uint8_t bit_offset,
                             const char* const* names, uint8_t name_count);
    /// Enumerated state — callback source.
    ChipDebugRegistry& state(const char* label, UIntFn fn,
                             const char* const* names, uint8_t name_count);

    /// Memory address — register source.
    ChipDebugRegistry& address(const char* label, uint16_t reg_offset, uint8_t bits = 16);
    /// Memory address — callback source.
    ChipDebugRegistry& address(const char* label, UIntFn fn, uint8_t bits = 16);

    /// Counter within a range — callback + static max.
    ChipDebugRegistry& counter(const char* label, UIntFn fn, uint32_t max);
    /// Counter within a range — callback + dynamic max.
    ChipDebugRegistry& counter(const char* label, UIntFn fn, UIntFn max_fn);

    /// Normalized 0.0–1.0 level (volume, amplitude, duty cycle).
    ChipDebugRegistry& level(const char* label, FloatFn fn);

    /// Color value — register source + optional system palette.
    ChipDebugRegistry& color(const char* label, uint16_t reg_offset,
                             const uint32_t* palette = nullptr, uint16_t palette_size = 0);
    /// Color value — callback source + optional system palette.
    ChipDebugRegistry& color(const char* label, UIntFn fn,
                             const uint32_t* palette = nullptr, uint16_t palette_size = 0);

    /// Frequency value — register or callback.
    ChipDebugRegistry& frequency(const char* label, uint16_t reg_offset, uint8_t bits = 8);
    ChipDebugRegistry& frequency(const char* label, UIntFn fn, uint8_t bits = 16);

    /// Signed integer value — callback.
    ChipDebugRegistry& signed_value(const char* label, UIntFn fn, uint8_t bits = 8);

    // -- Structured / composite kinds --

    /// Bitfield (per-bit labeled breakdown) — register source.
    ChipDebugRegistry& bitfield(const char* label, uint16_t reg_offset,
                                uint8_t bit_count, const char* const* labels);
    /// Bitfield — callback source.
    ChipDebugRegistry& bitfield(const char* label, UIntFn fn,
                                uint8_t bit_count, const char* const* labels);

    /// I/O port (data + DDR, pin-by-pin display).
    ChipDebugRegistry& port(const char* label,
                            UIntSource data_src, UIntSource ddr_src,
                            uint8_t pin_count = 8,
                            const char* const* pin_labels = nullptr);

    /// Timer composite (counter + latch + running + mode).
    ChipDebugRegistry& timer(const char* label,
                             UIntSource counter_src, UIntSource latch_src,
                             UIntSource running_src,
                             const char* mode_label = nullptr);
    ChipDebugRegistry& timer(const char* label,
                             UIntSource counter_src, UIntSource latch_src,
                             UIntSource running_src,
                             StringFn mode_fn);

    /// Audio channel composite.
    ChipDebugRegistry& audio_channel(const char* label,
                                     UIntSource enabled_src,
                                     UIntSource frequency_src,
                                     UIntSource volume_src,
                                     const char* const* waveform_names = nullptr,
                                     uint8_t waveform_count = 0,
                                     UIntSource waveform_src = RegSource{},
                                     VoidFn extra_fn = nullptr);

    /// Palette data — array of color entries.
    ChipDebugRegistry& palette(const char* label,
                               ByteDataFn data_fn,
                               uint16_t entries, uint8_t bits_per_entry = 8,
                               const uint32_t* sys_palette = nullptr,
                               uint16_t sys_palette_size = 0);

    /// Memory region (hex dump).
    ChipDebugRegistry& memory(const char* label,
                              ByteDataFn data_fn,
                              uint16_t base_address = 0, size_t max_display = 256);

    /// Pattern tile (sprite/character pixel data).
    ChipDebugRegistry& pattern_tile(const char* label,
                                    ByteDataFn data_fn,
                                    uint8_t width, uint8_t height, uint8_t bpp,
                                    const uint32_t* palette = nullptr,
                                    uint16_t palette_size = 0);

    /// Waveform buffer (audio sample ring buffer).
    ChipDebugRegistry& waveform_buffer(const char* label,
                                       FloatDataFn data_fn,
                                       uint32_t sample_rate = 44100);

    /// Raster position (scanline + cycle within frame bounds).
    ChipDebugRegistry& raster_position(const char* label,
                                       UIntSource scanline_src, UIntSource cycle_src,
                                       std::variant<uint32_t, UIntFn> total_lines,
                                       std::variant<uint32_t, UIntFn> total_cycles);

    /// Processor status flag string (NVUBDIZc style).
    ChipDebugRegistry& flag_string(const char* label, UIntSource value_src,
                                   const char* set_chars, const char* clear_chars,
                                   uint8_t bit_count = 8);

    /// Custom rendering callback — the escape hatch.
    /// The callback receives the chip instance and full ImGui context.
    ChipDebugRegistry& custom(VoidFn fn);

    // -- Indentation helpers --

    /// Set indent level for subsequent fields until the next category or
    /// explicit indent(0) call.
    ChipDebugRegistry& indent(uint8_t level);

    /// Add a visual separator line in the current category.
    ChipDebugRegistry& separator();

    /// Add a static text line (e.g. chip description header).
    ChipDebugRegistry& text(const char* static_text);
    /// Add a dynamic text line.
    ChipDebugRegistry& text(StringFn fn);

    // ---- Declaration-order rendering (DECL table walk) ----
    // Chips with a unified DECL(REG, FLD, CMP) table call this once after
    // set_registers().  The renderer walks the order array instead of
    // showing a flat register dump, rendering fields and compounds inline.
    void set_decl_order(const DeclOrderEntry* order, size_t order_count,
                        const FieldEntry* fields, size_t field_count,
                        const CompoundInfo* compounds, size_t compound_count,
                        CompoundReadFn compound_reader) {
        decl_order_           = order;
        decl_order_count_     = order_count;
        decl_fields_          = fields;
        decl_field_count_     = field_count;
        decl_compounds_       = compounds;
        decl_compound_count_  = compound_count;
        decl_compound_reader_ = compound_reader;
    }

    bool              has_decl_order()      const { return decl_order_ != nullptr; }
    const DeclOrderEntry* decl_order()      const { return decl_order_; }
    size_t            decl_order_count()    const { return decl_order_count_; }
    const FieldEntry* decl_fields()         const { return decl_fields_; }
    size_t            decl_field_count()    const { return decl_field_count_; }
    const CompoundInfo* decl_compounds()    const { return decl_compounds_; }
    size_t            decl_compound_count() const { return decl_compound_count_; }
    CompoundReadFn    decl_compound_reader() const { return decl_compound_reader_; }

    // ---- System palette (for Color kind rendering) ----
    void set_palette(const uint32_t* palette, uint16_t palette_size) {
        palette_      = palette;
        palette_size_ = palette_size;
    }
    const uint32_t* palette()      const { return palette_; }
    uint16_t        palette_size() const { return palette_size_; }

    // ---- Rendering (implemented in chip_debug_registry_gui.cpp) ----
    // Renders all categories and fields using ImGui.
    // No-op when CERMU_HAS_GUI is not defined.
    void render(const ChipBase* chip) const;

    // ---- Resolve a UIntSource to a value ----
    uint32_t read(const UIntSource& src, const ChipBase* chip) const;

    // ---- Resolve a variant<uint32_t, fn> ----
    static uint32_t resolve(const std::variant<uint32_t, UIntFn>& v, const ChipBase* chip) {
        if (auto* val = std::get_if<uint32_t>(&v)) return *val;
        return std::get<UIntFn>(v)(chip);
    }

private:
    uint16_t                    reg_base_address_ = 0;
    size_t                      reg_size_ = 0;
    const uint8_t*              reg_data_ = nullptr;
    const RegEntry*             reg_info_  = nullptr;
    std::vector<DebugCategory>  categories_;
    uint8_t                     current_indent_ = 0;

    // Declaration-order walk data (set via set_decl_order)
    const DeclOrderEntry*       decl_order_           = nullptr;
    size_t                      decl_order_count_     = 0;
    const FieldEntry*           decl_fields_          = nullptr;
    size_t                      decl_field_count_     = 0;
    const CompoundInfo*         decl_compounds_       = nullptr;
    size_t                      decl_compound_count_  = 0;
    CompoundReadFn              decl_compound_reader_ = nullptr;

    // System palette for Color kind rendering
    const uint32_t*             palette_              = nullptr;
    uint16_t                    palette_size_         = 0;

    // Get or create the current (last) category.  If none exists, creates "General".
    DebugCategory& current_category();

    // Push a field into the current category with the current indent.
    void push_field(DebugField&& f);
};

