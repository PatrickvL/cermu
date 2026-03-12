/*
 * pet_system.cpp — Commodore PET System Implementation
 *
 * Emulates the Commodore PET 4032 (BASIC 4.0, 32KB RAM, 40-column,
 * normal keyboard) at cycle level.
 *
 * Tick order mirrors real hardware:
 *   CRTC character clock → VIA tick → PIA ticks → CPU PHI2 → mem_tick → NMI → CPU PHI1
 *
 * The MC6845 CRTC drives display timing.  Its character clock is the CPU clock
 * (both run at 1 MHz from the same crystal).  The display callback renders
 * characters into the RGBA framebuffer via character ROM lookup.
 */

#include "systems/commodore/pet/pet_system.h"
#include "systems/commodore/pet/pet_constants.h"
#include "systems/commodore/pet/pet_keyboard_matrix.h"
#include "core/cermu.h"
#include "chip/input/commodore_keyboard.h"
#include "core/input/emu_key_sdl_map.h"
#include "core/input/keyboard_mapper.h"
#include "systems/commodore/basic_parser.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// Include chip headers
#include "chip/cpu/fam65xx/mos6502.h"
#include "chip/io/pia6820.h"
#include "chip/io/mos6522.h"
#include "chip/video/mc6845/mc6845.h"
#include "core/chip.h"

// Include ROM loader
#include "core/storage/rom_loader.h"
#include "core/config/path_discovery.h"

// File format handlers
#include "core/formats/format_registry.h"
#include "core/formats/prg_format.h"
#include "core/formats/d64_format.h"
#include "core/formats/tap_format.h"
#include "systems/commodore/commodore_load_helpers.h"
#include "devices/keyboard/commodore_keyboard_device.h"

// ============================================================================
// PET Monochrome Display Colors (green phosphor CRT)
// ============================================================================

// PET green phosphor — foreground = bright green, background = black
static constexpr uint32_t PET_COLOR_FG = 0xFF33FF33;   // ABGR: bright green
static constexpr uint32_t PET_COLOR_BG = 0xFF000000;   // ABGR: black

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_pet_hardware_traits() {
    HardwareTraits traits = {};

    // Display — monochrome 40×25 characters = 320×200 pixels
    traits.display.native_width  = pet_constants::DISPLAY_WIDTH;
    traits.display.native_height = pet_constants::DISPLAY_HEIGHT;
    traits.display.visible_width  = pet_constants::DISPLAY_WIDTH;
    traits.display.visible_height = pet_constants::DISPLAY_HEIGHT;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 2;
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = false;

    // 2-color palette: black + green (monochrome)
    traits.display.default_palette.push_back(PaletteColor(0, 0, 0, 255));           // Background
    traits.display.default_palette.push_back(PaletteColor(0x33, 0xFF, 0x33, 255));  // Foreground (green)

    // Audio — CB2 speaker (square wave, mono)
    traits.audio.format = AudioFormat::MONO_8BIT;
    traits.audio.sample_rate_hz = pet_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "PIA/VIA CB2 Speaker";

    // Timing — NTSC (early PETs were NTSC; PAL models came later)
    traits.timing.cpu_frequency_hz    = pet_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz  = pet_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = pet_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = pet_constants::CYCLES_PER_FRAME_NTSC;
    traits.timing.standard = VideoStandard::NTSC;

    // Memory options
    traits.memory_options.push_back({ "8KB RAM",  8192, 0, false });
    traits.memory_options.push_back({ "16KB RAM", 16384, 0, false });
    traits.memory_options.push_back({ "32KB RAM", 32768, 0, true });

    // Region options
    traits.video_standard_configs.push_back({
        "NTSC (60 Hz)", VideoStandard::NTSC, traits.timing, true
    });

    SystemTiming pal_timing = traits.timing;
    pal_timing.target_fps = 50;
    pal_timing.cycles_per_frame = pet_constants::CYCLES_PER_FRAME_PAL;
    pal_timing.standard = VideoStandard::PAL;
    traits.video_standard_configs.push_back({
        "PAL (50 Hz)", VideoStandard::PAL, pal_timing, false
    });

    return traits;
}

// ============================================================================
// BASIC V2 Parameters for PET
// ============================================================================

static const commodore_basic_params_t COMMODORE_BASIC_PET = {
    0x0401,  /* basic_start — PET BASIC starts at $0401 */
    0x9E,    /* sys_token */
    0x8F,    /* rem_token */
    0xC2,    /* peek_token */
    0x28,    /* basic_start_ptr_lo — PET uses $28/$29 for BASIC start */
    0x29,    /* basic_start_ptr_hi */
};

// ============================================================================
// File Format Support
// ============================================================================

static const format_descriptor_t* const pet_formats[] = {
    &PRG_FORMAT_DESCRIPTOR,
    &D64_FORMAT_DESCRIPTOR,
    &TAP_FORMAT_DESCRIPTOR,
    nullptr
};

// ============================================================================
// File Probe — confidence + configuration detection
// ============================================================================

static SystemProbeResult pet_probe_file(
    const format_descriptor_t* matched_format,
    const char* filepath,
    const uint8_t* data, size_t size)
{
    SystemProbeResult result{};

    if (!matched_format) return result;

    if (matched_format == &PRG_FORMAT_DESCRIPTOR && size >= 2) {
        uint16_t load_addr = data[0] | (data[1] << 8);

        // PET BASIC programs start at $0401
        if (load_addr == 0x0401) {
            result.confidence = 0.70f;  // $0401 is fairly PET-specific
        } else if (load_addr < 0x8000) {
            result.confidence = 0.30f;  // Could be ML loaded into PET RAM
        } else {
            result.confidence = 0.15f;  // Unlikely PET address
        }
    } else if (matched_format == &D64_FORMAT_DESCRIPTOR) {
        // D64 is shared across Commodore systems, low confidence
        result.confidence = 0.35f;
    } else if (matched_format == &TAP_FORMAT_DESCRIPTOR) {
        result.confidence = 0.40f;
    }

    return result;
}

// ============================================================================
// System Descriptor
// ============================================================================

static SystemDescriptor pet_descriptor = {
    "Commodore PET",
    "PET",
    "Commodore PET 4032 (1977) - 32KB RAM, 40-column monochrome display",
    "pet",
    {"PET", "PET4032", "PET 4032", "CBM 4032"},
    pet_formats,
    create_pet_hardware_traits(),
    pet_probe_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

PETSystem::PETSystem()
    : CommodoreSystem()
    , pins_(PET_BUS_DEFAULT_STATE)
{
    cycles_per_frame_ = pet_constants::CYCLES_PER_FRAME_NTSC;
    hardware_traits_  = create_pet_hardware_traits();
    current_palette_  = hardware_traits_.display.default_palette;

    // Pre-compute audio sample timing
    audio_cycles_per_sample_ = pet_constants::CPU_FREQ_HZ / audio_sample_rate_;
}

PETSystem::~PETSystem() {
    delete cpu_;   cpu_  = nullptr;
    delete pia1_;  pia1_ = nullptr;
    delete pia2_;  pia2_ = nullptr;
    delete via_;   via_  = nullptr;
    delete crtc_;  crtc_ = nullptr;

    if (keyboard_) {
        delete keyboard_;
        keyboard_ = nullptr;
    }
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& PETSystem::get_descriptor() const {
    return pet_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

bool PETSystem::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        const VideoStandardConfig& std_cfg = hardware_traits_.video_standard_configs[config_.region_option_index];
        cycles_per_frame_ = std_cfg.timing.cycles_per_frame;
    }

    return true;
}

// ============================================================================
// System Lifecycle
// ============================================================================

bool PETSystem::initialize() {
    printf("PET: Initializing system\n");

    // ── Create memory chips from manifest and wire bus ─────────────────
    bus_mem_.create_chips(&pins_);
    bus_mem_.apply(bus_);

    main_ram_chip_    = bus_mem_.template chip_as<RAMChip>(0);
    screen_ram_chip_  = bus_mem_.template chip_as<RAMChip>(1);
    basic_rom_b_chip_ = bus_mem_.template chip_as<ROMChip>(2);
    basic_rom_c_chip_ = bus_mem_.template chip_as<ROMChip>(3);
    basic_rom_d_chip_ = bus_mem_.template chip_as<ROMChip>(4);
    editor_rom_chip_  = bus_mem_.template chip_as<ROMChip>(5);
    kernal_rom_chip_  = bus_mem_.template chip_as<ROMChip>(6);

    // Screen RAM mirror at $8400-$87FF and configure memory map
    configure_memory_map();

    // Load ROMs into memory
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("PET: Warning - ROMs not loaded, system may not function correctly\n");
    }

    // ---- CPU (MOS 6502) ----
    cpu_ = new MOS6502();
    cpu_->init();
    cpu_->reset(0);

    // ---- CRTC (MC6845) ----
    crtc_ = new mc6845_t();
    crtc_->init();

    // Program CRTC with PET-standard 40×25 register values (BASIC 4.0 editor ROM does this,
    // but we prime them for display before KERNAL has run)
    crtc_->regs_[MC6845_R0_HTOTAL]       = 63;   // 64 characters per line (R0+1)
    crtc_->regs_[MC6845_R1_HDISPLAYED]   = 40;   // 40 visible characters
    crtc_->regs_[MC6845_R2_HSYNC_POS]    = 50;   // H-sync at char 50
    crtc_->regs_[MC6845_R3_SYNC_WIDTHS]  = 0x04; // H-sync width = 4, V-sync width = 0 (16 default)
    crtc_->regs_[MC6845_R4_VTOTAL]       = 32;   // 33 char rows per frame (R4+1)
    crtc_->regs_[MC6845_R5_VADJUST]      = 5;    // Vertical fine adjust
    crtc_->regs_[MC6845_R6_VDISPLAYED]   = 25;   // 25 visible rows
    crtc_->regs_[MC6845_R7_VSYNC_POS]    = 28;   // V-sync at row 28
    crtc_->regs_[MC6845_R9_MAX_SCANLINE] = 7;    // 8 scan lines per character (R9+1)
    crtc_->regs_[MC6845_R12_START_ADDR_HI] = 0x10; // Display start = $1000 (screen RAM offset)
    crtc_->regs_[MC6845_R13_START_ADDR_LO] = 0x00;

    // Wire CRTC callbacks for display rendering
    crtc_->on_display_char = [this](uint16_t ma, uint8_t ra, bool cursor) {
        this->crtc_display_char(ma, ra, cursor);
    };
    crtc_->on_vsync = [this]() { this->crtc_vsync(); };
    crtc_->on_hsync = [this]() { this->crtc_hsync(); };

    // ---- PIA 1 (keyboard + cassette sense) ----
    pia1_ = new pia6820_t();
    pia1_->init();
    pia1_->user_data       = this;
    pia1_->on_port_a_read  = pia1_port_a_read;
    pia1_->on_port_a_write = pia1_port_a_write;
    pia1_->on_port_b_read  = pia1_port_b_read;
    pia1_->on_port_b_write = pia1_port_b_write;

    // ---- PIA 2 (IEEE-488 bus) ----
    pia2_ = new pia6820_t();
    pia2_->init();
    pia2_->user_data = this;
    // IEEE-488 callbacks not wired yet — returns open bus ($FF)

    // ---- VIA (timers, CB2 speaker, user port) ----
    via_ = new mos6522_t();
    via_->reset();
    via_->interrupt_bit = BUS_IRQ_BIT;

    // ---- Keyboard ----
    keyboard_ = new commodore_keyboard_t();
    if (!keyboard_->init(&pet_keyboard_config)) {
        delete keyboard_;
        keyboard_ = nullptr;
        printf("PET: Warning: Could not create keyboard\n");
    }
    if (keyboard_) {
        // Create keyboard mapper (same pattern as VIC-20 / C16)
        KeyboardMapper* mapper = new KeyboardMapper();
        mapper->set_guest_keyboard(keyboard_);
        mapper->build_character_map_from_matrix(&pet_keyboard_config);
        mapper->register_default_synthetic_mappings();
        keyboard_mapper_.reset(mapper);
    }

    // Register chips for the Hardware debug menu
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 6502 CPU", "6502", "CPU", 0x0000);
    register_chip(static_cast<ChipBase*>(crtc_),
        "MC6845 CRTC", "6845", "Video", pet_constants::CRTC_BASE);
    register_chip(static_cast<ChipBase*>(pia1_),
        "PIA 1 (Keyboard)", "6820", "I/O", pet_constants::PIA1_BASE);
    register_chip(static_cast<ChipBase*>(pia2_),
        "PIA 2 (IEEE-488)", "6820", "I/O", pet_constants::PIA2_BASE);
    register_chip(static_cast<ChipBase*>(via_),
        "MOS 6522 VIA", "6522", "I/O", pet_constants::VIA_BASE);
    register_bus_chips(bus_mem_);

    printf("PET: Initialization complete\n");
    return true;
}

void PETSystem::shutdown() {
    printf("PET: Shutting down system\n");
    System::shutdown();
}

void PETSystem::reset() {
    printf("PET: Resetting system\n");

    // Reset all chips
    if (crtc_) crtc_->reset();
    if (pia1_) pia1_->reset();
    if (pia2_) pia2_->reset();
    if (via_)  via_->reset();

    // Clear RAM but preserve ROMs
    if (main_ram_chip_) {
        memset(main_ram_chip_->data(), 0, 32768);
    }
    if (screen_ram_chip_) {
        memset(screen_ram_chip_->data(), 0, 1024);
    }

    // Clear framebuffer
    if (rgba_framebuffer_ && rgba_width_ > 0 && rgba_height_ > 0) {
        memset(rgba_framebuffer_, 0, (size_t)rgba_width_ * rgba_height_ * sizeof(uint32_t));
    }

    // Reset audio state
    speaker_state_ = false;
    audio_write_pos_ = 0;
    audio_read_pos_ = 0;
    audio_cycle_counter_ = 0;

    // Reset display position
    screen_pixel_x_ = 0;
    screen_pixel_y_ = 0;

    // Reset CPU last
    if (cpu_) cpu_->reset(0);

    pins_ = PET_BUS_DEFAULT_STATE;
    total_cycles_ = 0;

    // Reset deferred loading state
    reset_load_state();
}

// ============================================================================
// Execution
// ============================================================================

// ============================================================================
// I/O Dispatch ($E800-$E8FF)
// ============================================================================
// The PET I/O page is decoded by address bits A4-A7:
//   $E810-$E813 → PIA 1 (keyboard)
//   $E820-$E823 → PIA 2 (IEEE-488)
//   $E840-$E84F → VIA (6522)
//   $E880-$E881 → CRTC (6845)
// Addresses are mirrored within the I/O page based on partial decoding.

uint8_t PETSystem::io_read(uint16_t addr) {
    uint8_t offset = addr & 0xFF;

    if ((offset & 0xF0) == 0x10) {
        // PIA 1 ($E810-$E81F → addr bits 1:0 select register)
        return pia1_ ? pia1_->read(addr & 0x03) : 0xFF;
    }
    if ((offset & 0xF0) == 0x20) {
        // PIA 2 ($E820-$E82F)
        return pia2_ ? pia2_->read(addr & 0x03) : 0xFF;
    }
    if ((offset & 0xF0) == 0x40) {
        // VIA ($E840-$E84F)
        if (via_) {
            // VIA registers are at offset 0-15 within the chip
            // Build a bus state for the VIA read
            bus_state_t vs = PET_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(vs, addr & 0x0F);
            BUS_SET_BIT(vs, BUS_RW_BIT);  // Read
            vs = via_->tick(vs);
            return BUS_GET_DATA(vs);
        }
        return 0xFF;
    }
    if ((offset & 0xF0) == 0x80) {
        // CRTC ($E880-$E88F → addr bit 0 selects address/data)
        return crtc_ ? crtc_->read(addr & 0x01) : 0xFF;
    }

    return 0xFF;  // Unmapped I/O
}

void PETSystem::io_write(uint16_t addr, uint8_t data) {
    uint8_t offset = addr & 0xFF;

    if ((offset & 0xF0) == 0x10) {
        // PIA 1
        if (pia1_) pia1_->write(addr & 0x03, data);
        return;
    }
    if ((offset & 0xF0) == 0x20) {
        // PIA 2
        if (pia2_) pia2_->write(addr & 0x03, data);
        return;
    }
    if ((offset & 0xF0) == 0x40) {
        // VIA
        if (via_) {
            bus_state_t vs = PET_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(vs, addr & 0x0F);
            BUS_CLR_BIT(vs, BUS_RW_BIT);  // Write
            BUS_SET_DATA(vs, data);
            via_->tick(vs);
        }
        return;
    }
    if ((offset & 0xF0) == 0x80) {
        // CRTC
        if (crtc_) crtc_->write(addr & 0x01, data);
        return;
    }
}

// ============================================================================
// Tick — one CPU cycle (1 MHz)
// ============================================================================

void PETSystem::tick() {
    bus_state_t s = PET_BUS_DEFAULT_STATE;

    // Preserve address and data from previous cycle
    BUS_SET_ADDR(s, BUS_GET_ADDR(pins_));
    BUS_SET_DATA(s, BUS_GET_DATA(pins_));

    // ---- Phase 1: CRTC character clock ----
    // MC6845 runs at the same 1 MHz character clock as the CPU.
    // The display callback fires during tick() for visible characters.
    if (crtc_) {
        crtc_->tick();
    }

    // ---- Phase 2: VIA tick (timers, interrupts) ----
    if (via_) {
        // VIA tick — don't pass CPU bus state; VIA is accessed via I/O dispatch.
        // But we need the VIA to tick for timer countdown + IRQ generation.
        bus_state_t via_bus = PET_BUS_DEFAULT_STATE;
        BUS_SET_BIT(via_bus, BUS_RW_BIT);  // Idle read (no chip select)
        via_bus = via_->tick(via_bus);

        // Propagate IRQ from VIA to CPU bus
        if (!BUS_GET_BIT(via_bus, BUS_IRQ_BIT)) {
            BUS_CLR_BIT(s, BUS_IRQ_BIT);
        }
    }

    // ---- Propagate PIA IRQ lines ----
    // PIA1: IRQA drives the main IRQ line
    if (pia1_ && (pia1_->irq_a1 || pia1_->irq_a2)) {
        BUS_CLR_BIT(s, BUS_IRQ_BIT);
    }

    // ---- Phase 3: CPU PHI2 ----
    s = cpu_->tick<MOS6502::Phase::PHI2>(s);

    // ---- Phase 4: Memory service ----
    {
        uint16_t addr = BUS_GET_ADDR(s);
        if (addr >= pet_constants::IO_START && addr < pet_constants::IO_END) {
            // I/O page ($E800-$E8FF) — manual dispatch to PIAs, VIA, CRTC
            if (!BUS_GET_BIT(s, BUS_RW_BIT)) {
                io_write(addr, BUS_GET_DATA(s));
            } else {
                BUS_SET_DATA(s, io_read(addr));
            }
        } else {
            s = bus_.tick(0, s);
        }
    }

    // NMI edge detection
    cpu_->sample_nmi_pin(s);

    // ---- Phase 5: CPU PHI1 ----
    s = cpu_->tick<MOS6502::Phase::PHI1>(s);

    // Restore R/W to read mode
    BUS_SET_BIT(s, BUS_RW_BIT);

    // ---- Audio sample generation ----
    audio_cycle_counter_++;
    if (audio_cycles_per_sample_ > 0 && audio_cycle_counter_ >= audio_cycles_per_sample_) {
        audio_cycle_counter_ = 0;
        // Write speaker state as float sample
        float sample = speaker_state_ ? 0.5f : -0.5f;
        audio_buffer_[audio_write_pos_ & 4095] = sample;
        audio_write_pos_++;
    }

    pins_ = s;
    total_cycles_++;
}

void PETSystem::run_frame() {
    // Check deferred loading
    check_deferred_load();

    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }

    tick_peripherals();
}

// ============================================================================
// CRTC Display Callbacks
// ============================================================================

void PETSystem::crtc_display_char(uint16_t ma, uint8_t ra, bool cursor) {
    if (!rgba_framebuffer_ || !screen_ram_chip_) return;

    // ma = character address from CRTC (relative to display start).
    // On PET, screen RAM is at $8000.  The CRTC display start (R12:R13) is
    // typically $1000 (so ma ranges from $1000 to $13E7 for 40×25).
    // We mask to get the offset within screen RAM.
    uint16_t screen_offset = ma & 0x03FF;           // 1000 chars max
    uint8_t char_code = screen_ram_chip_->data()[screen_offset];

    // Look up character ROM for this scan line
    // Character ROM is 4KB: 256 chars × 8 bytes (normal) + 256 chars × 8 (inverted)
    // Bits 7 of the character code select the inverted set
    bool inverted = (char_code & 0x80) != 0;
    uint8_t glyph_index = char_code & 0x7F;
    uint8_t pixel_row = char_rom_[(glyph_index * 8) + ra];

    if (inverted) {
        pixel_row = ~pixel_row;
    }

    // XOR with cursor if active
    if (cursor) {
        pixel_row = ~pixel_row;
    }

    // Calculate framebuffer position
    // screen_offset = row * 40 + col
    uint32_t char_col = screen_offset % pet_constants::SCREEN_COLS;
    uint32_t char_row = screen_offset / pet_constants::SCREEN_COLS;

    if (char_row >= static_cast<uint32_t>(pet_constants::SCREEN_ROWS)) return;
    if (char_col >= static_cast<uint32_t>(pet_constants::SCREEN_COLS)) return;

    uint32_t pixel_x = char_col * pet_constants::PET_CHAR_WIDTH;
    uint32_t pixel_y = char_row * pet_constants::PET_CHAR_HEIGHT + ra;

    if (pixel_y >= static_cast<uint32_t>(rgba_height_)) return;

    uint32_t* row_ptr = rgba_framebuffer_ + pixel_y * rgba_width_;

    // Render 8 pixels from the character ROM byte
    for (int bit = 7; bit >= 0; bit--) {
        uint32_t px = pixel_x + (7 - bit);
        if (px < static_cast<uint32_t>(rgba_width_)) {
            row_ptr[px] = (pixel_row & (1 << bit)) ? PET_COLOR_FG : PET_COLOR_BG;
        }
    }
}

void PETSystem::crtc_vsync() {
    // VSYNC — new frame starts.  Reset display position tracking.
    screen_pixel_x_ = 0;
    screen_pixel_y_ = 0;
}

void PETSystem::crtc_hsync() {
    // HSYNC — new scan line.
    // Not strictly needed since we compute framebuffer position from
    // character address, but useful for future timing refinements.
}

// ============================================================================
// PIA1 Callbacks — Keyboard Matrix Scanning
// ============================================================================
// PET keyboard: PIA1 Port A selects keyboard row (active-low),
// PIA1 Port B reads column data (active-low = key pressed).

uint8_t PETSystem::pia1_port_a_read(void* user_data) {
    // Port A is output (row select) — return the output latch
    PETSystem* sys = static_cast<PETSystem*>(user_data);
    return sys->keyboard_row_select_;
}

void PETSystem::pia1_port_a_write(void* user_data, uint8_t data) {
    // Store the row select for keyboard scanning
    PETSystem* sys = static_cast<PETSystem*>(user_data);
    sys->keyboard_row_select_ = data;
}

uint8_t PETSystem::pia1_port_b_read(void* user_data) {
    PETSystem* sys = static_cast<PETSystem*>(user_data);

    if (!sys->keyboard_) return 0xFF;

    // The PET uses a 4-to-16 decoder on PIA1 Port A bits[3:0] to select
    // one of 10 keyboard rows.  PIA1 Port B reads the column contacts for
    // that row.  col_open_contacts[row] holds the 8-bit column mask
    // (active-LOW: 0 = key pressed, 1 = open).
    uint8_t row = sys->keyboard_row_select_ & 0x0F;

    if (row < PET_KEYBOARD_ROWS) {
        // col_open_contacts is uint16_t, but PET only has 8 columns
        return static_cast<uint8_t>(sys->keyboard_->col_open_contacts[row]);
    }

    return 0xFF;  // No row selected or invalid row
}

void PETSystem::pia1_port_b_write(void* user_data, uint8_t data) {
    // Port B is input (column read) — writes are ignored
    (void)user_data;
    (void)data;
}

// ============================================================================
// VIA CB2 Callback — Speaker Output
// ============================================================================

void PETSystem::via_cb2_output(void* user_data, bool state) {
    PETSystem* sys = static_cast<PETSystem*>(user_data);
    sys->speaker_state_ = state;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* PETSystem::get_framebuffer() {
    return rgba_framebuffer_;
}

void PETSystem::get_display_dimensions(int* width, int* height) const {
    *width  = pet_constants::DISPLAY_WIDTH;
    *height = pet_constants::DISPLAY_HEIGHT;
}

void PETSystem::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

// ============================================================================
// Audio
// ============================================================================

uint32_t PETSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    uint32_t avail = audio_write_pos_ - audio_read_pos_;
    uint32_t to_read = (avail < max_samples) ? avail : max_samples;

    for (uint32_t i = 0; i < to_read; i++) {
        buffer[i] = audio_buffer_[(audio_read_pos_ + i) & 4095];
    }
    audio_read_pos_ += to_read;
    return to_read;
}

void PETSystem::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = sample_rate_hz;
    audio_cycles_per_sample_ = (sample_rate_hz > 0)
        ? pet_constants::CPU_FREQ_HZ / static_cast<uint32_t>(sample_rate_hz)
        : 0;
}

// ============================================================================
// Input
// ============================================================================

void PETSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (keyboard_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                keyboard_->key_down(ek, false);
            } else {
                keyboard_->key_up(ek, false);
            }
        }
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

void PETSystem::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset PET")) {
        reset();
    }
#endif
}

void PETSystem::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    // Memory option
    if (ImGui::BeginCombo("RAM Size", hardware_traits_.memory_options[config_.memory_option_index].name)) {
        for (int i = 0; i < static_cast<int>(hardware_traits_.memory_options.size()); i++) {
            bool selected = (i == config_.memory_option_index);
            if (ImGui::Selectable(hardware_traits_.memory_options[i].name, selected)) {
                config_.memory_option_index = i;
            }
        }
        ImGui::EndCombo();
    }
#endif
}

// ============================================================================
// CommodoreSystem Loading Hooks
// ============================================================================

uint8_t PETSystem::load_mem_read(void* ctx, uint16_t addr) {
    auto* sys = static_cast<PETSystem*>(ctx);
    if (addr < pet_constants::RAM_END_32K)
        return sys->main_ram_chip_->data()[addr];
    if (addr >= pet_constants::SCREEN_RAM_START && addr < pet_constants::SCREEN_RAM_END)
        return sys->screen_ram_chip_->data()[addr - pet_constants::SCREEN_RAM_START];
    return 0xFF;
}

void PETSystem::load_mem_write(void* ctx, uint16_t addr, uint8_t val) {
    auto* sys = static_cast<PETSystem*>(ctx);
    if (addr < pet_constants::RAM_END_32K) {
        sys->main_ram_chip_->data()[addr] = val;
    } else if (addr >= pet_constants::SCREEN_RAM_START && addr < pet_constants::SCREEN_RAM_END) {
        sys->screen_ram_chip_->data()[addr - pet_constants::SCREEN_RAM_START] = val;
    }
}

bool PETSystem::is_basic_ready() const {
    if (!main_ram_chip_ || !cpu_) return false;

    // PET BASIC warm-start vector at $0302/$0303 — should point to BASIC's main loop
    // For BASIC 4.0, the warm-start vector is typically $B3FF
    const uint8_t* ram = main_ram_chip_->data();
    uint16_t warmstart = ram[0x0302] | (ram[0x0303] << 8);
    if (warmstart < 0xB000 || warmstart > 0xE000) return false;

    // Keyboard buffer must be empty
    if (ram[pet_constants::KBD_BUFFER_COUNT] != 0) return false;

    // First boot: wait until BASIC's NEW has run (VARTAB at $2A/$2B != 0)
    if (!boot_completed_ && ram[0x002A] == 0 && ram[0x002B] == 0) return false;

    return true;
}

commodore_load_context_t PETSystem::build_load_context() {
    commodore_load_context_t ctx = {};
    ctx.system_name       = "PET";
    ctx.write_byte        = load_mem_write;
    ctx.write_block       = nullptr;
    ctx.mem_read          = load_mem_read;
    ctx.mem_ctx           = this;
    ctx.basic_params      = &COMMODORE_BASIC_PET;
    ctx.basic_start_addrs[0] = 0x0401;  // PET BASIC start
    ctx.default_raw_addr  = 0xA000;      // Expansion ROM area for raw ML
    ctx.set_pc            = nullptr;
    ctx.try_sys_from_filename = true;
    return ctx;
}

void PETSystem::inject_keys(const char* str) {
    if (!main_ram_chip_) return;
    uint8_t* ram = main_ram_chip_->data();
    int len = static_cast<int>(strlen(str));
    if (len > static_cast<int>(pet_constants::KBD_BUFFER_SIZE))
        len = static_cast<int>(pet_constants::KBD_BUFFER_SIZE);
    for (int i = 0; i < len; i++) {
        ram[pet_constants::KBD_BUFFER + i] = static_cast<uint8_t>(str[i]);
    }
    ram[pet_constants::KBD_BUFFER_COUNT] = static_cast<uint8_t>(len);
}

// ============================================================================
// ROM Loading
// ============================================================================

bool PETSystem::load_roms() {
    if (!basic_rom_b_chip_) {
        printf("PET: Cannot load ROMs - memory not initialized\n");
        return false;
    }

    // Discover ROM root path
    char rom_root[1024];
    if (!system_config_discover_rom_root("pet", rom_root, sizeof(rom_root))) {
        printf("PET: ROM root directory not found\n");
        return false;
    }

    printf("PET: ROM root discovered: %s\n", rom_root);

    // Character ROM — loaded into separate buffer, not main address space.
    // PET 4032 uses a 2KB character ROM (901447-10); we mirror it to fill 4KB.
    // PET 8032 uses a 4KB character ROM (901640-01).
    uint8_t char_buf[4096];
    const char* char_files_4k[] = {
        "characters.901640-01.bin",         // 4KB (8032/SuperPET)
        nullptr
    };
    bool char_ok = rom_loader_load_from_root(rom_root, char_files_4k,
                                              4096, char_buf, sizeof(char_buf));
    if (!char_ok) {
        // Try 2KB character ROM (PET 4032 and earlier)
        uint8_t char_buf_2k[2048];
        const char* char_files_2k[] = {
            "characters-2.901447-10.bin",   // VICE naming
            "characters.901447-10.bin",
            "chargen",
            "chargen.rom",
            "901447-10.bin",
            nullptr
        };
        char_ok = rom_loader_load_from_root(rom_root, char_files_2k,
                                             2048, char_buf_2k, sizeof(char_buf_2k));
        if (char_ok) {
            // Mirror 2KB ROM into 4KB buffer
            memcpy(char_buf, char_buf_2k, 2048);
            memcpy(char_buf + 2048, char_buf_2k, 2048);
        }
    }
    if (char_ok) {
        memcpy(char_rom_, char_buf, sizeof(char_rom_));
        printf("PET: Character ROM loaded\n");
    } else {
        printf("PET: Failed to load Character ROM\n");
    }

    // BASIC 4.0 ROM (12KB at $B000-$DFFF)
    // Consists of three 4KB chips: 901465-23 ($B000), 901465-20 ($C000), 901465-21 ($D000)
    uint8_t basic_buf[12288];
    const char* basic_files[] = {
        "basic-4.901465-23-20-21.bin",      // VICE combined 12KB
        nullptr
    };
    bool basic_ok = rom_loader_load_from_root(rom_root, basic_files,
                                               sizeof(basic_buf), basic_buf, sizeof(basic_buf));
    if (basic_ok) {
        memcpy(basic_rom_b_chip_->data(), basic_buf, 4096);
        memcpy(basic_rom_c_chip_->data(), basic_buf + 4096, 4096);
        memcpy(basic_rom_d_chip_->data(), basic_buf + 8192, 4096);
        printf("PET: BASIC 4.0 ROM loaded (12KB combined)\n");
    } else {
        // Try loading as three 4KB ROMs
        uint8_t rom_b[4096], rom_c[4096], rom_d[4096];
        const char* rom_b_files[] = { "basic-4-b000.901465-23.bin", "901465-23.bin", nullptr };
        const char* rom_c_files[] = { "basic-4-c000.901465-20.bin", "901465-20.bin", nullptr };
        const char* rom_d_files[] = { "basic-4-d000.901465-21.bin", "901465-21.bin", nullptr };
        bool b_ok = rom_loader_load_from_root(rom_root, rom_b_files, 4096, rom_b, sizeof(rom_b));
        bool c_ok = rom_loader_load_from_root(rom_root, rom_c_files, 4096, rom_c, sizeof(rom_c));
        bool d_ok = rom_loader_load_from_root(rom_root, rom_d_files, 4096, rom_d, sizeof(rom_d));
        if (b_ok && c_ok && d_ok) {
            memcpy(basic_rom_b_chip_->data(), rom_b, 4096);
            memcpy(basic_rom_c_chip_->data(), rom_c, 4096);
            memcpy(basic_rom_d_chip_->data(), rom_d, 4096);
            printf("PET: BASIC 4.0 ROM loaded (3 × 4KB)\n");
            basic_ok = true;
        } else {
            // Last resort: try 8KB combined at $C000 (missing $B000 bank)
            uint8_t basic8k[8192];
            const char* basic8k_files[] = { "basic4.rom", nullptr };
            bool ok8 = rom_loader_load_from_root(rom_root, basic8k_files, 8192, basic8k, sizeof(basic8k));
            if (ok8) {
                memcpy(basic_rom_c_chip_->data(), basic8k, 4096);
                memcpy(basic_rom_d_chip_->data(), basic8k + 4096, 4096);
                printf("PET: BASIC ROM loaded (8KB fallback at $C000)\n");
                basic_ok = true;
            } else {
                printf("PET: Failed to load BASIC ROM\n");
            }
        }
    }

    // Editor ROM (2KB at $E000-$E7FF — 40-col normal keyboard variant)
    uint8_t editor_buf[2048];
    const char* editor_files[] = {
        "edit-4-40-n-50Hz.901498-01.bin",
        "edit-4-40-n-60Hz.901499-01.bin",
        "editor.rom",
        "901498-01.bin",
        "901499-01.bin",
        nullptr
    };
    bool editor_ok = rom_loader_load_from_root(rom_root, editor_files,
                                                sizeof(editor_buf), editor_buf, sizeof(editor_buf));
    if (editor_ok) {
        memcpy(editor_rom_chip_->data(), editor_buf, sizeof(editor_buf));
        printf("PET: Editor ROM loaded\n");
    } else {
        printf("PET: Failed to load Editor ROM\n");
    }

    // Kernal ROM (4KB at $F000-$FFFF)
    uint8_t kernal_buf[4096];
    const char* kernal_files[] = {
        "kernal-4.901465-22.bin",           // VICE naming ✓
        "kernal4.rom",
        "kernal.rom",
        "901465-22.bin",
        nullptr
    };
    bool kernal_ok = rom_loader_load_from_root(rom_root, kernal_files,
                                                sizeof(kernal_buf), kernal_buf, sizeof(kernal_buf));
    if (kernal_ok) {
        memcpy(kernal_rom_chip_->data(), kernal_buf, sizeof(kernal_buf));
        printf("PET: Kernal ROM loaded\n");
    } else {
        printf("PET: Failed to load Kernal ROM\n");
    }

    return (kernal_ok && basic_ok && char_ok);
}

// ============================================================================
// Memory Map Configuration
// ============================================================================

void PETSystem::configure_memory_map() {
    using ChipId      = Bus::ChipId;
    using WriteChipId = Bus::WriteChipId;

    // Screen RAM mirror ($8400-$87FF → same data as $8000-$83FF)
    // Slot 1 (screen RAM) base_id gives the chip_id for pages $80-$83.
    // Map pages $84-$87 to the same chip pages.
    constexpr auto screen_base = ChipId(kPETChips.base_id(1, 8));
    bus_.fill_read_pages(0, 0x84, 4, screen_base);
    bus_.fill_write_pages(0, 0x84, 4, WriteChipId(screen_base));
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(pet_descriptor, []() {
    return std::make_unique<PETSystem>();
})
