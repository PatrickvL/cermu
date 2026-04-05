#pragma once
// =============================================================================
// typed_manifest.hpp — Unified type-preserving component manifest
// =============================================================================
//
// TypedManifest<Ts...> is the compile-time backbone for emulated systems.
// It carries the full type list of ALL board components — chips, ports, and
// dip switches — in a single variadic parameter pack.
//
// Key properties:
//   - Inherits ChipManifest<chip_count> (counting only chip types).
//     ManifestBusSpec, Board constructors, and bus wiring work unchanged.
//   - Stores PortSlot metadata separately for port initialization.
//   - Stores DipSwitchBankDescriptor pointers for dip switch init.
//   - Provides component_tuple typedef for std::tuple<Ts...> ownership.
//   - Provides find_nth<T, Nth>() for type-based index lookup into the
//     full component list (handles duplicate types).
//
// Usage:
//
//   inline constexpr auto kManifest = make_manifest(
//       Slot<MOS6502>        {.base_addr = 0x0000, .label = "CPU"},
//       Slot<RAMChip>        {.base_addr = 0x0000, .size_bytes = 0x10000},
//       Slot<PortExpansion>  {.name = "Expansion Connector"},
//       Slot<PortCompositeVideo> {.name = "Video Out", .default_device = "crt_green"},
//   );
//   // Type: TypedManifest<MOS6502, RAMChip, PortExpansion, PortCompositeVideo>
//   // IS-A ChipManifest<2> (only MOS6502 and RAMChip are chips)
//
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/typed_port.hpp"
#include "core/dip_switch.hpp"
#include "core/port.hpp"
#include <tuple>


// =============================================================================
// §0  Type classification helpers
// =============================================================================

// is_typed_port_v<T> is defined in typed_port.hpp
// is_dip_switch_component_v<T> is defined in dip_switch.hpp

/// True for chip types (anything that isn't a port or dip switch).
template<typename T>
inline constexpr bool is_chip_type_v =
    !is_typed_port_v<T> && !is_dip_switch_component_v<T>;

/// Count chip types in a parameter pack.
template<typename... Ts>
inline constexpr size_t count_chips_v = (... + (is_chip_type_v<Ts> ? size_t(1) : size_t(0)));

/// Count port types in a parameter pack.
template<typename... Ts>
inline constexpr size_t count_ports_v = (... + (is_typed_port_v<Ts> ? size_t(1) : size_t(0)));

/// Count dip switch types in a parameter pack.
template<typename... Ts>
inline constexpr size_t count_dips_v = (... + (is_dip_switch_component_v<Ts> ? size_t(1) : size_t(0)));


// =============================================================================
// §0.1  find_nth — compile-time index of the Nth occurrence of T in a pack
// =============================================================================

namespace manifest_detail {

template<typename Target, size_t Nth, size_t Pos, typename... Ts>
struct find_nth_impl;

// Found it at this position
template<typename Target, size_t Nth, size_t Pos, typename... Rest>
struct find_nth_impl<Target, Nth, Pos, Target, Rest...> {
    static constexpr size_t value =
        (Nth == 0) ? Pos : find_nth_impl<Target, Nth - 1, Pos + 1, Rest...>::value;
};

// Not a match — keep searching
template<typename Target, size_t Nth, size_t Pos, typename First, typename... Rest>
struct find_nth_impl<Target, Nth, Pos, First, Rest...> {
    static constexpr size_t value = find_nth_impl<Target, Nth, Pos + 1, Rest...>::value;
};

// Base case — not found (returns sizeof...(original pack) as past-end)
template<typename Target, size_t Nth, size_t Pos>
struct find_nth_impl<Target, Nth, Pos> {
    static constexpr size_t value = Pos;  // past-end sentinel
};

} // namespace manifest_detail


// =============================================================================
// §1  TypedManifest<Ts...> — unified component manifest
// =============================================================================

template<typename... Ts>
struct TypedManifest : ChipManifest<count_chips_v<Ts...>> {
    /// Full component type list (chips + ports + dip switches).
    using component_tuple = std::tuple<Ts...>;

    static constexpr size_t total_count = sizeof...(Ts);
    static constexpr size_t chip_count  = count_chips_v<Ts...>;
    static constexpr size_t port_count  = count_ports_v<Ts...>;
    static constexpr size_t dip_count   = count_dips_v<Ts...>;

    // ── Port slot metadata (constexpr) ──────────────────────────────────
    // Indexed by port order within the type list (0 = first port, etc.).
    PortSlot port_slots[port_count > 0 ? port_count : 1] = {};

    // ── DIP switch descriptors (constexpr pointers to static data) ──────
    const DipSwitchBankDescriptor* dip_descriptors[dip_count > 0 ? dip_count : 1] = {};

    // ── find_nth<T, Nth>() — index in the FULL type list ────────────────
    //
    // Returns the position of the Nth occurrence of T in the pack Ts...
    // For unique types, Nth defaults to 0.  Returns total_count (past-end)
    // if not found.  Works at compile time.
    //
    template<typename T, size_t Nth = 0>
    static constexpr size_t find_nth() {
        return manifest_detail::find_nth_impl<T, Nth, 0, Ts...>::value;
    }

    // ── Chain modifiers (shadow base to preserve type list) ──────────────

    [[nodiscard]] constexpr TypedManifest with_dynamic_pool(size_t pages) const noexcept {
        TypedManifest copy = *this;
        copy.num_dynamic_pages = pages;
        return copy;
    }

    [[nodiscard]] constexpr TypedManifest with_page_banking() const noexcept {
        TypedManifest copy = *this;
        for (size_t i = 0; i < chip_count; ++i)
            if (copy.chips[i].size_bytes > 0)
                copy.chips[i].bank_size = 0;
        return copy;
    }

    [[nodiscard]] constexpr TypedManifest with_sorted_ids() const noexcept {
        TypedManifest copy = *this;
        copy.sorted_ids = true;
        return copy;
    }
};


// =============================================================================
// §2  Slot<TypedPort<PT>> — port slot specialization
// =============================================================================
//
// Provides port-specific fields for the manifest declaration.
// PortType is implicit from the template parameter.
//

template<PortType PT>
struct Slot<TypedPort<PT>> {
    const char* name           = nullptr;   ///< Display name ("Control Port 1")
    int         port_number    = 0;         ///< 0 = no numbering, 1+ = player/slot
    bool        is_internal    = false;     ///< Internal connectors (keyboard) hidden from icon bar
    bool        is_bus         = false;     ///< Shared bus (IEC): multiple devices may attach
    const char* default_device = nullptr;   ///< DeviceRegistry ID for auto-attach, or nullptr
    const char* built_in_device = nullptr;  ///< DeviceRegistry ID for permanently attached device
};


// =============================================================================
// §3  Slot<DipSwitchBankComponent> — dip switch slot specialization
// =============================================================================

template<>
struct Slot<DipSwitchBankComponent> {
    const DipSwitchBankDescriptor* descriptor = nullptr;
    const char* label = nullptr;
};


// =============================================================================
// §4  make_manifest — unified factory for chips + ports + dip switches
// =============================================================================
//
// Processes each Slot<T> based on its category:
//   - Chip slots → ChipManifest::chips[] (with factory resolution)
//   - Port slots → TypedManifest::port_slots[]
//   - DIP switch slots → TypedManifest::dip_descriptors[]
//
// The resulting TypedManifest IS-A ChipManifest<chip_count>, so
// ManifestBusSpec and Board constructors work unchanged.
//

template<typename... Ts>
[[nodiscard]] constexpr TypedManifest<Ts...>
make_manifest(Slot<Ts>... slots) noexcept
{
    TypedManifest<Ts...> m{};
    size_t chip_idx = 0;
    size_t port_idx = 0;
    size_t dip_idx  = 0;

    // Process each slot via a fold expression over a generic lambda.
    // C++20 template lambdas let us dispatch on the slot's type.
    auto process = [&]<typename T>(Slot<T>& s) {
        if constexpr (is_typed_port_v<T>) {
            m.port_slots[port_idx++] = PortSlot{
                T::port_type, s.name, s.port_number,
                s.is_internal, s.is_bus, s.default_device,
                s.built_in_device
            };
        } else if constexpr (is_dip_switch_component_v<T>) {
            m.dip_descriptors[dip_idx++] = s.descriptor;
        } else {
            // Chip slot — same processing as make_chip_manifest.
            // Note: size_bytes need not be a power of 2 (e.g. VIC-20 3K expansion).
            m.chips[chip_idx++] = ChipSlot{
                s.base_addr, s.size_bytes, s.addr_mask,
                s.bank_size,
                s.effective_size,
                s.overlay_group,
                resolve_slot_factory<T>(),
                s.label,
                s.condition,
                s.rom
            };
        }
    };

    (process(slots), ...);
    return m;
}


// =============================================================================
// §5  bind_all — category-dispatched component binding
// =============================================================================
//
// Iterates the component tuple and dispatches by category:
//   - Chips: bind_chip() + register_component()
//   - Ports: init() from PortSlot + add_port_ref() (non-owning)
//   - DIP switches: init() from descriptor + register_component()
//
// The manifest provides metadata for each category, indexed separately.
// Slot order within each category matches declaration order.
//

template<typename BoardT, typename Tuple, typename Manifest>
void bind_all(BoardT& board, Tuple& components, const Manifest& manifest) {
    size_t chip_idx = 0;
    size_t port_idx = 0;
    size_t dip_idx  = 0;

    std::apply([&](auto&... comp) {
        ([&] {
            using T = std::remove_cvref_t<decltype(comp)>;
            if constexpr (is_typed_port_v<T>) {
                auto def = make_port_definition(manifest.port_slots[port_idx]);
                comp.init(def, manifest.port_slots[port_idx].port_number);
                board.add_port_ref(&comp);
                board.register_component(&comp);
                port_idx++;
            } else if constexpr (is_dip_switch_component_v<T>) {
                comp.bank.init(manifest.dip_descriptors[dip_idx]);
                board.register_component(&comp);
                dip_idx++;
            } else {
                // Chip
                board.bind_chip(chip_idx, &comp);
                board.register_component(&comp);
                chip_idx++;
            }
        }(), ...);
    }, components);
}
