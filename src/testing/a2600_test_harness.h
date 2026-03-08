#pragma once
// =============================================================================
// Atari 2600 Hardware Verification Test Harness
// =============================================================================
// Standalone test framework for verifying the Atari 2600's three chips at
// cycle-level accuracy:
//
//   1. TIA (CO10444) — Video generation, collision detection, audio, WSYNC
//   2. PIA 6532 RIOT — 128-byte RAM, timer (4 divider modes), I/O ports
//   3. MOS 6507 CPU  — Already validated by fam65xx test runners; tested
//                       here only at the system-integration level
//
// Additionally tests system-level concerns:
//   - Address decoding (13-bit → TIA / RIOT RAM / RIOT I/O / Cart ROM)
//   - Cartridge mapper bank switching (2K, 4K, F8, F6, F4, E0, FA, FE, 3F)
//   - CPU–TIA synchronization (WSYNC halts CPU until end of scanline)
//   - Frame timing (262 scanlines NTSC, 312 PAL)
//
// Usage:
//   1. Create harness: a2600_test::create()
//   2. Run built-in tests: a2600_test::run_all_builtin_tests()
//   3. Or build scripts programmatically and run them
//   4. Destroy: a2600_test::destroy()
// =============================================================================

#include "../chip/video/tia/tia.h"
#include "../chip/io/pia6532.h"
#include "../systems/atari2600/atari2600_constants.h"
#include "../systems/atari2600/mappers/a2600_mapper.h"
#include "../systems/atari2600/mappers/a2600_mapper_factory.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>

namespace a2600_test {

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

// TIA write register addresses (mirroring TIA constants for test scripts)
inline constexpr uint8_t TIA_W_VSYNC  = 0x00;
inline constexpr uint8_t TIA_W_VBLANK = 0x01;
inline constexpr uint8_t TIA_W_WSYNC  = 0x02;
inline constexpr uint8_t TIA_W_NUSIZ0 = 0x04;
inline constexpr uint8_t TIA_W_NUSIZ1 = 0x05;
inline constexpr uint8_t TIA_W_COLUP0 = 0x06;
inline constexpr uint8_t TIA_W_COLUP1 = 0x07;
inline constexpr uint8_t TIA_W_COLUPF = 0x08;
inline constexpr uint8_t TIA_W_COLUBK = 0x09;
inline constexpr uint8_t TIA_W_CTRLPF = 0x0A;
inline constexpr uint8_t TIA_W_REFP0  = 0x0B;
inline constexpr uint8_t TIA_W_REFP1  = 0x0C;
inline constexpr uint8_t TIA_W_PF0    = 0x0D;
inline constexpr uint8_t TIA_W_PF1    = 0x0E;
inline constexpr uint8_t TIA_W_PF2    = 0x0F;
inline constexpr uint8_t TIA_W_RESP0  = 0x10;
inline constexpr uint8_t TIA_W_RESP1  = 0x11;
inline constexpr uint8_t TIA_W_RESM0  = 0x12;
inline constexpr uint8_t TIA_W_RESM1  = 0x13;
inline constexpr uint8_t TIA_W_RESBL  = 0x14;
inline constexpr uint8_t TIA_W_AUDC0  = 0x15;
inline constexpr uint8_t TIA_W_AUDC1  = 0x16;
inline constexpr uint8_t TIA_W_AUDF0  = 0x17;
inline constexpr uint8_t TIA_W_AUDF1  = 0x18;
inline constexpr uint8_t TIA_W_AUDV0  = 0x19;
inline constexpr uint8_t TIA_W_AUDV1  = 0x1A;
inline constexpr uint8_t TIA_W_GRP0   = 0x1B;
inline constexpr uint8_t TIA_W_GRP1   = 0x1C;
inline constexpr uint8_t TIA_W_ENAM0  = 0x1D;
inline constexpr uint8_t TIA_W_ENAM1  = 0x1E;
inline constexpr uint8_t TIA_W_ENABL  = 0x1F;
inline constexpr uint8_t TIA_W_HMP0   = 0x20;
inline constexpr uint8_t TIA_W_HMP1   = 0x21;
inline constexpr uint8_t TIA_W_HMM0   = 0x22;
inline constexpr uint8_t TIA_W_HMM1   = 0x23;
inline constexpr uint8_t TIA_W_HMBL   = 0x24;
inline constexpr uint8_t TIA_W_VDELP0 = 0x25;
inline constexpr uint8_t TIA_W_VDELP1 = 0x26;
inline constexpr uint8_t TIA_W_VDELBL = 0x27;
inline constexpr uint8_t TIA_W_RESMP0 = 0x28;
inline constexpr uint8_t TIA_W_RESMP1 = 0x29;
inline constexpr uint8_t TIA_W_HMOVE  = 0x2A;
inline constexpr uint8_t TIA_W_HMCLR  = 0x2B;
inline constexpr uint8_t TIA_W_CXCLR  = 0x2C;

// TIA read register addresses
inline constexpr uint8_t TIA_R_CXM0P  = 0x00;
inline constexpr uint8_t TIA_R_CXM1P  = 0x01;
inline constexpr uint8_t TIA_R_CXP0FB = 0x02;
inline constexpr uint8_t TIA_R_CXP1FB = 0x03;
inline constexpr uint8_t TIA_R_CXM0FB = 0x04;
inline constexpr uint8_t TIA_R_CXM1FB = 0x05;
inline constexpr uint8_t TIA_R_CXBLPF = 0x06;
inline constexpr uint8_t TIA_R_CXPPMM = 0x07;
inline constexpr uint8_t TIA_R_INPT4  = 0x0C;
inline constexpr uint8_t TIA_R_INPT5  = 0x0D;

// RIOT register addresses (within I/O space, $0280+)
inline constexpr uint16_t RIOT_SWCHA    = 0x0280;
inline constexpr uint16_t RIOT_SWACNT   = 0x0281;
inline constexpr uint16_t RIOT_SWCHB    = 0x0282;
inline constexpr uint16_t RIOT_SWBCNT   = 0x0283;
inline constexpr uint16_t RIOT_INTIM    = 0x0284;
inline constexpr uint16_t RIOT_INSTAT   = 0x0285;
inline constexpr uint16_t RIOT_TIM1T    = 0x0294;
inline constexpr uint16_t RIOT_TIM8T    = 0x0295;
inline constexpr uint16_t RIOT_TIM64T   = 0x0296;
inline constexpr uint16_t RIOT_TIM1024T = 0x0297;

// ─────────────────────────────────────────────────────────────────────────────
// Script command types
// ─────────────────────────────────────────────────────────────────────────────

enum class cmd_type_t : uint8_t {
    RESET,
    TIA_WRITE,              // Write to TIA register (addr 0x00-0x2C)
    TIA_READ_EXPECT,        // Read TIA register and assert value
    RIOT_WRITE_IO,          // Write to RIOT I/O register
    RIOT_READ_IO_EXPECT,    // Read RIOT I/O register and assert
    RIOT_WRITE_RAM,         // Write to RIOT RAM (offset 0x00-0x7F)
    RIOT_READ_RAM_EXPECT,   // Read RIOT RAM and assert
    RUN_COLOR_CLOCKS,       // Run N TIA color clocks
    RUN_CPU_CYCLES,         // Run N CPU cycles (= 3N color clocks)
    RUN_SCANLINES,          // Run N complete scanlines (= 228N color clocks)
    EXPECT_COLLISION,       // Assert collision register bits
    EXPECT_HCOUNTER,        // Assert TIA h_counter value
    EXPECT_SCANLINE,        // Assert TIA scanline value
    EXPECT_WSYNC,           // Assert WSYNC pending state
    EXPECT_TIMER,           // Assert RIOT timer value
    EXPECT_TIMER_UNDERFLOW, // Assert RIOT timer underflow flag
    EXPECT_PIXEL_COLOR,     // Assert pixel color at (x, row) in framebuffer
    SET_INPUT,              // Set RIOT port input or TIA INPT pin
    LABEL,
};

struct command_t {
    cmd_type_t type;
    uint16_t   addr;        // Register address
    uint8_t    value;       // Value for write / expected value
    uint8_t    mask;        // Bit mask for read assertions (0xFF = exact match)
    uint32_t   count;       // Cycle/scanline count for RUN commands
    uint16_t   collision;   // Expected collision bits
    uint16_t   x_pos;       // Pixel x position
    uint16_t   y_pos;       // Pixel y position / visible row
    uint32_t   pixel_color; // Expected RGBA pixel value
    bool       bool_val;    // Expected bool for EXPECT_WSYNC etc.
    char       label[32];
    int        line_number;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test result
// ─────────────────────────────────────────────────────────────────────────────

enum class result_type_t : uint8_t { PASS, FAIL, INFO };

struct result_entry_t {
    result_type_t type;
    uint32_t      cycle;
    const char*   check_name;
    int           line_number;
    char          message[256];
};

// ─────────────────────────────────────────────────────────────────────────────
// Test script
// ─────────────────────────────────────────────────────────────────────────────

struct test_script_t {
    std::string            name;
    std::string            description;
    std::vector<command_t> commands;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test harness state
// ─────────────────────────────────────────────────────────────────────────────

struct harness_t {
    tia_t      tia;                  // TIA instance (standalone, no system)
    pia6532_t  riot;                 // RIOT instance (standalone)
    uint32_t   total_color_clocks;   // Total color clocks elapsed
    uint32_t   total_cpu_cycles;     // Total CPU cycles elapsed
    std::vector<result_entry_t> results;
    int        pass_count;
    int        fail_count;
    bool       verbose;

    // Scratch framebuffer for pixel tests
    static constexpr int FB_W = 160;
    static constexpr int FB_H = 262;
    uint32_t framebuffer[FB_W * FB_H];
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/// Create a new test harness with fresh TIA + RIOT instances.
harness_t* create(bool verbose = false);

/// Destroy harness.
void destroy(harness_t* h);

/// Reset harness state (TIA + RIOT to power-on).
void reset(harness_t* h);

// ── Direct chip access ──────────────────────────────────────────────────────

/// Write to TIA register (addr 0x00-0x2C).
void tia_write(harness_t* h, uint8_t addr, uint8_t value);

/// Read from TIA register (addr 0x00-0x0D).
uint8_t tia_read(harness_t* h, uint8_t addr);

/// Write to RIOT I/O register.
void riot_write_io(harness_t* h, uint16_t addr, uint8_t value);

/// Read from RIOT I/O register.
uint8_t riot_read_io(harness_t* h, uint16_t addr);

/// Write to RIOT RAM (offset 0x00-0x7F).
void riot_write_ram(harness_t* h, uint8_t offset, uint8_t value);

/// Read from RIOT RAM.
uint8_t riot_read_ram(harness_t* h, uint8_t offset);

/// Clock N color clocks (TIA time).
void clock_color_clocks(harness_t* h, uint32_t n);

/// Clock N CPU cycles (= 3N color clocks, also ticks RIOT timer).
void clock_cpu_cycles(harness_t* h, uint32_t n);

/// Clock N complete scanlines (= N × 228 color clocks).
void clock_scanlines(harness_t* h, uint32_t n);

// ── Script execution ────────────────────────────────────────────────────────

/// Execute a test script. Returns number of failures.
int run_script(harness_t* h, const test_script_t* script);

/// Print results summary.
void print_results(const harness_t* h, const test_script_t* script);

// ── Built-in test suites ────────────────────────────────────────────────────

/// Run all built-in A2600 hardware verification tests. Returns total failures.
int run_all_builtin_tests(harness_t* h, bool verbose = true);

// --- TIA tests ---
int test_tia_register_readback(harness_t* h);
int test_tia_collision_detection(harness_t* h);
int test_tia_collision_clear(harness_t* h);
int test_tia_playfield_basic(harness_t* h);
int test_tia_playfield_reflect(harness_t* h);
int test_tia_playfield_priority(harness_t* h);
int test_tia_player_graphics(harness_t* h);
int test_tia_player_reflect(harness_t* h);
int test_tia_player_nusiz_copies(harness_t* h);
int test_tia_player_vertical_delay(harness_t* h);
int test_tia_missile_basic(harness_t* h);
int test_tia_ball_basic(harness_t* h);
int test_tia_hmove_apply(harness_t* h);
int test_tia_hmclr(harness_t* h);
int test_tia_wsync(harness_t* h);
int test_tia_vsync_frame_boundary(harness_t* h);
int test_tia_vblank_blanks_output(harness_t* h);
int test_tia_color_registers(harness_t* h);
int test_tia_input_ports(harness_t* h);
int test_tia_audio_waveforms(harness_t* h);
int test_tia_grp_delayed_latch(harness_t* h);

// --- RIOT tests ---
int test_riot_ram_read_write(harness_t* h);
int test_riot_ram_full_coverage(harness_t* h);
int test_riot_timer_1t(harness_t* h);
int test_riot_timer_8t(harness_t* h);
int test_riot_timer_64t(harness_t* h);
int test_riot_timer_1024t(harness_t* h);
int test_riot_timer_underflow(harness_t* h);
int test_riot_port_a_ddr_masking(harness_t* h);
int test_riot_port_b_ddr_masking(harness_t* h);
int test_riot_port_input_override(harness_t* h);

// --- Mapper tests ---
int test_mapper_4k(harness_t* h);
int test_mapper_f8_bank_switching(harness_t* h);
int test_mapper_f6_bank_switching(harness_t* h);
int test_mapper_f4_bank_switching(harness_t* h);
int test_mapper_e0_segment_switching(harness_t* h);
int test_mapper_3f_tigervision(harness_t* h);
int test_mapper_fa_cbs_ram_plus(harness_t* h);
int test_mapper_2k(harness_t* h);
int test_mapper_factory_detection(harness_t* h);

// --- System integration tests ---
int test_address_decoding(harness_t* h);
int test_cpu_tia_sync_timing(harness_t* h);
int test_frame_cycle_count(harness_t* h);

// ─── Extended hardware accuracy tests ───────────────────────────────────────

// --- TIA extended ---
int test_tia_nusiz_all_modes(harness_t* h);
int test_tia_missile_widths(harness_t* h);
int test_tia_ball_sizes(harness_t* h);
int test_tia_score_mode(harness_t* h);
int test_tia_hmove_all_values(harness_t* h);
int test_tia_hmove_wrap_boundaries(harness_t* h);
int test_tia_playfield_pf1_bit_order(harness_t* h);
int test_tia_playfield_pf2_all_bits(harness_t* h);
int test_tia_collision_vblank_suppression(harness_t* h);
int test_tia_multi_collision(harness_t* h);
int test_tia_resmp_lock(harness_t* h);
int test_tia_nusiz_masking(harness_t* h);
int test_tia_ctrlpf_masking(harness_t* h);
int test_tia_resp_positioning(harness_t* h);
int test_tia_rsync_reset(harness_t* h);
int test_tia_player_double_quad_width(harness_t* h);
int test_tia_vdelbl_ball_delay(harness_t* h);
int test_tia_grp_48pixel_sequence(harness_t* h);
int test_tia_mid_scanline_color_change(harness_t* h);
int test_tia_hmove_clears_after_hmclr(harness_t* h);
int test_tia_input_latch_mode(harness_t* h);

// --- RIOT extended ---
int test_riot_timer_reload_during_countdown(harness_t* h);
int test_riot_instat_flag_persistence(harness_t* h);
int test_riot_timer_underflow_countdown(harness_t* h);
int test_riot_port_a_read_via_io(harness_t* h);
int test_riot_port_b_console_switches(harness_t* h);

// ─── Cycle-level accuracy tests ─────────────────────────────────────────────

// --- TIA cycle-level ---
int test_tia_hmove_blanking(harness_t* h);
int test_tia_score_mode_priority_interaction(harness_t* h);
int test_tia_missile_copies(harness_t* h);
int test_tia_visible_row_tracking(harness_t* h);
int test_tia_scanline_228_clocks(harness_t* h);
int test_tia_wsync_release_timing(harness_t* h);
int test_tia_vblank_transition_mid_scanline(harness_t* h);
int test_tia_color_clock_rendering_window(harness_t* h);
int test_tia_hmove_motion_sign_convention(harness_t* h);
int test_tia_playfield_score_mode_with_priority(harness_t* h);
int test_tia_collision_during_hblank(harness_t* h);
int test_tia_player_copy_positions_exact(harness_t* h);
int test_tia_resp_during_hblank(harness_t* h);
int test_tia_audio_div_counter_reload(harness_t* h);
int test_tia_palette_swizzle(harness_t* h);
int test_tia_grp_vdel_cross_latch_sequence(harness_t* h);
int test_tia_collision_read_bit_mapping(harness_t* h);
int test_tia_enam_bit1_only(harness_t* h);
int test_tia_enabl_write_updates_old_on_grp1(harness_t* h);

// --- RIOT cycle-level ---
int test_riot_timer_exact_divider_counting(harness_t* h);
int test_riot_io_address_decoding_bits(harness_t* h);
int test_riot_ram_address_range(harness_t* h);

// ─── Advanced accuracy tests ────────────────────────────────────────────────

// --- TIA advanced ---
int test_tia_late_hmove_no_blanking(harness_t* h);
int test_tia_write_address_mirroring(harness_t* h);
int test_tia_read_address_mirroring(harness_t* h);
int test_tia_playfield_all_40_pixels(harness_t* h);
int test_tia_collision_persistence_across_scanlines(harness_t* h);
int test_tia_multiple_hmove_same_scanline(harness_t* h);
int test_tia_audio_poly4_cycle_length(harness_t* h);
int test_tia_audio_poly5_cycle_length(harness_t* h);
int test_tia_audio_poly9_cycle_length(harness_t* h);
int test_tia_grp_immediate_effect_on_scanline(harness_t* h);
int test_tia_color_register_bit0_ignored(harness_t* h);
int test_tia_ball_color_in_score_mode(harness_t* h);
int test_tia_missile_width_all_copies(harness_t* h);
int test_tia_hmove_blanking_clears_each_scanline(harness_t* h);
int test_tia_resp_visible_exact_position(harness_t* h);
int test_tia_vblank_dump_paddle_capacitors(harness_t* h);
int test_tia_collision_all_15_pairs(harness_t* h);

// --- RIOT advanced ---
int test_riot_address_mirror_aliasing(harness_t* h);
int test_riot_timer_write_clears_underflow(harness_t* h);
int test_riot_ram_read_write_via_io(harness_t* h);

} // namespace a2600_test
