// =============================================================================
// C64 KERNAL ROM Patches — Implementation
// =============================================================================

#include "core/cermu.hpp"
#include "systems/commodore/c64/c64_kernal_patches.hpp"
#include <cstdio>
#include <cstring>

bool c64_patch_skip_memtest(C64System* c64) {
    if (!c64) {
        log_info("C64: WARNING — cannot patch KERNAL (system is null)\n");
        return false;
    }

    if (!c64->board_.kernal.data()) {
        log_info("C64: WARNING — cannot patch KERNAL (ROM not loaded)\n");
        return false;
    }

    uint8_t* rom = c64->board_.kernal.data();

    // $FD5F - $E000 = 0x1D5F (ROM offset for RAMTAS memory test)
    constexpr uint16_t RAMTAS_OFFSET = 0x1D5F;

    // Check for the original RAMTAS signature: LDX #$3C at $FD5F
    if (rom[RAMTAS_OFFSET] != 0xA2 || rom[RAMTAS_OFFSET + 1] != 0x3C) {
        // Already patched or unexpected KERNAL — skip silently
        return false;
    }

    // Patch: skip the read/write test loop, jump directly to SETTOP ($FD88)
    // after setting the top-of-memory pointer in $C1/$C2.
    static const uint8_t ramtas_patch[] = {
        0xA0, 0x00,       // LDY #$00
        0x85, 0xC1,       // STA $C1         (A is 0 from page clearing)
        0xA9, 0xA0,       // LDA #$A0        (top page = $A0)
        0x85, 0xC2,       // STA $C2
        0x4C, 0x88, 0xFD, // JMP $FD88       (skip to SETTOP)
    };
    memcpy(&rom[RAMTAS_OFFSET], ramtas_patch, sizeof(ramtas_patch));

    log_info("C64: Patched RAMTAS at $FD5F — memory test skipped (fast boot)\n");
    return true;
}
