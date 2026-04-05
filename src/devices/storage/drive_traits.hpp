#pragma once
// drive_traits.hpp — Compile-time trait descriptors for IEC disk drive families
//
// Follows the same NTTP (non-type template parameter) pattern as CPUTraits
// in fam65xx_processor_traits.hpp.  Each drive model is an inline constexpr
// DriveTraits value; the drive system template instantiates on a reference
// to it, using `if constexpr` for variant-specific behavior.
//
// 1541 family (single-sided 5.25" MFM/GCR, MOS 6502 + 2× VIA 6522):
//   - 1541     Original (long board, ~325425 ROM variants)
//   - 1541C    Cost-reduced, same hardware, different case
//   - 1541-II  External PSU, minor board revision
//
// Future families (different architecture — separate system templates):
//   - 1570/1571  Double-sided, CIA 6526 + WD1770, burst mode
//   - 1581       3.5" drive, WD1772, entirely different architecture
//   - CMD FD     3.5" drive, 65C816, custom controller

#include <cstdint>

// ── Drive family identifiers ────────────────────────────────────────────────

enum class DriveFamily : uint8_t {
    CBM_1541,    // 1541 / 1541C / 1541-II
    CBM_1570,    // 1570 (single-sided 1571)
    CBM_1571,    // 1571 (double-sided, burst mode)
    CBM_1581,    // 1581 (3.5", WD1772)
};

// ── Drive model identifiers (within a family) ──────────────────────────────

enum class DriveModel : uint8_t {
    CBM_1541,        // Original 1541 (various board revisions)
    CBM_1541C,       // Cost-reduced 1541 (same electronics, short board)
    CBM_1541_II,     // 1541-II (external PSU, JiffyDOS-ready)
};

// ── GCR encoding parameters ────────────────────────────────────────────────

struct GCRParams {
    uint8_t  tracks;              // Number of tracks (35 for 1541, 70 for 1571)
    uint8_t  max_sectors;         // Maximum sectors per track (21 for 1541)
    bool     double_sided;        // True for 1571
    uint32_t rpm;                 // Spindle speed (300 RPM for 1541)

    // Speed zone boundaries (1541: zones 0-3, different bit rates per zone)
    // Zone 0: tracks 31-35 (17 sectors), Zone 1: 25-30 (18), Zone 2: 18-24 (19), Zone 3: 1-17 (21)
    struct SpeedZone {
        uint8_t first_track;      // First track in this zone (1-based)
        uint8_t sectors;          // Sectors per track in this zone
        uint32_t bit_rate;        // Bits per second for this zone
    };
    static constexpr int MAX_SPEED_ZONES = 4;
    SpeedZone zones[MAX_SPEED_ZONES];
    uint8_t num_zones;
};

// ── Drive traits (NTTP-compatible constexpr struct) ─────────────────────────

struct DriveTraits {
    // Identity
    const char*  vendor;          // "Commodore"
    const char*  model_id;        // "1541", "1541C", "1541-II"
    const char*  display_name;    // "Commodore 1541"
    DriveFamily  family;
    DriveModel   model;

    // CPU
    uint32_t     cpu_clock_hz;    // 1000000 for 1541 (1 MHz)
    bool         cpu_has_illegal; // True for NMOS 6502

    // Memory map
    uint16_t     ram_size;        // 2048 for 1541
    uint16_t     rom_size;        // 16384 for 1541 (16 KB)
    uint16_t     rom_start;       // 0xC000 for 1541
    uint16_t     via1_base;       // 0x1800 for 1541 (IEC bus VIA)
    uint16_t     via2_base;       // 0x1C00 for 1541 (drive mechanics VIA)

    // ROM
    const char*  rom_filename;    // "1541-c000.901229-05.bin" etc.

    // Disk mechanics
    GCRParams    gcr;

    // ── Helpers ─────────────────────────────────────────────────────────

    constexpr bool is_1541_family() const { return family == DriveFamily::CBM_1541; }
    constexpr uint32_t ram_mask() const { return ram_size - 1; }  // Power-of-2 assumed
};

// ── 1541 GCR speed zones (shared across all 1541 variants) ─────────────────

inline constexpr GCRParams GCR_1541 = {
    .tracks       = 35,
    .max_sectors  = 21,
    .double_sided = false,
    .rpm          = 300,
    .zones = {
        // Zone 3: tracks 1-17, 21 sectors/track, ~307692 bps
        { .first_track = 1,  .sectors = 21, .bit_rate = 307692 },
        // Zone 2: tracks 18-24, 19 sectors/track, ~285714 bps
        { .first_track = 18, .sectors = 19, .bit_rate = 285714 },
        // Zone 1: tracks 25-30, 18 sectors/track, ~266667 bps
        { .first_track = 25, .sectors = 18, .bit_rate = 266667 },
        // Zone 0: tracks 31-35, 17 sectors/track, ~250000 bps
        { .first_track = 31, .sectors = 17, .bit_rate = 250000 },
    },
    .num_zones = 4,
};

// ── Concrete drive traits ──────────────────────────────────────────────────

inline constexpr DriveTraits CBM1541Traits = {
    .vendor        = "Commodore",
    .model_id      = "1541",
    .display_name  = "Commodore 1541",
    .family        = DriveFamily::CBM_1541,
    .model         = DriveModel::CBM_1541,
    .cpu_clock_hz  = 1000000,
    .cpu_has_illegal = true,
    .ram_size      = 2048,
    .rom_size      = 16384,
    .rom_start     = 0xC000,
    .via1_base     = 0x1800,
    .via2_base     = 0x1C00,
    .rom_filename  = "1541-c000.901229-05.bin",
    .gcr           = GCR_1541,
};

inline constexpr DriveTraits CBM1541CTraits = {
    .vendor        = "Commodore",
    .model_id      = "1541C",
    .display_name  = "Commodore 1541C",
    .family        = DriveFamily::CBM_1541,
    .model         = DriveModel::CBM_1541C,
    .cpu_clock_hz  = 1000000,
    .cpu_has_illegal = true,
    .ram_size      = 2048,
    .rom_size      = 16384,
    .rom_start     = 0xC000,
    .via1_base     = 0x1800,
    .via2_base     = 0x1C00,
    .rom_filename  = "1541-c000.251968-03.bin",   // 1541C ROM revision
    .gcr           = GCR_1541,
};

inline constexpr DriveTraits CBM1541IITraits = {
    .vendor        = "Commodore",
    .model_id      = "1541-II",
    .display_name  = "Commodore 1541-II",
    .family        = DriveFamily::CBM_1541,
    .model         = DriveModel::CBM_1541_II,
    .cpu_clock_hz  = 1000000,
    .cpu_has_illegal = true,
    .ram_size      = 2048,
    .rom_size      = 16384,
    .rom_start     = 0xC000,
    .via1_base     = 0x1800,
    .via2_base     = 0x1C00,
    .rom_filename  = "1541-II.251968-03.bin",     // 1541-II ROM
    .gcr           = GCR_1541,
};
