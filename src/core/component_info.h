#pragma once
// ============================================================================
// component_info.h — Identity hierarchy for emulated physical objects
// ============================================================================
//
// Every manufactured real-world object has a part/model number and a maker.
// This header provides a lightweight, header-only, constexpr-friendly type
// hierarchy that captures that identity data for any entity in the emulator:
// ICs, peripherals, connectors, complete systems.
//
//   ComponentInfo          ← base: part_number + manufacturer
//     ├── ChipInfo         ← IC: adds package_variant, date_code, lot_number
//     ├── DeviceInfo       ← peripheral: adds model identifier
//     └── SystemInfo       ← complete machine: adds short_name, description
//
// All types use std::string_view (non-owning, constexpr, no null footgun).
// Empty string_view means "not set".
//
// Rendering concerns (which fields to show in the GUI) are deliberately
// NOT part of this hierarchy — see ChipMarkings in package_types.h.
//

#include <string_view>

// ============================================================================
// ComponentInfo — base identity for any physical manufactured object
// ============================================================================

struct ComponentInfo {
    std::string_view part_number;    // "MOS6510", "1541-II", "DB-9"
    std::string_view manufacturer;   // "MOS Technology", "Commodore"

    constexpr ComponentInfo() = default;
    constexpr ComponentInfo(std::string_view pn, std::string_view mfr)
        : part_number(pn), manufacturer(mfr) {}

    virtual ~ComponentInfo() = default;
};

// ============================================================================
// ChipInfo — identity for an integrated circuit on a PCB
// ============================================================================

struct ChipInfo : ComponentInfo {
    std::string_view package_variant; // "C", "N", "W" — DIP suffix
    std::string_view date_code;       // "8401" — year+week of manufacture
    std::string_view lot_number;      // Batch identifier

    constexpr ChipInfo() = default;
    constexpr ChipInfo(std::string_view pn, std::string_view mfr)
        : ComponentInfo(pn, mfr) {}
    constexpr ChipInfo(std::string_view pn, std::string_view mfr,
                       std::string_view variant,
                       std::string_view date = {},
                       std::string_view lot  = {})
        : ComponentInfo(pn, mfr)
        , package_variant(variant)
        , date_code(date)
        , lot_number(lot) {}
};

// ============================================================================
// DeviceInfo — identity for a peripheral device (joystick, drive, tape, etc.)
// ============================================================================

struct DeviceInfo : ComponentInfo {
    std::string_view model;  // "Competition Pro", "1541-II"

    constexpr DeviceInfo() = default;
    constexpr DeviceInfo(std::string_view pn, std::string_view mfr,
                         std::string_view mdl = {})
        : ComponentInfo(pn, mfr), model(mdl) {}
};

// ============================================================================
// SystemInfo — identity for a complete emulated machine
// ============================================================================

struct SystemInfo : ComponentInfo {
    std::string_view short_name;   // "C64"
    std::string_view description;  // "Commodore 64 home computer"

    constexpr SystemInfo() = default;
    constexpr SystemInfo(std::string_view pn, std::string_view mfr,
                         std::string_view sn = {},
                         std::string_view desc = {})
        : ComponentInfo(pn, mfr), short_name(sn), description(desc) {}
};
