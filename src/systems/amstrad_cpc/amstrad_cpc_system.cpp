/*
 * amstrad_cpc_system.cpp — Amstrad CPC 464/664/6128 system implementation
 *
 * Tick loop:
 *   Z80A @ 4 MHz.  Gate Array generates interrupts every 52 HSYNCs.
 *   MC6845 drives display timing; Gate Array translates CRTC addresses to
 *   screen memory + mode/color decoding.  AY-3-8912 driven via PPI port.
 */

#include "amstrad_cpc_system.h"
#include "../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<CPCModel M>
static HardwareTraits create_cpc_hardware_traits() {
    using Traits = CPCModelTraits<M>;
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
// IMPLEMENTATION (stub — follows Spectrum pattern)
// ============================================================================

template<CPCModel M>
AmstradCPCSystem<M>::AmstradCPCSystem()
    : EmulatedSystem()
    , ay_(AYVariant::AY_3_8912)
    , pins_(CPC_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_cpc_hardware_traits<M>();
}

template<CPCModel M>
AmstradCPCSystem<M>::~AmstradCPCSystem() { delete cpu_; }

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

template<CPCModel M>
bool AmstradCPCSystem<M>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();
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
    gate_array_.reset();
    ram_.resize(Traits::ram_size_kb * 1024, 0x00);
    lower_rom_.resize(amstrad_cpc_constants::ROM_SIZE, 0xFF);
    upper_rom_.resize(amstrad_cpc_constants::ROM_SIZE, 0xFF);
    load_roms();
    system_ready_ = true;
    return true;
}

template<CPCModel M> void AmstradCPCSystem<M>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<CPCModel M> void AmstradCPCSystem<M>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    crtc_.init();
    ppi_.init();
    ay_.reset();
    gate_array_.reset();
}

template<CPCModel M>
void AmstradCPCSystem<M>::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = mem_tick(pins_);
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

template<CPCModel M> void AmstradCPCSystem<M>::run_frame() {
    for (uint32_t i = 0; i < amstrad_cpc_constants::TSTATES_PER_FRAME; ++i) tick();
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

template<CPCModel M>
bus_state_t AmstradCPCSystem<M>::mem_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // CPC6128 RAM banking — map address to physical RAM offset
    auto ram_offset = [&](uint16_t a) -> uint32_t {
        if constexpr (Traits::ram_size_kb == 128) {
            // 8 banking configurations: each maps 4 × 16KB pages to 8 × 16KB banks
            static constexpr uint8_t bank_table[8][4] = {
                {0, 1, 2, 3}, {0, 1, 2, 7}, {4, 5, 6, 7}, {0, 3, 2, 7},
                {0, 4, 2, 3}, {0, 5, 2, 3}, {0, 6, 2, 3}, {0, 7, 2, 3}
            };
            int page = a >> 14;
            int bank = bank_table[gate_array_.ram_config & 7][page];
            return static_cast<uint32_t>(bank) * 0x4000 + (a & 0x3FFF);
        } else {
            return a;
        }
    };

    if (is_read) {
        uint8_t data;
        if (addr < 0x4000) {
            // Lower ROM (BIOS) / RAM — ROM overlays RAM when enabled
            data = gate_array_.lower_rom_enabled ? lower_rom_[addr] : ram_[ram_offset(addr)];
        } else if (addr >= 0xC000) {
            // Upper ROM (BASIC) / RAM
            data = gate_array_.upper_rom_enabled ? upper_rom_[addr - 0xC000] : ram_[ram_offset(addr)];
        } else {
            data = ram_[ram_offset(addr)];
        }
        BUS_SET_DATA(pins, data);
    } else {
        // Writes always go to RAM (ROMs are read-only overlays)
        uint8_t data = BUS_GET_DATA(pins);
        ram_[ram_offset(addr)] = data;
    }

    return pins;
}
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

    // Gate Array (active when A15=0) — write-only
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
                break;
            case 3:  // RAM banking (CPC6128 only)
                if constexpr (Traits::ram_size_kb == 128) {
                    gate_array_.ram_config = data & 0x3F;
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
template<CPCModel M> bool AmstradCPCSystem<M>::load_roms() { return false; }

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
