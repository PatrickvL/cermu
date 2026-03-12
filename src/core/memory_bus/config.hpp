// =============================================================================
// config.hpp — Bus spec concept, feature probes, default spec
// =============================================================================
//
// Defines the compile-time contract every bus spec must satisfy,
// plus optional-feature probes that detect extra fields in the spec struct.
// Each probe defaults to "disabled" when the field is absent, giving every
// optional feature true zero cost.
//
// The BusSpecConcept is the single gate for all MemoryBus template
// parameters: if the concept doesn't accept a struct, nothing compiles.
//
// =============================================================================
#pragma once

#include "core/cermu.hpp"          // uint_least_bits_t, bitmix, FORCE_INLINE

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <type_traits>


// =============================================================================
// §1  Internal type-selection helpers
// =============================================================================

namespace membus_detail {

template<typename C, typename = void>
struct has_packed_rw_override : std::false_type {};
template<typename C>
struct has_packed_rw_override<C, std::void_t<decltype(C::PackedRW)>>
    : std::true_type {};

} // namespace membus_detail


// =============================================================================
// §2  Bus spec concept
// =============================================================================
//
// Required fields:
//   AddrType       — unsigned integer type for addresses
//   AddressBits    — number of address bits (e.g. 16, 14, 24)
//   PageBits       — log2(page size in bytes); must be in [1, AddressBits)
//   NumViewers     — number of independent bus agents (CPU, VIC-II, …)
//   MaxChipId      — highest chip id that maps directly into the unified buffer
//                    on the read side.
//   MaxWriteChipId — same for the write side.  Must be ≤ MaxChipId.
//   EnableMmio     — true to enable the register-file (MMIO) handler table
//   MaxMmioHandlers— max register-file handler slots (0 when !EnableMmio)
//
// Optional fields (absence = feature disabled, zero overhead):
//   PackedRW            — force packed (true) or separate (false) R/W tables
//   EnablePartialBus    — enable per-chip data-line ownership masks
//   DataType            — data-bus element type (default uint8_t)
//   CsLineBits          — when > 0, resolved chip id emitted into bus_state_t
//   MaxIndexedSubTables — max indexed sub-tables per viewer (0 = disabled)
//   IndexedSubBits      — max bits per indexed sub-table (entry count = 2^N)
//   MaxMaskedSubTables  — max masked sub-tables per viewer (0 = disabled)
//   MaxMaskedRegions    — max mask/match regions per masked sub-table
//

template<typename C>
concept BusSpecConcept = requires {
    typename C::AddrType;
    { C::AddressBits }     -> std::convertible_to<size_t>;
    { C::PageBits }        -> std::convertible_to<size_t>;
    { C::NumViewers }      -> std::convertible_to<size_t>;
    { C::MaxChipId }       -> std::convertible_to<size_t>;
    { C::MaxWriteChipId }  -> std::convertible_to<size_t>;
    { C::EnableMmio }      -> std::convertible_to<bool>;
    { C::MaxMmioHandlers } -> std::convertible_to<size_t>;
};


// =============================================================================
// §3  Optional-feature probes
// =============================================================================
//
// Each probe follows the pattern:
//   cfg_<feature>       — trait struct (value or type member)
//   cfg_<feature>_v     — constexpr shorthand for value
//   cfg_<feature>_t     — alias shorthand for type (when applicable)
//
// When the probed field is absent from the spec, the trait yields a safe
// default (false, 0, uint8_t, …) and the compiler eliminates all code gated
// on it through `if constexpr` or `std::conditional_t`.
//

// ── EnablePartialBus ──────────────────────────────────────────────────────────
template<typename C, typename = void>
struct spec_partial_bus : std::false_type {};
template<typename C>
struct spec_partial_bus<C, std::void_t<decltype(C::EnablePartialBus)>>
    : std::bool_constant<C::EnablePartialBus> {};

template<typename C>
inline constexpr bool spec_partial_bus_v = spec_partial_bus<C>::value;

// ── CsLineBits ────────────────────────────────────────────────────────────────
template<typename C, typename = void>
struct spec_cs_line_bits : std::integral_constant<size_t, 0> {};
template<typename C>
struct spec_cs_line_bits<C, std::void_t<decltype(C::CsLineBits)>>
    : std::integral_constant<size_t, C::CsLineBits> {};

template<typename C>
inline constexpr size_t spec_cs_line_bits_v = spec_cs_line_bits<C>::value;

// ── CsBitShift — starting bit position of the CS field in bus_state_t ────────
// Defaults to 55 (first reserved output pin).  Systems may override.
template<typename C, typename = void>
struct spec_cs_bit_shift : std::integral_constant<size_t, 55> {};
template<typename C>
struct spec_cs_bit_shift<C, std::void_t<decltype(C::CsBitShift)>>
    : std::integral_constant<size_t, C::CsBitShift> {};

template<typename C>
inline constexpr size_t spec_cs_bit_shift_v = spec_cs_bit_shift<C>::value;

// ── DataType ──────────────────────────────────────────────────────────────────
template<typename C, typename = void>
struct spec_data_type { using type = uint8_t; };
template<typename C>
struct spec_data_type<C, std::void_t<typename C::DataType>>
    { using type = typename C::DataType; };

template<typename C>
using spec_data_type_t = typename spec_data_type<C>::type;

// ── MaxIndexedSubTables ───────────────────────────────────────────────────────
// Number of indexed sub-tables per viewer.  0 = disabled.
template<typename C, typename = void>
struct spec_max_indexed_subs : std::integral_constant<size_t, 0> {};
template<typename C>
struct spec_max_indexed_subs<C, std::void_t<decltype(C::MaxIndexedSubTables)>>
    : std::integral_constant<size_t, C::MaxIndexedSubTables> {};

template<typename C>
inline constexpr size_t spec_max_indexed_subs_v = spec_max_indexed_subs<C>::value;

// ── IndexedSubBits ────────────────────────────────────────────────────────────
// Max address bits per indexed sub-table.  Entry count = 2^N.  0 when unused.
template<typename C, typename = void>
struct spec_indexed_sub_bits : std::integral_constant<size_t, 0> {};
template<typename C>
struct spec_indexed_sub_bits<C, std::void_t<decltype(C::IndexedSubBits)>>
    : std::integral_constant<size_t, C::IndexedSubBits> {};

template<typename C>
inline constexpr size_t spec_indexed_sub_bits_v = spec_indexed_sub_bits<C>::value;

// ── MaxMaskedSubTables ────────────────────────────────────────────────────────
// Number of masked sub-tables per viewer.  0 = disabled.
template<typename C, typename = void>
struct spec_max_masked_subs : std::integral_constant<size_t, 0> {};
template<typename C>
struct spec_max_masked_subs<C, std::void_t<decltype(C::MaxMaskedSubTables)>>
    : std::integral_constant<size_t, C::MaxMaskedSubTables> {};

template<typename C>
inline constexpr size_t spec_max_masked_subs_v = spec_max_masked_subs<C>::value;

// ── MaxMaskedRegions ──────────────────────────────────────────────────────────
// Max mask/match regions per masked sub-table.  0 when unused.
template<typename C, typename = void>
struct spec_max_masked_regions : std::integral_constant<size_t, 0> {};
template<typename C>
struct spec_max_masked_regions<C, std::void_t<decltype(C::MaxMaskedRegions)>>
    : std::integral_constant<size_t, C::MaxMaskedRegions> {};

template<typename C>
inline constexpr size_t spec_max_masked_regions_v = spec_max_masked_regions<C>::value;


// =============================================================================
// §4  Default bus spec
// =============================================================================

struct DefaultBusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits    = 16;
    static constexpr size_t PageBits       = 8;
    static constexpr size_t NumViewers     = 1;
    static constexpr size_t MaxChipId      = 255;  // 256 chip-id slots in unified buffer
    static constexpr size_t MaxWriteChipId = 255;
    static constexpr bool   EnableMmio     = true;
    static constexpr size_t MaxMmioHandlers= 4;
};
