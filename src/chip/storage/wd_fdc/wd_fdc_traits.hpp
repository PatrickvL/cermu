#pragma once
/*
 * wd_fdc_traits.hpp — Western Digital FDC Family Traits
 *
 * The WD177x/179x floppy disk controller family shares a common 4-register
 * interface and command set.  Variants differ in:
 *   - Seek timing (WD1772 has faster step rates: 2/3/5/6ms vs 6/12/20/30ms)
 *   - Signal polarity (WD1793 uses inverted data/control buses)
 *   - Side select (WD1773 has explicit side select output)
 *   - Clock rating (6 MHz standard, 8 MHz for TT/Mega STE)
 *
 * Family members:
 *   WD1770  — Standard FDC (Acorn Archimedes, Amiga A1010)
 *   WD1772  — Fast-seek variant (Atari ST, Amiga A500)
 *   WD1773  — Side select output (uncommon)
 *   WD1793  — Inverted signals (MSX, CPC, Spectrum +3)
 *   WD2793  — WD1793 with built-in data separator
 *   FD1793  — WD1793 equivalent (different package)
 */

#include <cstdint>

namespace wd_fdc {

// ============================================================================
// Trait flags
// ============================================================================

enum class FDCStepTiming : uint8_t {
    STANDARD,   // WD1770/1793: 6/12/20/30ms step rates
    FAST,       // WD1772: 2/3/5/6ms step rates
};

enum class FDCSignalPolarity : uint8_t {
    ACTIVE_HIGH,    // WD1770/1772: active-high data bus
    INVERTED,       // WD1793: inverted data/control buses
};

struct FDCTraits {
    const char*        chip_id;
    const char*        vendor;
    const char*        display_name;
    FDCStepTiming      step_timing;
    FDCSignalPolarity  signal_polarity;
    bool               has_side_select;     // WD1773 only
    bool               has_data_separator;  // WD2793 only
    uint32_t           max_clock_hz;        // Typical max clock rating

    constexpr bool is_fast_seek() const {
        return step_timing == FDCStepTiming::FAST;
    }

    constexpr bool is_inverted() const {
        return signal_polarity == FDCSignalPolarity::INVERTED;
    }
};

// ============================================================================
// Predefined traits
// ============================================================================

inline constexpr FDCTraits WD1770Traits = {
    "WD1770", "Western Digital", "WD1770 Floppy Disk Controller",
    FDCStepTiming::STANDARD,
    FDCSignalPolarity::ACTIVE_HIGH,
    false, false,
    8'000'000,
};

inline constexpr FDCTraits WD1772Traits = {
    "WD1772", "Western Digital", "WD1772 Floppy Disk Controller",
    FDCStepTiming::FAST,
    FDCSignalPolarity::ACTIVE_HIGH,
    false, false,
    8'000'000,
};

inline constexpr FDCTraits WD1793Traits = {
    "WD1793", "Western Digital", "WD1793 Floppy Disk Controller",
    FDCStepTiming::STANDARD,
    FDCSignalPolarity::INVERTED,
    false, false,
    8'000'000,
};

inline constexpr FDCTraits WD2793Traits = {
    "WD2793", "Western Digital", "WD2793 Floppy Disk Controller",
    FDCStepTiming::STANDARD,
    FDCSignalPolarity::INVERTED,
    false, true,
    8'000'000,
};

// ============================================================================
// Shared constants — identical register layout across all family members
// ============================================================================

// Command types (high nibble)
inline constexpr uint8_t CMD_RESTORE       = 0x00;
inline constexpr uint8_t CMD_SEEK          = 0x10;
inline constexpr uint8_t CMD_STEP          = 0x20;
inline constexpr uint8_t CMD_STEP_IN       = 0x40;
inline constexpr uint8_t CMD_STEP_OUT      = 0x60;
inline constexpr uint8_t CMD_READ_SECTOR   = 0x80;
inline constexpr uint8_t CMD_WRITE_SECTOR  = 0xA0;
inline constexpr uint8_t CMD_READ_ADDRESS  = 0xC0;
inline constexpr uint8_t CMD_READ_TRACK    = 0xE0;
inline constexpr uint8_t CMD_WRITE_TRACK   = 0xF0;
inline constexpr uint8_t CMD_FORCE_INT     = 0xD0;

// Status bits
inline constexpr uint8_t ST_BUSY       = 0x01;
inline constexpr uint8_t ST_DRQ        = 0x02;
inline constexpr uint8_t ST_LOST_DATA  = 0x04;
inline constexpr uint8_t ST_CRC_ERR    = 0x08;
inline constexpr uint8_t ST_RNF        = 0x10;  // Record Not Found
inline constexpr uint8_t ST_SPIN_UP    = 0x20;
inline constexpr uint8_t ST_WRITE_PROT = 0x40;
inline constexpr uint8_t ST_MOTOR_ON   = 0x80;

} // namespace wd_fdc
