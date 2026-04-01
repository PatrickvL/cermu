#include "systems/bbc/bbc_micro_system.hpp"
#include "core/cermu.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

using namespace mc6845::reg;

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// ============================================================================
// Hardware Traits
// ============================================================================

static HardwareTraits create_bbc_hardware_traits() {
    HardwareTraits traits = {};

    // Display — max resolution is Mode 0 (640×256)
    traits.display.native_width = bbc_constants::DISPLAY_WIDTH;
    traits.display.native_height = bbc_constants::DISPLAY_HEIGHT;
    traits.display.visible_width = bbc_constants::DISPLAY_WIDTH;
    traits.display.visible_height = bbc_constants::DISPLAY_HEIGHT;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 8;
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = false;

    // BBC Micro 8-color physical palette (active-high RGB, active accent accent)
    // The BBC produces 8 physical colors from 3-bit RGB via the Video ULA.
    // Logical-to-physical mapping is programmable.
    traits.display.default_palette = {
        PaletteColor(  0,   0,   0, 255),   // 0: Black
        PaletteColor(255,   0,   0, 255),   // 1: Red
        PaletteColor(  0, 255,   0, 255),   // 2: Green
        PaletteColor(255, 255,   0, 255),   // 3: Yellow
        PaletteColor(  0,   0, 255, 255),   // 4: Blue
        PaletteColor(255,   0, 255, 255),   // 5: Magenta
        PaletteColor(  0, 255, 255, 255),   // 6: Cyan
        PaletteColor(255, 255, 255, 255),   // 7: White
    };

    // Audio — SN76489 (3 tone + 1 noise)
    traits.audio.format = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz = bbc_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "SN76489";

    // Timing
    traits.timing.cpu_frequency_hz = bbc_constants::CPU_FREQ;
    traits.timing.video_frequency_hz = bbc_constants::MASTER_CLOCK;
    traits.timing.audio_sample_rate_hz = bbc_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps = bbc_constants::TARGET_FPS;
    traits.timing.cycles_per_frame = bbc_constants::CYCLES_PER_FRAME;
    traits.timing.standard = VideoStandard::PAL;

    // Memory options (BBC Model B has fixed 32 KB)
    traits.memory_options.push_back({
        "32KB RAM (Model B)",
        bbc_constants::RAM_SIZE,
        0,
        true
    });

    return traits;
}

// ============================================================================
// File Probe
// ============================================================================

static SystemProbeResult bbc_probe_file(
    const format_descriptor_t* /*matched_format*/,
    const char* filepath, const uint8_t* data, size_t size) {

    SystemProbeResult result = { 0.0f, {} };
    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
    if (ext) {
        // BBC Micro disc image formats
        if (cermu_strcasecmp(ext, ".ssd") == 0 || cermu_strcasecmp(ext, ".dsd") == 0) {
            result.confidence = 0.8f;
        }
        // UEF tape format
        else if (cermu_strcasecmp(ext, ".uef") == 0) {
            result.confidence = 0.8f;
        }
        // Raw binary (low confidence)
        else if (cermu_strcasecmp(ext, ".bin") == 0 || cermu_strcasecmp(ext, ".rom") == 0) {
            // Check for 16K sideways ROM size
            if (size == 16384) {
                result.confidence = 0.3f;
            } else {
                result.confidence = 0.1f;
            }
        }
    }
    (void)data;
    return result;
}

// ============================================================================
// System Descriptor
// ============================================================================

static SystemDescriptor bbc_descriptor = {
    "BBC Micro Model B",
    "BBC",
    "Acorn BBC Micro Model B (1981) — 32KB RAM, MOS 6502 @ 2 MHz, SN76489 sound",
    "bbc",
    {"BBC", "BBC-B", "BBCB", "BBC Micro", "BBCMicro"},
    nullptr,  // supported_formats — simple probe for now
    create_bbc_hardware_traits(),
    bbc_probe_file,
    "Acorn", 1981, fam65xx::MOS6502Traits.display_name, SystemType::Home
};

REGISTER_SYSTEM(bbc_descriptor, []() {
    return std::make_unique<BBCMicroSystem>();
});

// ============================================================================
// Constructor / Destructor
// ============================================================================

BBCMicroSystem::BBCMicroSystem()
    : System()
    , pins_(BBC_BUS_DEFAULT_STATE)
    , cycles_per_frame_(bbc_constants::CYCLES_PER_FRAME)
{
    hardware_traits_ = create_bbc_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
}

BBCMicroSystem::~BBCMicroSystem() {
    // Stop audio thread before chips are destroyed.
    audio_thread_.stop();
    // All chips owned by board_ — no manual cleanup.
    // memory_ points into the flat mem (owned by board_); don't free.
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& BBCMicroSystem::get_descriptor() const {
    return bbc_descriptor;
}

// ============================================================================
// Configuration
// ============================================================================

bool BBCMicroSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool BBCMicroSystem::apply_configuration() {
    return true;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool BBCMicroSystem::initialize() {
    printf("BBC Micro: Initializing system\n");
    register_board(&board_);

    // ── Pre-bind all value-typed chips, then factory-create remaining ───
    { size_t slot_idx_ = 0;
      BBC_MICRO_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);
    ram_chip_        = &board_.ram;
    paged_rom_chip_  = &board_.paged_rom;
    os_rom_chip_     = &board_.os_rom;

    // ── Convenience pointer for rendering functions ─────────────────────
    memory_ = ram_chip_->data();

    // ── Post-apply page table fixups ────────────────────────────────────
    rom_select_ = 15;
    configure_bus_memory_map();

    // Load ROMs (into flat mem via chip data pointers)
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("BBC Micro: Warning — ROMs not loaded, system will not boot correctly\n");
    }

    // ---- CPU (MOS 6502 @ 2 MHz) ----
    board_.m6502.init();
    board_.m6502.reset();

    // ---- CRTC (MC6845) ----
    board_.crtc.init();

    // Program CRTC with Mode 7 register values (the MOS does this too, but
    // we prime them so the display works even before the ROM runs)
    for (int i = 0; i < 14; i++) {
        board_.crtc.regs_[i] = bbc_constants::MODE7_CRTC_REGS[i];
    }

    // Wire CRTC callbacks
    board_.crtc.on_display_char = [this](uint16_t ma, uint8_t ra, bool cursor) {
        this->crtc_display_char(ma, ra, cursor);
    };
    board_.crtc.on_vsync = [this]() { this->crtc_vsync(); };
    board_.crtc.on_hsync = [this]() { this->crtc_hsync(); };

    // Set memory for video rendering
    board_.vidproc.set_memory(memory_);

    // Video output — composite video from VIDPROC
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vidproc.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // ---- Sound (SN76489) ----
    board_.psg.init();
    board_.psg.set_clock_frequency(bbc_constants::SN76489_CLOCK);
    board_.psg.set_audio_sample_rate(bbc_constants::DEFAULT_SAMPLE_RATE);

    // Wire SN76489 to audio thread — PSG is clocked at CRTC rate (1 MHz),
    // CPU runs at 2 MHz → 2 CPU cycles per PSG tick.
    psg_adapter_ = std::make_unique<WriteOnlySynthAdapter<sn76489_t>>(&board_.psg, 2);
    audio_thread_.register_engine(psg_adapter_.get());
    audio_thread_.start();

    // Wire SN76489 to audio signal port
    audio_port_ = std::make_unique<AudioPort>();
    board_.psg.set_audio_port(audio_port_.get());

    // ---- System VIA ($FE40-$FE5F) ----
    board_.sys_via.reset();
    board_.sys_via.interrupt_bit = BUS_IRQ_BIT;
    // Port A: keyboard column data + slow data bus
    // Port B: addressable latch control + VSYNC + light pen
    board_.sys_via.port_a_read_callback = sys_via_port_a_read;
    board_.sys_via.port_a_read_context = this;
    board_.sys_via.port_b_read_callback = sys_via_port_b_read;
    board_.sys_via.port_b_read_context = this;
    board_.sys_via.set_port_b_write_callback(sys_via_port_b_write, this);

    // ---- User VIA ($FE60-$FE7F) ----
    board_.user_via.reset();
    board_.user_via.interrupt_bit = BUS_IRQ_BIT;

    // ---- Video ULA defaults ----
    // Default palette: identity mapping (logical N → physical N)
    for (int i = 0; i < 16; i++) {
        // write_palette format: high nibble = logical, low nibble = encoded physical
        // Physical = ((data >> 1) & 7) ^ 7, so to get physical i: data = ((i ^ 7) << 1)
        board_.vidproc.write_palette((i << 4) | (((i & 0x07) ^ 0x07) << 1));
    }

    // ---- Keyboard ----
    std::memset(key_matrix_, 0, sizeof(key_matrix_));
    any_key_pressed_ = false;
    addressable_latch_ = 0;

    // Register all manifest-created chips for Hardware debug menu
    register_bus_chips(board_);

    printf("BBC Micro: System initialized\n");
    return true;
}

void BBCMicroSystem::shutdown() {
    printf("BBC Micro: Shutting down\n");
    audio_thread_.stop();
    System::shutdown();
}

void BBCMicroSystem::reset() {
    printf("BBC Micro: Resetting\n");

    // Reset all manifest chips (CRTC, PSG, VIAs; RAM/ROM are no-op)
    board_.reset_chips();
    if (system_ready_)  { board_.m6502.reset(); }

    // Re-establish VIA callbacks (reset clears them)
    board_.sys_via.interrupt_bit = BUS_IRQ_BIT;
    board_.sys_via.port_a_read_callback = sys_via_port_a_read;
    board_.sys_via.port_a_read_context = this;
    board_.sys_via.port_b_read_callback = sys_via_port_b_read;
    board_.sys_via.port_b_read_context = this;
    board_.sys_via.set_port_b_write_callback(sys_via_port_b_write, this);
    board_.user_via.interrupt_bit = BUS_IRQ_BIT;

    // Reset audio thread adapter (both threads quiescent during reset)
    audio_thread_.stop();
    if (psg_adapter_) psg_adapter_->reset();
    audio_thread_.start();

    // Reset state
    pins_ = BBC_BUS_DEFAULT_STATE;
    rom_select_ = 15;
    configure_bus_memory_map();
    addressable_latch_ = 0;
    std::memset(key_matrix_, 0, sizeof(key_matrix_));
    any_key_pressed_ = false;
    crtc_divider_ = 0;
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

// CS-tick architecture:
//   resolve() → service() handles RAM/ROM via page table.
//   CRTC, VIDPROC, System VIA, User VIA self-select via CS field.
//   ROM select ($FE30) handled as system glue after CS dispatch.

void BBCMicroSystem::tick() {
    bus_state_t s = pins_;

    // ---- CRTC character clock (1 MHz = every other CPU cycle) ----
    // The CRTC character clock divides the 2 MHz CPU clock by 2.
    // Register access happens at full CPU rate via CS dispatch below.
    crtc_divider_++;
    if (crtc_divider_ >= 2) {
        crtc_divider_ = 0;
        board_.crtc.tick();  // character clock only (void)
    }

    // ---- Propagate VIA interrupt state from previous cycle ----
    // VIA IRQ assertion persists in ifr/ier across ticks.  The CPU
    // samples IRQ during PHI2, so propagate before the CPU tick.
    if (board_.sys_via.ifr & board_.sys_via.ier & 0x7F) {
        if (board_.sys_via.interrupt_bit != 0)
            BUS_CLR_BIT(s, board_.sys_via.interrupt_bit);
    }
    if (board_.user_via.ifr & board_.user_via.ier & 0x7F) {
        if (board_.user_via.interrupt_bit != 0)
            BUS_CLR_BIT(s, board_.user_via.interrupt_bit);
    }

    // ---- CPU PHI2 — address/R#W valid on bus ----
    s = board_.m6502.tick<MOS6502::Phase::PHI2>(s);

    // ---- Address decode + flat-mem service + MMIO self-dispatch ----
    s = bus_.resolve(s);
    s = bus_.service(s);

    // VIA ticks: advance timers (every cycle) + CS-gated register access
    s = board_.sys_via.tick(s);
    s = board_.user_via.tick(s);

    // CRTC register access (CS-gated, no character clock — already ticked above)
    if (board_.crtc.is_cs_selected(s)) {
        s = BUS_GET_BIT(s, BUS_RW_BIT)
            ? board_.crtc.on_bus_read(s) : board_.crtc.on_bus_write(s);
        board_.crtc.mark_cs_serviced(s);
    }

    // Video ULA register access (CS-gated, write-only)
    s = board_.vidproc.tick(s);

    // ROM select register ($FE30) — system glue, not a chip.
    // Not mapped to any chip ID → CS field won't match any chip.
    if (unlikely(!BUS_GET_BIT(s, BUS_RW_BIT))) {
        uint16_t addr = BUS_GET_ADDR(s);
        if (addr == bbc_constants::ROM_SELECT_REG) {
            rom_select_ = BUS_GET_DATA(s) & 0x0F;
            update_paged_rom();
        }
    }

    // ---- NMI edge detection ----
    board_.m6502.sample_nmi_pin(s);

    // ---- CPU PHI1 ----
    s = board_.m6502.tick<MOS6502::Phase::PHI1>(s);

    BUS_SET_BIT(s, BUS_RW_BIT);
    pins_ = s;
    total_cycles_++;
}

void BBCMicroSystem::run_frame() {
    if (!video_port_) return;

    // Stream-driven: VIDPROC drives FrameEnd via CRTC timing
    auto& output = video_port_->output();
    const int frames = (speed_multiplier_ > 1.0) ? static_cast<int>(speed_multiplier_) : 1;
    for (int f = 0; f < frames; f++) {
        while (!output.frame_ended()) {
            tick();
        }
        video_port_->swap_frame();
    }
    audio_thread_.signal_progress(total_cycles_);
    tick_peripherals();
}

// ============================================================================
// Bus Configuration
// ============================================================================

void BBCMicroSystem::configure_bus_memory_map() {
    // apply() establishes the default linear map from the manifest:
    //   $00-$7F: RAM (read+write)
    //   $80-$BF: Paged ROM bank 0 (read) — clipped from 256 KB pool
    //   $C0-$FF: OS ROM (read) — overrides clipped paged ROM pages
    //
    // With EnableCs=true, apply() Phase 3 also creates MaskedSubTable
    // entries for SHEILA ($FE) with regions for CRTC, VIDPROC, two VIAs.
    // FRED ($FC) and JIM ($FD) remain unmapped → open-bus on read.
    board_.apply(bus_);

    // Unmap FRED ($FC) and JIM ($FD) — no hardware on the 1 MHz bus yet
    bus_.map_no_chip_selected(0, 0xFC, 2);

    // Map currently selected paged ROM bank to $80-$BF
    update_paged_rom();
}

void BBCMicroSystem::update_paged_rom() {
    board_.select_bank_at(bus_, 0, kBBCMicroChips.find<ROMChip>(),
                           rom_select_ & 0x0F, 0x80);
}

// ============================================================================
// System VIA Port B Write Callback — Addressable Latch + SN76489 Trigger
// ============================================================================
//
// The BBC Micro's 74LS259 addressable latch is driven by System VIA Port B:
//   PB0-PB2: latch address (0-7)
//   PB3:     latch data (1 = set, 0 = clear)
//
// Latch bit assignments:
//   D0: SN76489 /WE (active-low → falling edge triggers sound write)
//   D1-D2: speech processor /RS, /WS
//   D3: keyboard auto-scan enable
//   D4-D5: caps/shift lock LEDs
//
// SN76489 data comes from System VIA Port A.  The write sequence:
//   1. CPU writes data byte to VIA Port A (ORA)
//   2. CPU writes latch address 0, data=0 to VIA Port B (ORB) → /WE goes low
//   3. CPU writes latch address 0, data=1 to VIA Port B (ORB) → /WE goes high
// We detect the falling edge of latch bit 0 and enqueue a timestamped write.

void BBCMicroSystem::sys_via_port_b_write(void* context, uint8_t data) {
    auto* sys = static_cast<BBCMicroSystem*>(context);

    uint8_t latch_addr = data & 0x07;
    bool latch_data = (data >> 3) & 0x01;
    uint8_t old_latch = sys->addressable_latch_;

    if (latch_data)
        sys->addressable_latch_ |= (1 << latch_addr);
    else
        sys->addressable_latch_ &= ~(1 << latch_addr);

    // SN76489 /WE falling edge: old bit 0 was HIGH, now LOW
    if (latch_addr == 0 && (old_latch & 0x01) && !latch_data) {
        if (sys->psg_adapter_) {
            uint8_t psg_data = sys->board_.sys_via.regs_[PORTA];
            sys->psg_adapter_->cmd_queue().push_write(sys->total_cycles_, 0, psg_data);
        }
    }
}

// ============================================================================
// Sound Chip Write — triggered via System VIA Port B write callback
// ============================================================================
// The SN76489 /WE line is active-low.  On the BBC Micro, writing to the
// sound chip is a multi-step process via the System VIA:
//   1. Write data byte to VIA Port A (slow data bus)
//   2. Toggle addressable latch bit 0 low via Port B write (/WE active)
//   3. Toggle addressable latch bit 0 high via Port B write (/WE inactive)
// The port_b_write_callback above handles the latch and SN76489 trigger.

// ============================================================================
// CRTC Display Callbacks
// ============================================================================

void BBCMicroSystem::crtc_display_char(uint16_t ma, uint8_t ra, bool cursor) {
    if (false) return;
    board_.vidproc.display_char(ma, ra, cursor, board_.crtc.regs_[R9_MAX_SCANLINE]);
}

void BBCMicroSystem::crtc_vsync() {
    // Flush indexed frame through VIDPROC pixel unit at VSYNC
    board_.vidproc.vsync();

    // On real hardware, VSYNC connects to System VIA CA1 input.
    // The VIA detects the edge and sets the CA1 interrupt flag.
    // Since the current VIA implementation doesn't have CA1 pin handling,
    // we directly set the CA1 interrupt flag in IFR.
    board_.sys_via.ifr |= MOS6522_IFR_CA1;
}

void BBCMicroSystem::crtc_hsync() {
    // HSYNC — new scan line (no action needed for basic rendering)
}

// ============================================================================
// System VIA Callbacks
// ============================================================================

uint8_t BBCMicroSystem::sys_via_port_a_read(void* ctx, uint8_t /*output*/) {
    BBCMicroSystem* sys = static_cast<BBCMicroSystem*>(ctx);

    // System VIA Port A read: keyboard column data + slow data bus
    // PA0-PA6: keyboard data (active-low)
    // PA7: depends on what's being read (sound chip ready, etc.)
    //
    // When auto-scan is enabled (addressable latch bit 3), the keyboard
    // returns the state of the currently selected column.
    uint8_t keyboard_data = 0xFF;
    uint8_t col = sys->board_.sys_via.port_b.output() & 0x07;
    if (col < bbc_constants::KEYBOARD_COLS) {
        keyboard_data = sys->scan_keyboard(col);
    }

    return keyboard_data;
}

uint8_t BBCMicroSystem::sys_via_port_b_read(void* ctx, uint8_t /*output*/) {
    BBCMicroSystem* sys = static_cast<BBCMicroSystem*>(ctx);

    // System VIA Port B read:
    // PB0-PB3: addressable latch (active-low accent out, active accent read-back)
    // PB4: CRTC light pen strobe
    // PB5-PB6: unused
    // PB7: VSYNC (active-high)
    uint8_t pb = 0;
    if (sys->board_.crtc.v_sync_active) {
        pb |= 0x80;  // PB7 = VSYNC
    }
    return pb;
}

// ============================================================================
// Keyboard
// ============================================================================

void BBCMicroSystem::update_key_matrix(SDL_Keycode key, bool pressed) {
    // BBC Micro keyboard matrix: 10 columns × 8 rows
    // Map SDL keycodes to matrix positions.
    // This is a simplified mapping — a full implementation would use
    // the KeyboardMapper system (like Commodore systems do).

    // Column, Row pairs for common keys
    int col = -1, row = -1;

    switch (key) {
        // Row 0
        case SDLK_LSHIFT: case SDLK_RSHIFT: col = 0; row = 0; break;
        case SDLK_q:      col = 1; row = 0; break;
        case SDLK_3:      col = 2; row = 0; break;  // Actually varies — simplified
        case SDLK_4:      col = 3; row = 0; break;
        case SDLK_5:      col = 4; row = 0; break;

        // Alphanumeric keys — simplified mapping
        case SDLK_a:      col = 4; row = 1; break;
        case SDLK_s:      col = 5; row = 1; break;
        case SDLK_d:      col = 3; row = 2; break;
        case SDLK_f:      col = 4; row = 3; break;
        case SDLK_g:      col = 5; row = 3; break;
        case SDLK_h:      col = 5; row = 4; break;
        case SDLK_j:      col = 4; row = 5; break;
        case SDLK_k:      col = 4; row = 6; break;
        case SDLK_l:      col = 5; row = 6; break;
        case SDLK_z:      col = 6; row = 1; break;
        case SDLK_x:      col = 4; row = 2; break;
        case SDLK_c:      col = 5; row = 2; break;
        case SDLK_v:      col = 6; row = 3; break;
        case SDLK_b:      col = 6; row = 4; break;
        case SDLK_n:      col = 5; row = 5; break;
        case SDLK_m:      col = 6; row = 5; break;
        case SDLK_w:      col = 2; row = 1; break;
        case SDLK_e:      col = 2; row = 2; break;
        case SDLK_r:      col = 3; row = 3; break;
        case SDLK_t:      col = 2; row = 3; break;
        case SDLK_y:      col = 4; row = 4; break;
        case SDLK_u:      col = 3; row = 5; break;
        case SDLK_i:      col = 2; row = 5; break;
        case SDLK_o:      col = 3; row = 6; break;
        case SDLK_p:      col = 3; row = 7; break;

        // Number row
        case SDLK_1:      col = 3; row = 0; break;
        case SDLK_2:      col = 3; row = 1; break;
        case SDLK_0:      col = 2; row = 7; break;
        case SDLK_6:      col = 3; row = 4; break;
        case SDLK_7:      col = 2; row = 4; break;
        case SDLK_8:      col = 1; row = 5; break;
        case SDLK_9:      col = 2; row = 6; break;

        // Special keys
        case SDLK_RETURN:    col = 4; row = 9; break;
        case SDLK_SPACE:     col = 6; row = 2; break;
        case SDLK_BACKSPACE: col = 5; row = 9; break;
        case SDLK_TAB:       col = 6; row = 0; break;
        case SDLK_ESCAPE:    col = 7; row = 0; break;

        // Cursor keys
        case SDLK_LEFT:   col = 1; row = 9; break;
        case SDLK_RIGHT:  col = 7; row = 9; break;
        case SDLK_UP:     col = 3; row = 9; break;
        case SDLK_DOWN:   col = 2; row = 9; break;

        default: break;
    }

    if (col >= 0 && row >= 0 &&
        col < static_cast<int>(bbc_constants::KEYBOARD_COLS) &&
        row < static_cast<int>(bbc_constants::KEYBOARD_ROWS)) {
        key_matrix_[col][row] = pressed;
    }

    // Update any_key_pressed flag
    any_key_pressed_ = false;
    for (int c = 0; c < static_cast<int>(bbc_constants::KEYBOARD_COLS); c++) {
        for (int r = 0; r < static_cast<int>(bbc_constants::KEYBOARD_ROWS); r++) {
            if (key_matrix_[c][r]) {
                any_key_pressed_ = true;
                return;
            }
        }
    }
}

uint8_t BBCMicroSystem::scan_keyboard(uint8_t column) const {
    if (column >= bbc_constants::KEYBOARD_COLS) return 0xFF;

    // Return active-low column data for the selected column
    uint8_t result = 0xFF;
    for (uint32_t row = 0; row < bbc_constants::KEYBOARD_ROWS; row++) {
        if (key_matrix_[column][row]) {
            result &= ~(1 << row);
        }
    }
    return result;
}

// ============================================================================
// File Loading
// ============================================================================

bool BBCMicroSystem::load_file(const char* filepath) {
    printf("BBC Micro: Loading file: %s\n", filepath);
    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
    if (!ext) {
        printf("BBC Micro: Unknown file type\n");
        return false;
    }

    // TODO: Implement SSD/DSD disc image loading
    // TODO: Implement UEF tape loading
    // TODO: Implement sideways ROM loading (.rom)
    printf("BBC Micro: File format not yet supported: %s\n", ext);
    return false;
}

// ============================================================================
// Display
// ============================================================================


// ============================================================================
// Input
// ============================================================================

void BBCMicroSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    update_key_matrix(key, pressed);
}

// ============================================================================
// Audio
// ============================================================================

uint32_t BBCMicroSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }
    if (false) return 0;
    return board_.psg.audio_read(buffer, max_samples);
}

void BBCMicroSystem::set_audio_sample_rate(int sample_rate_hz) {
    if (true) {
        board_.psg.set_audio_sample_rate(sample_rate_hz);
    }
}

// ============================================================================
// GUI
// ============================================================================

void BBCMicroSystem::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset BBC Micro")) {
        reset();
    }
    ImGui::Separator();
    ImGui::Text("ROM Bank: %d", rom_select_);
    ImGui::Text("Video Mode: %d",
        board_.vidproc.get_display_mode(board_.crtc.regs_[R9_MAX_SCANLINE]));
#endif
}

void BBCMicroSystem::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("BBC Micro Model B Configuration");
    ImGui::Separator();
    ImGui::Text("CPU: MOS 6502A @ 2 MHz");
    ImGui::Text("RAM: 32 KB");
    ImGui::Text("Sound: SN76489");
    ImGui::Text("Video: MC6845 CRTC + Video ULA");
#endif
}

// ============================================================================
// ROM Loading
// ============================================================================

bool BBCMicroSystem::load_roms() {
    // Discover ROM root path
    const char* search_names[] = {"bbc", "bbcb", "bbc-b", "bbcmicro", nullptr};
    char rom_root[1024];
    if (!system_config_discover_rom_root(search_names, rom_root, sizeof(rom_root))) {
        printf("BBC Micro: ROM root directory not found\n");
        return false;
    }

    printf("BBC Micro: ROM root: %s\n", rom_root);

    // Load manifest-declared ROMs (MOS ROM at slot 2)
    bool os_ok = board_.load_roms(rom_root, "BBC Micro");

    // BASIC ROM (BBC BASIC II — 16 KB) → into paged ROM pool slot 15
    uint8_t* basic_rom_data = paged_rom_chip_->data()
                            + 15 * bbc_constants::PAGED_ROM_SIZE;
    bool basic_ok = rom_loader_load_from_root(
        rom_root,
        "basic2.rom|BASIC2.rom|basic.rom|bbc_basic.rom|BASIC-2.rom",
        bbc_constants::PAGED_ROM_SIZE,
        basic_rom_data, bbc_constants::PAGED_ROM_SIZE);
    if (!basic_ok) {
        printf("BBC Micro: BASIC ROM not found\n");
    }

    return os_ok;  // System won't boot without OS ROM
}
