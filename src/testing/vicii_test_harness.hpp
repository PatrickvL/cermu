#pragma once
// =============================================================================
// VIC-II Register Test Harness
// =============================================================================
// Embeds a 6510 machine-code test program that exercises all VIC-II register
// bitfields and communicates results back to the C++ verification layer via
// a zero-page signalling protocol.
//
// Activated by --vicii-test on the command line.
//
// KERNAL boot is accelerated by patching RAMTAS to skip the memory test
// (~1.2M cycles → ~8K cycles for the page-clear portion only).
// =============================================================================

#include "systems/commodore/c64/c64_system.hpp"
#include <cstdint>
#include <cstdio>

// ---------------------------------------------------------------------------
// Zero-page signalling protocol (6510 → C++ host)
// ---------------------------------------------------------------------------
// The 6510 test program is much faster than one frame — it completes all tests
// within a few frames.  Instead of per-frame polling, results are accumulated
// in a RAM buffer that the harness reads after the program signals "done".
//
// Zero-page locations:
//   $02  = done flag: $FF = all tests finished
//   $08  = results-write-pointer low byte
//   $09  = results-write-pointer high byte
//
// Results buffer at $0400 (screen RAM, 1 KB):
//   Each entry = 5 bytes:  [test_num, sub_test, result, expected, actual]
//   Maximum ~200 entries in 1 KB.
// ---------------------------------------------------------------------------
namespace vicii_test {

constexpr uint16_t ZP_DONE_FLAG   = 0x02;  // $FF = all tests complete
constexpr uint16_t ZP_RESULT_PTR_LO = 0x08;
constexpr uint16_t ZP_RESULT_PTR_HI = 0x09;

constexpr uint8_t  RESULT_PASS    = 0x01;
constexpr uint8_t  RESULT_FAIL    = 0xFF;
constexpr uint8_t  DONE_SIGNAL    = 0xFF;

// Results buffer location and entry size
constexpr uint16_t RESULTS_BASE   = 0x0400;  // Screen RAM
constexpr uint8_t  RESULT_ENTRY_SIZE = 5;    // bytes per result
constexpr uint16_t RESULTS_MAX    = 200;     // max entries

// Load address for the test program in C64 RAM
constexpr uint16_t TEST_LOAD_ADDR = 0xC000;

// Sprite data address (page-aligned, in default VIC bank $0000-$3FFF)
constexpr uint16_t SPRITE_DATA_ADDR = 0x2000;

// ---------------------------------------------------------------------------
// Test group identifiers
// ---------------------------------------------------------------------------
enum test_group_t : uint8_t {
    GROUP_REG_RW        = 0x10,  // Register read/write roundtrip
    GROUP_COLOR_MASK    = 0x20,  // 4-bit color register masking
    GROUP_READONLY      = 0x30,  // Read-only register behavior
    GROUP_SPRITE_COORD  = 0x40,  // Sprite coordinate registers
    GROUP_SPRITE_ENABLE = 0x50,  // Sprite enable/display
    GROUP_SPRITE_FLAGS  = 0x60,  // Sprite flag registers (priority, MC, XE, YE)
    GROUP_RASTER        = 0x70,  // Raster counter / IRQ
    GROUP_COLLISION     = 0x80,  // Collision registers
    GROUP_MEMORY_PTR    = 0x90,  // Memory pointer register $D018
};

// ---------------------------------------------------------------------------
// Harness state
// ---------------------------------------------------------------------------
struct vicii_test_state_t {
    bool  active;             // Test mode enabled
    bool  program_injected;   // Test program written to RAM
    bool  all_done;           // All tests completed
    int   total_pass;
    int   total_fail;
    int   frames_run;
    int   max_frames;         // Safety limit
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Patch the KERNAL ROM in-place to skip the RAMTAS memory test
/// AND redirect the BASIC cold-start JMP to the test program.
/// Must be called AFTER bus.init_flat_mem_pointers().
///
/// NOTE: The RAMTAS skip is now delegated to the shared c64_patch_skip_memtest()
/// in c64_kernal_patches.h.  This function still owns Patch B (BASIC redirect).
void patch_kernal_for_test(C64System* c64);
/* TODO:
Once the kernal boot is passed after the RAMTAS call, it would be good to
restore the original contents of the KERNAL ROM area that got patched, so
client code can't even detect anymore it was there.  An alternative would be
to detect the CPU emulator PC entering the RAMTAS function and immediately
jump past it.  That way there would not even be a patch in ROM, at the cost
of detecting PC hitting the RAMTAS ROM code...
*/

/// Inject the 6510 test program into RAM at TEST_LOAD_ADDR.
void inject_test_program(C64System* c64);

/// Initialize harness state.
void harness_init(vicii_test_state_t* state);

/// Poll once per frame; returns true while tests are still running.
bool harness_poll(vicii_test_state_t* state, C64System* c64);

/// Read the results buffer from RAM and print detailed results.
void harness_read_results(vicii_test_state_t* state, C64System* c64);

/// Print final summary.
void harness_summary(const vicii_test_state_t* state);

} // namespace vicii_test
