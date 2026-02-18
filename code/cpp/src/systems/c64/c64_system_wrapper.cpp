#include "c64_system_wrapper.h"
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
    
    printf("C64: System initialized successfully\n");
    return true;
}

void C64SystemWrapper::shutdown() {
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

    // Parse the file into a format result
    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("C64: Failed to load file: %s\n", result.error_msg);
        format_load_result_free(&result);
        return false;
    }

    // Defer loading until KERNAL/BASIC boot completes.
    // The CPU starts at $FCE2 (KERNAL reset vector) and must complete its
    // full boot sequence — IOINIT, RAMTAS, RESTOR, screen init, BASIC cold
    // start — before we write program data to RAM. This prevents BASIC's
    // NEW routine from zeroing $0801/$0802 and corrupting the loaded program.
    //
    // FUTURE OPTIMIZATION: Programs that load outside the BASIC area (e.g.
    // raw ML at $C000+) and don't rely on BASIC pointers or KERNAL-
    // initialized hardware could be loaded immediately, saving ~1.5M cycles
    // of emulated boot time. Combined with KERNAL memory-test/clear loop
    // patching, this could enable near-instant startup for many programs.
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
        apply_sid_load(sid);
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
// SID Loader — Inject 6502 player stub for SID music playback
// ============================================================================
//
// For PSID files with play_addr != 0:
//   1. Write tune data to load_addr
//   2. Write a small 6502 player stub in $0340–$03FF (cassette buffer area)
//   3. The stub: SEI, call init(subtune), hook CIA1 Timer A IRQ to call play,
//      set timer period for correct playback rate, CLI, loop forever
//
// For PSID files with play_addr == 0 (tune provides own IRQ handler):
//   1. Write tune data to load_addr
//   2. Call init(subtune) — the tune sets up its own interrupt vectors
//
// For RSID files:
//   1. Write tune data to load_addr
//   2. Call init(subtune) — tune handles its own environment completely
//
// The KERNAL IRQ dispatcher at $FF48 does:
//   PHA / TXA / PHA / TYA / PHA / JMP ($0314)
// The default ($0314) = $EA31 (normal KERNAL IRQ handler).
// $EA81 is the KERNAL IRQ exit: PLA / TAY / PLA / TAX / PLA / RTI.
//
// Our player stub hooks $0314/$0315 to point to a small routine that:
//   JSR play_addr / LDA $DC0D (ack CIA1) / JMP $EA81 (KERNAL exit)
//
// Speed: The speed_flags word has one bit per subtune (0-based):
//   bit=0 → vertical blank rate (~50Hz PAL, ~60Hz NTSC)
//   bit=1 → CIA timer (default 60Hz)
// PAL clock = 985248 Hz → 50Hz = 19705 cycles, 60Hz = 16421 cycles
// ============================================================================

void C64SystemWrapper::apply_sid_load(const sid_header_t* sid) {
    if (!sid || !c64_ || !c64_->ram) return;

    const char* type_str = (sid->type == SID_TYPE_RSID) ? "RSID" : "PSID";
    printf("C64: %s loader — \"%s\" by %s\n", type_str, sid->name, sid->author);
    printf("C64: load=$%04X init=$%04X play=$%04X songs=%u default=%u\n",
           sid->load_addr, sid->init_addr, sid->play_addr,
           sid->num_songs, sid->start_song);

    uint8_t* ram = c64_->ram->memory;
    mos6510_t* cpu = (mos6510_t*)c64_->mos6510;

    // ---- Step 1: Write tune payload to C64 RAM ----
    const program_data_t* prog = &pending_load_.result.program;
    if (prog->data && prog->data_size > 0) {
        memcpy(&ram[sid->load_addr], prog->data, prog->data_size);
        printf("C64: SID payload written: $%04X–$%04X (%zu bytes)\n",
               sid->load_addr,
               (unsigned)(sid->load_addr + prog->data_size - 1),
               prog->data_size);
    }

    // ---- Step 2: Set SID revision from metadata (v2+ flags) ----
    if (sid->version >= 2 && sid->sid_model != SID_MODEL_UNKNOWN) {
        sid_revision_t rev;
        if (sid->sid_model == SID_MODEL_8580) {
            rev = SID_REVISION_8580_R5;
        } else {
            // 6581 or "both" — prefer 6581 (most tunes are authored for it)
            rev = SID_REVISION_6581_R4AR;
        }
        if (c64_->sid) {
            mos6581_set_revision(c64_->sid, rev);
            pending_sid_revision_ = rev;
            printf("C64: SID revision set to %s (from SID file flags)\n",
                   rev == SID_REVISION_8580_R5 ? "MOS 8580" : "MOS 6581");
        }
    }

    // ---- Step 3: Determine playback timer period ----
    // Check the speed flag for the default subtune
    uint16_t subtune = sid->start_song;
    if (subtune > 0) subtune--;  // Convert 1-based to 0-based index

    bool use_cia_rate = false;
    if (subtune < 32) {
        use_cia_rate = (sid->speed_flags >> subtune) & 1;
    }

    // PAL 50Hz: 985248/50 = 19705 cycles
    // CIA 60Hz: 985248/60 = 16421 cycles
    uint16_t timer_period = use_cia_rate ? 16421 : 19705;
    printf("C64: Speed flag for subtune %u: %s (timer=%u cycles)\n",
           subtune + 1, use_cia_rate ? "CIA" : "VBI", timer_period);

    // ---- Step 4: Inject 6502 player stub at $0340 (cassette buffer) ----
    //
    // The stub structure (PSID with play_addr != 0):
    //
    //   $0340: SEI                      ; Disable interrupts during setup
    //   $0341: LDA #<play_addr
    //   $0343: STA $0314               ; IRQ vector low → our handler
    //   $0346: LDA #>play_addr
    //   $0348: STA $0315               ; IRQ vector high
    //   $034B: LDA #<timer_lo
    //   $034D: STA $DC04               ; CIA1 Timer A low
    //   $0350: LDA #>timer_hi
    //   $0352: STA $DC05               ; CIA1 Timer A high
    //   $0355: LDA #$81
    //   $0357: STA $DC0D               ; Enable CIA1 Timer A IRQ
    //   $035A: LDA #$11
    //   $035C: STA $DC0E               ; Start Timer A, continuous mode
    //   $035F: LDA #(subtune-1)
    //   $0361: JSR init_addr           ; Call tune init
    //   $0364: CLI                      ; Enable interrupts
    //   $0365: JMP $0365               ; Infinite loop (IRQ handles play)
    //
    //   IRQ handler at $0380:
    //   $0380: JSR play_addr           ; Call play routine
    //   $0383: LDA $DC0D               ; Acknowledge CIA1 interrupt
    //   $0386: JMP $EA81               ; KERNAL IRQ exit (restore regs + RTI)
    //

    const uint16_t STUB_BASE = 0x0340;
    const uint16_t IRQ_HANDLER = 0x0380;

    bool needs_timer_irq = (sid->type == SID_TYPE_PSID && sid->play_addr != 0);

    if (needs_timer_irq) {
        // Build the IRQ handler first (at $0380)
        uint16_t p = IRQ_HANDLER;
        ram[p++] = 0x20;  // JSR play_addr
        ram[p++] = (uint8_t)(sid->play_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->play_addr >> 8);
        ram[p++] = 0xAD;  // LDA $DC0D (acknowledge CIA1 interrupt)
        ram[p++] = 0x0D;
        ram[p++] = 0xDC;
        ram[p++] = 0x4C;  // JMP $EA81 (KERNAL IRQ exit)
        ram[p++] = 0x81;
        ram[p++] = 0xEA;

        // Build the init stub (at $0340)
        p = STUB_BASE;
        ram[p++] = 0x78;  // SEI

        // Hook IRQ vector $0314/$0315 → our handler at IRQ_HANDLER
        ram[p++] = 0xA9;  // LDA #<IRQ_HANDLER
        ram[p++] = (uint8_t)(IRQ_HANDLER & 0xFF);
        ram[p++] = 0x8D;  // STA $0314
        ram[p++] = 0x14;
        ram[p++] = 0x03;
        ram[p++] = 0xA9;  // LDA #>IRQ_HANDLER
        ram[p++] = (uint8_t)(IRQ_HANDLER >> 8);
        ram[p++] = 0x8D;  // STA $0315
        ram[p++] = 0x15;
        ram[p++] = 0x03;

        // Set CIA1 Timer A period
        ram[p++] = 0xA9;  // LDA #<timer_period
        ram[p++] = (uint8_t)(timer_period & 0xFF);
        ram[p++] = 0x8D;  // STA $DC04
        ram[p++] = 0x04;
        ram[p++] = 0xDC;
        ram[p++] = 0xA9;  // LDA #>timer_period
        ram[p++] = (uint8_t)(timer_period >> 8);
        ram[p++] = 0x8D;  // STA $DC05
        ram[p++] = 0x05;
        ram[p++] = 0xDC;

        // Enable CIA1 Timer A interrupt
        ram[p++] = 0xA9;  // LDA #$81
        ram[p++] = 0x81;
        ram[p++] = 0x8D;  // STA $DC0D
        ram[p++] = 0x0D;
        ram[p++] = 0xDC;

        // Start Timer A in continuous mode
        ram[p++] = 0xA9;  // LDA #$11
        ram[p++] = 0x11;
        ram[p++] = 0x8D;  // STA $DC0E
        ram[p++] = 0x0E;
        ram[p++] = 0xDC;

        // Call init subroutine: LDA #subtune, JSR init_addr
        ram[p++] = 0xA9;  // LDA #subtune (0-based)
        ram[p++] = (uint8_t)subtune;
        ram[p++] = 0x20;  // JSR init_addr
        ram[p++] = (uint8_t)(sid->init_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->init_addr >> 8);

        // CLI + infinite loop
        ram[p++] = 0x58;  // CLI
        uint16_t loop_addr = p;
        ram[p++] = 0x4C;  // JMP loop_addr
        ram[p++] = (uint8_t)(loop_addr & 0xFF);
        ram[p++] = (uint8_t)(loop_addr >> 8);

        printf("C64: Player stub at $%04X, IRQ handler at $%04X\n", STUB_BASE, IRQ_HANDLER);
    } else {
        // PSID with play_addr==0, or RSID:
        // Just call init with subtune in A register, then loop.
        // The tune's init routine sets up its own IRQ handler.
        uint16_t p = STUB_BASE;
        ram[p++] = 0xA9;  // LDA #subtune (0-based)
        ram[p++] = (uint8_t)subtune;
        ram[p++] = 0x20;  // JSR init_addr
        ram[p++] = (uint8_t)(sid->init_addr & 0xFF);
        ram[p++] = (uint8_t)(sid->init_addr >> 8);
        uint16_t loop_addr = p;
        ram[p++] = 0x4C;  // JMP loop_addr
        ram[p++] = (uint8_t)(loop_addr & 0xFF);
        ram[p++] = (uint8_t)(loop_addr >> 8);

        printf("C64: Init-only stub at $%04X (tune manages own IRQ)\n", STUB_BASE);
    }

    // ---- Step 5: Set CPU to execute the stub ----
    mos6510_set_a(cpu, (uint8_t)subtune);
    mos6510_set_x(cpu, 0);
    mos6510_set_y(cpu, 0);
    mos6510_set_s(cpu, 0xFF);    // Reset stack
    mos6510_set_pc(cpu, STUB_BASE);
    mos6510_transition_to_fetch(cpu);  // Reset pipeline for clean fetch

    printf("C64: PC set to $%04X — SID playback starting\n", STUB_BASE);
}

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
    // C64 joystick support would go here
    // TODO: Implement joystick handling
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
    if (!c64_ || !c64_->sid) return;

    // SID revision selector
    const char* sid_labels[] = { "MOS 6581", "MOS 8580" };
    int current = (c64_->sid->revision == SID_REVISION_8580_R5) ? 1 : 0;
    if (ImGui::Combo("SID Revision", &current, sid_labels, IM_ARRAYSIZE(sid_labels))) {
        sid_revision_t rev = (current == 1) ? SID_REVISION_8580_R5 : SID_REVISION_6581_R4AR;
        mos6581_set_revision(c64_->sid, rev);
        pending_sid_revision_ = rev;
        config_.custom_settings["sid_revision"] = sid_labels[current];
        printf("C64: SID revision changed to %s\n", sid_labels[current]);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("6581: analog filter distortion, volume-click digi\n"
                          "8580: cleaner filter, no distortion");
    }
#endif
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
