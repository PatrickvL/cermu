/*
 * pce_system.cpp — NEC PC Engine / TurboGrafx-16 system implementation
 *
 * Tick loop:
 *   HuC6280 (65C02 stand-in) runs at 7.16 MHz master clock.
 *   VDC ticks at dot clock rate (configurable via VCE).
 *   Timer decrements every 1024 CPU cycles.
 *   PSG output integrated into HuC6280 (stubbed as audio port writes).
 *
 * Memory model:
 *   The HuC6280 has 8 MPR registers mapping 8KB pages into a 21-bit
 *   physical space.  CPU logical address = (MPR[addr>>13] << 13) | (addr & 0x1FFF).
 *   Page $FF is I/O space, page $F8 is RAM.
 */

#include "core/cermu.hpp"
#include "systems/pce/pce_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<PCEVariant V>
static HardwareTraits create_pce_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = pce_constants::DISPLAY_WIDTH;
    traits.display.native_height   = pce_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = pce_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = pce_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 512;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = pce_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 2;
    traits.audio.chip_name         = "HuC6280 PSG";

    traits.timing.cpu_frequency_hz   = pce_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = pce_constants::TARGET_FPS;
    traits.timing.cycles_per_frame   = pce_constants::CYCLES_PER_FRAME;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor pce_descriptor = {
    "PC Engine", "PCE",
    "NEC PC Engine — HuC6280 + HuC6270 VDC (1987)",
    "pce", {"PC Engine", "PCE", "PC-Engine"},
    nullptr,
    create_pce_hardware_traits<PCEVariant::PCE>(),
    nullptr,
    "NEC / Hudson Soft", 1987, "HuC6280 (65C02)", SystemType::Console
};

static SystemDescriptor tg16_descriptor = {
    "TurboGrafx-16", "TG16",
    "NEC TurboGrafx-16 — HuC6280 + HuC6270 VDC (1989, USA)",
    "pce", {"TurboGrafx-16", "TG16", "TurboGrafx"},
    nullptr,
    create_pce_hardware_traits<PCEVariant::TG16>(),
    nullptr,
    "NEC", 1989, "HuC6280 (65C02)", SystemType::Console
};

static SystemDescriptor sgx_descriptor = {
    "SuperGrafx", "SGX",
    "NEC SuperGrafx — Dual HuC6270 VDC (1989)",
    "pce", {"SuperGrafx", "SGX", "PC Engine SuperGrafx"},
    nullptr,
    create_pce_hardware_traits<PCEVariant::SGX>(),
    nullptr,
    "NEC", 1989, "HuC6280 (65C02)", SystemType::Console
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<PCEVariant V>
PCEngineSystem<V>::PCEngineSystem()
    : System()
    , pins_(PCE_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_pce_hardware_traits<V>();
    // Default MPR: page 0xFF → I/O, page 0xF8 → RAM, rest → ROM
    mpr_[0] = 0xFF;  // $0000: I/O page (reset vector fetch area)
    mpr_[1] = 0xF8;  // $2000: RAM
    for (int i = 2; i < 8; i++) mpr_[i] = 0x00;
}

template<PCEVariant V>
PCEngineSystem<V>::~PCEngineSystem() = default;

template<PCEVariant V>
const SystemDescriptor& PCEngineSystem<V>::get_descriptor() const {
    if constexpr (V == PCEVariant::PCE) return pce_descriptor;
    else if constexpr (V == PCEVariant::TG16) return tg16_descriptor;
    else return sgx_descriptor;
}

template<PCEVariant V>
bool PCEngineSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<PCEVariant V>
bool PCEngineSystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<PCEVariant V>
bool PCEngineSystem<V>::initialize() {
    log_info("PC Engine: Initializing system (%s)\n",
             V == PCEVariant::TG16 ? "TG16" : V == PCEVariant::SGX ? "SGX" : "PCE");
    register_board(&board_);

    bind_all(board_, board_.components_, kPCEManifest);

    port_manifest_       = kPCEManifest.port_slots;
    port_manifest_count_ = kPCEManifest.port_count;

    pins_ = board_.cpu.init();
    board_.vdc.reset();
    board_.vce.reset();

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(pce_constants::CPU_FREQ_HZ / 6,
                           pce_constants::DEFAULT_SAMPLE_RATE);

    system_ready_ = true;
    log_info("PC Engine: System initialized\n");
    return true;
}

template<PCEVariant V>
void PCEngineSystem<V>::shutdown() {
    system_ready_ = false;
}

template<PCEVariant V>
void PCEngineSystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.cpu.reset(pins_);

    // Reset MPR to default bank mapping
    mpr_[0] = 0xFF;  // I/O
    mpr_[1] = 0xF8;  // RAM
    for (int i = 2; i < 8; i++) mpr_[i] = 0x00;

    timer_reload_ = 0;
    timer_value_ = 0;
    timer_enable_ = false;
    timer_counter_ = 0;
    irq_disable_ = 0;
    irq_pending_ = 0;
    joypad_state_ = 0xFF;
    joypad_sel_ = 0;
    joypad_clr_ = 0;
    frame_counter_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<PCEVariant V>
void PCEngineSystem<V>::tick() {
    // VDC tick (dot clock)
    bus_state_t vdc_bus = 0;
    vdc_bus = board_.vdc.tick(vdc_bus);

    // Bridge VDC IRQ → CPU
    if (board_.vdc.irq_pending())
        irq_pending_ |= IRQ1_BIT;
    else
        irq_pending_ &= ~IRQ1_BIT;

    // CPU tick — HuC6280 is a WDC 65C02 derivative
    pins_ = board_.cpu.template tick<HUDSON_HUC6280::Phase::PHI2>(pins_);

    // Memory / I/O dispatch (65C02 always performs a bus cycle on PHI2)
    {
        uint16_t addr = BUS_GET_ADDR(pins_) & 0xFFFF;
        uint32_t phys = map_address(addr);
        bool is_read = BUS_GET_BIT(pins_, BUS_RW_BIT);

        if (mpr_[addr >> 13] == 0xFF) {
            // I/O page
            uint16_t io_addr = addr & 0x1FFF;
            if (is_read)
                BUS_SET_DATA(pins_, io_read(io_addr));
            else
                io_write(io_addr, BUS_GET_DATA(pins_));
        } else if (mpr_[addr >> 13] == 0xF8) {
            // RAM page
            uint16_t ram_addr = addr & 0x1FFF;
            uint8_t* ram = board_.ram.data();
            if (is_read)
                BUS_SET_DATA(pins_, ram[ram_addr]);
            else
                ram[ram_addr] = BUS_GET_DATA(pins_);
        } else {
            // ROM
            uint8_t* rom = board_.rom.data();
            if (rom && is_read) {
                BUS_SET_DATA(pins_, rom[phys & (pce_constants::MAX_ROM_SIZE - 1)]);
            }
        }
    }

    pins_ = board_.cpu.template tick<HUDSON_HUC6280::Phase::PHI1>(pins_);

    // IRQ delivery to CPU (active-low on 65C02 IRQ line)
    uint8_t active_irqs = irq_pending_ & ~irq_disable_;
    if (active_irqs)
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    else
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);

    // Timer prescaler (fires every 1024 CPU cycles)
    if (timer_enable_) {
        timer_counter_++;
        if (timer_counter_ >= 1024) {
            timer_counter_ = 0;
            if (timer_value_ == 0) {
                timer_value_ = timer_reload_;
                irq_pending_ |= TIMER_BIT;
            } else {
                timer_value_--;
            }
        }
    }

    frame_counter_++;
    total_cycles_++;
}

template<PCEVariant V>
void PCEngineSystem<V>::run_frame() {
    uint32_t start = total_cycles_;
    uint32_t cpf = pce_constants::CYCLES_PER_FRAME;
    while (total_cycles_ - start < cpf) {
        tick();
    }
}

// ============================================================================
// ADDRESS MAPPING
// ============================================================================

template<PCEVariant V>
uint32_t PCEngineSystem<V>::map_address(uint16_t logical) const noexcept {
    uint8_t page = mpr_[logical >> 13];
    return (static_cast<uint32_t>(page) << 13) | (logical & 0x1FFF);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<PCEVariant V>
uint8_t PCEngineSystem<V>::io_read(uint16_t addr) noexcept {
    uint16_t port = addr & 0x1C00;

    if (port == pce_constants::IO_VDC) {
        bus_state_t vdc_bus = 0;
        BUS_SET_ADDR(vdc_bus, addr & 0x03);
        BUS_SET_BIT(vdc_bus, BUS_RW_BIT);
        vdc_bus = board_.vdc.on_bus_read(vdc_bus);
        return BUS_GET_DATA(vdc_bus);
    }
    if (port == pce_constants::IO_VCE) {
        bus_state_t vce_bus = 0;
        BUS_SET_ADDR(vce_bus, addr & 0x07);
        BUS_SET_BIT(vce_bus, BUS_RW_BIT);
        vce_bus = board_.vce.on_bus_read(vce_bus);
        return BUS_GET_DATA(vce_bus);
    }
    if (port == pce_constants::IO_TIMER) {
        return timer_value_ & 0x7F;
    }
    if (port == pce_constants::IO_JOYPAD) {
        // 4-way mux: sel=0 → dpad, sel=1 → buttons
        if (joypad_sel_)
            return (joypad_state_ >> 4) & 0x0F;
        else
            return joypad_state_ & 0x0F;
    }
    if (port == pce_constants::IO_IRQ) {
        uint8_t reg = addr & 0x03;
        if (reg == 2) return irq_disable_;
        if (reg == 3) return irq_pending_;
    }
    return 0xFF;
}

template<PCEVariant V>
void PCEngineSystem<V>::io_write(uint16_t addr, uint8_t data) noexcept {
    uint16_t port = addr & 0x1C00;

    if (port == pce_constants::IO_VDC) {
        bus_state_t vdc_bus = 0;
        BUS_SET_ADDR(vdc_bus, addr & 0x03);
        BUS_SET_DATA(vdc_bus, data);
        board_.vdc.on_bus_write(vdc_bus);
        return;
    }
    if (port == pce_constants::IO_VCE) {
        bus_state_t vce_bus = 0;
        BUS_SET_ADDR(vce_bus, addr & 0x07);
        BUS_SET_DATA(vce_bus, data);
        board_.vce.on_bus_write(vce_bus);
        return;
    }
    if (port == pce_constants::IO_PSG) {
        // TODO: Route to HuC6280 PSG channels
        return;
    }
    if (port == pce_constants::IO_TIMER) {
        uint8_t reg = addr & 0x01;
        if (reg == 0) {
            timer_reload_ = data & 0x7F;
        } else {
            bool was_enabled = timer_enable_;
            timer_enable_ = (data & 0x01) != 0;
            if (!was_enabled && timer_enable_) {
                timer_value_ = timer_reload_;
                timer_counter_ = 0;
            }
        }
        return;
    }
    if (port == pce_constants::IO_JOYPAD) {
        joypad_sel_ = data & 0x01;
        joypad_clr_ = (data >> 1) & 0x01;
        return;
    }
    if (port == pce_constants::IO_IRQ) {
        uint8_t reg = addr & 0x03;
        if (reg == 2)
            irq_disable_ = data & 0x07;
        else if (reg == 3)
            irq_pending_ &= ~TIMER_BIT;  // Timer IRQ acknowledge
        return;
    }
}

// ============================================================================
// FILE / AUDIO / INPUT
// ============================================================================

template<PCEVariant V>
bool PCEngineSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* rom = board_.rom.data();
    if (!rom) return false;

    // PC Engine HuCard ROMs: .pce, 256KB–1MB
    // Some have a 512-byte header (check size % 8192 != 0)
    if (!load_raw_rom_mirrored(filepath, rom, pce_constants::MAX_ROM_SIZE, 0,
                               get_descriptor().name, program_title_))
        return false;

    // Set initial MPR for ROM banking (pages 0-0x7F)
    mpr_[0] = 0xFF;  // I/O
    mpr_[1] = 0xF8;  // RAM
    mpr_[2] = 0x00;  // ROM bank 0
    mpr_[3] = 0x01;  // ROM bank 1
    mpr_[4] = 0x02;  // ROM bank 2
    mpr_[5] = 0x03;  // ROM bank 3
    mpr_[6] = 0x04;  // ROM bank 4
    mpr_[7] = 0x00;  // Reset vector page (bank 0)

    reset();
    return true;
}

template<PCEVariant V>
uint32_t PCEngineSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<PCEVariant V>
void PCEngineSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
}

template<PCEVariant V>
void PCEngineSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // PC Engine pad: Up, Down, Left, Right, I, II, Select, Run
    // joypad_state_: bits 0-3=dpad (UDLR), bits 4-7=buttons (I,II,Sel,Run)
    auto set_bit = [](uint8_t& reg, int bit, bool p) {
        if (p) reg &= ~(1 << bit); else reg |= (1 << bit);
    };

    switch (key) {
        case SDLK_UP:     set_bit(joypad_state_, 0, pressed); break;
        case SDLK_DOWN:   set_bit(joypad_state_, 1, pressed); break;
        case SDLK_LEFT:   set_bit(joypad_state_, 2, pressed); break;
        case SDLK_RIGHT:  set_bit(joypad_state_, 3, pressed); break;
        case SDLK_z:      set_bit(joypad_state_, 4, pressed); break;  // I
        case SDLK_x:      set_bit(joypad_state_, 5, pressed); break;  // II
        case SDLK_BACKSPACE: set_bit(joypad_state_, 6, pressed); break;  // Select
        case SDLK_RETURN: set_bit(joypad_state_, 7, pressed); break;  // Run
        default: break;
    }
}

template<PCEVariant V>
bool PCEngineSystem<V>::load_roms() {
    return true;  // PC Engine has no system ROM (HuCard-only)
}

// ============================================================================
// TEMPLATE INSTANTIATION
// ============================================================================

template class PCEngineSystem<PCEVariant::PCE>;
template class PCEngineSystem<PCEVariant::TG16>;
template class PCEngineSystem<PCEVariant::SGX>;

REGISTER_SYSTEM(pce_descriptor, [] {
    return std::make_unique<PCEngineSystem<PCEVariant::PCE>>();
});

REGISTER_SYSTEM(tg16_descriptor, [] {
    return std::make_unique<PCEngineSystem<PCEVariant::TG16>>();
});

REGISTER_SYSTEM(sgx_descriptor, [] {
    return std::make_unique<PCEngineSystem<PCEVariant::SGX>>();
});
