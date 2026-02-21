#pragma once

#include <cstdint>
#include "../../core/chip.h"

// ============================================================================
// MOS Technology 2114 Static RAM — 1K × 4-bit
// ============================================================================
//
// Used as Color RAM in C64 at $D800–$DBFF.
// Only the lower 4 bits of each byte are significant (color indices 0–15).
// https://www.amiga-stuff.com/hardware/1kx4-sram.html
//
// HARDWARE CONNECTION: PLA _GRW Signal Control
// ============================================
// In C64 hardware, the #WE (Write Enable) pin is driven by the PLA's _GRW
// output.  The PLA blocks writes when:
//   - I/O region is disabled (Character ROM visible instead)
//   - Address is outside $D800–$DBFF
//   - CPU is reading, not writing
//   - Memory banking prevents I/O access
//
class MOS2114 : public ChipBase {
public:
    uint8_t memory[1024];  // 1K × 4-bit Color RAM (public for VIC-II access)

    MOS2114();
    ~MOS2114() override = default;

    // --- ChipBase interface ---
    ChipIdentity chip_identity() const override;
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;

    // --- Bus interface (C-compatible statics for I/O handler table) ---
    static bus_state_t bus_read(void* context, bus_state_t bus_state);
    static bus_state_t bus_write(void* context, bus_state_t bus_state);
};

// Backward-compatibility typedef
using mos2114_t = MOS2114;

// Legacy chip descriptor (used by c64.cpp System8Bit test path)
extern chip_descriptor_t mos2114_descriptor;

// Legacy lifecycle helpers
MOS2114* mos2114_create();
void     mos2114_destroy(MOS2114* chip);

// Legacy free-function bus wrappers (used by c64_bus.cpp I/O handler table)
bus_state_t mos2114_read(void* context, bus_state_t bus_state);
bus_state_t mos2114_write(void* context, bus_state_t bus_state);
