/*
 * amstrad_cpc_system.cpp — Amstrad CPC 464/664/6128 system implementation
 *
 * Tick loop:
 *   Z80A @ 4 MHz.  Gate Array generates interrupts every 52 HSYNCs.
 *   MC6845 drives display timing; Gate Array translates CRTC addresses to
 *   screen memory + mode/color decoding.  AY-3-8912 driven via PPI port.
 */

#include "systems/amstrad_cpc/amstrad_cpc_system.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<CPCModel M>
static HardwareTraits create_cpc_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = amstrad_cpc_constants::FB_WIDTH;
    traits.display.native_height   = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.visible_width   = amstrad_cpc_constants::FB_WIDTH;
    traits.display.visible_height  = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = amstrad_cpc_constants::GA_COLOR_COUNT;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8912";

    traits.timing.cpu_frequency_hz   = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = amstrad_cpc_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor cpc464_descriptor = {
    "Amstrad CPC 464", "CPC464",
    "Amstrad CPC 464 — Z80A @ 4MHz, 64KB RAM, integrated tape (1984)",
    "amstrad_cpc", {"CPC464", "CPC", "AmstradCPC"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC464>(), nullptr
};

static SystemDescriptor cpc664_descriptor = {
    "Amstrad CPC 664", "CPC664",
    "Amstrad CPC 664 — Z80A @ 4MHz, 64KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC664"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC664>(), nullptr
};

static SystemDescriptor cpc6128_descriptor = {
    "Amstrad CPC 6128", "CPC6128",
    "Amstrad CPC 6128 — Z80A @ 4MHz, 128KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC6128"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC6128>(), nullptr
};

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

template<CPCModel M>
AmstradCPCSystem<M>::AmstradCPCSystem()
    : System()
    , ay_(AYVariant::AY_3_8912)
    , pins_(CPC_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_cpc_hardware_traits<M>();
}

template<CPCModel M>
AmstradCPCSystem<M>::~AmstradCPCSystem() {}

template<CPCModel M>
const SystemDescriptor& AmstradCPCSystem<M>::get_descriptor() const {
    if constexpr (M == CPCModel::CPC464) return cpc464_descriptor;
    else if constexpr (M == CPCModel::CPC664) return cpc664_descriptor;
    else return cpc6128_descriptor;
}

template<CPCModel M>
bool AmstradCPCSystem<M>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<CPCModel M>
bool AmstradCPCSystem<M>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<CPCModel M>
bool AmstradCPCSystem<M>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Pre-bind stack-member chips, then factory-create all chips ─────
    board_.bind_chip(cpc_chips::kCrtcSlot, &crtc_);
    board_.bind_chip(cpc_chips::kPpiSlot,  &ppi_);
    board_.bind_chip(cpc_chips::kAySlot,   &ay_);
    board_.bind_chip(cpc_chips::kGaSlot,   &gate_array_);
    board_.create_chips(&pins_);

    // ── Configure page tables for this variant ──────────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = board_.template cpu<ZilogZ80A>();
    pins_ = board_.cpu_chip()->init();
    crtc_.init();
    // Gate array drives interrupts from CRTC HSYNC (every 52 HSYNCs)
    crtc_.on_hsync = [this]() {
        gate_array_.interrupt_counter++;
        if (gate_array_.interrupt_counter >= 52) {
            gate_array_.interrupt_counter = 0;
            gate_array_.interrupt_pending = true;
        }
    };
    ppi_.init();
    ay_.init();

    load_roms();
    // ── Cache RAM chip pointer for rendering ───────────────────────
    ram_chip_ = board_.template chip_as<RAMChip>(cpc_chips::kRamSlot);

    // ── GPU indexed palette rendering ───────────────────────────────
    pixel_.set_framebuffer(framebuffer_,
                           amstrad_cpc_constants::FB_WIDTH,
                           amstrad_cpc_constants::FB_HEIGHT);
    register_gpu_palette(&pixel_,
                         amstrad_cpc_constants::HARDWARE_PALETTE,
                         amstrad_cpc_constants::GA_INK_VALUES);
    // ── Register all manifest chips for Hardware menu ────────────────
    register_bus_chips(board_);

    printf("%s: System initialized (%dKB RAM)\n", Traits::name, Traits::ram_size_kb);
    system_ready_ = true;
    return true;
}

template<CPCModel M>
void AmstradCPCSystem<M>::shutdown() { cpu_ = nullptr; system_ready_ = false; }

template<CPCModel M>
void AmstradCPCSystem<M>::reset() {
    if (!cpu_) return;
    // Reset all manifest chips (CRTC, PPI, AY, Gate Array; RAM/ROM are no-op)
    board_.reset_chips();
    pins_ = board_.cpu_chip()->reset(pins_);
    configure_bus_memory_map();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<CPCModel M>
void AmstradCPCSystem<M>::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // Gate array interrupt: IRQ is level-sensitive, keep asserted while pending
    if (gate_array_.interrupt_pending) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    }

    // CRTC + AY tick at 1 MHz (CPU clock / 4)
    if ((total_cycles_ & 3) == 0) {
        crtc_.tick();
        ay_.tick();
    }

    total_cycles_++;
}

template<CPCModel M>
void AmstradCPCSystem<M>::run_frame() {
    for (uint32_t i = 0; i < amstrad_cpc_constants::TSTATES_PER_FRAME; ++i) tick();
    render_frame();
}

template<CPCModel M> bool AmstradCPCSystem<M>::load_file(const char*) { return false; }
template<CPCModel M> uint32_t* AmstradCPCSystem<M>::get_framebuffer() { return framebuffer_; }
template<CPCModel M> void AmstradCPCSystem<M>::get_display_dimensions(int* w, int* h) const {
    *w = amstrad_cpc_constants::FB_WIDTH; *h = amstrad_cpc_constants::FB_HEIGHT;
}
template<CPCModel M> void AmstradCPCSystem<M>::set_framebuffer(uint32_t*, int, int) {}
template<CPCModel M> uint32_t AmstradCPCSystem<M>::get_audio_samples(float*, uint32_t) { return 0; }
template<CPCModel M> void AmstradCPCSystem<M>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<CPCModel M> void AmstradCPCSystem<M>::handle_keyboard_event(SDL_Keycode, bool) {}
template<CPCModel M> void AmstradCPCSystem<M>::render_system_menu_items() {}
template<CPCModel M> void AmstradCPCSystem<M>::render_configuration_ui() {}
template<CPCModel M> void AmstradCPCSystem<M>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// VIDEO RENDERING — decode screen RAM into indexed framebuffer
// ============================================================================
//
// CPC screen memory layout (standard 320×200 mode 1):
//   80 bytes per line, 200 visible lines, interleaved in 2 KB blocks:
//     Lines 0,8,16,24... at offset 0
//     Lines 1,9,17,25... at offset 2048
//     Lines 2,10,18...   at offset 4096
//     etc. (8 blocks of 2 KB = 16 KB total)
//
//   Address for pixel line y, byte x:
//     addr = screen_base + (y / 8) * 80 + x + (y % 8) * 2048
//
//   Screen base derived from CRTC R12:R13 (typically $C000 = R12=0x30, R13=0x00)
//
// Pixel decoding by mode (all modes use interleaved bit planes within each byte):
//   Mode 0: 2 pixels/byte (4bpp), bits: px0={b7,b5,b3,b1}, px1={b6,b4,b2,b0}
//   Mode 1: 4 pixels/byte (2bpp), bits: px0={b7,b3}, px1={b6,b2}, px2={b5,b1}, px3={b4,b0}
//   Mode 2: 8 pixels/byte (1bpp), bits: px0=b7, px1=b6, ..., px7=b0
//
// Color: pixel value → pen index → gate_array_.ink[pen] → hardware color (0-31)
// Framebuffer: 640×400 (mode 2 resolution, each native line drawn twice)

template<CPCModel M>
void AmstradCPCSystem<M>::render_frame() {
    if (!ram_chip_) return;

    const uint8_t* ram = ram_chip_->data();
    const uint8_t* ink = gate_array_.ink;
    int mode = gate_array_.screen_mode;

    // Screen base from CRTC display start address (R12:R13)
    uint16_t crtc_start = (crtc_.regs_[MC6845_R12_START_ADDR_HI] << 8)
                        | crtc_.regs_[MC6845_R13_START_ADDR_LO];
    // Gate Array maps CRTC address bits [13:12] → 16 KB bank
    uint32_t screen_base = (crtc_start & 0x3000) << 2;  // bits 13:12 → address bits 15:14

    constexpr int FB_W = amstrad_cpc_constants::FB_WIDTH;

    std::memset(frame_indices_, 0, sizeof(frame_indices_));

    for (int y = 0; y < 200; y++) {
        // Interleaved address: scan lines within character row spaced 2048 bytes apart
        uint32_t line_base = screen_base + (y / 8) * 80 + (y % 8) * 2048;
        uint8_t* dst0 = frame_indices_ + (y * 2) * FB_W;
        uint8_t* dst1 = frame_indices_ + (y * 2 + 1) * FB_W;

        for (int x_byte = 0; x_byte < 80; x_byte++) {
            uint8_t byte = ram[(line_base + x_byte) & 0xFFFF];
            int fb_x;

            switch (mode) {
                case 0: {
                    // Mode 0: 2 pixels per byte, 16 colors
                    // pixel 0: bits {7,5,3,1} → value 0-15, pixel 1: bits {6,4,2,0}
                    uint8_t px0 = ((byte >> 7) & 1) | (((byte >> 5) & 1) << 1)
                                | (((byte >> 3) & 1) << 2) | (((byte >> 1) & 1) << 3);
                    uint8_t px1 = ((byte >> 6) & 1) | (((byte >> 4) & 1) << 1)
                                | (((byte >> 2) & 1) << 2) | (((byte >> 0) & 1) << 3);
                    uint8_t c0 = ink[px0 & 0x0F] & 0x1F;
                    uint8_t c1 = ink[px1 & 0x0F] & 0x1F;
                    // Each mode 0 pixel = 4 framebuffer pixels wide
                    fb_x = x_byte * 8;
                    dst0[fb_x] = dst0[fb_x+1] = dst0[fb_x+2] = dst0[fb_x+3] = c0;
                    dst0[fb_x+4] = dst0[fb_x+5] = dst0[fb_x+6] = dst0[fb_x+7] = c1;
                    dst1[fb_x] = dst1[fb_x+1] = dst1[fb_x+2] = dst1[fb_x+3] = c0;
                    dst1[fb_x+4] = dst1[fb_x+5] = dst1[fb_x+6] = dst1[fb_x+7] = c1;
                    break;
                }
                case 1: {
                    // Mode 1: 4 pixels per byte, 4 colors
                    // px0={b7,b3}, px1={b6,b2}, px2={b5,b1}, px3={b4,b0}
                    uint8_t px0 = ((byte >> 7) & 1) | (((byte >> 3) & 1) << 1);
                    uint8_t px1 = ((byte >> 6) & 1) | (((byte >> 2) & 1) << 1);
                    uint8_t px2 = ((byte >> 5) & 1) | (((byte >> 1) & 1) << 1);
                    uint8_t px3 = ((byte >> 4) & 1) | (((byte >> 0) & 1) << 1);
                    uint8_t c0 = ink[px0] & 0x1F;
                    uint8_t c1 = ink[px1] & 0x1F;
                    uint8_t c2 = ink[px2] & 0x1F;
                    uint8_t c3 = ink[px3] & 0x1F;
                    // Each mode 1 pixel = 2 framebuffer pixels wide
                    fb_x = x_byte * 8;
                    dst0[fb_x] = dst0[fb_x+1] = c0;
                    dst0[fb_x+2] = dst0[fb_x+3] = c1;
                    dst0[fb_x+4] = dst0[fb_x+5] = c2;
                    dst0[fb_x+6] = dst0[fb_x+7] = c3;
                    dst1[fb_x] = dst1[fb_x+1] = c0;
                    dst1[fb_x+2] = dst1[fb_x+3] = c1;
                    dst1[fb_x+4] = dst1[fb_x+5] = c2;
                    dst1[fb_x+6] = dst1[fb_x+7] = c3;
                    break;
                }
                default:
                case 2: {
                    // Mode 2: 8 pixels per byte, 2 colors
                    fb_x = x_byte * 8;
                    for (int bit = 7; bit >= 0; --bit) {
                        uint8_t px = (byte >> bit) & 1;
                        uint8_t c = ink[px] & 0x1F;
                        dst0[fb_x] = c;
                        dst1[fb_x] = c;
                        fb_x++;
                    }
                    break;
                }
            }
        }
    }

    pixel_.flush_indexed_frame(frame_indices_, amstrad_cpc_constants::HARDWARE_PALETTE);
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<CPCModel M>
void AmstradCPCSystem<M>::configure_bus_memory_map() {
    // apply() maps base-layer chips (RAM) and skips overlay_group > 0 (ROMs).
    board_.apply(bus_);

    if constexpr (Traits::ram_size_kb == 128) {
        // 6128: remap RAM banks per current gate_array_.ram_config.
        // Default config 0 = {0,1,2,3} — identity, matches apply() output.
        update_banking();
    }

    // Build overlay snapshots from the manifest's overlay_group tags.
    // 4 modes: {none, lower, upper, both} — derived from groups 1+2.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);

    // Apply current ROM overlay state.
    apply_rom_overlay();
}

// ── RAM banking (CPC 6128 only) + rebuild overlay snapshots ──────────────
// Called when gate_array_.ram_config changes (Gate Array opcode 11xxxxxx).
// Remaps RAM bank assignments, rebuilds overlay snapshots for the new base
// state, then re-applies the current ROM overlay.
template<CPCModel M>
void AmstradCPCSystem<M>::update_banking() {
    if constexpr (Traits::ram_size_kb == 128) {
        // CPC 6128: 8 banking configurations mapping 4 logical pages to 8 physical banks
        static constexpr uint8_t bank_table[8][4] = {
            {0, 1, 2, 3}, {0, 1, 2, 7}, {4, 5, 6, 7}, {0, 3, 2, 7},
            {0, 4, 2, 3}, {0, 5, 2, 3}, {0, 6, 2, 3}, {0, 7, 2, 3}
        };
        uint8_t config = gate_array_.ram_config & 7;
        constexpr size_t kPagesPerBank = 64;  // 16384 / 256
        for (int pg = 0; pg < 4; ++pg) {
            board_.select_bank_at(bus_, 0, cpc_chips::kRamSlot,
                                   bank_table[config][pg],
                                   pg * kPagesPerBank);
        }
    }

    // Rebuild overlay snapshots for the new RAM base state, then apply.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);
    apply_rom_overlay();
}

// ── ROM overlay snapshot selection ───────────────────────────────────────
// Loads the pre-computed snapshot for the current ROM enable state.
// Mode bits: 0 = lower ROM (group 1), 1 = upper ROM (group 2).
template<CPCModel M>
void AmstradCPCSystem<M>::apply_rom_overlay() {
    size_t mode = (gate_array_.lower_rom_enabled ? 1 : 0)
               |  (gate_array_.upper_rom_enabled ? 2 : 0);
    bus_.load_snapshot(0, snapshots_[0][mode]);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<CPCModel M>
bus_state_t AmstradCPCSystem<M>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1 asserted simultaneously
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        gate_array_.interrupt_pending = false;
        BUS_SET_BIT(pins, BUS_IRQ_BIT);  // Deassert INT
        BUS_SET_DATA(pins, 0xFF);
        return pins;
    }

    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // CPC uses partial address-line decoding for I/O

    // Gate Array (active when A15=0) -- write-only
    if (!is_read && !(addr & 0x8000)) {
        switch (data >> 6) {
            case 0:  // Pen select
                gate_array_.pen_select = data & 0x1F;
                break;
            case 1:  // Set color for current pen
                if (gate_array_.pen_select < amstrad_cpc_constants::GA_PEN_COUNT)
                    gate_array_.ink[gate_array_.pen_select] = data & 0x1F;
                break;
            case 2:  // Screen mode + ROM control + interrupt reset
                gate_array_.screen_mode = data & 0x03;
                gate_array_.lower_rom_enabled = !(data & 0x04);
                gate_array_.upper_rom_enabled = !(data & 0x08);
                if (data & 0x10) {
                    gate_array_.interrupt_counter = 0;
                    gate_array_.interrupt_pending = false;
                    BUS_SET_BIT(pins, BUS_IRQ_BIT);
                }
                apply_rom_overlay();  // ROM visibility changed
                break;
            case 3:  // RAM banking (CPC6128 only)
                if constexpr (Traits::ram_size_kb == 128) {
                    gate_array_.ram_config = data & 0x3F;
                    update_banking();  // RAM bank configuration changed → rebuild + apply
                }
                break;
        }
    }

    // CRTC 6845 (active when A14=0)
    if (!(addr & 0x4000)) {
        // A9:A8 selects function: 00=reg select, 01=data write, 10=status read, 11=data read
        uint8_t crtc_func = (addr >> 8) & 0x03;
        if (is_read) {
            if (crtc_func >= 2) {
                BUS_SET_DATA(pins, crtc_.read(crtc_func & 1));
            }
        } else {
            if (crtc_func < 2) {
                crtc_.write(crtc_func & 1, data);
            }
        }
    }

    // PPI 8255 (active when A11=0)
    if (!(addr & 0x0800)) {
        uint8_t ppi_reg = addr & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, ppi_.read(ppi_reg));
        } else {
            ppi_.write(ppi_reg, data);
            // AY-3-8912 is controlled via PPI Port C bits 7:6 (BDIR/BC1)
            // and Port A carries the data bus
            uint8_t port_c = ppi_.get_port_c_output();
            bool bdir = (port_c >> 7) & 1;
            bool bc1  = (port_c >> 6) & 1;
            if (bdir && bc1) {
                ay_.latch_address(ppi_.get_port_a_output());
            } else if (bdir && !bc1) {
                ay_.write_register(ppi_.get_port_a_output());
            } else if (!bdir && bc1) {
                ppi_.set_port_a_input(ay_.read_register());
            }
        }
    }

    return pins;
}

template<CPCModel M>
bool AmstradCPCSystem<M>::load_roms() { return false; }

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class AmstradCPCSystem<CPCModel::CPC464>;
template class AmstradCPCSystem<CPCModel::CPC664>;
template class AmstradCPCSystem<CPCModel::CPC6128>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(cpc464_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC464>>(); });
REGISTER_SYSTEM(cpc664_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC664>>(); });
REGISTER_SYSTEM(cpc6128_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC6128>>(); });
