#include "core/cermu.hpp"
#include "systems/commodore/c64/c64_system.hpp"
#include "systems/commodore/c64/c64_kernal_patches.hpp"
#include "systems/commodore/c64/c64_sid_player.hpp"
#include "systems/commodore/pla_banking.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "core/input/emu_key_sdl_map.hpp"
// gui_state_t dependency eliminated — chip debug uses base class,
// system menu items are inlined, test binary dialog removed.
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/palette_selector.hpp"
#endif
#include "core/formats/format_registry.hpp"
#include "core/formats/prg_format.hpp"
#include "core/formats/bin_format.hpp"
#include "core/formats/d64_format.hpp"
#include "core/formats/t64_format.hpp"
#include "core/formats/tap_format.hpp"
#include "core/formats/crt_format.hpp"
#include "core/formats/lnx_format.hpp"
#include "core/formats/sid_format.hpp"
#include "systems/commodore/commodore_load_helpers.hpp"
#include "chip/cpu/fam65xx/mos6510.hpp"
// CPU (fam65xx) is a native C++ ChipBase — no separate GUI header needed
#include "chip/video/vic_ii/mos6569.hpp"
#include "chip/video/vic_ii/mos6567.hpp"
// VIC-II is a native C++ ChipBase — no separate GUI header needed
// MOS6526 is a native C++ ChipBase — no separate GUI header needed
// MOS2114 is a native C++ ChipBase — no separate GUI header needed
// MOS6581 is a native C++ ChipBase — no separate GUI header needed
#include "chip/logic/pla.hpp"
#include "systems/commodore/c64/c64_pla_chip.hpp"
#include "core/chip.hpp"
#include "chip/memory/memory_chip.hpp"
#include "core/config/path_discovery.hpp"
#include "systems/commodore/c64/c64_keyboard_matrix.hpp"
#include "devices/input/joystick_device.hpp"
#include "devices/input/lightpen_device.hpp"
#include "devices/storage/drive_1541.hpp"
#include "devices/storage/datasette_1530.hpp"
#include "devices/keyboard/commodore_keyboard_device.hpp"
#include "systems/commodore/prg_content_analysis.hpp"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include "systems/commodore/c64/c64_constants.hpp"

// Bitmasks for cartridge control lines stored in system_lines_
static constexpr uint8_t SYS_MASK_EXROM = 0x01;
static constexpr uint8_t SYS_MASK_GAME  = 0x02;

/**
 * C64 System Implementation
 *
 * ARCHITECTURE:
 * =============
 * This file implements the C64 system as a self-contained System.
 * All core functions (initialize, shutdown, tick, reset, framebuffer, PLA
 * generation, memory init, CPU banking callback) live here.
 *
 * CYCLE COUNTING:
 * ===============
 * - total_cycles_: Base class cycle counter (incremented by system_tick)
 *
 * FRAMEBUFFER MANAGEMENT:
 * =======================
 * - VIC-II drives the composite video output via VideoPort
 */

/** Check if load address is a typical C64 address */
static bool is_c64_load_address(uint16_t addr) {
    return addr == c64_constants::BASIC_START || addr == 0xC000 || addr == 0x0800 ||
           addr == 0x4000 || addr == c64_constants::ROML_BASE || addr == c64_constants::KERNAL_BASE;
}

// ============================================================================
// C64 file probe — unified confidence + configuration detection
// ============================================================================

static SystemProbeResult c64_probe_file(
    const format_descriptor_t* matched_format,
    const char* filepath,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result{};

    if (!matched_format) return result;

    // --- Dispatch based on which format the generic layer matched ---

    if (matched_format == &PRG_FORMAT_DESCRIPTOR) {
        if (size >= 2) {
            uint16_t load_addr = data[0] | (data[1] << 8);
            if (load_addr == c64_constants::BASIC_START)
                result.confidence = 0.95f;              // C64 BASIC start
            else if (load_addr == 0xC000 || load_addr == 0x0800 || load_addr == 0x4000)
                result.confidence = 0.85f;              // Common C64 ML addresses
            else if (load_addr == 0x1001)
                result.confidence = 0.6f;               // VIC-20 / C16 territory
            else
                result.confidence = 0.7f;               // Generic PRG (C64 most common)

            // Content analysis: BASIC version + MMIO references
            if (size > 6) {
                const uint8_t* payload = data + 2;
                size_t payload_len = size - 2;
                float basic_conf = basic_program_confidence(payload, payload_len, load_addr);

                if (basic_conf >= 0.5f) {
                    // Looks like BASIC — penalise if BASIC 3.5 tokens found
                    if ((load_addr == c64_constants::BASIC_START || load_addr == 0x1001)
                        && has_basic35_tokens(payload, payload_len))
                        result.confidence *= 0.30f;  // BASIC 3.5 → not C64
                    // Note: we intentionally do NOT scan SYS-stub ML for MMIO
                    // here.  The C64 I/O ranges ($D400-$DFFF SID/CIA) are too
                    // broad (~2.3 % of the address space) — data segments after
                    // the SYS entry point create abundant false positives.
                } else {
                    // Pure machine language — scan entire payload for MMIO
                    uint32_t mmio = scan_6502_mmio_references(payload, payload_len);
                    int c64_hits   = count_mmio_flags(mmio & MMIO_ANY_C64);
                    int c64_strong = count_mmio_flags(mmio & MMIO_C64_STRONG);
                    int vic20_hits = count_mmio_flags(mmio & MMIO_ANY_VIC20);
                    int c16_hits   = count_mmio_flags(mmio & MMIO_ANY_C16);

                    // Require at least two "strong" C64 hits (2 of SID/CIA1/CIA2)
                    // and 3+ total C64 ranges.  Fewer is not enough: SID alone
                    // ($D400-$D7FF = 1.6 %) plus one CIA is readily hit by random
                    // data in long binaries interpreted as code.
                    // Suppress if TED ($FF00-$FF1F) references also present —
                    // those addresses are C16-specific, C64-impossible (kernal ROM).
                    if (c64_hits >= 3 && c64_strong >= 2 && c16_hits == 0)
                        result.confidence = std::max(result.confidence, 0.91f);
                    else if (vic20_hits >= 2 && c64_hits == 0)
                        result.confidence *= 0.50f;
                    else if (c16_hits >= 1 && c64_hits == 0)
                        result.confidence *= 0.50f;
                }
            }
        }

    } else if (matched_format == &LNX_FORMAT_DESCRIPTOR) {
        commodore_lynx_t lynx;
        if (lynx.open_mem(data, size)) {
            commodore_lynx_directory_t dir;
            if (lynx.read_directory(&dir)) {
                bool found_c64 = false;
                bool found_any = false;
                for (unsigned i = 0; i < dir.file_count; i++) {
                    if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                        size_t off = dir.entries[i].data_offset;
                        if (off + 1 < lynx.data_size) {
                            uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off+1] << 8);
                            found_any = true;
                            if (is_c64_load_address(addr)) { found_c64 = true; break; }
                        }
                    }
                }
                lynx.close();
                result.confidence = found_c64 ? 0.95f : (found_any ? 0.5f : 0.6f);
            } else {
                lynx.close();
                result.confidence = 0.6f;
            }
        } else {
            result.confidence = 0.6f;
        }

    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        commodore_d64_t d64;
        if (d64.open_mem(data, size)) {
            commodore_prg_t prg = {};
            if (d64.extract_first_prg(&prg)) {
                result.confidence = is_c64_load_address(prg.load_addr) ? 0.95f : 0.6f;
                commodore_prg_free(&prg);
            } else {
                result.confidence = (size == D64_STANDARD_SIZE || size == D64_STANDARD_SIZE_ERR ||
                                     size == D64_EXTENDED_SIZE || size == D64_EXTENDED_SIZE_ERR)
                                    ? 0.7f : 0.6f;
            }
            d64.close();
        } else {
            result.confidence = 0.6f;
        }

    } else if (matched_format == &T64_FORMAT_DESCRIPTOR) {
        result.confidence = 0.85f;  // T64 archives are C64-centric

    } else if (matched_format == &TAP_FORMAT_DESCRIPTOR) {
        int platform = commodore_tap_identify_platform_mem(data, size);
        if (platform == 0)      result.confidence = 0.95f;  // C64 TAP
        else if (platform == 1) result.confidence = 0.3f;   // VIC-20 TAP
        else                    result.confidence = 0.6f;    // Unknown

    } else if (matched_format == &CRT_FORMAT_DESCRIPTOR) {
        if (size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0)
            result.confidence = 1.0f;

    } else if (matched_format == &SID_FORMAT_DESCRIPTOR) {
        if (size >= 4 && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0)) {
            result.confidence = 1.0f;
            // SID v2+ flags encode video standard and chip model
            sid_header_t sid_hdr;
            if (sid_parse_header(data, size, &sid_hdr) && sid_hdr.version >= 2) {
                if (sid_hdr.video == SID_VIDEO_NTSC)
                    result.configuration.region_option_index = 1;       // NTSC
                else if (sid_hdr.video == SID_VIDEO_PAL)
                    result.configuration.region_option_index = 0;       // PAL
                if (sid_hdr.sid_model == SID_MODEL_8580)
                    result.configuration.custom_settings["sid_revision"] = "MOS 8580";
                else if (sid_hdr.sid_model == SID_MODEL_6581)
                    result.configuration.custom_settings["sid_revision"] = "MOS 6581";
            }
        } else {
            result.confidence = 0.9f;  // Extension match only
        }

    } else if (matched_format == &BIN_FORMAT_DESCRIPTOR) {
        result.confidence = 0.4f;
    }

    // =================================================================
    // =================================================================
    // Filepath heuristics — variant-specific keyword boost is now
    // handled generically by SystemRegistry::identify_system() using
    // aliases.  Only configuration hints (region) remain here.
    // =================================================================
    if (filepath) {
        std::string lower(filepath);
        for (auto& c : lower) c = static_cast<char>(tolower(c));

        // Region hint
        if (lower.find("ntsc") != std::string::npos)
            result.configuration.region_option_index = 1;
    }

    return result;
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
    traits.display.native_width = c64_constants::DISPLAY_WIDTH_PAL;
    traits.display.native_height = c64_constants::DISPLAY_HEIGHT_PAL;
    traits.display.visible_width = c64_constants::DISPLAY_WIDTH_PAL;
    traits.display.visible_height = c64_constants::DISPLAY_HEIGHT_PAL;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 0;  // Direct RGB
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // Audio
    traits.audio.format = AudioFormat::STEREO_16BIT;
    traits.audio.sample_rate_hz = c64_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 2;
    traits.audio.chip_name = "SID 6581";
    
    // Timing (PAL default)
    // cycles_per_frame must match the VIC-II's actual frame length
    // (MOS6569: 312 lines × 63 cycles = 19656) so that each run_frame()
    // produces exactly one video frame.  target_fps is the nearest integer
    // for display/audio calculations; precise pacing uses cycles/clock.
    traits.timing.cpu_frequency_hz = c64_constants::CPU_FREQ_PAL;
    traits.timing.video_frequency_hz = c64_constants::CPU_FREQ_PAL;
    traits.timing.audio_sample_rate_hz = c64_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = c64_constants::TARGET_FPS_PAL;
    traits.timing.cycles_per_frame = 19656;   // MOS6569 PAL: 312 × 63
    traits.timing.standard = VideoStandard::PAL;

    // Region options
    traits.video_standard_configs.push_back({
        "PAL",
        VideoStandard::PAL,
        traits.timing,
        true
    });

    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = c64_constants::CPU_FREQ_NTSC;
    ntsc_timing.video_frequency_hz = c64_constants::CPU_FREQ_NTSC;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 17095;   // MOS6567R8 NTSC: 263 × 65
    ntsc_timing.standard = VideoStandard::NTSC;

    traits.video_standard_configs.push_back({
        "NTSC",
        VideoStandard::NTSC,
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
    "c64",
    {"C64", "C-64", "Commodore 64"},
    c64_formats,
    create_c64_hardware_traits(),
    c64_probe_file,
    "Commodore", 1982, fam65xx::MOS6510Traits.display_name, SystemType::Home
};

C64System::C64System()
    : CommodoreSystem()  // Call base class constructor
    
{
    cycles_per_frame_ = 19705;  // PAL: c64_constants::CPU_FREQ_PAL / c64_constants::TARGET_FPS_PAL
    
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
// Chip creation + callback helpers
// ============================================================================

// CIA2 Port A change callback — updates VIC-II bank select
static void cia2_port_a_bank_callback(void* context, uint8_t port_a_value) {
    C64System* c64 = static_cast<C64System*>(context);
    vicii_base_t::memory_bank_change(c64->vicii, port_a_value & 0x03);
}

// CPU I/O port banking callback — updates PLA memory mode
static void cpu_banking_callback(void* context, uint8_t banking_state) {
    C64System* c64 = static_cast<C64System*>(context);
    c64->on_banking_change(banking_state);
}

// Context structure for CIA1 joystick + VIC-II lightpen callbacks.
// Defined here (above initialize) so the global instance is visible.
struct C64PortCallbackContext {
    C64System*   c64;
    C64System*   system;
};
static C64PortCallbackContext s_port_callback_ctx;

// Forward declarations — full definitions follow after the callback section.
static uint8_t c64_cia1_port_a_read_with_joystick(void* context, uint8_t port_a_output);
static uint8_t c64_cia1_port_b_read_with_joystick(void* context, uint8_t port_b_output);
static bool c64_vicii_lp_pin_read(void* context);

bool C64System::initialize() {
    if (initialized_) {
        return true;  // Already initialized
    }

    initialized_ = true;

    // Register main board (owns flat mem and chip binding)
    register_board(&board_);

    // =========================================================================
    // System infrastructure
    // =========================================================================

    // Cleanup helper for error paths.
    auto cleanup = [this]() {
        if (this->keyboard) {
            delete this->keyboard;
            this->keyboard = nullptr;
        }
        initialized_ = false;
    };

    // Bus pull-up defaults and cartridge lines (no cartridge)
    // Equivalent of old C64_BUS_DEFAULT_STATE macro — sets all pull-up lines
    default_state_ = MOS6510::default_bus_state()
                   | BUS_BIT(BUS_BA_BIT) | BUS_BIT(BUS_CNT_BIT)
                   | BUS_BIT(BUS_FLAG_BIT) | BUS_DATA_MASK;
    bus_state_     = default_state_;
    system_lines_  = SYS_MASK_EXROM | SYS_MASK_GAME;

    // =========================================================================
    // Bind all components (chips + ports) from the manifest
    // =========================================================================
    bind_all(board_, board_.components_, kC64Chips);
    port_manifest_       = kC64Chips.port_slots;
    port_manifest_count_ = kC64Chips.port_count;
    board_.create_chips(&bus_state_);
    board_.apply(bus_);

    // Convenience pointers — all point into board_ component fields.
    this->cpu      = &board_.cpu;
    this->ram      = &board_.ram;
    this->roml     = &board_.roml;
    this->basic    = &board_.basic;
    this->romh     = &board_.romh;
    this->vicii    = &board_.vicii;
    this->charrom  = &board_.charrom;
    this->sid      = &board_.sid;
    this->colorram = &board_.colorram;
    this->cia1     = &board_.cia1;
    this->cia2     = &board_.cia2;
    this->kernal   = &board_.kernal;

    if (!this->cpu) { cleanup(); return false; }

    // =========================================================================
    // Post-creation chip initialization (callbacks, timing, etc.)
    // =========================================================================

    // VIC-II
    this->vicii->init_base(
        get_vicii_standard() == VIC_PAL ? MOS6569_traits : MOS6567R8_traits,
        vicii_base_t::memory_bank_change);
    this->vicii->colorram = this->colorram;

    // SID — clock, timing, revision
    this->sid->init();
    {
        bool is_pal = (get_vicii_standard() == VIC_PAL);
        float cpu_clock = is_pal ? static_cast<float>(c64_constants::CPU_FREQ_PAL) : static_cast<float>(c64_constants::CPU_FREQ_NTSC);
        this->sid->set_cpu_clock(cpu_clock);
        this->sid->set_timing(is_pal);
    }

    // CIA1 — IRQ, TOD timing
    this->cia1->configured_interrupt_bit = BUS_IRQ_BIT;
    this->cia1->cycles_tod[0] = 1000000 / 60;
    this->cia1->cycles_tod[1] = 1000000 / 50;
    this->cia1->reset();

    // CIA2 — NMI, TOD timing
    this->cia2->configured_interrupt_bit = BUS_NMI_BIT;
    this->cia2->cycles_tod[0] = 1000000 / 60;
    this->cia2->cycles_tod[1] = 1000000 / 50;
    this->cia2->reset();

    // Create keyboard matrix
    this->keyboard = new commodore_keyboard_t();
    if (!this->keyboard->init(&c64_keyboard_config)) {
        log_info("ERROR: Failed to create keyboard\n");
        delete this->keyboard;
        this->keyboard = nullptr;
        cleanup();
        return false;
    }
    log_info("C64: Keyboard matrix initialized (all keys released)\n");

    // No cartridge I/O by default
    this->io1 = nullptr;
    this->io2 = nullptr;

    // =========================================================================
    // Wire I/O dispatch — IndexedSubTable + MMIO handlers
    // =========================================================================
    init_io_dispatch();

    // VIC-II memory read callback — routes through MemoryBus viewer 1
    this->vicii->bus.bus = nullptr;   // No longer using c64_bus_t
    this->vicii->bus.bank_change = nullptr;
    this->vicii->bus.mem_read = [](void* ctx, bus_state_t bus, uint16_t addr) -> bus_state_t {
        auto* sys = static_cast<C64System*>(ctx);
        // Use peek_byte for VIC-II viewer (viewer 1) — returns 0xFF for unmapped
        uint8_t data = sys->bus_.peek_byte(addr, C64BusSpec::Vic);
        BUS_SET_DATA(bus, data);
        return bus;
    };
    this->vicii->bus.mem_read_ctx = this;

    // =========================================================================
    // PLA memory maps — generate 32×2 ModeSnapshots
    // =========================================================================
    if (!pla_maps_generate()) { cleanup(); return false; }

    // Load ROMs from configured paths
    memory_init();

    // =========================================================================
    // Wire callbacks and initialize CPU
    // =========================================================================

    // CIA2 Port A → VIC-II bank selection
    this->cia2->port_a_change_callback = cia2_port_a_bank_callback;
    this->cia2->port_a_callback_context = this;
    cia2_port_a_bank_callback(this, this->cia2->port_a_value);  // Set initial bank

    // CIA2 interrupt line → NMI (CIA1 defaults to IRQ)
    this->cia2->configured_interrupt_bit = BUS_NMI_BIT;

    // Initialize CPU and point it at the reset vector
    cpu->init();
    cpu->init_io_port();
    cpu->bank_change_fn = cpu_banking_callback;
    cpu->bank_change_ctx = this;

    // Reset the CPU to start the hardware-accurate 7-cycle RESET sequence.
    cpu->reset();

    // Sync PLA banking with the freshly-reset IO port so KERNAL ROM is
    // visible during the vector fetch ticks.
    uint8_t banking_bits = cpu->io_port_regs.data
                         & cpu->io_port_regs.ddr
                         & 0x07;
    cpu_banking_callback(this, banking_bits);

    // =========================================================================
    // Phase 5: System-level initialization
    // =========================================================================

    // Track the actual VIC-II standard this system was created with.
    created_vicii_standard_ = get_vicii_standard();

    // Apply SID revision from configuration
    if (this->sid) {
        this->sid->set_revision(pending_sid_revision_);
        const char* rev_name = (pending_sid_revision_ == SID_REVISION_8580_R5) ? "MOS 8580" : "MOS 6581";
        log_info("C64: SID revision initialized as %s\n", rev_name);
    }

    // Create the layered keyboard mapper for character-based input
    if (this->keyboard) {
        keyboard_mapper_.reset(create_c64_keyboard_mapper(this->keyboard));
    }

    // Register chips for the Hardware menu and debug windows
    register_bus_chips(board_);
    {
        auto pla = std::make_unique<PLA906114>();
        pla->set_display_name("PLA / Address Decoder");
        pla->set_short_name("PLA");
#ifdef CERMU_HAS_GUI
        pla->set_system_context(this, c64_pla_render_debug, c64_pla_render_settings);
#endif
        register_chip(std::move(pla));
    }

    // Wire VIC-II to composite video output port
    video_port_ = std::make_unique<CompositeVideoPort>();
    vicii->set_video_out(&video_port_->output());

    // Compute back porch for signal→framebuffer reconstruction.
    // Back porch = distance in samples from HSync falling edge to first visible pixel.
    const auto& vt = (get_vicii_standard() == VIC_PAL) ? MOS6569_traits : MOS6567R8_traits;
    const uint16_t ppl = vt.cycles_per_line * 8;
    const int back_porch = (int(vt.first_visible_x_coord) - int(vt.hsync_end) + ppl) % ppl;
    video_port_->bind_display(nullptr, nullptr,
                              c64_constants::DISPLAY_WIDTH_PAL, back_porch);
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire SID to audio signal port
    audio_port_ = std::make_unique<AudioPort>();
    sid->set_audio_port(audio_port_.get());

    // =========================================================================
    // Wire connector-port callbacks (CIA1 joystick, VIC-II lightpen, keyboard)
    // =========================================================================

    // Attach internal keyboard device (PORT_KEYBOARD is the last manifest port)
    {
        auto kb_device = std::make_unique<CommodoreKeyboardDevice>(this->keyboard);
        auto* kb_raw = kb_device.get();
        get_port(PORT_KEYBOARD)->attach_device(kb_raw);
        owned_devices_.push_back(std::move(kb_device));
    }

    // Wire joystick-aware CIA1 callbacks (keyboard + wired-AND joystick)
    s_port_callback_ctx.c64 = this;
    s_port_callback_ctx.system = this;
    this->cia1->port_a_read_callback = c64_cia1_port_a_read_with_joystick;
    this->cia1->port_a_read_context  = &s_port_callback_ctx;
    this->cia1->port_b_read_callback = c64_cia1_port_b_read_with_joystick;
    this->cia1->port_b_read_context  = &s_port_callback_ctx;

    // Wire VIC-II LP pin read callback (Control Port 1 pin 6 → VIC-II LP input)
    this->vicii->bus.lp_pin_read    = c64_vicii_lp_pin_read;
    this->vicii->bus.lp_pin_context = &s_port_callback_ctx;

    log_info("C64: System initialized successfully\n");
    return true;
}

void C64System::shutdown() {
    // Free any pending load that was never applied
    if (pending_load_.active) {
        pending_load_.result.release();
        pending_load_.active = false;
    }
    if (initialized_) {
        // Destroy keyboard (not a manifest chip — manually managed)
        if (this->keyboard) {
            delete this->keyboard;
            this->keyboard = nullptr;
        }

        // Null out convenience pointers (Board owns the chip lifetimes)
        this->cpu      = nullptr;
        this->ram      = nullptr;
        this->roml     = nullptr;
        this->basic    = nullptr;
        this->romh     = nullptr;
        this->vicii    = nullptr;
        this->charrom  = nullptr;
        this->sid      = nullptr;
        this->colorram = nullptr;
        this->cia1     = nullptr;
        this->cia2     = nullptr;
        this->kernal   = nullptr;

        initialized_ = false;
    }

    System::shutdown();
}

void C64System::reset() {
    // Clear any pending deferred load (will be re-set by the next load_file call)
    reset_load_state();
    sid_player_active_ = false;
    active_sid_data_.clear();
    if (initialized_) {
        log_info("C64 System: Performing system-wide reset...\n");

        // Reset CIA chips first (they control interrupts and I/O)
        if (this->cia1) this->cia1->reset();
        if (this->cia2) this->cia2->reset();

        // Reset VIC-II to clear sprite pipeline state
        if (this->vicii) this->vicii->reset();

        // Reset SID — clears all registers, envelopes, and the sample ring buffer
        if (this->sid) this->sid->reset();

        // Reset CPU last (so it can read the reset vector after other chips are ready)
        // Use mos6510_reset (not mos6510_init) to properly reset the instruction
        // decoder state (current_handler, half_cycle, opcode_entry).  init() only
        // reinitialises the IO port — it leaves the CPU mid-instruction, which
        // causes a segfault when emulation resumes with an inconsistent pipeline.
        if (this->cpu) {
            cpu->reset();
            cpu->bank_change_fn = cpu_banking_callback;
            cpu->bank_change_ctx = this;

            // Trigger banking callback so PLA matches the freshly-reset IO port
            uint8_t banking_bits = cpu->io_port_regs.data
                                 & cpu->io_port_regs.ddr
                                 & 0x07;
            cpu_banking_callback(this, banking_bits);
        }

        // Reset keyboard
        if (this->keyboard) this->keyboard->reset();

        // Reset cycle counter
        total_cycles_ = 0;

        // Clear the memory locations that is_basic_ready() checks, so stale
        // values from the previous session don't cause premature detection.
        // KERNAL boot will set these properly: RAMTAS clears zero page
        // (including $2D), $E453 copies the vector table ($0302/$0303),
        // and NEW sets VARTAB ($2D) to TXTTAB+2.
        if (this->ram) {
            this->ram->data()[0x0302] = 0;
            this->ram->data()[0x0303] = 0;
            this->ram->data()[0x002D] = 0;
            // Clear the keyboard buffer count so is_basic_ready() doesn't
            // get stuck waiting for a stale non-zero $C6 left by a
            // previously running program.
            this->ram->data()[c64_constants::KBD_BUFFER_COUNT] = 0;
        }

        // Reset serial trap state
        serial_trap_ = {};

        log_info("C64 System: Reset complete\n");
    }
}

// ============================================================================
// KERNAL SERIAL TRAPS — IEC bus trap handlers
// ============================================================================
// Intercept KERNAL serial bus routines to provide instant drive I/O.
// Addresses match the standard C64 KERNAL ROM (901227-03).
// Same approach as VICE's serial-trap.c but dispatching directly to our
// Drive1541Device channel buffers.
// ============================================================================

// KERNAL serial routine addresses (from VICE c64.c c64_serial_traps[])
static constexpr uint16_t TRAP_SERIAL_LISTEN      = 0xED24;
static constexpr uint16_t TRAP_SERIAL_SA_LISTEN    = 0xED37;
static constexpr uint16_t TRAP_SERIAL_SEND_BYTE    = 0xED41;
static constexpr uint16_t TRAP_SERIAL_RECEIVE_BYTE = 0xEE14;
static constexpr uint16_t TRAP_SERIAL_READY        = 0xEEA9;
static constexpr uint16_t TRAP_RESUME_ADDRESS      = 0xEDAB;  // RTS in KERNAL

// KERNAL zero-page addresses for serial I/O
static constexpr uint16_t ZP_BSOUR  = 0x95;   // Buffered character for serial bus
static constexpr uint16_t ZP_TMP_IN = 0xA4;   // Temp storage for received byte
static constexpr uint16_t ZP_STATUS = 0x90;   // I/O status word (ST)

// IEC command byte masks
static constexpr uint8_t IEC_LISTEN_MASK   = 0x20;
static constexpr uint8_t IEC_TALK_MASK     = 0x40;
static constexpr uint8_t IEC_SECOND_MASK   = 0x60;
static constexpr uint8_t IEC_CLOSE_MASK    = 0xE0;
static constexpr uint8_t IEC_OPEN_MASK     = 0xF0;
static constexpr uint8_t IEC_UNLISTEN      = 0x3F;
static constexpr uint8_t IEC_UNTALK        = 0x5F;
static constexpr uint8_t IEC_DEVNR_MASK    = 0x0F;

Drive1541Device* C64System::find_iec_drive(int device_number) {
    if (device_number < 4) return nullptr;
    auto* port = get_port(PORT_IEC_SERIAL);
    if (!port) return nullptr;
    for (auto* dev : port->get_attached_devices()) {
        auto* drive = dynamic_cast<Drive1541Device*>(dev);
        if (drive && drive->get_device_number() == device_number) return drive;
    }
    return nullptr;
}

bool C64System::check_serial_traps(uint16_t pc) {
    switch (pc) {
        case TRAP_SERIAL_LISTEN:
        case TRAP_SERIAL_SA_LISTEN:
            return serial_trap_attention();
        case TRAP_SERIAL_SEND_BYTE:
            return serial_trap_send();
        case TRAP_SERIAL_RECEIVE_BYTE:
            return serial_trap_receive();
        case TRAP_SERIAL_READY:
            return serial_trap_ready();
        default:
            return false;
    }
}

bool C64System::serial_trap_attention() {
    uint8_t iecdata = ram->data()[ZP_BSOUR];

    if (iecdata == IEC_UNLISTEN) {
        // UNLISTEN — finalize pending OPEN (send accumulated filename)
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) {
            drive->trap_unlisten();
        }
        serial_trap_.active_device = -1;
    } else if (iecdata == IEC_UNTALK) {
        // UNTALK — end talk session
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) {
            drive->trap_untalk();
        }
        serial_trap_.active_device = -1;
    } else if ((iecdata & 0xF0) == IEC_LISTEN_MASK || (iecdata & 0xF0) == IEC_TALK_MASK) {
        // LISTEN or TALK — address a device
        serial_trap_.active_device = iecdata & IEC_DEVNR_MASK;
        serial_trap_.trap_device = iecdata;
        serial_trap_.trap_secondary = 0;
    } else if ((iecdata & 0xF0) == IEC_SECOND_MASK) {
        // SECONDARY — set secondary address for data transfer
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) {
            drive->trap_second(iecdata & 0x0F);
        }
    } else if ((iecdata & 0xF0) == IEC_OPEN_MASK) {
        // OPEN — begin opening a channel (filename follows via CIOUT)
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) {
            drive->trap_open(iecdata & 0x0F);
        }
    } else if ((iecdata & 0xF0) == IEC_CLOSE_MASK) {
        // CLOSE — close a channel
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) {
            drive->trap_close(iecdata & 0x0F);
        }
    }

    // Check if the addressed device is present
    if (serial_trap_.active_device >= 4) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (!drive) {
            ram->data()[ZP_STATUS] |= 0x80;  // Device not present
        }
    }

    // Clear carry and interrupt disable flags (as the real KERNAL would)
    uint8_t p = cpu->get(P);
    p &= ~0x01;  // Clear carry
    p &= ~0x04;  // Clear interrupt disable
    cpu->set(P, p);

    // Resume at the KERNAL's RTS
    cpu->set(PC, TRAP_RESUME_ADDRESS);
    cpu->transition_to_fetch();
    return true;
}

bool C64System::serial_trap_send() {
    // Only handle if we have a valid device
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    uint8_t iecdata = ram->data()[ZP_BSOUR];

    // If no secondary address was sent, default to SA 0
    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC_SECOND_MASK;
        drive->trap_second(0);
    }

    drive->trap_send(iecdata);

    // Clear carry and interrupt disable
    uint8_t p = cpu->get(P);
    p &= ~0x01;
    p &= ~0x04;
    cpu->set(P, p);

    cpu->set(PC, TRAP_RESUME_ADDRESS);
    cpu->transition_to_fetch();
    return true;
}

bool C64System::serial_trap_receive() {
    // Only handle if we have a valid device
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    // If no secondary address was sent, default to SA 0
    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC_SECOND_MASK;
        drive->trap_second(0);
    }

    uint8_t data = 0;
    int status = drive->trap_receive(data);

    // Store received byte in TMP_IN and A register
    ram->data()[ZP_TMP_IN] = data;
    cpu->set(A, data);

    // Set/update I/O status (ST)
    if (status) {
        ram->data()[ZP_STATUS] |= static_cast<uint8_t>(status);
    }

    // Set CPU flags to match the received byte
    uint8_t p = cpu->get(P);
    p &= ~0x01;  // Clear carry
    p &= ~0x04;  // Clear interrupt disable
    // Set N (sign) and Z (zero) flags based on data
    if (data & 0x80) p |= 0x80; else p &= ~0x80;
    if (data == 0)   p |= 0x02; else p &= ~0x02;
    cpu->set(P, p);

    cpu->set(PC, TRAP_RESUME_ADDRESS);
    cpu->transition_to_fetch();
    return true;
}

bool C64System::serial_trap_ready() {
    // Only handle if we have a valid device on the bus
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    // Fake the serial-ready check: pretend the bus signals are fine
    cpu->set(A, 1);

    uint8_t p = cpu->get(P);
    p &= ~0x80;  // Clear sign
    p &= ~0x02;  // Clear zero
    p &= ~0x04;  // Clear interrupt disable
    cpu->set(P, p);

    cpu->set(PC, TRAP_RESUME_ADDRESS);
    cpu->transition_to_fetch();
    return true;
}

// ============================================================================
// system_tick — inline system tick (performance-critical hot loop)
// ============================================================================
// This is the core emulation loop — ticks all chips in correct phase order.
// ============================================================================

void C64System::system_tick() {
    total_cycles_++;

    // Start each cycle with pull-up resistors (default_state: IRQ=1, NMI=1, BA=1, AEC=1, RDY=1)
    bus_state_t s = default_state_;
    BUS_SET_ADDR(s, BUS_GET_ADDR(bus_state_));
    BUS_SET_DATA(s, BUS_GET_DATA(bus_state_));

    // PHASE 1: VIC-II PHI1 — g-access read, pixel sequencing
    s = vicii->tick_phi1(s);

    // PHASE 1.5: CIA PHI2 — apply pending interrupt lines before CPU
    s = cia2->tick_phi2(s);
    s = cia1->tick_phi2(s);

    // BA→RDY wiring (direct bit test + set/clear)
    if (BUS_GET_BIT(s, BUS_BA_BIT))
        BUS_SET_BIT(s, BUS_RDY_BIT);
    else
        BUS_CLR_BIT(s, BUS_RDY_BIT);

    // PHASE 2: CPU PHI2 — instruction execution (direct C++ call, inlineable)
    s = cpu->tick<MOS6510::Phase::PHI2>(s);

    // PHASE 3: Address decode + buffer service + MMIO self-dispatch.
    // AEC determines CPU vs VIC-II bus ownership for read viewer selection.
    // Writes always use CPU viewer (viewer 0).
    {
        const bool is_write = !BUS_GET_BIT(s, BUS_RW_BIT);
        const size_t viewer = (is_write || BUS_GET_BIT(s, BUS_AEC_BIT))
                                  ? C64BusSpec::Cpu : C64BusSpec::Vic;
        s = bus_.resolve(s, viewer);  // page table → CS field
        s = bus_.service(s);          // buffer (RAM/ROM) access — MMIO IDs skipped

        // MMIO self-dispatch — each chip checks is_cs_selected() and handles
        // its own register I/O.  Chips not selected return bus unchanged.
        s = vicii->tick_mmio(s);
        s = sid->tick_mmio(s);
        s = colorram->tick_mmio(s);
        s = cia1->tick_mmio(s);
        s = cia2->tick_mmio(s);

        // Debug cart capture ($D7FF) — VICE test convention, not real hardware.
        if (unlikely(debug_cart_enabled_ && !BUS_GET_BIT(s, BUS_RW_BIT)
                     && BUS_GET_ADDR(s) == 0xD7FF)) {
            debug_cart_value_ = BUS_GET_DATA(s);
            debug_cart_written_ = true;
        }
    }

    // NMI edge detection — sample after bus dispatch (post-dispatch state)
    cpu->sample_nmi_pin(s);

    // PHASE 3.1: VIC-II PHI2 — c/p/s-access data delivery
    vicii->tick_phi2(s);

    // PHASE 3.5: CIA PHI1 — timer counting, TOD, interrupt generation
    s = cia2->tick_phi1(s);
    s = cia1->tick_phi1(s);

    // PHASE 4: CPU PHI1 — prepare next fetch (direct C++ call, inlineable)
    s = cpu->tick<MOS6510::Phase::PHI1>(s);

    // KERNAL serial trap check — intercept IEC bus routines at instruction boundaries
    if (serial_traps_enabled_) {
        if (cpu->opdone()) {
            uint16_t pc = cpu->get(PC);
            // All serial trap addresses are in the $ED00-$EEFF range
            if (pc >= 0xED00 && pc < 0xEF00) {
                check_serial_traps(pc);
            }
        }
    }

    // Restore R/W line to read mode
    BUS_SET_BIT(s, BUS_RW_BIT);

    // PHASE 5: SID — sound generation (audio only, register I/O handled above)
    s = sid->tick_audio(s);

    bus_state_ = s;
}

void C64System::tick() {
    if (initialized_) {
        system_tick();

        // Check if a deferred file load is waiting for BASIC to reach READY
        check_deferred_load();
    }
}

void C64System::run_frame() {
    if (!initialized_ || !video_port_) return;

    // Update per-frame state for peripheral devices before cycle loop.
    // Lightpen: pass display rect so it can convert SDL mouse → VIC-II coords.
    update_lightpen_display_rect();

    // Run until the video chip drives FrameEnd into the output — the
    // frame boundary is implicit in the video signal, just like on a CRT.
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        system_tick();
    }

    video_port_->swap_frame();

    // Check deferred load once per frame (only active during boot)
    check_deferred_load();

    // Tick all attached peripheral devices (datasette timing, 1541 IEC, etc.)
    tick_peripherals();
}

// ============================================================================
// Commodore Load Helper Callbacks -- C64-specific
// ============================================================================

static uint8_t c64_mem_read(void* ctx, uint16_t addr) {
    auto* ram = static_cast<RAMChip*>(ctx);
    return ram->data()[addr];
}

static void c64_mem_write_byte(void* ctx, uint16_t addr, uint8_t val) {
    auto* ram = static_cast<RAMChip*>(ctx);
    ram->data()[addr] = val;
}

static void c64_mem_write_block(void* ctx, uint16_t addr,
                                const uint8_t* data, size_t len) {
    auto* ram = static_cast<RAMChip*>(ctx);
    memcpy(&ram->data()[addr], data, len);
}

// ============================================================================
// CommodoreSystem virtual hook implementations — C64
// ============================================================================

bool C64System::is_basic_ready() const {
    if (!initialized_ || !this->ram) return false;

    const uint8_t* ram = this->ram->data();

    // BASIC warm-start vector at $0302/$0303 = $A483.  NEW must also have
    // run (VARTAB $2D != 0) to avoid $0801/$0802 corruption on first boot.
    if (ram[0x0302] != 0x83 || ram[0x0303] != 0xA4)
        return false;
    if (ram[c64_constants::KBD_BUFFER_COUNT] != 0)
        return false;
    if (!boot_completed_ && ram[0x002D] == 0)
        return false;

    return true;
}

commodore_load_context_t C64System::build_load_context() {
    commodore_load_context_t ctx = {};
    ctx.system_name     = "C64";
    ctx.write_byte      = c64_mem_write_byte;
    ctx.write_block     = c64_mem_write_block;
    ctx.mem_read        = c64_mem_read;
    ctx.mem_ctx         = this->ram;
    ctx.basic_params    = &COMMODORE_BASIC_C64;
    ctx.basic_start_addrs[0] = c64_constants::BASIC_START;
    ctx.default_raw_addr = 0xC000;
    ctx.set_pc          = nullptr;
    ctx.pc_ctx          = nullptr;
    return ctx;
}

void C64System::inject_keys(const char* str) {
    if (!this->ram) return;
    int len = static_cast<int>(strlen(str));
    if (len > 10) len = 10;  // C64 keyboard buffer capacity
    for (int i = 0; i < len; i++) {
        this->ram->data()[c64_constants::KBD_BUFFER_BASE + i] =
            static_cast<uint8_t>(str[i]);
    }
    this->ram->data()[c64_constants::KBD_BUFFER_COUNT] =
        static_cast<uint8_t>(len);
}

bool C64System::on_file_parsed(format_load_result_t& result,
                               const char* filepath) {
    (void)filepath;

    // Clear SID player state from any previous load
    sid_player_active_ = false;
    active_sid_data_.clear();

    // For SID files: ensure the C64 is configured with the correct region
    // (PAL/NTSC) and SID revision, reset for clean state, and patch KERNAL
    // to skip the RAMTAS memory test for near-instant boot.
    const sid_header_t* sid = sid_get_metadata(&result);
    if (sid) {
        ensure_compatible_for_sid(sid);

        // Set rich program title from SID metadata
        if (sid->name[0]) {
            program_title_ = sid->name;
            if (sid->author[0]) {
                program_title_ += " - ";
                program_title_ += sid->author;
            }
        }
    }

    return true;
}

bool C64System::pre_apply_pending_load() {
    const sid_header_t* sid = sid_get_metadata(&pending_load_.result);
    if (!sid) return false;

    // RSID with init_addr=0: BASIC program SID — fall through to standard path
    bool is_basic_sid = (sid->type == SID_TYPE_RSID && sid->init_addr == 0);
    if (is_basic_sid) {
        log_info("C64: RSID BASIC program — loading as standard BASIC PRG\n");
        return false;
    }

    // Inject 6502 SID player stub
    uint16_t subtune = sid->start_song;
    if (subtune > 0) subtune--;

    c64_apply_sid_load(this, sid, &pending_load_.result.program, subtune);

    // Keep SID header and payload for interactive subtune switching
    active_sid_header_ = *sid;
    const auto& prog = pending_load_.result.program;
    if (prog.data && prog.data_size > 0) {
        active_sid_data_.assign(prog.data, prog.data + prog.data_size);
    } else {
        active_sid_data_.clear();
    }
    active_subtune_ = subtune;
    sid_player_active_ = true;

    if (sid->version >= 2 && sid->sid_model != SID_MODEL_UNKNOWN) {
        pending_sid_revision_ = (sid->sid_model == SID_MODEL_8580)
                                ? SID_REVISION_8580_R5
                                : SID_REVISION_6581_R4AR;
    }

    return true;  // Handled — skip DISK_FAST/TAPE/STANDARD paths
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
    vicii_standard_t needed_standard = get_vicii_standard();  // default: keep
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
    // not the configuration value which may have been updated without
    // recreating the chip.
    bool region_changed = (needed_standard != created_vicii_standard_);

    if (region_changed) {
        log_info("C64: SID requires %s — recreating system (was %s)\n",
               needed_standard == VIC_NTSC ? "NTSC" : "PAL",
               get_vicii_standard() == VIC_NTSC ? "NTSC" : "PAL");

        // Save current peripheral assignments before shutdown
        // destroys owned_devices_ and ports.
        std::vector<std::pair<int, std::string>> saved_devices;
        {
            const auto& ports = get_ports();
            for (int i = 0; i < static_cast<int>(ports.size()); ++i) {
                if (auto* dev = ports[i]->get_attached_device()) {
                    saved_devices.emplace_back(i, dev->get_id());
                }
            }
        }

        shutdown();

        // Update the configuration before recreation
        config_.region_option_index = needed_region_index;
        pending_sid_revision_ = needed_revision;

        // Sync cycles_per_frame_ and hardware_traits_ via apply_configuration
        apply_configuration();

        if (!initialize()) {
            log_info("C64: ERROR — failed to reinitialize after region change\n");
            return;
        }

        // Re-attach peripherals that were present before the reinit
        if (!saved_devices.empty()) {
            for (auto& [port_idx, dev_id] : saved_devices) {
                attach_device_to_port(port_idx, dev_id.c_str());
            }
            auto_assign_controller_keymaps();
        } else {
            attach_default_peripherals();
        }
    } else {
        // Same region — just update SID revision in-place and reset
        if (needed_revision != pending_sid_revision_) {
            pending_sid_revision_ = needed_revision;
            if (initialized_ && this->sid) {
                this->sid->set_revision(needed_revision);
                log_info("C64: SID revision set to %s (from SID file flags)\n",
                       needed_revision == SID_REVISION_8580_R5 ? "MOS 8580" : "MOS 6581");
            }
        }
        reset();
    }

    // ---- Patch KERNAL for fast SID boot ----
    patch_skip_memtest();

    // Reset boot-completed flag so the deferred load machinery works
    boot_completed_ = false;
}

// ============================================================================

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
    } else if (initialized_ && this->keyboard) {
        // No mapper â€” convert SDL keycode to EmuKey and pass through
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                this->keyboard->key_down(ek, false);
            } else {
                this->keyboard->key_up(ek, false);
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
            auto* iec_port = get_port(PORT_IEC_SERIAL);
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

    auto* port = get_port(port_index);
    if (port) {
        auto* device = port->get_attached_device();
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
// Right / + / PageDown:  next subtune (wraps from last → first)
// Left / - / PageUp:     previous subtune (wraps from first → last)
// =============================================================================

bool C64System::handle_sid_player_key(SDL_Keycode key) {
    if (!initialized_ || active_sid_header_.num_songs == 0) return false;

    const uint16_t num_songs = active_sid_header_.num_songs;
    int new_subtune = -1;

    // Digit keys: 1→subtune 1, 2→subtune 2, ..., 9→subtune 9, 0→subtune 10
    if (key >= SDLK_0 && key <= SDLK_9) {
        int digit = (key == SDLK_0) ? 10 : (key - SDLK_0);
        if (digit <= num_songs) {
            new_subtune = digit - 1;  // Convert to 0-based
        }
    }
    // Cursor right / Plus / = / PageDown = next subtune (with wrapping)
    else if (key == SDLK_RIGHT || key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_PAGEDOWN) {
        new_subtune = (active_subtune_ + 1) % num_songs;
    }
    // Cursor left / Minus / PageUp = previous subtune (with wrapping)
    else if (key == SDLK_LEFT || key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_PAGEUP) {
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
    c64_sid_switch_subtune(this, &active_sid_header_,
                            active_sid_data_.data(), active_sid_data_.size(),
                            active_subtune_);
    return true;
}

void C64System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset C64")) {
        reset();
    }
#endif
}

const char* C64System::get_mode_label() const {
    return sid_player_active_ ? "SID Player" : nullptr;
}

std::string C64System::get_subtitle_info() const {
    if (!sid_player_active_ || active_sid_header_.num_songs <= 1) return {};
    return "[" + std::to_string(active_subtune_ + 1) + "/" +
           std::to_string(active_sid_header_.num_songs) + "]";
}

void C64System::render_debug_windows(void* gui_state, std::mutex& emu_mutex) {
#ifdef CERMU_HAS_GUI
    if (!initialized_) return;
    System::render_debug_windows(gui_state, emu_mutex);
#endif
}

// ============================================================================
// Chip Registration — populate registered_chips_ for Hardware menu + debug
// ============================================================================

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

vicii_standard_t C64System::get_vicii_standard() const {
    return (config_.region_option_index == 1) ? VIC_NTSC : VIC_PAL;
}

bool C64System::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
        hardware_traits_.timing = std_cfg.timing;  // Keep active timing in sync
        cached_target_fps_ = std_cfg.timing.target_fps;  // Keep FPS pacing in sync
    }

    // Apply SID revision from custom settings
    auto sid_it = config_.custom_settings.find("sid_revision");
    if (sid_it != config_.custom_settings.end()) {
        sid_revision_t rev = SID_REVISION_6581_R4AR;
        if (sid_it->second == "MOS 8580") {
            rev = SID_REVISION_8580_R5;
        }
        if (initialized_ && this->sid) {
            this->sid->set_revision(rev);
            log_info("C64: SID revision set to %s\n", sid_it->second.c_str());
        }
        // Store for later (SID may not exist yet during initial config)
        pending_sid_revision_ = rev;
    }

    // Apply display palette selection
    if (initialized_)
        apply_display_palette_();

    return true;
}


void C64System::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    if (initialized_ && this->vicii) {
        if (palette_selector::render(*this->vicii, config_.custom_settings)) {
            set_configuration(config_);
            apply_configuration();
        }
    }
#endif
}

// ============================================================================
// JOYSTICK-AWARE CIA1 PORT CALLBACKS
// ============================================================================
// These replace the default CIA1 port callbacks set during initialize().
// They first call the keyboard scanning logic, then AND-in the joystick
// state from the connector port (wired-AND, matching real hardware).

/// CIA1 Port A read callback — combines keyboard reverse-scan with Control Port 2 joystick.
static uint8_t c64_cia1_port_a_read_with_joystick(void* context, uint8_t port_a_output) {
    auto* ctx = static_cast<C64PortCallbackContext*>(context);
    auto* c64 = ctx->c64;

    // Keyboard reverse scanning
    uint8_t col_state = 0xFF;
    if (c64 && c64->keyboard && c64->cia1) {
        uint8_t port_b_output = c64->cia1->port_b_value;
        uint8_t row_select = ~port_b_output;
        for (int row = 0; row < 8; row++) {
            if (row_select & (1 << row)) {
                col_state &= static_cast<uint8_t>(c64->keyboard->row_open_contacts[row]);
            }
        }
    }

    // AND-in Control Port 2 joystick state (bits 0-4 of CIA1 PA)
    // Joystick connector signals map to CIA1 PA:
    //   JOY_UP(0)→PA0, JOY_DOWN(1)→PA1, JOY_LEFT(2)→PA2, JOY_RIGHT(3)→PA3, JOY_FIRE(6)→PA4
    if (ctx->system) {
        auto* port = ctx->system->get_port(C64System::PORT_CONTROL2);
        if (port && port->get_attached_device()) {
            uint32_t dev_signals = port->get_attached_device()->get_output_signals();
            // Map connector signal bits to CIA1 PA bits
            uint8_t joy_mask = 0xFF;
            if (!(dev_signals & (1u << PortSignals::JOY_UP)))    joy_mask &= ~0x01;
            if (!(dev_signals & (1u << PortSignals::JOY_DOWN)))  joy_mask &= ~0x02;
            if (!(dev_signals & (1u << PortSignals::JOY_LEFT)))  joy_mask &= ~0x04;
            if (!(dev_signals & (1u << PortSignals::JOY_RIGHT))) joy_mask &= ~0x08;
            if (!(dev_signals & (1u << PortSignals::JOY_FIRE)))  joy_mask &= ~0x10;
            col_state &= joy_mask;
        }
    }

    return col_state;
}

/// CIA1 Port B read callback — combines keyboard forward-scan with Control Port 1 joystick.
static uint8_t c64_cia1_port_b_read_with_joystick(void* context, uint8_t port_b_output) {
    auto* ctx = static_cast<C64PortCallbackContext*>(context);
    auto* c64 = ctx->c64;

    // Keyboard forward scanning
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
    if (ctx->system) {
        auto* port = ctx->system->get_port(C64System::PORT_CONTROL1);
        if (port && port->get_attached_device()) {
            uint32_t dev_signals = port->get_attached_device()->get_output_signals();
            uint8_t joy_mask = 0xFF;
            if (!(dev_signals & (1u << PortSignals::JOY_UP)))    joy_mask &= ~0x01;
            if (!(dev_signals & (1u << PortSignals::JOY_DOWN)))  joy_mask &= ~0x02;
            if (!(dev_signals & (1u << PortSignals::JOY_LEFT)))  joy_mask &= ~0x04;
            if (!(dev_signals & (1u << PortSignals::JOY_RIGHT))) joy_mask &= ~0x08;
            if (!(dev_signals & (1u << PortSignals::JOY_FIRE)))  joy_mask &= ~0x10;
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
    auto* lightpen = ctx->system ? ctx->system->get_cached_lightpen() : nullptr;
    if (!lightpen) return true;

    uint16_t beam_x = ctx->c64->vicii->get_x_coordinate();
    uint16_t beam_y = ctx->c64->vicii->get_raster_counter();
    return lightpen->get_lp_pin_state(beam_x, beam_y);
}

void C64System::update_lightpen_display_rect() {
    if (!cached_lightpen_) return;
    const auto& rect = get_display_screen_rect();
    cached_lightpen_->set_display_screen_rect(rect.x, rect.y, rect.w, rect.h);
}

void C64System::on_port_device_changed(int port_index) {
    // Update cached lightpen for Control Port 1
    if (port_index == PORT_CONTROL1) {
        cached_lightpen_ = nullptr;
        auto* port = get_port(PORT_CONTROL1);
        if (port) {
            auto* device = port->get_attached_device();
            if (device && strcmp(device->get_id(), "lightpen") == 0) {
                cached_lightpen_ = static_cast<LightpenDevice*>(device);
            }
        }
    }

    // Update serial-traps-enabled flag when IEC serial port changes
    if (port_index == PORT_IEC_SERIAL) {
        serial_traps_enabled_ = false;
        auto* port = get_port(PORT_IEC_SERIAL);
        if (port) {
            for (auto* dev : port->get_attached_devices()) {
                if (dynamic_cast<Drive1541Device*>(dev)) {
                    serial_traps_enabled_ = true;
                    break;
                }
            }
        }
    }
}

uint32_t C64System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!initialized_ || !buffer || max_samples == 0) return 0;
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }
    if (!this->sid) return 0;
    this->sid->generate_samples(buffer, max_samples);
    return max_samples;
}

void C64System::set_audio_sample_rate(int sample_rate_hz) {
    if (initialized_ && this->sid && sample_rate_hz > 0) {
        log_info("C64: Updating SID sample rate from %.0f to %d Hz\n",
               this->sid->sample_rate, sample_rate_hz);
        this->sid->set_sample_rate(static_cast<float>(sample_rate_hz));
    }
}

// ============================================================================
// KERNAL Patches
// ============================================================================

bool C64System::patch_skip_memtest() {
    return c64_patch_skip_memtest(this);
}

// ============================================================================
// PLA Memory Map Generation
// ============================================================================

// Convert shared PlaOutput enum back to C64-specific C64PlaChipId for debug GUI.
static constexpr C64PlaChipId kPlaOutputToC64Id[] = {
    C64PlaChipId::ram,       // PlaOutput::ram
    C64PlaChipId::basic,     // PlaOutput::basic
    C64PlaChipId::kernal,    // PlaOutput::kernal
    C64PlaChipId::charrom,   // PlaOutput::charrom
    C64PlaChipId::io,        // PlaOutput::io
    C64PlaChipId::roml,      // PlaOutput::roml
    C64PlaChipId::romh,      // PlaOutput::romh
    C64PlaChipId::unmapped,  // PlaOutput::unmapped
};

bool C64System::pla_maps_generate() {
    const auto no_chip_rd = C64ChipId(C64PT::kNoChipSelected);
    const auto no_chip_wr = C64WriteId(C64PT::kNoChipSelectedWrite);

    // Build the chip-ID mapping from the C64 manifest's PLA chip table.
    PlaChipMapping<C64BusSpec> mapping;
    auto entry = [](C64PlaChipId id) -> PlaChipMapping<C64BusSpec>::ReadEntry {
        const auto& d = kC64PlaChipTable[size_t(id)];
        return {d.base_id, d.bank_mask};
    };
    mapping.ram           = entry(C64PlaChipId::ram);
    mapping.basic         = entry(C64PlaChipId::basic);
    mapping.kernal        = entry(C64PlaChipId::kernal);
    mapping.charrom       = entry(C64PlaChipId::charrom);
    mapping.roml          = entry(C64PlaChipId::roml);
    mapping.romh          = entry(C64PlaChipId::romh);
    mapping.io_sub_read   = C64Bus::indexed_sub_chip(0);
    mapping.io_sub_write  = C64Bus::indexed_sub_write_chip(0);
    mapping.no_chip_read  = no_chip_rd;
    mapping.no_chip_write = no_chip_wr;
    mapping.ram_write_base = C64WriteId(kC64PlaChipTable[size_t(C64PlaChipId::ram)].base_id);
    mapping.ram_write_mask = kC64PlaChipTable[size_t(C64PlaChipId::ram)].bank_mask;

    // Generate all 32 PLA mode snapshots with debug info collection
    PlaDebugInfo debug{};
    generate_pla_mode_snapshots<C64BusSpec>(
        bus_, mapping, C64BusSpec::Cpu, C64BusSpec::Vic,
        cpu_snapshots_, vicii_snapshots_, &debug);

    // Convert PlaOutput debug arrays to C64PlaChipId for the PLA debug GUI
    for (int mode = 0; mode < 32; mode++) {
        for (int bank = 0; bank < 16; bank++) {
            pla_cpu_read_chip_[mode][bank]  = kPlaOutputToC64Id[size_t(debug.cpu_read[mode][bank])];
            pla_cpu_write_chip_[mode][bank] = kPlaOutputToC64Id[size_t(debug.cpu_write[mode][bank])];
            pla_vicii_read_chip_[mode][bank] = kPlaOutputToC64Id[size_t(debug.vic_read[mode][bank])];
        }
    }

    // Set initial mode ($1F = standard, no cartridge)
    uint8_t initial_mode = generate_pla_mode(0x07);
    mode_switch(initial_mode);

    log_info("C64: PLA banking initialized (mode=$%02X, %zu snapshots per viewer)\n",
           initial_mode, kC64NumPlaModes);
    return true;
}

// ============================================================================
// I/O DISPATCH — IndexedSubTable + MMIO handlers
// ============================================================================

void C64System::init_io_dispatch() {
    // Register MMIO handlers (kept for debug tools and non-CS fallback paths)
    (void)bus_.register_handler({this->vicii,    vicii_base_t::registers_read, vicii_base_t::registers_write});
    (void)bus_.register_handler({this->sid,      mos6581_t::registers_read,  mos6581_t::registers_write});
    (void)bus_.register_handler({this->colorram, MOS2114::bus_read,          MOS2114::bus_write});
    (void)bus_.register_handler({this->cia1,     mos6526_t::registers_read,  mos6526_t::registers_write});
    (void)bus_.register_handler({this->cia2,     mos6526_t::registers_read,  mos6526_t::registers_write});

    // Create the IndexedSubTable for the I/O page ($D000-$DFFF)
    // 4 bits → 16 × 256 B entries, bit_shift=8 (extract bits 11-8)
    const int io_sub = bus_.add_indexed_sub_table(C64BusSpec::Cpu, 4, 8);
    (void)bus_.add_indexed_sub_table(C64BusSpec::Vic, 4, 8);

    // Real MMIO chip IDs — assigned by BusMap Phase 3 above all sentinels.
    // resolve() embeds these in the CS field; each chip self-selects via
    // is_cs_selected() in tick_mmio().
    auto cs_rd = [](uint16_t id) { return C64ChipId(id); };
    auto cs_wr = [](uint16_t id) { return C64WriteId(id); };
    const uint16_t idVicII  = this->vicii->bus_chip_id();
    const uint16_t idSid    = this->sid->bus_chip_id();
    const uint16_t idColRam = this->colorram->bus_chip_id();
    const uint16_t idCia1   = this->cia1->bus_chip_id();
    const uint16_t idCia2   = this->cia2->bus_chip_id();

    // Populate sub-table entries with real CS chip IDs
    // VIC-II: $D000-$D3FF (pages 0-3)
    for (int i = 0; i < 4; ++i)
        bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, i, cs_rd(idVicII), cs_wr(idVicII));
    // SID: $D400-$D7FF (pages 4-7)
    for (int i = 4; i < 8; ++i)
        bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, i, cs_rd(idSid), cs_wr(idSid));
    // Color RAM: $D800-$DBFF (pages 8-11)
    for (int i = 8; i < 12; ++i)
        bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, i, cs_rd(idColRam), cs_wr(idColRam));
    // CIA1: $DC00-$DCFF (page 12)
    bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, 12, cs_rd(idCia1), cs_wr(idCia1));
    // CIA2: $DD00-$DDFF (page 13)
    bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, 13, cs_rd(idCia2), cs_wr(idCia2));
    // I/O1: $DE00-$DEFF (page 14) — expansion port, floating bus
    bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, 14,
        C64ChipId(C64PT::kNoChipSelected), C64WriteId(C64PT::kNoChipSelectedWrite));
    // I/O2: $DF00-$DFFF (page 15) — expansion port, floating bus
    bus_.set_indexed_entry(C64BusSpec::Cpu, io_sub, 15,
        C64ChipId(C64PT::kNoChipSelected), C64WriteId(C64PT::kNoChipSelectedWrite));

    log_info("C64: I/O dispatch initialized (CS-tick, sub-table IDs: VIC-II=%u SID=%u ColRAM=%u CIA1=%u CIA2=%u)\n",
           idVicII, idSid, idColRam, idCia1, idCia2);
}

// ============================================================================
// BANKING — mode_switch, on_banking_change, generate_pla_mode
// ============================================================================

void C64System::mode_switch(uint8_t mode) {
    pla_banking_mode_ = mode & 0x1F;
    bus_.load_snapshot(C64BusSpec::Cpu, cpu_snapshots_[pla_banking_mode_]);
    bus_.load_snapshot(C64BusSpec::Vic, vicii_snapshots_[pla_banking_mode_]);
}

void C64System::on_banking_change(uint8_t banking_state) {
    uint8_t pla_mode = generate_pla_mode(banking_state & 0x07);
    mode_switch(pla_mode);
}

uint8_t C64System::generate_pla_mode(uint8_t cpu_port_bits) const {
    uint8_t pla_mode = cpu_port_bits & 0x07;
    pla_mode |= ((system_lines_ & SYS_MASK_EXROM) ? 0x08 : 0);
    pla_mode |= ((system_lines_ & SYS_MASK_GAME)  ? 0x10 : 0);
    return pla_mode;
}

// ============================================================================
// CARTRIDGE SIGNALS — EXROM/GAME
// ============================================================================

void C64System::set_exrom_signal(bool active) {
    if (active)
        system_lines_ &= ~SYS_MASK_EXROM;
    else
        system_lines_ |= SYS_MASK_EXROM;
    // Re-derive PLA mode from current CPU port bits + new cartridge signals
    uint8_t cpu_port_bits = pla_banking_mode_ & 0x07;
    mode_switch(generate_pla_mode(cpu_port_bits));
}

void C64System::set_game_signal(bool active) {
    if (active)
        system_lines_ &= ~SYS_MASK_GAME;
    else
        system_lines_ |= SYS_MASK_GAME;
    uint8_t cpu_port_bits = pla_banking_mode_ & 0x07;
    mode_switch(generate_pla_mode(cpu_port_bits));
}

void C64System::set_cartridge_signals(bool exrom_active, bool game_active) {
    if (exrom_active) system_lines_ &= ~SYS_MASK_EXROM;
    else              system_lines_ |= SYS_MASK_EXROM;
    if (game_active)  system_lines_ &= ~SYS_MASK_GAME;
    else              system_lines_ |= SYS_MASK_GAME;
    uint8_t cpu_port_bits = pla_banking_mode_ & 0x07;
    mode_switch(generate_pla_mode(cpu_port_bits));
}

bool C64System::get_exrom_signal() const {
    return (system_lines_ & SYS_MASK_EXROM) == 0;
}

bool C64System::get_game_signal() const {
    return (system_lines_ & SYS_MASK_GAME) == 0;
}

// ============================================================================
// DEBUG MEMORY ACCESS
// ============================================================================

uint8_t C64System::read_memory(uint16_t addr) {
    bus_state_t s = bus_state_;
    BUS_SET_ADDR(s, addr);
    BUS_SET_BIT(s, BUS_RW_BIT);
    s = bus_.tick(s);
    return BUS_GET_DATA(s);
}

void C64System::write_memory(uint16_t addr, uint8_t value) {
    bus_state_t s = bus_state_;
    BUS_SET_ADDR(s, addr);
    BUS_SET_DATA(s, value);
    BUS_CLR_BIT(s, BUS_RW_BIT);
    (void)bus_.tick(s);
}

// ============================================================================
// Memory Initialization
// ============================================================================

void C64System::memory_init() {
    // -------------------------------------------------------------------------
    // Initialize RAM (normal boot: clear to zero)
    // -------------------------------------------------------------------------
    if (this->ram && this->ram->data()) {
        log_info("Normal boot mode: RAM cleared\n");
        memset(this->ram->data(), 0, 0x10000);
    } else {
        log_info("ERROR: RAM memory pointer is NULL!\n");
    }

    // -------------------------------------------------------------------------
    // Initialize Color RAM
    // -------------------------------------------------------------------------
    if (this->colorram) {
        memset(this->colorram->memory, 0, 1024);
    }

    // -------------------------------------------------------------------------
    // Load ROMs from manifest metadata (filenames in C64_FOR_EACH_SYSTEM_CHIP)
    // -------------------------------------------------------------------------
    char rom_root_path[1024];
    if (system_config_discover_rom_root("c64", rom_root_path, sizeof(rom_root_path))) {
        board_.load_roms(rom_root_path, "C64");
    } else {
        log_info("C64: ROM root not found, skipping ROM loading\n");
    }

    // Cartridge ROMs: not loaded by default (filled with 0xFF if present)
    if (this->roml && this->roml->data()) {
        memset(this->roml->data(), 0xFF, c64_constants::BASIC_ROM_SIZE);
    }
    if (this->romh && this->romh->data()) {
        memset(this->romh->data(), 0xFF, c64_constants::BASIC_ROM_SIZE);
    }
}

// Register C64 system with the registry
REGISTER_SYSTEM(c64_descriptor, []() {
    return std::make_unique<C64System>();
})
