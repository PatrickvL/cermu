#include "c64_system_wrapper.h"
#include "c64_kernal_patches.h"
#include "c64_sid_player.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "../../gui/imgui_interface.h"
#include "imgui.h"
#include "../../core/formats/format_registry.h"
#include "../../core/formats/prg_format.h"
#include "../../core/formats/d64_format.h"
#include "../../core/formats/t64_format.h"
#include "../../core/formats/tap_format.h"
#include "../../core/formats/crt_format.h"
#include "../../core/formats/lnx_format.h"
#include "../../core/formats/sid_format.h"
#include "../../core/formats/commodore_load_helpers.h"
#include "../../chip/cpu/fam65xx/mos6510.h"
#include "../../devices/input/joystick_device.h"
#include "../../devices/storage/drive_1541.h"
#include "../../devices/storage/datasette_device.h"
#include "../../devices/keyboard/commodore_keyboard_device.h"
#include <cstring>
#include <cstdio>
#include <cctype>

/**
 * C64 System Wrapper Implementation
 *
 * CURRENT ARCHITECTURE:
 * =====================
 * This file implements the wrapper layer that adapts the C-style c64_t system
 * to the C++ EmulatedSystem interface. It delegates most work to C functions
 * in c64.cpp.
 *
 * CYCLE COUNTING:
 * ===============
 * - c64_->total_cycles: Internal C64 cycle counter (updated by c64_system_tick)
 * - total_cycles_: Base class cycle counter (synced in tick() method)
 * - Both are kept in sync to maintain consistency
 *
 * FRAMEBUFFER MANAGEMENT:
 * =======================
 * - VIC-II owns the actual framebuffer: c64_->vicii->pixel.framebuffer
 * - get_framebuffer() returns pointer to VIC-II's buffer
 * - set_framebuffer() delegates to c64_set_framebuffer() which calls vicii_set_framebuffer()
 *
 * FUTURE MERGER NOTES:
 * ====================
 * When merging into unified C64System class, this file's logic should become
 * direct methods of C64System. The c64_t pointer would be eliminated and its
 * members would become direct members of C64System class.
 *
 * Key functions to convert:
 * - Constructor logic from c64_system_create()
 * - Destructor logic from c64_system_destroy()
 * - tick() from c64_system_tick()
 * - reset() from c64_system_reset()
 * - All chip management code
 */

/** Check if load address is a typical C64 address */
static bool is_c64_load_address(uint16_t addr) {
    return addr == 0x0801 || addr == 0xC000 || addr == 0x0800 ||
           addr == 0x4000 || addr == 0x8000 || addr == 0xE000;
}

// C64 file detection
static float c64_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    // Check extensions
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        // PRG files â€” check load address
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            if (size >= 2) {
                uint16_t load_addr = data[0] | (data[1] << 8);
                // C64 BASIC start address gives highest confidence
                if (load_addr == 0x0801) return 0.95f;
                // Common C64 ML addresses
                if (load_addr == 0xC000 || load_addr == 0x0800 || load_addr == 0x4000) return 0.85f;
                // VIC-20/C16 address â€” lower confidence
                if (load_addr == 0x1001) return 0.6f;
                return 0.7f;  // Generic PRG â€” C64 is the most common Commodore system
            }
        }
        // LNX files â€” Lynx archive; parse to inspect contained files' load addresses
        if (strcmp(ext, ".lnx") == 0 || strcmp(ext, ".LNX") == 0) {
            commodore_lynx_t lynx;
            if (commodore_lynx_open(filepath, &lynx)) {
                commodore_lynx_directory_t dir;
                if (commodore_lynx_read_directory(&lynx, &dir)) {
                    // Check ALL PRG entries' load addresses for C64 addresses
                    bool found_c64 = false;
                    bool found_any = false;
                    for (unsigned i = 0; i < dir.file_count; i++) {
                        if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                            size_t off = dir.entries[i].data_offset;
                            if (off + 1 < lynx.data_size) {
                                uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off+1] << 8);
                                found_any = true;
                                if (is_c64_load_address(addr)) {
                                    found_c64 = true;
                                    break;
                                }
                            }
                        }
                    }
                    commodore_lynx_close(&lynx);
                    if (found_c64) return 0.95f;
                    if (found_any) return 0.5f;
                }
                commodore_lynx_close(&lynx);
            }
            return 0.6f;  // Could not inspect â€” C64 is most common
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            // D64 disk images â€” inspect first PRG's load address to distinguish systems
            commodore_d64_t d64;
            if (commodore_d64_open(filepath, &d64)) {
                commodore_prg_t prg = {};
                if (commodore_d64_extract_first_prg(&d64, &prg)) {
                    float score = is_c64_load_address(prg.load_addr) ? 0.95f : 0.6f;
                    commodore_prg_free(&prg);
                    commodore_d64_close(&d64);
                    return score;
                }
                commodore_d64_close(&d64);
            }
            // Could not inspect â€” still likely C64 (most common system)
            if (size == D64_STANDARD_SIZE || size == D64_STANDARD_SIZE_ERR ||
                size == D64_EXTENDED_SIZE || size == D64_EXTENDED_SIZE_ERR) {
                return 0.7f;
            }
            return 0.6f;  // Non-standard size but .d64 extension
        }
        if (strcmp(ext, ".t64") == 0 || strcmp(ext, ".T64") == 0) {
            // T64 tape archives are C64-centric
            return 0.85f;
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            // Check TAP header to see if this is specifically a C64 tape
            int platform = commodore_tap_identify_platform(filepath);
            if (platform == 0) return 0.95f;  // C64 TAP
            if (platform == 1) return 0.3f;   // VIC-20 TAP
            return 0.6f;  // Unknown or error â€” C64 is most common
        }
        if (strcmp(ext, ".crt") == 0 || strcmp(ext, ".CRT") == 0) {
            // CRT cartridge files
            if (size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0) {
                return 1.0f;  // Perfect match
            }
        }
        if (strcmp(ext, ".sid") == 0 || strcmp(ext, ".SID") == 0) {
            // SID music files — check PSID/RSID magic
            if (size >= 4 && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0)) {
                return 1.0f;  // Perfect match — unambiguous magic
            }
            return 0.9f;  // Extension match only
        }
    }
    
    return 0.0f;
}

/** Formats the C64 can load — used by SystemDescriptor and file dialogs. */
static const format_descriptor_t* const c64_formats[] = {
    &PRG_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR, &CRT_FORMAT_DESCRIPTOR,
    &T64_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR,
    &SID_FORMAT_DESCRIPTOR, &BIN_FORMAT_DESCRIPTOR, nullptr
};

static HardwareTraits create_c64_hardware_traits() {
    HardwareTraits traits;
    
    // Display
    traits.display.native_width = 403;
    traits.display.native_height = 284;
    traits.display.visible_width = 403;
    traits.display.visible_height = 284;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 0;  // Direct RGB
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // Audio
    traits.audio.format = AudioFormat::STEREO_16BIT;
    traits.audio.sample_rate_hz = 44100;
    traits.audio.channels = 2;
    traits.audio.chip_name = "SID 6581";
    
    // Timing (PAL default)
    traits.timing.cpu_frequency_hz = 985248;
    traits.timing.video_frequency_hz = 985248;
    traits.timing.audio_sample_rate_hz = 44100;
    traits.timing.target_fps = 50;
    traits.timing.cycles_per_frame = 19705;
    traits.timing.region = VideoRegion::PAL;

    // Region options
    traits.region_options.push_back({
        "PAL",
        VideoRegion::PAL,
        traits.timing,
        true
    });

    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = 1022727;
    ntsc_timing.video_frequency_hz = 1022727;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 17045;   // 1022727 / 60
    ntsc_timing.region = VideoRegion::NTSC;

    traits.region_options.push_back({
        "NTSC",
        VideoRegion::NTSC,
        ntsc_timing,
        false
    });

    // SID revision option
    traits.custom_options.push_back({
        "sid_revision",
        "SID Revision",
        "MOS 6581 has analog filter distortion and volume-click digi; "
        "MOS 8580 has a cleaner filter with no distortion.",
        { "MOS 6581", "MOS 8580" },
        0  // 6581 default
    });

    return traits;
}

static SystemDescriptor c64_descriptor = {
    "Commodore 64",
    "C64",
    "8-bit home computer with VIC-II graphics and SID sound chip (1982)",
    c64_formats,
    create_c64_hardware_traits(),
    c64_can_load_file
};

C64SystemWrapper::C64SystemWrapper()
    : EmulatedSystem()  // Call base class constructor
    , c64_(nullptr)
    , cycles_per_frame_(19705)  // PAL: 985248 Hz / 50 fps
    , gui_state_(nullptr)
{
    // Initialize C64-specific config with defaults
    c64_config_.vicii_standard = VIC_PAL;
    c64_config_.rom_config = nullptr;
    c64_config_.test_mode = C64_TEST_MODE_NORMAL;
    c64_config_.test_binary_config = nullptr;
    c64_config_.roml_present = false;
    c64_config_.romh_present = false;
    c64_config_.roml_filename = nullptr;
    c64_config_.romh_filename = nullptr;
    c64_config_.initial_exrom_state = true;
    c64_config_.initial_game_state = true;
    
    // Initialize base class members
    hardware_traits_ = c64_descriptor.hardware_traits;
    speed_multiplier_ = 1.0f;
    
    // Create persistent GUI state for menu handling
#ifdef IMGUI_VERSION
    gui_state_ = gui_create_state();
#else
    gui_state_ = nullptr;
#endif
}

C64SystemWrapper::~C64SystemWrapper() {
    shutdown();
    
    // Destroy persistent GUI state
#ifdef IMGUI_VERSION
    if (gui_state_) {
        gui_destroy_state(static_cast<gui_state_t*>(gui_state_));
        gui_state_ = nullptr;
    }
#endif
}

const SystemDescriptor& C64SystemWrapper::get_descriptor() const {
    return c64_descriptor;
}

bool C64SystemWrapper::initialize() {
    if (c64_) {
        return true;  // Already initialized
    }
    
    c64_ = c64_system_create(&c64_config_);
    if (!c64_) {
        printf("C64: Failed to create system\n");
        return false;
    }

    // Track the actual VIC-II standard this system was created with.
    // apply_configuration() can desync c64_config_.vicii_standard from
    // reality without recreating the chip; this field stays in sync.
    created_vicii_standard_ = c64_config_.vicii_standard;
    
    // Apply SID revision from configuration (set before initialize)
    if (c64_->sid) {
        mos6581_set_revision(c64_->sid, pending_sid_revision_);
        const char* rev_name = (pending_sid_revision_ == SID_REVISION_8580_R5) ? "MOS 8580" : "MOS 6581";
        printf("C64: SID revision initialized as %s\n", rev_name);
    }
    
    // Create the layered keyboard mapper for character-based input
    if (c64_->keyboard) {
        keyboard_mapper_.reset(create_c64_keyboard_mapper(c64_->keyboard));
    }
    
    // Set up connector ports and wire them to the C64 hardware
    setup_connector_ports();
    
    printf("C64: System initialized successfully\n");
    return true;
}

void C64SystemWrapper::shutdown() {
    // Detach all devices before destroying the system
    for (auto& port : connector_ports_) {
        port->detach_device();
    }
    owned_devices_.clear();
    connector_ports_.clear();

    // Free any pending load that was never applied
    if (pending_load_.active) {
        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
    }
    if (c64_) {
        c64_system_destroy(c64_);
        c64_ = nullptr;
    }
}

void C64SystemWrapper::reset() {
    // Clear any pending deferred load (will be re-set by the next load_file call)
    if (pending_load_.active) {
        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
    }
    boot_completed_ = false;
    sid_player_active_ = false;
    active_sid_data_.clear();
    if (c64_) {
        c64_system_reset(c64_);

        // Clear the memory locations that is_basic_ready() checks, so stale
        // values from the previous session don't cause premature detection.
        // KERNAL boot will set these properly: RAMTAS clears zero page
        // (including $2D), $E453 copies the vector table ($0302/$0303),
        // and NEW sets VARTAB ($2D) to TXTTAB+2.
        if (c64_->ram) {
            c64_->ram->memory[0x0302] = 0;
            c64_->ram->memory[0x0303] = 0;
            c64_->ram->memory[0x002D] = 0;
            // Clear the keyboard buffer count so is_basic_ready() doesn't
            // get stuck waiting for a stale non-zero $C6 left by a
            // previously running program.
            c64_->ram->memory[0x00C6] = 0;
        }
    }
}

void C64SystemWrapper::tick() {
    if (c64_) {
        c64_system_tick(c64_);
        
        // CRITICAL: Sync base class cycle counter with C64's internal counter
        total_cycles_ = c64_->total_cycles;

        // Check if a deferred file load is waiting for BASIC to reach READY
        if (pending_load_.active && is_basic_ready()) {
            apply_pending_load();
        }
    }
}

void C64SystemWrapper::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);

    // Hot loop: call the C system tick directly to avoid per-cycle overhead
    // from the wrapper tick() (which checks pending_load_ on every cycle).
    // This cuts ~19,705 virtual dispatches + conditional checks per frame.
    if (c64_) {
        for (uint32_t i = 0; i < adjusted_cycles; i++) {
            c64_system_tick(c64_);
        }

        // Sync cycle counter once per frame instead of per-cycle
        total_cycles_ = c64_->total_cycles;

        // Check deferred load once per frame (only active during boot)
        if (pending_load_.active && is_basic_ready()) {
            apply_pending_load();
        }

        // Tick all attached peripheral devices (datasette timing, 1541 IEC, etc.)
        tick_peripherals();
    }
}

// ============================================================================
// Commodore Load Helper Callbacks -- C64-specific
// ============================================================================

static uint8_t c64_mem_read(void* ctx, uint16_t addr) {
    ram_t* ram = static_cast<ram_t*>(ctx);
    return ram->memory[addr];
}

static void c64_mem_write_byte(void* ctx, uint16_t addr, uint8_t val) {
    ram_t* ram = static_cast<ram_t*>(ctx);
    ram->memory[addr] = val;
}

static void c64_mem_write_block(void* ctx, uint16_t addr,
                                const uint8_t* data, size_t len) {
    ram_t* ram = static_cast<ram_t*>(ctx);
    memcpy(&ram->memory[addr], data, len);
}

bool C64SystemWrapper::load_file(const char* filepath) {
    if (!c64_) {
        printf("C64: System not initialized\n");
        return false;
    }

    printf("C64: Loading file: %s\n", filepath);

    // Clear any previous pending load
    if (pending_load_.active) {
        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
    }
    sid_player_active_ = false;
    active_sid_data_.clear();

    // Parse the file into a format result
    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("C64: Failed to load file: %s\n", result.error_msg);
        format_load_result_free(&result);
        return false;
    }

    // For SID files: ensure the C64 is configured with the correct region
    // (PAL/NTSC) and SID revision, reset for clean state, and patch KERNAL
    // to skip the RAMTAS memory test for near-instant boot.
    const sid_header_t* sid_check = sid_get_metadata(&result);
    if (sid_check) {
        ensure_compatible_for_sid(sid_check);
    }

    // Defer loading until KERNAL/BASIC boot completes.
    // The CPU starts at $FCE2 (KERNAL reset vector) and must complete its
    // full boot sequence — IOINIT, RAMTAS, RESTOR, screen init, BASIC cold
    // start — before we write program data to RAM. This prevents BASIC's
    // NEW routine from zeroing $0801/$0802 and corrupting the loaded program.
    pending_load_.result = result;  // Transfer ownership (don't free yet)
    pending_load_.filepath = filepath;
    pending_load_.active = true;

    printf("C64: File parsed — deferred until BASIC READY\n");
    return true;
}

bool C64SystemWrapper::is_basic_ready() const {
    if (!c64_ || !c64_->ram) return false;

    const uint8_t* ram = c64_->ram->memory;

    // The BASIC warm-start vector at $0302/$0303 is set to $A483 by the
    // very first subroutine of the cold-start sequence (JSR $E453, which
    // copies the vector table to $0300-$030B).  However, BASIC's NEW
    // routine — which zeros $0801/$0802 — doesn't run until the THIRD
    // subroutine (JSR $E422 → JMP $A644).  If we inject program data
    // after the vector is set but BEFORE NEW runs, NEW will overwrite
    // our first two bytes at $0801/$0802 with $00, making BASIC think
    // no program exists.
    //
    // To avoid this, we also check VARTAB ($2D).  During cold boot,
    // RAMTAS clears all of zero page ($2D = $00).  The BASIC cold-start
    // code at $E3BF does NOT touch $2D.  Only NEW (at $A651) sets it to
    // TXTTAB+2 = $03.  So $2D != $00 guarantees NEW has already run.
    //
    // After the first boot completes, we skip the VARTAB check because
    // a program could legitimately set VARTAB to an address whose low
    // byte is $00 (e.g. $1000).
    if (ram[0x0302] != 0x83 || ram[0x0303] != 0xA4)
        return false;
    if (ram[0x00C6] != 0)
        return false;
    if (!boot_completed_ && ram[0x002D] == 0)
        return false;

    return true;
}

void C64SystemWrapper::apply_pending_load() {
    if (!pending_load_.active || !c64_) return;

    // =========================================================================
    // SID FILE PATH — Inject 6502 player stub instead of BASIC auto-run
    // =========================================================================
    const sid_header_t* sid = sid_get_metadata(&pending_load_.result);
    if (sid) {
        // Compute 0-based subtune index from the 1-based start_song
        uint16_t subtune = sid->start_song;
        if (subtune > 0) subtune--;

        c64_apply_sid_load(c64_, sid, &pending_load_.result.program, subtune);

        // Keep a copy of the SID header and payload for subtune switching
        active_sid_header_ = *sid;
        const auto& prog = pending_load_.result.program;
        if (prog.data && prog.data_size > 0) {
            active_sid_data_.assign(prog.data, prog.data + prog.data_size);
        } else {
            active_sid_data_.clear();
        }
        active_subtune_ = subtune;
        sid_player_active_ = true;

        // Track the SID revision that was applied so the GUI stays in sync
        if (sid->version >= 2 && sid->sid_model != SID_MODEL_UNKNOWN) {
            pending_sid_revision_ = (sid->sid_model == SID_MODEL_8580)
                                    ? SID_REVISION_8580_R5
                                    : SID_REVISION_6581_R4AR;
        }
        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
        boot_completed_ = true;
        return;
    }

    // =========================================================================
    // STANDARD PATH — PRG / D64 / T64 / CRT / LNX / BIN
    // =========================================================================
    printf("C64: BASIC READY — applying deferred load\n");

    commodore_load_context_t ctx = {};
    ctx.system_name     = "C64";
    ctx.write_byte      = c64_mem_write_byte;
    ctx.write_block     = c64_mem_write_block;
    ctx.mem_read        = c64_mem_read;
    ctx.mem_ctx         = c64_->ram;
    ctx.basic_params    = &COMMODORE_BASIC_C64;
    ctx.basic_start_addrs[0] = 0x0801;
    ctx.default_raw_addr = 0xC000;
    // No set_pc for deferred loads — BASIC programs use RUN injection,
    // and even ML programs benefit from full KERNAL init already done.
    ctx.set_pc          = nullptr;
    ctx.pc_ctx          = nullptr;

    commodore_apply_load_result(&ctx, &pending_load_.result,
                                pending_load_.filepath.c_str());

    format_load_result_free(&pending_load_.result);
    pending_load_.active = false;
    boot_completed_ = true;
}

// ============================================================================
// ensure_compatible_for_sid — Auto-configure the C64 for SID file requirements
// ============================================================================
//
// Strategy:
//   1. Determine the needed VIC-II standard (PAL/NTSC) from the SID header's
//      video field.  If "UNKNOWN" or "BOTH", keep the current standard.
//   2. Determine the needed SID chip revision from the SID header's model
//      field.  If "UNKNOWN" or "BOTH", keep the current revision.
//   3. If the region must change, we must fully recreate the C64 because the
//      VIC-II chip is wired to a specific standard at creation time.
//      shutdown() → update config → initialize() gives a fresh system.
//   4. If the region is already correct, just reset() for a clean state.
//   5. In both cases, apply the SID revision and patch the KERNAL to skip
//      the RAMTAS memory test (fast boot for SID playback).
// ============================================================================

void C64SystemWrapper::ensure_compatible_for_sid(const sid_header_t* sid) {
    if (!sid) return;

    // ---- Determine needed video standard ----
    vicii_standard_t needed_standard = c64_config_.vicii_standard;  // default: keep
    int needed_region_index = config_.region_option_index;

    if (sid->video == SID_VIDEO_PAL) {
        needed_standard = VIC_PAL;
        needed_region_index = 0;
    } else if (sid->video == SID_VIDEO_NTSC) {
        needed_standard = VIC_NTSC;
        needed_region_index = 1;
    }
    // SID_VIDEO_UNKNOWN / SID_VIDEO_BOTH → keep current

    // ---- Determine needed SID revision ----
    sid_revision_t needed_revision = pending_sid_revision_;  // default: keep
    if (sid->version >= 2 && sid->sid_model == SID_MODEL_6581) {
        needed_revision = SID_REVISION_6581_R4AR;
    } else if (sid->version >= 2 && sid->sid_model == SID_MODEL_8580) {
        needed_revision = SID_REVISION_8580_R5;
    }
    // SID_MODEL_UNKNOWN / SID_MODEL_BOTH → keep current

    // ---- Apply region change (requires full recreation) ----
    // Compare against the standard the VIC-II was actually created with,
    // NOT c64_config_.vicii_standard which apply_configuration() may have
    // updated without recreating the chip.
    bool region_changed = (needed_standard != created_vicii_standard_);

    if (region_changed) {
        printf("C64: SID requires %s — recreating system (was %s)\n",
               needed_standard == VIC_NTSC ? "NTSC" : "PAL",
               c64_config_.vicii_standard == VIC_NTSC ? "NTSC" : "PAL");

        shutdown();

        // Update the internal config before recreation
        c64_config_.vicii_standard = needed_standard;
        config_.region_option_index = needed_region_index;
        pending_sid_revision_ = needed_revision;

        // Sync cycles_per_frame_ and hardware_traits_ via apply_configuration
        apply_configuration();

        if (!initialize()) {
            printf("C64: ERROR — failed to reinitialize after region change\n");
            return;
        }
    } else {
        // Same region — just update SID revision in-place and reset
        if (needed_revision != pending_sid_revision_) {
            pending_sid_revision_ = needed_revision;
            if (c64_ && c64_->sid) {
                mos6581_set_revision(c64_->sid, needed_revision);
                printf("C64: SID revision set to %s (from SID file flags)\n",
                       needed_revision == SID_REVISION_8580_R5 ? "MOS 8580" : "MOS 6581");
            }
        }
        reset();
    }

    // ---- Patch KERNAL for fast SID boot ----
    c64_patch_skip_memtest(c64_);

    // Reset boot-completed flag so the deferred load machinery works
    boot_completed_ = false;
}

// ============================================================================
uint32_t* C64SystemWrapper::get_framebuffer() {
    if (c64_ && c64_->vicii) {
        return c64_->vicii->pixel.framebuffer;
    }
    return nullptr;
}

void C64SystemWrapper::get_display_dimensions(int* width, int* height) const {
    // VIC-II visible area
    *width = 403;
    *height = 284;
}

void C64SystemWrapper::set_framebuffer(uint32_t* buffer, int width, int height) {
    if (c64_) {
        c64_set_framebuffer(c64_, buffer, width, height);
    }
}

void C64SystemWrapper::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Legacy path â€” still used when handle_keyboard_event_ex is not called
    // (e.g., from the old C64-only GUI, test harness, or non-SDL input)
    if (keyboard_mapper_) {
        // Route through the mapper with minimal info
        if (pressed) {
            keyboard_mapper_->process_key_down(
                key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(
                key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (c64_ && c64_->keyboard) {
        // No mapper â€” convert SDL keycode to EmuKey and pass through
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                commodore_keyboard_key_down(c64_->keyboard, ek, false);
            } else {
                commodore_keyboard_key_up(c64_->keyboard, ek, false);
            }
        }
    }
}

void C64SystemWrapper::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    // SID player subtune selection — intercept before keyboard mapper
    if (sid_player_active_ && pressed && !repeat) {
        if (handle_sid_player_key(key)) return;
    }

    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up(key, scancode, mod);
        }
    } else {
        // Fallback to legacy handler
        if (!repeat) {
            handle_keyboard_event(key, pressed);
        }
    }
}

void C64SystemWrapper::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void C64SystemWrapper::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

void C64SystemWrapper::handle_controller_event(int controller, int button, bool pressed) {
    // Map host controller events to the joystick device attached to the
    // appropriate control port.  Controller 0 → Port 2 (the normal C64
    // joystick port for single-player games), controller 1 → Port 1.
    int port_index = (controller == 0) ? PORT_CONTROL2 : PORT_CONTROL1;

    if (port_index < static_cast<int>(connector_ports_.size())) {
        auto* device = connector_ports_[port_index]->get_attached_device();
        auto* joy = dynamic_cast<JoystickDevice*>(device);
        if (joy) {
            // Map SDL controller buttons to joystick directions
            // SDL_CONTROLLER_BUTTON_DPAD_UP = 11, DOWN=12, LEFT=13, RIGHT=14, A=0
            switch (button) {
                case 11: joy->set_up(pressed);    break;  // DPAD_UP
                case 12: joy->set_down(pressed);  break;  // DPAD_DOWN
                case 13: joy->set_left(pressed);  break;  // DPAD_LEFT
                case 14: joy->set_right(pressed); break;  // DPAD_RIGHT
                case 0:  joy->set_fire(pressed);  break;  // A button = FIRE
                case 1:  joy->set_fire(pressed);  break;  // B button = FIRE (alt)
                default: break;
            }
        }
    }
}

// =============================================================================
// SID Player — Subtune selection via keyboard
// =============================================================================
//
// Digits 1-9:  select subtune 1-9 directly (0-based index 0-8)
// Digit 0:     select subtune 10 (0-based index 9)
// Right arrow:  next subtune (wraps from last → first)
// Left arrow:   previous subtune (wraps from first → last)
// =============================================================================

bool C64SystemWrapper::handle_sid_player_key(SDL_Keycode key) {
    if (!c64_ || active_sid_header_.num_songs == 0) return false;

    const uint16_t num_songs = active_sid_header_.num_songs;
    int new_subtune = -1;

    // Digit keys: 1→subtune 1, 2→subtune 2, ..., 9→subtune 9, 0→subtune 10
    if (key >= SDLK_0 && key <= SDLK_9) {
        int digit = (key == SDLK_0) ? 10 : (key - SDLK_0);
        if (digit <= num_songs) {
            new_subtune = digit - 1;  // Convert to 0-based
        }
    }
    // Cursor right = next subtune (with wrapping)
    else if (key == SDLK_RIGHT) {
        new_subtune = (active_subtune_ + 1) % num_songs;
    }
    // Cursor left = previous subtune (with wrapping)
    else if (key == SDLK_LEFT) {
        new_subtune = (active_subtune_ == 0) ? (num_songs - 1)
                                              : (active_subtune_ - 1);
    }
    // ESC = exit application while in SID player mode
    else if (key == SDLK_ESCAPE) {
        request_quit();
        return true;
    }

    if (new_subtune < 0) return false;
    if ((uint16_t)new_subtune == active_subtune_) return true;  // Already playing

    active_subtune_ = (uint16_t)new_subtune;
    c64_sid_switch_subtune(c64_, &active_sid_header_,
                            active_sid_data_.data(), active_sid_data_.size(),
                            active_subtune_);
    return true;
}

void C64SystemWrapper::render_system_menu_items() {
    // Forward menu creation to the old C64 GUI code
    // This allows C64-specific menus (Load Test Binary, Chip Debug Windows, etc.)
    // to appear in the multi_emu interface
    
#ifdef IMGUI_VERSION
    if (gui_state_ && c64_) {
        // Call the C64-specific menu rendering function from the old GUI
        // Using persistent gui_state_ so menu clicks persist
        gui_render_c64_system_menu_items(c64_, static_cast<gui_state_t*>(gui_state_));
    }
#endif
}

void C64SystemWrapper::render_debug_windows(void* gui_state) {
#ifdef IMGUI_VERSION
    if (!gui_state_ || !c64_) return;
    
    gui_state_t* state = static_cast<gui_state_t*>(gui_state_);
    
    // Render test binary dialog if needed
    if (state->show_test_binary_dialog) {
        gui_render_test_binary_dialog(c64_, state, nullptr);
    }
    // Iterate through all chips and render their debug windows
    system_8bit_t* sys = &c64_->system;
    for (uint8_t chip_id = 0; chip_id < sys->chip_count; chip_id++) {
        chip_entry_t* entry = &sys->chips[chip_id];
        
        if (state->show_chip_debug[chip_id] &&
            entry->desc &&
            entry->desc->render_debug_window) {
            
            entry->desc->render_debug_window(
                entry->chip,
                &state->show_chip_debug[chip_id]
            );
        }
    }
#endif
}

uint32_t C64SystemWrapper::get_target_fps() const {
    // Return target FPS from the currently-selected region (PAL=50, NTSC=60)
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[config_.region_option_index].timing.target_fps;
    }
    return 50;  // PAL default
}

void C64SystemWrapper::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    // Note: cycles_per_frame_ stays at the base value (region-dependent).
    // The multiplier is applied in run_frame() via:
    //   adjusted_cycles = cycles_per_frame_ * speed_multiplier_
}

// Note: get_total_cycles() now provided by base class (returns total_cycles_)
// However, C64 has its own cycle counter, so we need to sync it
// For now, we'll update total_cycles_ in tick() method

// Note: get_speed_multiplier() now provided by base class (returns speed_multiplier_)

// Note: Hardware trait queries (get_hardware_traits, get_current_timing,
// get_display_traits, get_audio_traits) now provided by base class
// Note: get_configuration() now provided by base class (returns config_)

bool C64SystemWrapper::set_configuration(const SystemConfiguration& config) {
    config_ = config;  // Update base class SystemConfiguration
    // TODO: Map SystemConfiguration changes to c64_config_ when needed
    return true;
}

bool C64SystemWrapper::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        const RegionOption& region = hardware_traits_.region_options[config_.region_option_index];
        cycles_per_frame_ = region.timing.cycles_per_frame;
        hardware_traits_.timing = region.timing;  // Keep active timing in sync

        // Map to legacy c64_config_t
        c64_config_.vicii_standard =
            (region.region == VideoRegion::NTSC) ? VIC_NTSC : VIC_PAL;
    }

    // Apply SID revision from custom settings
    auto sid_it = config_.custom_settings.find("sid_revision");
    if (sid_it != config_.custom_settings.end()) {
        sid_revision_t rev = SID_REVISION_6581_R4AR;
        if (sid_it->second == "MOS 8580") {
            rev = SID_REVISION_8580_R5;
        }
        if (c64_ && c64_->sid) {
            mos6581_set_revision(c64_->sid, rev);
            printf("C64: SID revision set to %s\n", sid_it->second.c_str());
        }
        // Store for later (SID may not exist yet during initial config)
        pending_sid_revision_ = rev;
    }

    return true;
}

// ============================================================================
// Auto-detect optimal configuration from file contents
// ============================================================================
SystemConfiguration C64SystemWrapper::detect_optimal_configuration(
    const char* filepath, const uint8_t* data, size_t size) {

    SystemConfiguration config = EmulatedSystem::detect_optimal_configuration(filepath, data, size);

    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;

    // --- CRT cartridge: byte 8 of the header specifies hardware type ---
    // Byte 0x08-0x09 = hardware type.  Region cannot be derived directly,
    // but certain cartridges are NTSC-only.  For now, leave at PAL default.

    // --- PRG heuristic: filenames containing "ntsc" suggest NTSC ---
    if (filepath) {
        // Case-insensitive substring search in the filename
        const char* name = strrchr(filepath, '/');
        if (!name) name = strrchr(filepath, '\\');
        if (!name) name = filepath; else name++;

        // Simple case-insensitive search for "ntsc" in the filename
        std::string lower_name(name);
        for (auto& c : lower_name) c = static_cast<char>(tolower(c));

        if (lower_name.find("ntsc") != std::string::npos) {
            // Select NTSC region (index 1)
            if (hardware_traits_.region_options.size() > 1) {
                config.region_option_index = 1;
                printf("C64: Filename contains 'ntsc' â€” selecting NTSC region\n");
            }
        }
    }

    // --- SID file: v2+ flags encode video standard directly ---
    if (ext && (strcmp(ext, ".sid") == 0 || strcmp(ext, ".SID") == 0)) {
        if (data && size >= 4 &&
            (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0)) {
            sid_header_t sid_hdr;
            if (sid_parse_header(data, size, &sid_hdr) && sid_hdr.version >= 2) {
                if (sid_hdr.video == SID_VIDEO_NTSC) {
                    if (hardware_traits_.region_options.size() > 1) {
                        config.region_option_index = 1;  // NTSC
                        printf("C64: SID flags specify NTSC — selecting NTSC region\n");
                    }
                } else if (sid_hdr.video == SID_VIDEO_PAL) {
                    config.region_option_index = 0;  // PAL
                    printf("C64: SID flags specify PAL — selecting PAL region\n");
                }
                // SID_VIDEO_BOTH or UNKNOWN: keep default (PAL)

                // Propagate SID chip model so apply_configuration pre-selects it
                if (sid_hdr.sid_model == SID_MODEL_8580) {
                    config.custom_settings["sid_revision"] = "MOS 8580";
                    printf("C64: SID flags specify 8580 chip\n");
                } else if (sid_hdr.sid_model == SID_MODEL_6581) {
                    config.custom_settings["sid_revision"] = "MOS 6581";
                    printf("C64: SID flags specify 6581 chip\n");
                }
                // SID_MODEL_BOTH or UNKNOWN: keep user's current selection
            }
        }
    }

    // --- TAP file: header byte 0x0C indicates platform/standard ---
    if (ext && (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0)) {
        // TAP v1 header: byte 0x0C = platform (0 = C64, 1 = VIC-20)
        // The TAP spec doesn't directly encode PAL/NTSC, so we keep default.
    }

    // --- D64 disk image: some SID tunes store region in metadata ---
    // Not enough reliable data in D64 to determine region automatically.

    return config;
}

void C64SystemWrapper::render_configuration_ui() {
#ifdef IMGUI_VERSION
    // SID revision is a creation-time setting — selectable only in the
    // system selection dialog via custom_options / custom_settings.
    // It cannot be changed at runtime because the SID filter model and
    // internal state are tightly coupled to the chosen revision.

    // Peripheral connector UI is rendered generically by the GUI layer
    // via EmulatedSystem::render_peripheral_connector_ui() — no C64-specific
    // duplication needed here.
#endif
}

// ============================================================================
// CONNECTOR PORT SETUP
// ============================================================================

// Connector definitions for C64 system ports
static const ConnectorDefinition c64_control_port_1_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Control Port 1",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false
};

static const ConnectorDefinition c64_control_port_2_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Control Port 2",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false
};

static const ConnectorDefinition c64_iec_serial_def = {
    ConnectorType::IEC_SERIAL,
    "IEC Serial Bus",
    ConnectorSignals::IEC_SERIAL_SIGNALS,
    ConnectorSignals::IEC_SERIAL_SIGNAL_COUNT,
    false
};

static const ConnectorDefinition c64_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Port",
    ConnectorSignals::CASSETTE_PORT_SIGNALS,
    ConnectorSignals::CASSETTE_PORT_SIGNAL_COUNT,
    false
};

static const ConnectorDefinition c64_user_port_def = {
    ConnectorType::USER_PORT,
    "User Port",
    ConnectorSignals::USER_PORT_SIGNALS,
    ConnectorSignals::USER_PORT_SIGNAL_COUNT,
    false
};

// Expansion port definition (minimal — cartridge insertion is handled separately)
static const SignalLine expansion_signals[] = {
    { "EXROM", SignalDirection::INPUT,  0 },
    { "GAME",  SignalDirection::INPUT,  1 },
    { "RESET", SignalDirection::OUTPUT, 2 },
};
static const ConnectorDefinition c64_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    expansion_signals,
    3,
    false
};

// ============================================================================
// JOYSTICK-AWARE CIA1 PORT CALLBACKS
// ============================================================================
// These replace the default CIA1 port callbacks set by c64_system_create().
// They first call the original keyboard scanning logic, then AND-in the
// joystick state from the connector port (wired-AND, matching real hardware).

// Context structure passed to the CIA1 callback overrides
struct C64PortCallbackContext {
    c64_t*              c64;
    C64SystemWrapper*   wrapper;
};

// Global instances (one per wrapper lifetime — safe because only one C64 at a time)
static C64PortCallbackContext s_port_callback_ctx;

/// CIA1 Port A read callback — combines keyboard reverse-scan with Control Port 2 joystick.
static uint8_t c64_cia1_port_a_read_with_joystick(void* context, uint8_t port_a_output) {
    auto* ctx = static_cast<C64PortCallbackContext*>(context);
    auto* c64 = ctx->c64;

    // Keyboard reverse scanning (same as original c64.cpp logic)
    uint8_t col_state = 0xFF;
    if (c64 && c64->keyboard && c64->cia1) {
        uint8_t port_b_output = c64->cia1->port_b_value;
        uint8_t row_select = ~port_b_output;
        for (int row = 0; row < 8; row++) {
            if (row_select & (1 << row)) {
                col_state &= c64->keyboard->row_open_contacts[row];
            }
        }
    }

    // AND-in Control Port 2 joystick state (bits 0-4 of CIA1 PA)
    // Joystick connector signals map to CIA1 PA:
    //   JOY_UP(0)→PA0, JOY_DOWN(1)→PA1, JOY_LEFT(2)→PA2, JOY_RIGHT(3)→PA3, JOY_FIRE(6)→PA4
    if (ctx->wrapper) {
        auto* port = ctx->wrapper->get_connector_port(C64SystemWrapper::PORT_CONTROL2);
        if (port && port->get_attached_device()) {
            uint32_t dev_signals = port->get_attached_device()->get_output_signals();
            // Map connector signal bits to CIA1 PA bits
            uint8_t joy_mask = 0xFF;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_UP)))    joy_mask &= ~0x01;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_DOWN)))  joy_mask &= ~0x02;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_LEFT)))  joy_mask &= ~0x04;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_RIGHT))) joy_mask &= ~0x08;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_FIRE)))  joy_mask &= ~0x10;
            col_state &= joy_mask;
        }
    }

    return col_state;
}

/// CIA1 Port B read callback — combines keyboard forward-scan with Control Port 1 joystick.
static uint8_t c64_cia1_port_b_read_with_joystick(void* context, uint8_t port_b_output) {
    auto* ctx = static_cast<C64PortCallbackContext*>(context);
    auto* c64 = ctx->c64;

    // Keyboard forward scanning (same as original c64.cpp logic)
    uint8_t row_state = 0xFF;
    if (c64 && c64->keyboard && c64->cia1) {
        uint8_t port_a_value = c64->cia1->port_a_value;
        uint8_t column_select = ~port_a_value;
        for (int col = 0; col < 8; col++) {
            if (column_select & (1 << col)) {
                row_state &= static_cast<uint8_t>(c64->keyboard->col_open_contacts[col]);
            }
        }
    }

    // AND-in Control Port 1 joystick state (bits 0-4 of CIA1 PB)
    if (ctx->wrapper) {
        auto* port = ctx->wrapper->get_connector_port(C64SystemWrapper::PORT_CONTROL1);
        if (port && port->get_attached_device()) {
            uint32_t dev_signals = port->get_attached_device()->get_output_signals();
            uint8_t joy_mask = 0xFF;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_UP)))    joy_mask &= ~0x01;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_DOWN)))  joy_mask &= ~0x02;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_LEFT)))  joy_mask &= ~0x04;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_RIGHT))) joy_mask &= ~0x08;
            if (!(dev_signals & (1u << ConnectorSignals::JOY_FIRE)))  joy_mask &= ~0x10;
            row_state &= joy_mask;
        }
    }

    return row_state;
}

void C64SystemWrapper::setup_connector_ports() {
    connector_ports_.clear();

    // PORT_CONTROL1 = 0 — Control Port 1 (directly connected to CIA1 Port B bits 0-4)
    add_connector_port(c64_control_port_1_def, 1);

    // PORT_CONTROL2 = 1 — Control Port 2 (directly connected to CIA1 Port A bits 0-4)
    add_connector_port(c64_control_port_2_def, 2);

    // PORT_IEC_SERIAL = 2 — IEC Serial Bus (connected to CIA2 Port A bits 3-5)
    add_connector_port(c64_iec_serial_def, 0);

    // PORT_CASSETTE = 3 — Cassette Port (CPU I/O port + CIA1 FLAG)
    add_connector_port(c64_cassette_def, 0);

    // PORT_USER = 4 — User Port (CIA2 Port B + control lines)
    add_connector_port(c64_user_port_def, 0);

    // PORT_EXPANSION = 5 — Expansion Port (cartridge slot)
    add_connector_port(c64_expansion_def, 0);

    // PORT_KEYBOARD = 6 — Internal Keyboard (always attached)
    static const ConnectorDefinition c64_keyboard_def = {
        ConnectorType::CUSTOM, "Keyboard", nullptr, 0, true  // is_internal
    };
    int kb_port = add_connector_port(c64_keyboard_def, 0);

    // Attach internal keyboard device
    auto kb_device = std::make_unique<CommodoreKeyboardDevice>(c64_ ? c64_->keyboard : nullptr);
    auto* kb_raw = kb_device.get();
    connector_ports_[kb_port]->attach_device(kb_raw);
    owned_devices_.push_back(std::move(kb_device));

    // Default: attach joystick to Control Port 2 (most C64 games use port 2)
    attach_device_to_port(1, "joystick");

    // Wire joystick-aware CIA1 callbacks (replace the defaults set by c64_system_create)
    if (c64_ && c64_->cia1) {
        s_port_callback_ctx.c64 = c64_;
        s_port_callback_ctx.wrapper = this;

        c64_->cia1->port_a_read_callback = c64_cia1_port_a_read_with_joystick;
        c64_->cia1->port_a_read_context  = &s_port_callback_ctx;
        c64_->cia1->port_b_read_callback = c64_cia1_port_b_read_with_joystick;
        c64_->cia1->port_b_read_context  = &s_port_callback_ctx;
        printf("C64: Wired joystick-aware CIA1 port callbacks\n");
    }

    printf("C64: Created %zu connector ports\n", connector_ports_.size());
}

uint32_t C64SystemWrapper::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!c64_ || !c64_->sid || !buffer || max_samples == 0) return 0;
    mos6581_generate_samples(c64_->sid, buffer, max_samples);
    return max_samples;
}

void C64SystemWrapper::set_audio_sample_rate(int sample_rate_hz) {
    if (c64_ && c64_->sid && sample_rate_hz > 0) {
        printf("C64: Updating SID sample rate from %.0f to %d Hz\n",
               c64_->sid->sample_rate, sample_rate_hz);
        mos6581_set_sample_rate(c64_->sid, static_cast<float>(sample_rate_hz));
    }
}

// Register C64 system with the registry
REGISTER_SYSTEM(c64_descriptor, []() {
    return std::make_unique<C64SystemWrapper>();
})
