#pragma once
// =============================================================================
// VIC-II Pixel Verification Tests
// =============================================================================
// Phase 2 of the VIC-II test suite: verifies that register writes produce
// the correct visible pixel output in the framebuffer.
//
// Unlike Phase 1 (register R/W tests via embedded 6510 code), these tests
// are driven entirely from C++:
//   1. Write VIC-II registers through the bus (triggering proper side effects)
//   2. Poke RAM for screen/color/sprite data
//   3. Run N frames to ensure rendering
//   4. Sample specific framebuffer pixels and compare to expected palette colors
//
// This catches bugs where registers read back correctly but rendering is wrong.
// =============================================================================

#include "../systems/commodore/c64/c64_system.h"
#include "../core/emulated_system.h"
#include <cstdint>

namespace vicii_test {

// ---------------------------------------------------------------------------
// Pixel test results
// ---------------------------------------------------------------------------
struct pixel_test_results_t {
    int total_pass;
    int total_fail;
    int total_tests;    // Number of pixel comparison checks
    int total_groups;   // Number of test groups executed
};

// ---------------------------------------------------------------------------
// Run all pixel verification tests.
//
// Prerequisites:
//   - c64 system is initialized and running
//   - Framebuffer is allocated and set via set_framebuffer()
//   - Phase 1 register tests have completed (CPU sitting in JMP halt loop)
//
// The function directly pokes VIC-II registers through the bus, sets up
// screen/color/sprite data in RAM, runs frames, and checks framebuffer pixels.
// ---------------------------------------------------------------------------
pixel_test_results_t run_pixel_verification_tests(
    C64System* c64,
    EmulatedSystem* system,
    uint32_t* framebuffer,
    int fb_width,
    int fb_height);

} // namespace vicii_test
