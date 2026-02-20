#include "c64_system.h"
#include "c64_kernal_patches.h"
#include "c64_sid_player.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/emu_key_sdl_map.h"
// gui_state_t dependency eliminated — chip debug uses base class,
// system menu items are inlined, test binary dialog removed.
#ifdef IMGUI_VERSION
#include "imgui.h"
#endif
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
#include "../../chip/video/vic_ii/mos6569.h"
#include "../../chip/video/vic_ii/mos6567.h"
#include "../../chip/logic/pla.h"
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"
#include "c64_keyboard_matrix.h"
#include "../../devices/input/joystick_device.h"
#include "../../devices/input/lightpen_device.h"
#include "../../devices/storage/drive_1541.h"
#include "../../devices/storage/datasette_device.h"
#include "../../devices/keyboard/commodore_keyboard_device.h"
#include <cstring>
#include <cstdio>
#include <cctype>

/**
 * C64 System Wrapper Implementation
 *
 * ARCHITECTURE:
 * =============
 * This file implements the C64 system as a self-contained EmulatedSystem.
 * All core functions (initialize, shutdown, tick, reset, framebuffer, PLA
 * generation, memory init, CPU banking callback) are implemented directly —
 * no delegations to c64.cpp remain.
 *
 * c64.cpp still exists for the test framework's independent code path
 * (c64_system_create/init/tick/reset/destroy), which will eventually be
 * refactored to use the class interface.
 *
 * CYCLE COUNTING:
 * ===============
 * - c64_->total_cycles: Internal C64 cycle counter (updated by system_tick)
 * - total_cycles_: Base class cycle counter (synced once per frame in run_frame)
 *
 * FRAMEBUFFER MANAGEMENT:
 * =======================
 * - VIC-II owns the framebuffer: c64_->vicii->pixel.framebuffer
 * - get_framebuffer() returns pointer to VIC-II's buffer
 * - set_framebuffer() calls vicii_set_framebuffer() directly
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

C64System::C64System()
    : CommodoreSystem()  // Call base class constructor
    , c64_(nullptr)
{
    cycles_per_frame_ = 19705;  // PAL: 985248 Hz / 50 fps
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
}

C64System::~C64System() {
    shutdown();
}

const SystemDescriptor& C64System::get_descriptor() const {
    return c64_descriptor;
}

// ============================================================================
// Chip creation + callback helpers (absorbed from c64.cpp)
// ============================================================================

// Create a chip and return the pointer (no legacy registry involvement).
static inline void* create_chip(chip_descriptor_t* desc, unsigned int size) {
    void* chip;
    if (desc == &rom_descriptor)
        chip = rom_system_create_with_size(desc, size);
    else
        chip = desc->create(desc);

    if (!chip) {
        printf("ERROR: Failed to create chip: %s\n", desc->description);
    }
    return chip;
}

// Destroy a single chip via its descriptor.
static inline void destroy_chip(void* chip, chip_descriptor_t* desc) {
    if (chip && desc && desc->destroy) {
        desc->destroy(chip);
    }
}

// CIA2 Port A change callback — updates VIC-II bank select
static void cia2_port_a_bank_callback(void* context, uint8_t port_a_value) {
    c64_t* c64 = static_cast<c64_t*>(context);
    vicii_memory_bank_change(c64->vicii, port_a_value & 0x03);
}

// CPU I/O port banking callback — updates PLA memory mode
// Static with extern "C" linkage so it can serve as a C function pointer
// for the mos6510 chip descriptor's bank_change field.
extern "C" {
static void cpu_banking_callback(void* context, uint8_t banking_state) {
    c64_t* c64 = static_cast<c64_t*>(context);
    c64_bus_on_banking_change(&c64->bus, banking_state);
}
}


bool C64System::initialize() {
    if (c64_) {
        return true;  // Already initialized
    }

    // Point c64_ at the embedded struct (will be nulled on failure)
    c64_ = &c64_data_;

    // =========================================================================
    // System infrastructure
    // =========================================================================
    chip_descriptor_t* vicii_descriptor =
        (c64_config_.vicii_standard == VIC_PAL) ? &mos6569_descriptor : &mos6567_descriptor;

    // Cleanup helper for error paths — destroys keyboard + all created chips,
    // resets the pointer and zeroes the embedded struct for re-use.
    auto cleanup = [this, vicii_descriptor]() {
        if (c64_->keyboard) {
            commodore_keyboard_destroy(c64_->keyboard);
            c64_->keyboard = nullptr;
        }
        // Destroy each chip individually (no legacy registry)
        destroy_chip(c64_->kernal, &rom_descriptor);
        destroy_chip(c64_->cia2, &mos6526_descriptor);
        destroy_chip(c64_->cia1, &mos6526_descriptor);
        destroy_chip(c64_->colorram, &mos2114_descriptor);
        destroy_chip(c64_->sid, &mos6581_descriptor);
        destroy_chip(c64_->vicii, vicii_descriptor);
        destroy_chip(c64_->charrom, &rom_descriptor);
        destroy_chip(c64_->cartridge_romh, &rom_descriptor);
        destroy_chip(c64_->basic, &rom_descriptor);
        destroy_chip(c64_->cartridge_roml, &rom_descriptor);
        destroy_chip(c64_->mos6510, &mos6510_descriptor);
        destroy_chip(c64_->ram, &ram_descriptor);
        c64_ = nullptr;
        c64_data_ = {};
    };

    // Initialize bus as embedded struct (not heap-allocated)
    c64_->bus.desc = &c64_bus_descriptor;
    c64_->bus.c64 = c64_;

    // Bus pull-up defaults and cartridge lines (no cartridge)
    c64_->bus.default_state = C64_BUS_DEFAULT_STATE();
    c64_->bus.state = c64_->bus.default_state;
    c64_->bus.system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;

    // =========================================================================
    // Create all chips
    // =========================================================================
    if (!(c64_->ram = static_cast<ram_t*>(create_chip(&ram_descriptor, 65536)))) { cleanup(); return false; }
    if (!(c64_->mos6510 = create_chip(&mos6510_descriptor, 4096))) { cleanup(); return false; }
    if (!(c64_->cartridge_roml = static_cast<rom_t*>(create_chip(&rom_descriptor, 8192)))) { cleanup(); return false; }
    if (!(c64_->basic = static_cast<rom_t*>(create_chip(&rom_descriptor, 8192)))) { cleanup(); return false; }
    if (!(c64_->cartridge_romh = static_cast<rom_t*>(create_chip(&rom_descriptor, 8192)))) { cleanup(); return false; }
    if (!(c64_->charrom = static_cast<rom_t*>(create_chip(&rom_descriptor, 4096)))) { cleanup(); return false; }
    if (!(c64_->vicii = static_cast<vicii_t*>(create_chip(vicii_descriptor, 1024)))) { cleanup(); return false; }
    if (!(c64_->sid = static_cast<mos6581_t*>(create_chip(&mos6581_descriptor, 1024)))) { cleanup(); return false; }

    // Configure SID timing to match C64 CPU clock
    {
        bool is_pal = (c64_config_.vicii_standard == VIC_PAL);
        float cpu_clock = is_pal ? 985248.0f : 1022727.0f;
        mos6581_set_cpu_clock(c64_->sid, cpu_clock);
        mos6581_set_timing(c64_->sid, is_pal);
    }

    if (!(c64_->colorram = static_cast<mos2114_t*>(create_chip(&mos2114_descriptor, 1024)))) { cleanup(); return false; }
    c64_->vicii->colorram = c64_->colorram;

    // VIC-II bank selection via bank_base offset; no bus-level callback needed
    c64_->vicii->bus.bus = &c64_->bus;
    c64_->vicii->bus.bank_change = nullptr;

    if (!(c64_->cia1 = static_cast<mos6526_t*>(create_chip(&mos6526_descriptor, 256)))) { cleanup(); return false; }
    if (!(c64_->cia2 = static_cast<mos6526_t*>(create_chip(&mos6526_descriptor, 256)))) { cleanup(); return false; }

    // Create keyboard matrix
    c64_->keyboard = commodore_keyboard_create(&c64_keyboard_config);
    if (!c64_->keyboard) {
        printf("ERROR: Failed to create keyboard\n");
        cleanup();
        return false;
    }
    commodore_keyboard_reset(c64_->keyboard);
    printf("C64: Keyboard matrix initialized (all keys released)\n");

    if (!(c64_->kernal = static_cast<rom_t*>(create_chip(&rom_descriptor, 8192)))) { cleanup(); return false; }

    // No cartridge I/O by default
    c64_->io1 = nullptr;
    c64_->io2 = nullptr;

    // =========================================================================
    // PLA memory maps and bus initialization
    // =========================================================================
    if (!pla_maps_generate()) { cleanup(); return false; }

    printf("VIC-II memory mapping for mode 0x07:\n");
    for (int bank = 0; bank < 16; bank++) {
        uint8_t chip = c64_->bus.vicii_chip_per_bank[bank];
        printf("  Bank %d (0x%04X-0x%04X): CHIP=%d (%s)\n",
               bank, bank * 0x1000, (bank + 1) * 0x1000 - 1,
               chip, c64_chips_to_title(chip));
    }

    // Attach bus and load ROMs from configured paths
    c64_bus_system_attach(&c64_->bus, c64_);
    memory_init(&c64_config_);
    c64_bus_init_unified_pointers(&c64_->bus, c64_, &c64_config_);

    // =========================================================================
    // Wire callbacks and initialize CPU
    // =========================================================================

    // CIA2 Port A → VIC-II bank selection
    c64_->cia2->port_a_change_callback = cia2_port_a_bank_callback;
    c64_->cia2->port_a_callback_context = c64_;
    cia2_port_a_bank_callback(c64_, c64_->cia2->port_a_value);  // Set initial bank

    // CPU I/O port → PLA memory banking
    mos6510_descriptor.bank_change = cpu_banking_callback;

    // NOTE: CIA1 keyboard callbacks are NOT set here — setup_connector_ports()
    // installs joystick-aware versions that supersede the basic ones.

    // CIA2 interrupt line → NMI (CIA1 defaults to IRQ)
    c64_->cia2->configured_interrupt_bit = BUS_NMI_BIT;

    // Initialize CPU and point it at the reset vector
    mos6510_desc_t cpu_desc = {};
    mos6510_init(static_cast<mos6510_t*>(c64_->mos6510), &cpu_desc);
    mos6510_set_bank_change_context(static_cast<mos6510_t*>(c64_->mos6510), c64_);

    uint16_t reset_vector = c64_read_kernal_reset_vector(&c64_->bus);
    mos6510_set_pc(static_cast<mos6510_t*>(c64_->mos6510), reset_vector);
    mos6510_set_ab(static_cast<mos6510_t*>(c64_->mos6510), reset_vector);
    printf("C64: CPU reset vector $%04X loaded\n", reset_vector);

    // NOTE: VIC-II bus.bus is already wired above. SID bus_interface is unused.
    // No legacy bus_attach iteration needed.

    // =========================================================================
    // Phase 5: Wrapper-level initialization
    // =========================================================================

    // Track the actual VIC-II standard this system was created with.
    created_vicii_standard_ = c64_config_.vicii_standard;

    // Apply SID revision from configuration
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

void C64System::shutdown() {
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
        // Destroy keyboard
        if (c64_->keyboard) {
            commodore_keyboard_destroy(c64_->keyboard);
            c64_->keyboard = nullptr;
        }
        // Destroy all chips individually (no legacy registry)
        chip_descriptor_t* vdesc = (created_vicii_standard_ == VIC_PAL) ? &mos6569_descriptor : &mos6567_descriptor;
        destroy_chip(c64_->kernal, &rom_descriptor);
        destroy_chip(c64_->cia2, &mos6526_descriptor);
        destroy_chip(c64_->cia1, &mos6526_descriptor);
        destroy_chip(c64_->colorram, &mos2114_descriptor);
        destroy_chip(c64_->sid, &mos6581_descriptor);
        destroy_chip(c64_->vicii, vdesc);
        destroy_chip(c64_->charrom, &rom_descriptor);
        destroy_chip(c64_->cartridge_romh, &rom_descriptor);
        destroy_chip(c64_->basic, &rom_descriptor);
        destroy_chip(c64_->cartridge_roml, &rom_descriptor);
        destroy_chip(c64_->mos6510, &mos6510_descriptor);
        destroy_chip(c64_->ram, &ram_descriptor);

        c64_ = nullptr;
        // Zero the embedded struct for clean re-initialization
        c64_data_ = {};
    }
}

void C64System::reset() {
    // Clear any pending deferred load (will be re-set by the next load_file call)
    if (pending_load_.active) {
        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
    }
    boot_completed_ = false;
    sid_player_active_ = false;
    active_sid_data_.clear();
    if (c64_) {
        printf("C64 System: Performing system-wide reset...\n");

        // Reset CIA chips first (they control interrupts and I/O)
        if (c64_->cia1) mos6526_reset(c64_->cia1);
        if (c64_->cia2) mos6526_reset(c64_->cia2);

        // Reset VIC-II to clear sprite pipeline state
        if (c64_->vicii) vicii_reset(c64_->vicii);

        // Reset SID — clears all registers, envelopes, and the sample ring buffer
        if (c64_->sid) mos6581_reset(c64_->sid);

        // Reset CPU last (so it can read the reset vector after other chips are ready)
        if (c64_->mos6510) {
            mos6510_desc_t cpu_desc = {};
            mos6510_init((mos6510_t*)c64_->mos6510, &cpu_desc);
            mos6510_set_bank_change_context((mos6510_t*)c64_->mos6510, c64_);

            uint16_t reset_vector = c64_read_kernal_reset_vector(&c64_->bus);
            mos6510_set_pc((mos6510_t*)c64_->mos6510, reset_vector);
            mos6510_set_ab((mos6510_t*)c64_->mos6510, reset_vector);
        }

        // Reset keyboard
        if (c64_->keyboard) commodore_keyboard_reset(c64_->keyboard);

        // Reset cycle counter
        c64_->total_cycles = 0;

        // Clear the memory locations that is_basic_ready() checks, so stale
        // values from the previous session don't cause premature detection.
        if (c64_->ram) {
            c64_->ram->memory[0x0302] = 0;
            c64_->ram->memory[0x0303] = 0;
            c64_->ram->memory[0x002D] = 0;
            c64_->ram->memory[0x00C6] = 0;
        }

        printf("C64 System: Reset complete\n");
    }
}

// ============================================================================
// system_tick — inline system tick (performance-critical hot loop)
// ============================================================================
// This is the core emulation loop — ticks all chips in correct phase order.
// Inlined from c64_system_tick() to eliminate function call overhead.
// ============================================================================

void C64System::system_tick() {
    c64_t* c64 = c64_;

    c64->total_cycles++;
    c64_bus_t* bus = &(c64->bus);

    // Start each cycle with pull-up resistors (default_state: IRQ=1, NMI=1, BA=1, AEC=1, RDY=1)
    bus_state_t s = bus->default_state;
    BUS_SET_ADDR(s, BUS_GET_ADDR(bus->state));
    BUS_SET_DATA(s, BUS_GET_DATA(bus->state));

    // PHASE 1: VIC-II PHI1 — g-access read, pixel sequencing
    s = vicii_tick_phi1(c64->vicii, s);

    // PHASE 1.5: CIA PHI2 — apply pending interrupt lines before CPU
    s = mos6526_tick_phi2(c64->cia2, s);
    s = mos6526_tick_phi2(c64->cia1, s);

    // BA→RDY wiring (direct bit manipulation to preserve IRQ/NMI from CIAs)
    if (BUS_GET_LINES(s) & BUS_MASK_BA)
        s |= BUS_BIT(BUS_RDY_BIT);
    else
        s &= ~BUS_BIT(BUS_RDY_BIT);

    // PHASE 2: CPU PHI2 — instruction execution
    s = mos6510_tick_phi2(c64->mos6510, s);

    // PHASE 3: Memory service (AEC determines CPU vs VIC-II bus ownership)
    s = c64_memory_tick(bus, s);

    // PHASE 3.1: VIC-II PHI2 — c/p/s-access data delivery
    vicii_tick_phi2(c64->vicii, s);

    // PHASE 3.5: CIA PHI1 — timer counting, TOD, interrupt generation
    s = mos6526_tick_phi1(c64->cia2, s);
    s = mos6526_tick_phi1(c64->cia1, s);

    // PHASE 4: CPU PHI1 — prepare next fetch
    s = mos6510_tick_phi1(c64->mos6510, s);

    // Restore R/W line to read mode
    s |= BUS_BIT(BUS_RW_BIT);

    // PHASE 5: SID — sound generation
    s = mos6581_tick(c64->sid, s);

    bus->state = s;
}

void C64System::tick() {
    if (c64_) {
        system_tick();

        // CRITICAL: Sync base class cycle counter with C64's internal counter
        total_cycles_ = c64_->total_cycles;

        // Check if a deferred file load is waiting for BASIC to reach READY
        if (pending_load_.active && is_basic_ready()) {
            apply_pending_load();
        }
    }
}

void C64System::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);

    if (c64_) {
        // Update per-frame state for peripheral devices before cycle loop.
        update_lightpen_display_rect();

        for (uint32_t i = 0; i < adjusted_cycles; i++) {
            system_tick();
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

bool C64System::load_file(const char* filepath) {
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

    // Determine load mode based on format type
    LoadMode mode = LoadMode::DIRECT;
    if (result.format) {
        const char* fmt = result.format->name;
        if (fmt && strcmp(fmt, "D64") == 0) {
            mode = LoadMode::DISK_FAST;
        } else if (fmt && strcmp(fmt, "TAP") == 0) {
            mode = LoadMode::TAPE_INSERTED;
        }
    }

    // Defer loading until KERNAL/BASIC boot completes.
    // The CPU starts at $FCE2 (KERNAL reset vector) and must complete its
    // full boot sequence — IOINIT, RAMTAS, RESTOR, screen init, BASIC cold
    // start — before we write program data to RAM. This prevents BASIC's
    // NEW routine from zeroing $0801/$0802 and corrupting the loaded program.
    pending_load_.result = result;  // Transfer ownership (don't free yet)
    pending_load_.filepath = filepath;
    pending_load_.active = true;
    pending_load_.mode = mode;

    printf("C64: File parsed (mode=%s) — deferred until BASIC READY\n",
           mode == LoadMode::DISK_FAST ? "DISK_FAST" :
           mode == LoadMode::TAPE_INSERTED ? "TAPE_INSERTED" : "DIRECT");
    return true;
}

bool C64System::is_basic_ready() const {
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

void C64System::apply_pending_load() {
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
    // DISK_FAST PATH — D64: Insert disk into 1541 + extract first PRG to RAM
    // =========================================================================
    if (pending_load_.mode == LoadMode::DISK_FAST) {
        printf("C64: BASIC READY — DISK_FAST load\n");

        // Find a 1541 drive on the IEC serial bus
        auto* iec_port = get_connector_port(PORT_IEC_SERIAL);
        Drive1541Device* drive = nullptr;
        if (iec_port) {
            for (auto* dev : iec_port->get_attached_devices()) {
                drive = dynamic_cast<Drive1541Device*>(dev);
                if (drive) break;  // Use the first available 1541
            }
        }

        if (drive) {
            drive->insert_disk(pending_load_.filepath.c_str());
            printf("C64: D64 inserted into drive %d\n", drive->get_device_number());
        } else {
            printf("C64: No 1541 drive attached — D64 not mounted\n");
        }

        // Also fast-load the extracted PRG into RAM for instant start
        if (pending_load_.result.type == FORMAT_LOAD_PROGRAM &&
            pending_load_.result.program.data) {
            commodore_load_context_t ctx = {};
            ctx.system_name     = "C64";
            ctx.write_byte      = c64_mem_write_byte;
            ctx.write_block     = c64_mem_write_block;
            ctx.mem_read        = c64_mem_read;
            ctx.mem_ctx         = c64_->ram;
            ctx.basic_params    = &COMMODORE_BASIC_C64;
            ctx.basic_start_addrs[0] = 0x0801;
            ctx.default_raw_addr = 0xC000;

            commodore_apply_load_result(&ctx, &pending_load_.result,
                                        pending_load_.filepath.c_str());
        } else if (drive) {
            // No PRG extracted — inject LOAD"*",8,1 + RUN for native disk load
            const char* load_cmd = "LOAD\"*\",8,1\r";
            int len = (int)strlen(load_cmd);
            if (len > 10) len = 10;
            for (int i = 0; i < len; i++) {
                c64_mem_write_byte(c64_->ram, (uint16_t)(0x0277 + i), (uint8_t)load_cmd[i]);
            }
            c64_mem_write_byte(c64_->ram, 0x00C6, (uint8_t)len);
        }

        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
        boot_completed_ = true;
        return;
    }

    // =========================================================================
    // TAPE_INSERTED PATH — TAP: Load tape into datasette + inject LOAD
    // =========================================================================
    if (pending_load_.mode == LoadMode::TAPE_INSERTED) {
        printf("C64: BASIC READY — TAPE_INSERTED load\n");

        // Find the datasette on the cassette port
        auto* cass_port = get_connector_port(PORT_CASSETTE);
        DatasetteDevice* datasette = nullptr;
        if (cass_port) {
            datasette = dynamic_cast<DatasetteDevice*>(cass_port->get_attached_device());
        }

        if (datasette) {
            datasette->load_tap(pending_load_.filepath.c_str());
            datasette->press_play();
            printf("C64: TAP loaded into datasette, PLAY pressed\n");

            // Inject LOAD + RETURN to start tape loading
            const char* load_cmd = "LOAD\r";
            int len = (int)strlen(load_cmd);
            for (int i = 0; i < len; i++) {
                c64_mem_write_byte(c64_->ram, (uint16_t)(0x0277 + i), (uint8_t)load_cmd[i]);
            }
            c64_mem_write_byte(c64_->ram, 0x00C6, (uint8_t)len);
        } else {
            printf("C64: No datasette attached — TAP not loaded\n");
        }

        format_load_result_free(&pending_load_.result);
        pending_load_.active = false;
        boot_completed_ = true;
        return;
    }

    // =========================================================================
    // STANDARD PATH — PRG / T64 / CRT / LNX / BIN
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

void C64System::ensure_compatible_for_sid(const sid_header_t* sid) {
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
uint32_t* C64System::get_framebuffer() {
    if (c64_ && c64_->vicii) {
        return c64_->vicii->pixel.framebuffer;
    }
    return nullptr;
}

void C64System::get_display_dimensions(int* width, int* height) const {
    // VIC-II visible area
    *width = 403;
    *height = 284;
}

void C64System::set_framebuffer(uint32_t* buffer, int width, int height) {
    if (c64_ && c64_->vicii && buffer) {
        vicii_set_framebuffer(c64_->vicii, buffer, width, height);
    }
}

void C64System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
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

void C64System::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    // SID player subtune selection — intercept before keyboard mapper
    if (sid_player_active_ && pressed && !repeat) {
        if (handle_sid_player_key(key)) return;
    }

    // Disc flip hotkeys — Alt+N (next) / Alt+P (prev) on drive 8
    if (pressed && !repeat && (mod & KMOD_ALT)) {
        if (key == SDLK_n || key == SDLK_p) {
            auto* iec_port = get_connector_port(PORT_IEC_SERIAL);
            if (iec_port) {
                for (auto* dev : iec_port->get_attached_devices()) {
                    auto* drive = dynamic_cast<Drive1541Device*>(dev);
                    if (drive && !drive->get_fliplist().empty()) {
                        if (key == SDLK_n) drive->flip_next();
                        else                drive->flip_prev();
                        return;
                    }
                }
            }
        }
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

void C64System::handle_controller_event(int controller, int button, bool pressed) {
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

bool C64System::handle_sid_player_key(SDL_Keycode key) {
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

void C64System::render_system_menu_items() {
#ifdef IMGUI_VERSION
    if (ImGui::MenuItem("Reset C64")) {
        reset();
    }
#endif
}

void C64System::render_debug_windows(void* gui_state) {
#ifdef IMGUI_VERSION
    if (!c64_) return;
    
    // Chip debug/settings windows — use base class dispatch via get_chip_info()
    EmulatedSystem::render_debug_windows(gui_state);
#endif
}

// ============================================================================
// Chip Info / Debug / Settings — generic interface (Hardware menu)
// ============================================================================

// Chip indices for get_chip_info() / render_chip_debug_window()
enum C64ChipIndex {
    C64_CI_CPU = 0,
    C64_CI_VICII,
    C64_CI_SID,
    C64_CI_CIA1,
    C64_CI_CIA2,
    C64_CI_COLORRAM,
    C64_CI_RAM,
    C64_CI_BASIC_ROM,
    C64_CI_KERNAL_ROM,
    C64_CI_CHARROM,
    C64_CI_CART_ROML,
    C64_CI_CART_ROMH,
    C64_CI_PLA,
    C64_CI_COUNT
};

std::vector<ChipInfo> C64System::get_chip_info() const {
    return {
        { "MOS 6510 CPU",                "6510",       "CPU",    0x0000, false, false },
        { "VIC-II (MOS 6569/6567)",       "VIC-II",     "Video",  0xD000, false, false },
        { "SID (MOS 6581/8580)",          "SID",        "Audio",  0xD400, true,  false },
        { "CIA 1 (MOS 6526)",             "CIA 1",      "I/O",    0xDC00, false, false },
        { "CIA 2 (MOS 6526)",             "CIA 2",      "I/O",    0xDD00, false, false },
        { "Color RAM (MOS 2114)",         "Color RAM",  "I/O",    0xD800, false, false },
        { "RAM (64KB)",                   "RAM",        "Memory", 0x0000, false, false },
        { "BASIC ROM (8KB)",              "BASIC",      "Memory", 0xA000, false, false },
        { "KERNAL ROM (8KB)",             "KERNAL",     "Memory", 0xE000, false, false },
        { "Character ROM (4KB)",          "CHARROM",    "Memory", 0xD000, false, false },
        { "Cartridge ROM Low (8KB)",      "ROML",       "Memory", 0x8000, false, false },
        { "Cartridge ROM High (8KB)",     "ROMH",       "Memory", 0xA000, false, false },
        { "PLA / Address Decoder",        "PLA",        "Bus",    0x0000, false, false },
    };
}

void C64System::render_chip_debug_window(int chip_index, bool* show) {
    if (!c64_ || !show || !*show) return;
#ifdef IMGUI_VERSION
    switch (chip_index) {
        case C64_CI_SID:
            if (c64_->sid) {
                mos6581_render_debug_window(c64_->sid, show);
            }
            break;
        // Other chips: not yet implemented
        default:
            break;
    }
#else
    (void)chip_index;
#endif
}

void C64System::render_chip_settings_window(int chip_index, bool* show) {
    if (!c64_ || !show || !*show) return;
    // No chip settings windows implemented yet
    (void)chip_index;
}

// Note: get_target_fps() and set_speed_multiplier() are now provided by
// CommodoreSystem base class.

// Note: get_total_cycles() now provided by base class (returns total_cycles_)
// However, C64 has its own cycle counter, so we need to sync it
// For now, we'll update total_cycles_ in tick() method

// Note: get_speed_multiplier() now provided by base class (returns speed_multiplier_)

// Note: Hardware trait queries (get_hardware_traits, get_current_timing,
// get_display_traits, get_audio_traits) now provided by base class
// Note: get_configuration() now provided by base class (returns config_)
// Note: set_configuration() now provided by CommodoreSystem base class

bool C64System::apply_configuration() {
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
SystemConfiguration C64System::detect_optimal_configuration(
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

void C64System::render_configuration_ui() {
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
    false, false
};

static const ConnectorDefinition c64_control_port_2_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Control Port 2",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition c64_iec_serial_def = {
    ConnectorType::IEC_SERIAL,
    "IEC Serial Bus",
    ConnectorSignals::IEC_SERIAL_SIGNALS,
    ConnectorSignals::IEC_SERIAL_SIGNAL_COUNT,
    false,  // is_internal
    true    // is_bus — shared bus, multiple drives/printers
};

static const ConnectorDefinition c64_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Port",
    ConnectorSignals::CASSETTE_PORT_SIGNALS,
    ConnectorSignals::CASSETTE_PORT_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition c64_user_port_def = {
    ConnectorType::USER_PORT,
    "User Port",
    ConnectorSignals::USER_PORT_SIGNALS,
    ConnectorSignals::USER_PORT_SIGNAL_COUNT,
    false, false
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
    false, false
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
    C64System*   wrapper;
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
        auto* port = ctx->wrapper->get_connector_port(C64System::PORT_CONTROL2);
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
        auto* port = ctx->wrapper->get_connector_port(C64System::PORT_CONTROL1);
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

/// VIC-II LP pin read callback.
/// Called every VIC-II cycle from vicii_tick_phi1.  Uses the cached lightpen
/// pointer (set by on_port_device_changed) to avoid per-cycle lookups.
/// Returns true (HIGH) when no lightpen or no trigger, false (LOW) when the
/// beam matches the pen's target position.
static bool c64_vicii_lp_pin_read(void* context) {
    auto* ctx = static_cast<C64PortCallbackContext*>(context);
    auto* lightpen = ctx->wrapper ? ctx->wrapper->get_cached_lightpen() : nullptr;
    if (!lightpen) return true;

    uint16_t beam_x = vicii_get_x_coordinate(ctx->c64->vicii);
    uint16_t beam_y = vicii_get_raster_counter(ctx->c64->vicii);
    return lightpen->get_lp_pin_state(beam_x, beam_y);
}

void C64System::setup_connector_ports() {
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
        ConnectorType::CUSTOM, "Keyboard", nullptr, 0, true, false  // is_internal, not bus
    };
    int kb_port = add_connector_port(c64_keyboard_def, 0);

    // Attach internal keyboard device
    auto kb_device = std::make_unique<CommodoreKeyboardDevice>(c64_ ? c64_->keyboard : nullptr);
    auto* kb_raw = kb_device.get();
    connector_ports_[kb_port]->attach_device(kb_raw);
    owned_devices_.push_back(std::move(kb_device));

    // Default devices: mouse in Port 1, joystick in Port 2.
    // Port 2 is the standard game port; Port 1 provides mouse for GEOS/etc.
    // Users can manually swap these or attach a lightpen via the connector menu.
    attach_device_to_port(PORT_CONTROL1, "mouse_1351");
    attach_device_to_port(PORT_CONTROL2, "joystick");

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

    // Wire VIC-II LP pin read callback (Control Port 1 pin 6 → VIC-II LP input)
    if (c64_ && c64_->vicii) {
        c64_->vicii->bus.lp_pin_read    = c64_vicii_lp_pin_read;
        c64_->vicii->bus.lp_pin_context = &s_port_callback_ctx;
        printf("C64: Wired VIC-II lightpen pin callback\n");
    }

    printf("C64: Created %zu connector ports\n", connector_ports_.size());
}

void C64System::update_lightpen_display_rect() {
    if (!cached_lightpen_) return;
    const auto& rect = get_display_screen_rect();
    cached_lightpen_->set_display_screen_rect(rect.x, rect.y, rect.w, rect.h);
}

void C64System::on_port_device_changed(int port_index) {
    if (port_index != PORT_CONTROL1) return;
    cached_lightpen_ = nullptr;
    auto* port = get_connector_port(PORT_CONTROL1);
    if (!port) return;
    auto* device = port->get_attached_device();
    if (device && strcmp(device->get_id(), "lightpen") == 0) {
        cached_lightpen_ = static_cast<LightpenDevice*>(device);
    }
}

uint32_t C64System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!c64_ || !c64_->sid || !buffer || max_samples == 0) return 0;
    mos6581_generate_samples(c64_->sid, buffer, max_samples);
    return max_samples;
}

void C64System::set_audio_sample_rate(int sample_rate_hz) {
    if (c64_ && c64_->sid && sample_rate_hz > 0) {
        printf("C64: Updating SID sample rate from %.0f to %d Hz\n",
               c64_->sid->sample_rate, sample_rate_hz);
        mos6581_set_sample_rate(c64_->sid, static_cast<float>(sample_rate_hz));
    }
}

// ============================================================================
// PLA Memory Map Generation (absorbed from c64.cpp)
// ============================================================================

bool C64System::pla_maps_generate() {
    c64_bus_t* bus = &(c64_->bus);

    // Create a temporary PLA instance for generating memory maps
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla)
        return false;

    // Generate all 32 memory modes using PLA
    c64_bus_generate_all_pla_modes(bus, (struct pla_906114_01_s*)pla);

    // Clean up PLA instance
    pla_906114_01_destroy(pla);

    // Calculate initial PLA mode from system_lines (EXROM/GAME) and default CPU port value
    // CPU I/O port initializes to $17 (bits 0-2 = 0b111 = LORAM=1, HIRAM=1, CHAREN=1)
    // Combined with system_lines (EXROM=1, GAME=1) this gives mode $1F
    uint8_t cpu_port_bits = 0x07;  // Default from init_io_port(): $17 & $07 = $07
    uint8_t initial_pla_mode = c64_bus_generate_pla_mode(bus, cpu_port_bits);
    c64_bus_mode_switch(bus, initial_pla_mode);

    printf("C64 initial banking: PLA mode=$%02X (standard config, no cartridge)\n", initial_pla_mode);

    return true;
}

// ============================================================================
// Memory Initialization (absorbed from c64.cpp)
// ============================================================================

// Helper: Initialize RAM with debug test patterns
static void memory_init_debug_patterns(ram_t* ram) {
    if (!ram || !ram->memory) {
        printf("ERROR: RAM pointer invalid for debug pattern initialization\n");
        return;
    }

    printf("Initializing RAM with debug test patterns...\n");

    // Clear zero page and stack
    memset(ram->memory, 0, 0x0200);

    // Fill remaining RAM with random bytes for realistic uninitialized memory behavior
    for (uint32_t addr = 0x0200; addr < 0x10000; addr++) {
        ram->memory[addr] = (uint8_t)rand();
    }

    // Add test pattern to video matrix at $0400 in all VIC-II banks
    for (int bank = 0; bank < 4; bank++) {
        uint16_t base = bank * 0x4000 + 0x0400;
        for (int i = 0; i < 1000; i++) {
            ram->memory[base + i] = (uint8_t)((base + i) & 0xFF);
        }
        printf("  Bank %d: Screen memory at $%04X filled with address pattern\n", bank, base);
    }
}

// Helper: Initialize Color RAM with debug patterns
static void colorram_init_debug(mos2114_t* colorram) {
    if (!colorram || !colorram->memory) {
        printf("ERROR: Color RAM pointer invalid for debug initialization\n");
        return;
    }

    // Randomize all 1024 color RAM locations (4-bit values 0-15)
    for (int i = 0; i < 1024; i++) {
        colorram->memory[i] = (uint8_t)(rand() & 0x0F);
    }
    printf("Color RAM initialized with random colors\n");
}

void C64System::memory_init(const c64_config_t* config) {
    // Use default ROM configuration if none provided
    const rom_config_t* rom_config = config && config->rom_config ?
                                      config->rom_config :
                                      system_config_get_default_roms();

    // Discover ROM root path for C64 system
    char rom_root_path[1024];
    bool rom_root_found = system_config_discover_rom_root("c64", rom_root_path, sizeof(rom_root_path));

    // -------------------------------------------------------------------------
    // Initialize RAM
    // -------------------------------------------------------------------------
    if (c64_->ram && c64_->ram->memory) {
        c64_test_mode_t test_mode = config ? config->test_mode : C64_TEST_MODE_NORMAL;

        switch (test_mode) {
            case C64_TEST_MODE_NORMAL:
                printf("Normal boot mode: RAM cleared\n");
                memset(c64_->ram->memory, 0, 0x10000);
                break;

            case C64_TEST_MODE_DEBUG_PATTERNS:
                memory_init_debug_patterns(c64_->ram);
                break;

            case C64_TEST_MODE_PRG_FILE:
                if (config && config->test_binary_config && config->test_binary_config->filename) {
                    printf("Loading PRG file: %s\n", config->test_binary_config->filename);
                    memset(c64_->ram->memory, 0, 0x10000);
                    commodore_prg_t prg = {};
                    if (commodore_prg_load(config->test_binary_config->filename, &prg)) {
                        memcpy(&c64_->ram->memory[prg.load_addr], prg.data, prg.data_size);
                        printf("  Loaded $%04X-$%04X (%zu bytes)\n",
                               prg.load_addr, prg.end_addr, prg.data_size);
                        commodore_prg_free(&prg);
                    } else {
                        printf("ERROR: Failed to load PRG file, falling back to normal init\n");
                        memset(c64_->ram->memory, 0, 0x10000);
                    }
                } else {
                    printf("ERROR: PRG mode selected but no filename provided\n");
                    memset(c64_->ram->memory, 0, 0x10000);
                }
                break;

            case C64_TEST_MODE_BIN_FILE:
                if (config && config->test_binary_config && config->test_binary_config->filename) {
                    printf("Loading BIN file: %s at $%04X\n",
                           config->test_binary_config->filename,
                           config->test_binary_config->load_address);
                    memset(c64_->ram->memory, 0, 0x10000);
                    uint8_t* bin_data = NULL;
                    size_t bin_size = 0;
                    if (commodore_bin_load(config->test_binary_config->filename,
                                          &bin_data, &bin_size)) {
                        uint16_t addr = config->test_binary_config->load_address;
                        if (addr + bin_size <= 0x10000) {
                            memcpy(&c64_->ram->memory[addr], bin_data, bin_size);
                            printf("  Loaded %zu bytes at $%04X\n", bin_size, addr);
                        }
                        free(bin_data);
                    } else {
                        printf("ERROR: Failed to load BIN file, falling back to normal init\n");
                        memset(c64_->ram->memory, 0, 0x10000);
                    }
                } else {
                    printf("ERROR: BIN mode selected but no filename provided\n");
                    memset(c64_->ram->memory, 0, 0x10000);
                }
                break;

            default:
                printf("WARNING: Unknown test mode, using normal init\n");
                memset(c64_->ram->memory, 0, 0x10000);
                break;
        }
    } else {
        printf("ERROR: RAM memory pointer is NULL!\n");
    }

    // -------------------------------------------------------------------------
    // Initialize Color RAM
    // -------------------------------------------------------------------------
    if (c64_->colorram && c64_->colorram->memory) {
        c64_test_mode_t test_mode = config ? config->test_mode : C64_TEST_MODE_NORMAL;
        if (test_mode == C64_TEST_MODE_DEBUG_PATTERNS) {
            colorram_init_debug(c64_->colorram);
        } else {
            memset(c64_->colorram->memory, 0, 1024);
        }
    }

    // -------------------------------------------------------------------------
    // Load ROMs from files
    // -------------------------------------------------------------------------
    struct { rom_t* rom; const char** filenames; uint16_t size; const char* name; } roms[] = {
        { c64_->basic,   rom_config ? (const char**)rom_config->basic_rom_filenames   : nullptr, 8192, "BASIC" },
        { c64_->kernal,  rom_config ? (const char**)rom_config->kernal_rom_filenames  : nullptr, 8192, "KERNAL" },
        { c64_->charrom, rom_config ? (const char**)rom_config->chargen_rom_filenames : nullptr, 4096, "Character" },
    };

    for (auto& r : roms) {
        if (!r.rom || !r.rom->memory) {
            if (r.rom) printf("Warning: %s ROM has no allocated memory\n", r.name);
            continue;
        }
        printf("[ROM-INIT] Processing %s ROM (size=%u memory=%p)\n", r.name, r.size, (void*)r.rom->memory);

        bool loaded = false;
        if (rom_root_found && r.filenames) {
            loaded = rom_loader_load_from_root(rom_root_path, r.filenames, r.size,
                                               r.rom->memory, r.size);
            if (!loaded) printf("Warning: Failed to load %s ROM\n", r.name);
        } else if (!rom_root_found) {
            printf("Warning: ROM root not found, skipping %s ROM loading\n", r.name);
        }

        if (!loaded) {
            memset(r.rom->memory, 0xFF, r.size);
        }
    }

    // Cartridge ROMs: not loaded by default (filled with 0xFF if present)
    if (c64_->cartridge_roml && c64_->cartridge_roml->memory) {
        memset(c64_->cartridge_roml->memory, 0xFF, 8192);
    }
    if (c64_->cartridge_romh && c64_->cartridge_romh->memory) {
        memset(c64_->cartridge_romh->memory, 0xFF, 8192);
    }
}

// Register C64 system with the registry
REGISTER_SYSTEM(c64_descriptor, []() {
    return std::make_unique<C64System>();
})
