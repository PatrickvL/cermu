#include "systems/bbc/bbc_micro_system.hpp"
#include "core/cermu.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

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
    bbc_probe_file
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
    delete cpu_;    cpu_ = nullptr;
    delete crtc_;   crtc_ = nullptr;
    delete psg_;    psg_ = nullptr;
    // memory_ points into the unified buffer (owned by board_); don't free.
    // Paged ROM data lives in the unified buffer; no manual cleanup.
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

    // ── Factory-create memory chips from manifest ─────────────────────
    board_.create_chips(&pins_);
    ram_chip_        = board_.chip_as<RAMChip>(bbc_chips::kRamSlot);
    paged_rom_chip_  = board_.chip_as<ROMChip>(bbc_chips::kPagedRomSlot);
    os_rom_chip_     = board_.chip_as<ROMChip>(bbc_chips::kOsRomSlot);

    // ── Convenience pointer for rendering functions ─────────────────────
    memory_ = ram_chip_->data();

    // ── Post-apply page table fixups ────────────────────────────────────
    rom_select_ = 15;
    configure_bus_memory_map();

    // Load ROMs (into unified buffer via chip data pointers)
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("BBC Micro: Warning — ROMs not loaded, system will not boot correctly\n");
    }

    // ---- CPU (MOS 6502 @ 2 MHz) ----
    cpu_ = new MOS6502();
    cpu_->init();
    cpu_->reset(0);

    // ---- CRTC (MC6845) ----
    crtc_ = new mc6845_t();
    crtc_->init();

    // Program CRTC with Mode 7 register values (the MOS does this too, but
    // we prime them so the display works even before the ROM runs)
    for (int i = 0; i < 14; i++) {
        crtc_->regs_[i] = bbc_constants::MODE7_CRTC_REGS[i];
    }

    // Wire CRTC callbacks
    crtc_->on_display_char = [this](uint16_t ma, uint8_t ra, bool cursor) {
        this->crtc_display_char(ma, ra, cursor);
    };
    crtc_->on_vsync = [this]() { this->crtc_vsync(); };
    crtc_->on_hsync = [this]() { this->crtc_hsync(); };

    // ---- Sound (SN76489) ----
    psg_ = new sn76489_t(SN76489Variant::SN76489);
    psg_->init();
    psg_->set_clock_frequency(bbc_constants::SN76489_CLOCK);
    psg_->set_audio_sample_rate(bbc_constants::DEFAULT_SAMPLE_RATE);

    // ---- System VIA ($FE40-$FE5F) ----
    system_via_.reset();
    system_via_.interrupt_bit = BUS_IRQ_BIT;
    // Port A: keyboard column data + slow data bus
    // Port B: addressable latch control + VSYNC + light pen
    system_via_.port_a_read_callback = sys_via_port_a_read;
    system_via_.port_a_read_context = this;
    system_via_.port_b_read_callback = sys_via_port_b_read;
    system_via_.port_b_read_context = this;

    // ---- User VIA ($FE60-$FE7F) ----
    user_via_.reset();
    user_via_.interrupt_bit = BUS_IRQ_BIT;

    // ---- Video ULA defaults ----
    video_ula_control_ = 0x00;
    std::memset(video_ula_palette_, 0, sizeof(video_ula_palette_));
    // Default palette: identity mapping (logical N → physical N)
    for (int i = 0; i < 16; i++) {
        video_ula_palette_[i] = i & 0x07;
    }

    // ---- Keyboard ----
    std::memset(key_matrix_, 0, sizeof(key_matrix_));
    any_key_pressed_ = false;
    addressable_latch_ = 0;

    // Register chips for Hardware debug menu
    register_chips();

    // Register memory and ROM chips — transfer ownership
    register_bus_chips(board_);

    printf("BBC Micro: System initialized\n");
    return true;
}

void BBCMicroSystem::shutdown() {
    printf("BBC Micro: Shutting down\n");
    System::shutdown();
}

void BBCMicroSystem::reset() {
    printf("BBC Micro: Resetting\n");

    // Reset all chips
    if (cpu_)  { cpu_->reset(0); }
    if (crtc_) { crtc_->reset(); }
    if (psg_)  { psg_->reset(); }
    system_via_.reset();
    system_via_.interrupt_bit = BUS_IRQ_BIT;
    system_via_.port_a_read_callback = sys_via_port_a_read;
    system_via_.port_a_read_context = this;
    system_via_.port_b_read_callback = sys_via_port_b_read;
    system_via_.port_b_read_context = this;
    user_via_.reset();
    user_via_.interrupt_bit = BUS_IRQ_BIT;

    // Reset state
    pins_ = BBC_BUS_DEFAULT_STATE;
    rom_select_ = 15;
    configure_bus_memory_map();
    video_ula_control_ = 0x00;
    addressable_latch_ = 0;
    std::memset(key_matrix_, 0, sizeof(key_matrix_));
    any_key_pressed_ = false;
    crtc_divider_ = 0;
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

void BBCMicroSystem::tick() {
    bus_state_t s = pins_;

    // ---- CRTC character clock (1 MHz = every other CPU cycle) ----
    crtc_divider_++;
    if (crtc_divider_ >= 2) {
        crtc_divider_ = 0;
        if (crtc_) {
            crtc_->tick();
        }
        // SN76489 internal clock is master/16 = 250 kHz = 1 tick per 4 CRTC clocks
        // But we approximate by ticking PSG at 1 MHz (= CRTC rate) and adjusting
        // the sample rate math.  The SN76489 internal divider is already handled
        // by the counter reload values being relative to the internal clock.
        if (psg_) {
            psg_->tick();
        }
    }

    // ---- VIA tick (both VIAs) ----
    {
        bus_state_t via_bus = BBC_BUS_DEFAULT_STATE;
        BUS_SET_BIT(via_bus, BUS_RW_BIT);
        via_bus = system_via_.tick(via_bus);
        if (!BUS_GET_BIT(via_bus, BUS_IRQ_BIT)) {
            BUS_CLR_BIT(s, BUS_IRQ_BIT);
        }
    }
    {
        bus_state_t via_bus = BBC_BUS_DEFAULT_STATE;
        BUS_SET_BIT(via_bus, BUS_RW_BIT);
        via_bus = user_via_.tick(via_bus);
        if (!BUS_GET_BIT(via_bus, BUS_IRQ_BIT)) {
            BUS_CLR_BIT(s, BUS_IRQ_BIT);
        }
    }

    // ---- CPU PHI2 ----
    s = cpu_->tick<MOS6502::Phase::PHI2>(s);

    // ---- Memory / I/O service ----
    {
        uint16_t addr = BUS_GET_ADDR(s);
        if (unlikely(addr >= bbc_constants::FRED_START && addr <= bbc_constants::SHEILA_END)) {
            s = sheila_tick(s);     // FRED/JIM/SHEILA I/O ($FC00-$FEFF)
        } else {
            s = bus_.tick(0, s);    // Memory dispatch via MemoryBus
        }
    }

    // ---- NMI edge detection ----
    cpu_->sample_nmi_pin(s);

    // ---- CPU PHI1 ----
    s = cpu_->tick<MOS6502::Phase::PHI1>(s);

    BUS_SET_BIT(s, BUS_RW_BIT);
    pins_ = s;
    total_cycles_++;
}

void BBCMicroSystem::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }
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
    // We fix up: write-protect ROM regions, unmap I/O pages, select active bank.
    board_.apply(bus_);

    // Write-protect ROM regions ($80-$FF)
    for (size_t page = 0x80; page < 0x100; ++page)
        bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);

    // Unmap FRED ($FC), JIM ($FD), SHEILA ($FE) — handled by sheila_tick()
    bus_.map_no_chip_selected(0, 0xFC, 3);

    // Map currently selected paged ROM bank to $80-$BF
    update_paged_rom();
}

void BBCMicroSystem::update_paged_rom() {
    using ChipId = typename PT::ChipId;

    constexpr size_t kPagesPerBank = bbc_constants::PAGED_ROM_SIZE / Bus::kPageSize;  // 64
    constexpr size_t kRomPoolBase  = kBBCMicroChips.base_id(bbc_chips::kPagedRomSlot, 8);

    size_t bank_offset = (rom_select_ & 0x0F) * kPagesPerBank;
    bus_.fill_read_pages(0, 0x80, kPagesPerBank, ChipId(kRomPoolBase + bank_offset));
}

// ============================================================================
// SHEILA / FRED / JIM I/O Dispatch ($FC00-$FEFF)
// ============================================================================

bus_state_t BBCMicroSystem::sheila_tick(bus_state_t s) {
    uint16_t addr = BUS_GET_ADDR(s);

    // FRED ($FC00-$FCFF) and JIM ($FD00-$FDFF): 1 MHz bus, not implemented
    if (addr < bbc_constants::SHEILA_START) {
        if (BUS_GET_BIT(s, BUS_RW_BIT))
            BUS_SET_DATA(s, 0xFF);
        return s;
    }

    // SHEILA I/O page ($FE00-$FEFF)
    if (BUS_GET_BIT(s, BUS_RW_BIT)) {
        // ---- Read cycle ----
        uint8_t data = 0xFF;

        if (addr >= bbc_constants::CRTC_BASE && addr <= bbc_constants::CRTC_END) {
            data = crtc_->read(addr);
        }
        else if (addr >= bbc_constants::SYSTEM_VIA_BASE && addr <= bbc_constants::SYSTEM_VIA_END) {
            bus_state_t via_s = BBC_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(via_s, addr - bbc_constants::SYSTEM_VIA_BASE);
            BUS_SET_BIT(via_s, BUS_RW_BIT);
            via_s = system_via_.registers_read(via_s);
            data = BUS_GET_DATA(via_s);
        }
        else if (addr >= bbc_constants::USER_VIA_BASE && addr <= bbc_constants::USER_VIA_END) {
            bus_state_t via_s = BBC_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(via_s, addr - bbc_constants::USER_VIA_BASE);
            BUS_SET_BIT(via_s, BUS_RW_BIT);
            via_s = user_via_.registers_read(via_s);
            data = BUS_GET_DATA(via_s);
        }

        BUS_SET_DATA(s, data);
    } else {
        // ---- Write cycle ----
        uint8_t data = BUS_GET_DATA(s);

        if (addr >= bbc_constants::CRTC_BASE && addr <= bbc_constants::CRTC_END) {
            crtc_->write(addr, data);
        }
        else if (addr == bbc_constants::VIDEO_ULA_CONTROL) {
            video_ula_control_ = data;
        }
        else if (addr == bbc_constants::VIDEO_ULA_PALETTE) {
            // Palette register: bits 7-4 = logical color, bits 3-1 = physical color (EOR)
            // bit 0 is complement of bit 0 of physical
            uint8_t logical = (data >> 4) & 0x0F;
            // Physical color: bits 3-1 directly, bit 0 is inverted
            uint8_t physical = ((data >> 1) & 0x07) ^ 0x07;
            video_ula_palette_[logical] = physical;
        }
        else if (addr == bbc_constants::ROM_SELECT_REG) {
            rom_select_ = data & 0x0F;
            update_paged_rom();
        }
        else if (addr >= bbc_constants::SYSTEM_VIA_BASE && addr <= bbc_constants::SYSTEM_VIA_END) {
            bus_state_t via_s = BBC_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(via_s, addr - bbc_constants::SYSTEM_VIA_BASE);
            BUS_SET_DATA(via_s, data);
            BUS_CLR_BIT(via_s, BUS_RW_BIT);
            system_via_.registers_write(via_s);

            // Check if writing to Port B triggers sound chip or addressable latch
            uint8_t via_reg = (addr - bbc_constants::SYSTEM_VIA_BASE) & 0x0F;
            if (via_reg == 0x00) {  // ORB — Port B output
                // Addressable latch: PB0-PB2 = address, PB3 = data
                uint8_t latch_addr = data & 0x07;
                bool latch_data = (data >> 3) & 0x01;
                if (latch_data) {
                    addressable_latch_ |= (1 << latch_addr);
                } else {
                    addressable_latch_ &= ~(1 << latch_addr);
                }
            }
        }
        else if (addr >= bbc_constants::USER_VIA_BASE && addr <= bbc_constants::USER_VIA_END) {
            bus_state_t via_s = BBC_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(via_s, addr - bbc_constants::USER_VIA_BASE);
            BUS_SET_DATA(via_s, data);
            BUS_CLR_BIT(via_s, BUS_RW_BIT);
            user_via_.registers_write(via_s);
        }
    }

    return s;
}

// ============================================================================
// Sound Chip Write — triggered via System VIA Port A
// ============================================================================
// The SN76489 /WE line is active-low.  On the BBC Micro, writing to the
// sound chip is a multi-step process via the System VIA:
//   1. Write data nibble to VIA Port A (slow data bus)
//   2. Toggle addressable latch bit 0 low (/WE active)
//   3. Toggle addressable latch bit 0 high (/WE inactive)
// We handle this in the VIA Port A read callback by checking the latch state.

// ============================================================================
// CRTC Display Callbacks
// ============================================================================

void BBCMicroSystem::crtc_display_char(uint16_t ma, uint8_t ra, bool cursor) {
    if (!rgba_framebuffer_ || !memory_) return;

    int mode = get_display_mode();

    if (mode == 7) {
        // Mode 7: Teletext — character-based display
        render_mode7_char(ma & 0x03FF, ra, cursor);
    } else {
        // Modes 0-6: Bitmap display
        render_bitmap_pixels(ma, ra, cursor);
    }
}

void BBCMicroSystem::crtc_vsync() {
    // VSYNC — start of new frame
    screen_pixel_x_ = 0;
    screen_pixel_y_ = 0;

    // On real hardware, VSYNC connects to System VIA CA1 input.
    // The VIA detects the edge and sets the CA1 interrupt flag.
    // Since the current VIA implementation doesn't have CA1 pin handling,
    // we directly set the CA1 interrupt flag in IFR.
    system_via_.ifr |= MOS6522_IFR_CA1;
}

void BBCMicroSystem::crtc_hsync() {
    // HSYNC — new scan line (no action needed for basic rendering)
}

// ============================================================================
// Mode 7 (Teletext) Rendering
// ============================================================================

void BBCMicroSystem::render_mode7_char(uint16_t screen_offset, uint8_t ra, bool cursor) {
    // Mode 7 screen RAM is at $7C00-$7FFF (1000 bytes for 40×25)
    // Characters are 6-bit (bits 6:0) Teletext codes.
    // For now, render as simple ASCII-like text using a built-in font.
    static constexpr uint16_t MODE7_SCREEN_RAM = 0x7C00;

    if (screen_offset >= 1000) return;

    uint8_t char_code = memory_[MODE7_SCREEN_RAM + screen_offset];

    // Simple ASCII rendering for characters 0x20-0x7F
    // Teletext control codes (0x00-0x1F) are used for color switching etc.
    // For initial implementation, render printable chars white on black
    uint8_t pixel_row = 0;
    bool is_control = (char_code < 0x20);

    if (!is_control) {
        // Use a minimal built-in 8×8 font for printable ASCII
        // (The real SAA5050 Teletext chip has a 12×20 character cell
        //  but for initial bring-up, 8-pixel-wide chars are sufficient)
        // TODO: Implement SAA5050 Teletext character generator
        pixel_row = 0;  // Placeholder — all blank until font is loaded
    }

    if (cursor && ra < 8) {
        pixel_row = ~pixel_row;
    }

    // Calculate framebuffer position
    uint32_t char_col = screen_offset % bbc_constants::MODE7_COLS;
    uint32_t char_row = screen_offset / bbc_constants::MODE7_COLS;

    if (char_row >= bbc_constants::MODE7_ROWS) return;

    // Mode 7: each character is 16×20 pixels (to fill 640×500 → scaled to 640×256)
    // But we render at native DISPLAY_HEIGHT=256, so scale vertically
    uint32_t pixel_x = char_col * 16;
    uint32_t pixel_y = char_row * 10 + (ra / 2);  // 20 scanlines → 10 pixels visible

    if (pixel_y >= static_cast<uint32_t>(rgba_height_)) return;
    if (pixel_x + 16 > static_cast<uint32_t>(rgba_width_)) return;

    uint32_t fg_color = 0xFFFFFFFF;  // White
    uint32_t bg_color = 0xFF000000;  // Black
    uint32_t* row_ptr = rgba_framebuffer_ + pixel_y * rgba_width_;

    // Render 8 source pixels, doubled to 16 output pixels
    for (int bit = 7; bit >= 0; bit--) {
        uint32_t color = (pixel_row & (1 << bit)) ? fg_color : bg_color;
        uint32_t px = pixel_x + (7 - bit) * 2;
        if (px < static_cast<uint32_t>(rgba_width_)) row_ptr[px] = color;
        if (px + 1 < static_cast<uint32_t>(rgba_width_)) row_ptr[px + 1] = color;
    }
}

// ============================================================================
// Bitmap Mode Rendering (Modes 0-6)
// ============================================================================

void BBCMicroSystem::render_bitmap_pixels(uint16_t ma, uint8_t ra, bool cursor) {
    // Bitmap modes: the CRTC address (MA) and raster address (RA) combine
    // to address screen RAM.  The Video ULA unpacks bytes into pixels
    // according to the mode's bits-per-pixel setting.
    //
    // Screen RAM address calculation:
    //   byte_addr = ((MA & 0x1FFF) | (RA & 0x07) << 13) * 8 (approximately)
    //   The exact mapping depends on the mode.
    //
    // For initial implementation, we use a simplified mapping.

    // Screen RAM starts at different addresses depending on mode:
    //   Mode 0: $3000 (20 KB), Mode 1: $3000 (20 KB), Mode 2: $3000 (20 KB)
    //   Mode 3: $4000 (16 KB), Mode 4: $5800 (10 KB), Mode 5: $5800 (10 KB)
    //   Mode 6: $6000 (8 KB)

    uint16_t byte_addr = (ma * 8) + ra;
    if (byte_addr >= bbc_constants::RAM_SIZE) return;

    uint8_t screen_byte = memory_[byte_addr];

    if (cursor) screen_byte = ~screen_byte;

    int ppb = get_pixels_per_byte();
    int colors = get_colors_per_mode();

    // Determine pixel width (how many framebuffer pixels per source pixel)
    int pixel_width = bbc_constants::DISPLAY_WIDTH / (ppb * 40);
    if (pixel_width < 1) pixel_width = 1;

    // Calculate screen position from CRTC counters
    // (Simplified: use ma to determine column/row)
    uint32_t col = (ma % 40);
    uint32_t row = (ma / 40);

    uint32_t pixel_x = col * ppb * pixel_width;
    uint32_t pixel_y = row * 8 + ra;

    if (pixel_y >= static_cast<uint32_t>(rgba_height_)) return;

    uint32_t* row_ptr = rgba_framebuffer_ + pixel_y * rgba_width_;

    // Unpack screen byte into pixels based on bits-per-pixel
    for (int p = 0; p < ppb; p++) {
        uint8_t color_index = 0;

        if (colors == 2) {
            // 1 bpp: 8 pixels per byte (Mode 0, 3, 4, 6)
            color_index = (screen_byte >> (7 - p)) & 0x01;
        } else if (colors == 4) {
            // 2 bpp: 4 pixels per byte (Mode 1, 5)
            // Bits are interleaved: pixel N uses bits (7-N) and (3-N)
            int bit_hi = (screen_byte >> (7 - p)) & 0x01;
            int bit_lo = (screen_byte >> (3 - p)) & 0x01;
            color_index = (bit_hi << 1) | bit_lo;
        } else if (colors == 16) {
            // 4 bpp: 2 pixels per byte (Mode 2)
            if (p == 0) {
                color_index = ((screen_byte >> 7) & 1) << 3 |
                              ((screen_byte >> 5) & 1) << 2 |
                              ((screen_byte >> 3) & 1) << 1 |
                              ((screen_byte >> 1) & 1);
            } else {
                color_index = ((screen_byte >> 6) & 1) << 3 |
                              ((screen_byte >> 4) & 1) << 2 |
                              ((screen_byte >> 2) & 1) << 1 |
                              ((screen_byte >> 0) & 1);
            }
        }

        // Map logical color through Video ULA palette to physical color
        uint8_t physical = video_ula_palette_[color_index & 0x0F] & 0x07;
        uint32_t rgba = 0xFF000000;
        if (physical < static_cast<uint8_t>(current_palette_.size())) {
            rgba = (0xFF << 24) |
                   (current_palette_[physical].b << 16) |
                   (current_palette_[physical].g << 8) |
                   current_palette_[physical].r;
        }

        // Write pixel(s) to framebuffer
        for (int w = 0; w < pixel_width; w++) {
            uint32_t px = pixel_x + p * pixel_width + w;
            if (px < static_cast<uint32_t>(rgba_width_)) {
                row_ptr[px] = rgba;
            }
        }
    }
}

// ============================================================================
// Video ULA Helpers
// ============================================================================

int BBCMicroSystem::get_display_mode() const {
    // The display mode is determined by the Video ULA control register
    // and CRTC programming.  Bits 4-6 of the control register determine
    // the number of characters per line, which maps to modes:
    //   Mode 0: 80 chars, 2 colors    (640×256)
    //   Mode 1: 40 chars, 4 colors    (320×256)
    //   Mode 2: 20 chars, 16 colors   (160×256)
    //   Mode 3: 80 chars, 2 colors    (640×250, text with gaps)
    //   Mode 4: 40 chars, 2 colors    (320×256)
    //   Mode 5: 20 chars, 4 colors    (160×256)
    //   Mode 6: 40 chars, 2 colors    (320×250, text with gaps)
    //   Mode 7: Teletext (SAA5050)    (40×25 characters)
    //
    // For now, detect Mode 7 by checking CRTC R9 (max scanline):
    // Mode 7 uses 18 scanlines per row; other modes use 7.
    if (crtc_ && crtc_->regs_[MC6845_R9_MAX_SCANLINE] >= 18) {
        return 7;
    }
    // Use bits 4-6 of video ULA control for other modes
    uint8_t chars_per_line_sel = (video_ula_control_ >> 4) & 0x07;
    // Approximate mapping — the real hardware also considers clock rate bit
    switch (chars_per_line_sel) {
        case 0: return 0;  // 10 chars → Mode 2 (16 colors)
        case 1: return 5;  // 20 chars → Mode 5 (4 colors)
        case 2: return 1;  // 20 chars → Mode 1 (4 colors)
        case 3: return 4;  // 40 chars → Mode 4 (2 colors)
        case 4: return 6;  // 40 chars → Mode 6 (text)
        case 5: return 3;  // 40 chars → Mode 3 (text)
        case 6: return 0;  // 80 chars → Mode 0 (2 colors)
        default: return 0;
    }
}

int BBCMicroSystem::get_pixels_per_byte() const {
    int mode = get_display_mode();
    switch (mode) {
        case 0: case 3: case 4: case 6: return 8;   // 1 bpp
        case 1: case 5:                 return 4;   // 2 bpp
        case 2:                         return 2;   // 4 bpp
        default:                        return 8;
    }
}

int BBCMicroSystem::get_colors_per_mode() const {
    int mode = get_display_mode();
    switch (mode) {
        case 0: case 3: case 4: case 6: return 2;
        case 1: case 5:                 return 4;
        case 2:                         return 16;
        default:                        return 2;
    }
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
    uint8_t col = sys->system_via_.port_b.output() & 0x07;
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
    if (sys->crtc_ && sys->crtc_->v_sync_active) {
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

uint32_t* BBCMicroSystem::get_framebuffer() {
    return rgba_framebuffer_;
}

void BBCMicroSystem::get_display_dimensions(int* width, int* height) const {
    *width = bbc_constants::DISPLAY_WIDTH;
    *height = bbc_constants::DISPLAY_HEIGHT;
}

void BBCMicroSystem::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

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
    if (!buffer || max_samples == 0 || !psg_) return 0;
    return psg_->audio_read(buffer, max_samples);
}

void BBCMicroSystem::set_audio_sample_rate(int sample_rate_hz) {
    if (psg_) {
        psg_->set_audio_sample_rate(sample_rate_hz);
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
    ImGui::Text("Video Mode: %d", get_display_mode());
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
// Speed Control
// ============================================================================

void BBCMicroSystem::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
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

    // OS ROM (MOS 1.20 — 16 KB) → into unified buffer via os_rom_chip_
    uint8_t* os_rom_data = os_rom_chip_->data();
    const char* os_files[] = {
        "os12.rom",
        "os.rom",
        "OS-1.20.rom",
        "MOS120.rom",
        "bbc_os.rom",
        "os1.2.rom",
        nullptr
    };
    bool os_ok = rom_loader_load_from_root(rom_root, os_files,
                                           bbc_constants::OS_ROM_SIZE,
                                           os_rom_data, bbc_constants::OS_ROM_SIZE);
    if (!os_ok) {
        printf("BBC Micro: OS ROM not found\n");
    }

    // BASIC ROM (BBC BASIC II — 16 KB) → into paged ROM pool slot 15
    uint8_t* basic_rom_data = paged_rom_chip_->data()
                            + 15 * bbc_constants::PAGED_ROM_SIZE;
    const char* basic_files[] = {
        "basic2.rom",
        "BASIC2.rom",
        "basic.rom",
        "bbc_basic.rom",
        "BASIC-2.rom",
        nullptr
    };
    bool basic_ok = rom_loader_load_from_root(rom_root, basic_files,
                                              bbc_constants::PAGED_ROM_SIZE,
                                              basic_rom_data, bbc_constants::PAGED_ROM_SIZE);
    if (!basic_ok) {
        printf("BBC Micro: BASIC ROM not found\n");
    }

    return os_ok;  // System won't boot without OS ROM
}

// ============================================================================
// Chip Registration
// ============================================================================

void BBCMicroSystem::register_chips() {
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 6502A CPU", "6502A", "CPU", 0x0000);
    register_chip(static_cast<ChipBase*>(crtc_),
        "MC6845 CRTC", "MC6845", "Video", bbc_constants::CRTC_BASE);
    register_chip(static_cast<ChipBase*>(psg_),
        "SN76489 PSG", "SN76489", "Sound", 0);
    register_chip(&system_via_,
        "System VIA (6522)", "VIA-S", "I/O", bbc_constants::SYSTEM_VIA_BASE);
    register_chip(&user_via_,
        "User VIA (6522)", "VIA-U", "I/O", bbc_constants::USER_VIA_BASE);
}
