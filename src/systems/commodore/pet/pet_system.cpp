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

#include "systems/commodore/pet/pet_system.hpp"
#include "systems/commodore/pet/pet_constants.hpp"
#include "systems/commodore/pet/pet_keyboard_matrix.hpp"
#include "core/cermu.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include "core/input/keyboard_mapper.hpp"
#include "systems/commodore/basic_parser.hpp"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// Include chip headers
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/io/pia6820.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/mc6845/mc6845.hpp"
#include "core/chip.hpp"

using namespace mc6845::reg;

// Include ROM loader
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"

// File format handlers
#include "core/formats/format_registry.hpp"
#include "core/formats/prg_format.hpp"
#include "core/formats/d64_format.hpp"
#include "core/formats/tap_format.hpp"
#include "systems/commodore/commodore_load_helpers.hpp"
#include "devices/keyboard/commodore_keyboard_device.hpp"

// ============================================================================
// PET Monochrome Display Colors (green phosphor CRT)
// ============================================================================

// PET palette moved to pet_constants::PALETTE (indexed rendering)

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
    // I/O chips owned by board_ — no manual cleanup.
    // memory chips owned by board_ — no manual cleanup.

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
    register_board(&board_);

    // ── Bind value-typed chips from Chips, then create remaining ─────
    board_.bind_chipset();
    board_.create_chips(&pins_);
    board_.apply(bus_);

    main_ram_chip_    = board_.template find<RAMChip>();
    screen_ram_chip_  = board_.template find<RAMChip>(1);
    basic_rom_b_chip_ = board_.template find<ROMChip>();
    basic_rom_c_chip_ = board_.template find<ROMChip>(1);
    basic_rom_d_chip_ = board_.template find<ROMChip>(2);
    editor_rom_chip_  = board_.template find<ROMChip>(3);
    kernal_rom_chip_  = board_.template find<ROMChip>(4);
    cpu_              = &board_.cpu();
    crtc_             = &board_.video();
    pia1_             = &board_.io();
    pia2_             = &board_.chips().pia2;
    via_              = &board_.sound();

    // Screen RAM mirror at $8400-$87FF and configure memory map
    configure_memory_map();

    // Load ROMs into memory
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("PET: Warning - ROMs not loaded, system may not function correctly\n");
    }

    // ---- CPU (MOS 6502) ----
    board_.cpu().init();
    board_.cpu().reset();

    // ---- CRTC (MC6845) ----
    crtc_->init();

    // Program CRTC with PET-standard 40×25 register values (BASIC 4.0 editor ROM does this,
    // but we prime them for display before KERNAL has run)
    crtc_->regs_[R0_HTOTAL]       = 63;   // 64 characters per line (R0+1)
    crtc_->regs_[R1_HDISPLAYED]   = 40;   // 40 visible characters
    crtc_->regs_[R2_HSYNC_POS]    = 50;   // H-sync at char 50
    crtc_->regs_[R3_SYNC_WIDTHS]  = 0x04; // H-sync width = 4, V-sync width = 0 (16 default)
    crtc_->regs_[R4_VTOTAL]       = 32;   // 33 char rows per frame (R4+1)
    crtc_->regs_[R5_VADJUST]      = 5;    // Vertical fine adjust
    crtc_->regs_[R6_VDISPLAYED]   = 25;   // 25 visible rows
    crtc_->regs_[R7_VSYNC_POS]    = 28;   // V-sync at row 28
    crtc_->regs_[R9_MAX_SCANLINE] = 7;    // 8 scan lines per character (R9+1)
    crtc_->regs_[R12_START_ADDR_HI] = 0x10; // Display start = $1000 (screen RAM offset)
    crtc_->regs_[R13_START_ADDR_LO] = 0x00;

    // GPU indexed palette rendering via CRTC's built-in character renderer
    palette_.set(pet_constants::PALETTE, 2);
    std::memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    crtc_->configure_char_render(
        nullptr, pixel_buffer_,
        char_rom_, screen_ram_chip_->data(),
        pet_constants::SCREEN_COLS,
        pet_constants::PET_CHAR_HEIGHT,
        pet_constants::DISPLAY_WIDTH,
        1, 0,  // fg=1 (green), bg=0 (black)
        pet_constants::PALETTE, 2,
        0x03FF,  // vram_mask — 1K screen RAM
        0x80     // invert_bit — bit 7 selects inverted charset
    );

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              pet_constants::DISPLAY_WIDTH, 1);
    video_port_->set_palette(palette_.data(), 2);
    video_port_->bind_frame_output(&last_frame_data_);

    // VSYNC/HSYNC callbacks (display rendering handled by CRTC internally)
    crtc_->on_vsync = [this]() { this->crtc_vsync(); };
    crtc_->on_hsync = [this]() { this->crtc_hsync(); };

    // ---- PIA 1 (keyboard + cassette sense) ----
    pia1_->init();
    pia1_->user_data       = this;
    pia1_->on_port_a_read  = pia1_port_a_read;
    pia1_->on_port_a_write = pia1_port_a_write;
    pia1_->on_port_b_read  = pia1_port_b_read;
    pia1_->on_port_b_write = pia1_port_b_write;

    // ---- PIA 2 (IEEE-488 bus) ----
    pia2_->init();
    pia2_->user_data = this;
    // IEEE-488 callbacks not wired yet — returns open bus ($FF)

    // ---- VIA (timers, CB2 speaker, user port) ----
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

    // Register all manifest-created chips for Hardware debug menu
    register_bus_chips(board_);

    printf("PET: Initialization complete\n");
    return true;
}

void PETSystem::shutdown() {
    printf("PET: Shutting down system\n");
    System::shutdown();
}

void PETSystem::reset() {
    printf("PET: Resetting system\n");

    // Reset all manifest chips (CRTC, PIAs, VIA; RAM/ROM are no-op)
    board_.reset_chips();

    // Clear RAM but preserve ROMs
    if (main_ram_chip_) {
        memset(main_ram_chip_->data(), 0, 32768);
    }
    if (screen_ram_chip_) {
        memset(screen_ram_chip_->data(), 0, 1024);
    }

    // Clear pixel buffer
    std::memset(pixel_buffer_, 0, sizeof(pixel_buffer_));

    // Reset audio state
    speaker_state_ = false;
    audio_write_pos_ = 0;
    audio_read_pos_ = 0;
    audio_cycle_counter_ = 0;

    // Reset CPU last
    board_.cpu().reset();

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
            s = bus_.tick(s);
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

    if (!video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
    tick_peripherals();
}

// ============================================================================
// CRTC Display Callbacks
// ============================================================================

void PETSystem::crtc_display_char(uint16_t /* ma */, uint8_t /* ra */, bool /* cursor */) {
    // Character rendering is now handled by MC6845's built-in indexed
    // renderer (configure_char_render).  This callback is retained for
    // potential future per-character effects but is currently a no-op.
}

void PETSystem::crtc_vsync() {
    // Flush pixel buffer to video output
    if (video_port_) {
        auto& output = video_port_->output();
        for (int y = 0; y < pet_constants::DISPLAY_HEIGHT; y++) {
            const uint8_t* line = pixel_buffer_ + y * pet_constants::DISPLAY_WIDTH;
            output.drive({0, SyncFlag::HSync});
            for (int x = 0; x < pet_constants::DISPLAY_WIDTH; x++) {
                output.drive({line[x], SyncFlag::BeamOn});
            }
        }
        output.drive({0, SyncFlag::FrameEnd});
    }
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

    // Load manifest-declared ROMs (Editor ROM at $E000, Kernal ROM at $F000)
    bool ok = board_.load_roms(rom_root, "PET");

    // Character ROM — loaded into separate buffer, not main address space.
    // PET 4032 uses a 2KB character ROM (901447-10); we mirror it to fill 4KB.
    // PET 8032 uses a 4KB character ROM (901640-01).
    uint8_t char_buf[4096];
    bool char_ok = rom_loader_load_from_root(rom_root,
                                              "characters.901640-01.bin",
                                              4096, char_buf, sizeof(char_buf));
    if (!char_ok) {
        // Try 2KB character ROM (PET 4032 and earlier)
        uint8_t char_buf_2k[2048];
        char_ok = rom_loader_load_from_root(rom_root,
            "characters-2.901447-10.bin|characters.901447-10.bin|chargen|chargen.rom|901447-10.bin",
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
    bool basic_ok = rom_loader_load_from_root(rom_root,
                                               "basic-4.901465-23-20-21.bin",
                                               sizeof(basic_buf), basic_buf, sizeof(basic_buf));
    if (basic_ok) {
        memcpy(basic_rom_b_chip_->data(), basic_buf, 4096);
        memcpy(basic_rom_c_chip_->data(), basic_buf + 4096, 4096);
        memcpy(basic_rom_d_chip_->data(), basic_buf + 8192, 4096);
        printf("PET: BASIC 4.0 ROM loaded (12KB combined)\n");
    } else {
        // Try loading as three 4KB ROMs
        uint8_t rom_b[4096], rom_c[4096], rom_d[4096];
        bool b_ok = rom_loader_load_from_root(rom_root, "basic-4-b000.901465-23.bin|901465-23.bin", 4096, rom_b, sizeof(rom_b));
        bool c_ok = rom_loader_load_from_root(rom_root, "basic-4-c000.901465-20.bin|901465-20.bin", 4096, rom_c, sizeof(rom_c));
        bool d_ok = rom_loader_load_from_root(rom_root, "basic-4-d000.901465-21.bin|901465-21.bin", 4096, rom_d, sizeof(rom_d));
        if (b_ok && c_ok && d_ok) {
            memcpy(basic_rom_b_chip_->data(), rom_b, 4096);
            memcpy(basic_rom_c_chip_->data(), rom_c, 4096);
            memcpy(basic_rom_d_chip_->data(), rom_d, 4096);
            printf("PET: BASIC 4.0 ROM loaded (3 × 4KB)\n");
            basic_ok = true;
        } else {
            // Last resort: try 8KB combined at $C000 (missing $B000 bank)
            uint8_t basic8k[8192];
            bool ok8 = rom_loader_load_from_root(rom_root, "basic4.rom", 8192, basic8k, sizeof(basic8k));
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

    return (ok && basic_ok && char_ok);
}

// ============================================================================
// Memory Map Configuration
// ============================================================================

void PETSystem::configure_memory_map() {
    // Screen RAM mirror ($8400-$87FF) is now handled declaratively by the
    // manifest: the slot declares size_bytes=2048 with addr_mask=0x03FF,
    // so apply() wraps all accesses to the physical 1 KB chip.
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(pet_descriptor, []() {
    return std::make_unique<PETSystem>();
})
