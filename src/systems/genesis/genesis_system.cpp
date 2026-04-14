/*
 * genesis_system.cpp — Sega Genesis / Mega Drive system implementation
 *
 * Tick loop:
 *   M68000 @ 7.67 MHz drives the main bus.
 *   Z80 @ 3.58 MHz handles FM sound (Z80 runs ~7 cycles per 15 M68K cycles).
 *   VDP renders at 3× dot clock rate relative to M68K.
 *   YM2612 FM synthesis runs at M68K / 7 internal rate.
 *   SN76489 PSG clocked at Z80 / 16.
 *
 * Address decoding (68000, 24-bit):
 *   $000000–$3FFFFF : Cart ROM
 *   $A00000–$A0FFFF : Z80 RAM (with bus arbitration)
 *   $A10000–$A1001F : I/O area
 *   $A11100          : Z80 bus request
 *   $A11200          : Z80 reset
 *   $C00000–$C0001F  : VDP
 *   $FF0000–$FFFFFF  : 68K work RAM (64KB)
 */

#include "core/cermu.hpp"
#include "systems/genesis/genesis_system.hpp"
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

template<GenesisVariant V>
static HardwareTraits create_genesis_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = genesis_constants::DISPLAY_WIDTH;
    traits.display.native_height   = genesis_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = genesis_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = genesis_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 64;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = genesis_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 2;
    traits.audio.chip_name         = "YM2612 + SN76489";

    traits.timing.cpu_frequency_hz   = genesis_constants::M68K_FREQ_NTSC;
    traits.timing.target_fps         = genesis_constants::TARGET_FPS_NTSC;
    traits.timing.cycles_per_frame   = genesis_constants::CYCLES_PER_FRAME_NTSC;
    traits.timing.standard           = (V == GenesisVariant::MEGADRIVE)
                                        ? VideoStandard::PAL : VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor genesis_descriptor = {
    "Sega Genesis", "Genesis",
    "Sega Genesis — MC68000 + Z80, YM2612, VDP (1988)",
    "genesis", {"Genesis", "Sega Genesis", "Mega Drive"},
    nullptr,
    create_genesis_hardware_traits<GenesisVariant::GENESIS>(),
    nullptr,
    "Sega", 1988, "MC68000 + Z80", SystemType::Console
};

static SystemDescriptor megadrive_descriptor = {
    "Sega Mega Drive", "MegaDrive",
    "Sega Mega Drive — MC68000 + Z80, YM2612, VDP (1988, PAL)",
    "genesis", {"MegaDrive", "Mega Drive", "MD"},
    nullptr,
    create_genesis_hardware_traits<GenesisVariant::MEGADRIVE>(),
    nullptr,
    "Sega", 1988, "MC68000 + Z80", SystemType::Console
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<GenesisVariant V>
GenesisSystem<V>::GenesisSystem()
    : System()
    , m68k_pins_(GENESIS_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_genesis_hardware_traits<V>();
    std::memset(io_data_, 0x7F, sizeof(io_data_));
    std::memset(io_ctrl_, 0, sizeof(io_ctrl_));
}

template<GenesisVariant V>
GenesisSystem<V>::~GenesisSystem() = default;

template<GenesisVariant V>
const SystemDescriptor& GenesisSystem<V>::get_descriptor() const {
    if constexpr (V == GenesisVariant::GENESIS) return genesis_descriptor;
    else return megadrive_descriptor;
}

template<GenesisVariant V>
bool GenesisSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<GenesisVariant V>
bool GenesisSystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<GenesisVariant V>
bool GenesisSystem<V>::initialize() {
    log_info("Genesis: Initializing system (%s)\n",
             V == GenesisVariant::GENESIS ? "NTSC" : "PAL");
    register_board(&board_);

    bind_all(board_, board_.components_, kGenesisManifest);

    port_manifest_       = kGenesisManifest.port_slots;
    port_manifest_count_ = kGenesisManifest.port_count;

    m68k_pins_ = board_.m68k.init();
    z80_pins_  = board_.z80.init();
    board_.vdp.reset();
    board_.fm.reset();
    board_.psg.init();

    board_.psg.set_clock_frequency(genesis_constants::Z80_FREQ / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(genesis_constants::DEFAULT_SAMPLE_RATE,
                           genesis_constants::DEFAULT_SAMPLE_RATE);
    board_.fm.set_audio_port(audio_port_.get());

    system_ready_ = true;
    log_info("Genesis: System initialized\n");
    return true;
}

template<GenesisVariant V>
void GenesisSystem<V>::shutdown() {
    system_ready_ = false;
}

template<GenesisVariant V>
void GenesisSystem<V>::reset() {
    board_.reset_chips();
    m68k_pins_ = board_.m68k.reset(m68k_pins_);
    z80_pins_  = board_.z80.reset(z80_pins_);
    z80_bus_granted_ = false;
    z80_reset_ = true;
    frame_counter_ = 0;
    std::memset(io_data_, 0x7F, sizeof(io_data_));
    std::memset(io_ctrl_, 0, sizeof(io_ctrl_));
}

// ============================================================================
// EXECUTION
// ============================================================================

template<GenesisVariant V>
void GenesisSystem<V>::tick() {
    // M68K tick
    m68k_pins_ = board_.m68k.tick(m68k_pins_);
    m68k_pins_ = m68k_bus_dispatch(m68k_pins_);

    // Z80 tick — run ~7 Z80 cycles per 15 M68K cycles (approximate)
    if (!z80_reset_ && !z80_bus_granted_) {
        if ((frame_counter_ & 1) == 0) {
            z80_pins_ = board_.z80.tick(z80_pins_);
        }
    }

    // VDP dot clock (7× M68K for H40, but approximated here)
    bus_state_t vdp_bus = 0;
    board_.vdp.tick(vdp_bus);

    // FM synthesis tick (M68K / 6)
    if ((frame_counter_ % 6) == 0) {
        bus_state_t fm_bus = 0;
        board_.fm.tick(fm_bus);
    }

    // PSG tick
    if ((frame_counter_ & 0x0F) == 0) {
        board_.psg.tick();
    }

    frame_counter_++;
    total_cycles_++;
}

template<GenesisVariant V>
void GenesisSystem<V>::run_frame() {
    uint32_t start = total_cycles_;
    uint32_t cpf = genesis_constants::CYCLES_PER_FRAME_NTSC;
    while (total_cycles_ - start < cpf) {
        tick();
    }
}

// ============================================================================
// 68K BUS DISPATCH (24-bit address decoding)
// ============================================================================

template<GenesisVariant V>
bus_state_t GenesisSystem<V>::m68k_bus_dispatch(bus_state_t pins) {
    // Only dispatch on valid bus cycle
    if (!BUS_GET_BIT(pins, M68K_AS_BIT)) return pins;

    uint32_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // $000000–$3FFFFF : Cart ROM
    if (addr < 0x400000) {
        uint8_t* rom = board_.rom.data();
        if (rom && is_read) {
            BUS_SET_DATA(pins, rom[addr & (genesis_constants::MAX_ROM_SIZE - 1)]);
        }
        return pins;
    }

    // $FF0000–$FFFFFF : Main RAM
    if (addr >= 0xFF0000) {
        uint8_t* ram = board_.main_ram.data();
        uint16_t ram_addr = addr & 0xFFFF;
        if (is_read) {
            BUS_SET_DATA(pins, ram[ram_addr]);
        } else {
            ram[ram_addr] = BUS_GET_DATA(pins);
        }
        return pins;
    }

    // $C00000–$C0001F : VDP
    if ((addr & 0xE00000) == 0xC00000) {
        bus_state_t vdp_bus = pins;
        BUS_SET_ADDR(vdp_bus, addr & 0x1F);
        if (is_read)
            vdp_bus = board_.vdp.on_bus_read(vdp_bus);
        else
            board_.vdp.on_bus_write(vdp_bus);
        BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        return pins;
    }

    // $A10000–$A1001F : I/O registers
    if (addr >= 0xA10000 && addr <= 0xA1001F) {
        uint8_t reg = (addr & 0x1F) >> 1;
        if (is_read) {
            if (addr == genesis_constants::IO_VERSION)
                BUS_SET_DATA(pins, (V == GenesisVariant::MEGADRIVE) ? 0xE0 : 0xA0);
            else if (reg < 3)
                BUS_SET_DATA(pins, io_data_[reg]);
        } else {
            if (reg < 3) io_data_[reg] = BUS_GET_DATA(pins);
        }
        return pins;
    }

    // $A11100 : Z80 bus request
    if (addr == genesis_constants::Z80_BUS_REQ || addr == genesis_constants::Z80_BUS_REQ + 1) {
        if (is_read) {
            BUS_SET_DATA(pins, z80_bus_granted_ ? 0x00 : 0x01);
        } else {
            z80_bus_granted_ = (BUS_GET_DATA(pins) & 0x01) != 0;
        }
        return pins;
    }

    // $A11200 : Z80 reset
    if (addr == genesis_constants::Z80_RESET || addr == genesis_constants::Z80_RESET + 1) {
        if (!is_read) {
            z80_reset_ = !(BUS_GET_DATA(pins) & 0x01);
            if (z80_reset_) {
                z80_pins_ = board_.z80.reset(z80_pins_);
            }
        }
        return pins;
    }

    // $A00000–$A0FFFF : Z80 area (RAM + YM2612 + PSG)
    if ((addr & 0xFF0000) == 0xA00000) {
        uint16_t z80_addr = addr & 0xFFFF;
        if (z80_addr < genesis_constants::Z80_RAM_SIZE) {
            uint8_t* zram = board_.z80_ram.data();
            if (is_read)
                BUS_SET_DATA(pins, zram[z80_addr]);
            else
                zram[z80_addr] = BUS_GET_DATA(pins);
        } else if (z80_addr >= 0x4000 && z80_addr < 0x4004) {
            // YM2612
            bus_state_t fm_bus = 0;
            BUS_SET_ADDR(fm_bus, z80_addr & 0x03);
            BUS_SET_DATA(fm_bus, BUS_GET_DATA(pins));
            if (is_read) {
                fm_bus = board_.fm.on_bus_read(fm_bus);
                BUS_SET_DATA(pins, BUS_GET_DATA(fm_bus));
            } else {
                board_.fm.on_bus_write(fm_bus);
            }
        }
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE / AUDIO / INPUT
// ============================================================================

template<GenesisVariant V>
bool GenesisSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* rom = board_.rom.data();
    if (!rom) return false;

    // Genesis ROMs: .md / .gen / .bin, up to 4MB
    // Skip 512-byte SMD header if present
    if (!load_raw_rom_mirrored(filepath, rom, genesis_constants::MAX_ROM_SIZE, 0,
                               get_descriptor().name, program_title_))
        return false;

    // Parse ROM header for title ($100–$1FF)
    if (rom[0x100] >= 0x20 && rom[0x100] < 0x7F) {
        // Domestic name at $120, international at $150
        char title[49] = {};
        std::memcpy(title, rom + 0x150, 48);
        for (int i = 47; i >= 0 && title[i] == ' '; i--) title[i] = '\0';
        if (title[0]) program_title_ = title;
    }

    reset();
    return true;
}

template<GenesisVariant V>
uint32_t GenesisSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<GenesisVariant V>
void GenesisSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

template<GenesisVariant V>
void GenesisSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // 3-button pad: Up, Down, Left, Right, A, B, C, Start
    // Active-low, active select multiplexing via TH pin
    auto set_bit = [](uint8_t& reg, int bit, bool p) {
        if (p) reg &= ~(1 << bit); else reg |= (1 << bit);
    };

    switch (key) {
        case SDLK_UP:     set_bit(io_data_[0], 0, pressed); break;
        case SDLK_DOWN:   set_bit(io_data_[0], 1, pressed); break;
        case SDLK_LEFT:   set_bit(io_data_[0], 2, pressed); break;
        case SDLK_RIGHT:  set_bit(io_data_[0], 3, pressed); break;
        case SDLK_z:      set_bit(io_data_[0], 4, pressed); break;  // B
        case SDLK_x:      set_bit(io_data_[0], 5, pressed); break;  // C
        case SDLK_a:      set_bit(io_data_[0], 6, pressed); break;  // A
        case SDLK_RETURN: set_bit(io_data_[0], 7, pressed); break;  // Start
        default: break;
    }
}

template<GenesisVariant V>
bool GenesisSystem<V>::load_roms() {
    return true;  // Genesis has no system ROM
}

// ============================================================================
// TEMPLATE INSTANTIATION
// ============================================================================

template class GenesisSystem<GenesisVariant::GENESIS>;
template class GenesisSystem<GenesisVariant::MEGADRIVE>;

REGISTER_SYSTEM(genesis_descriptor, [] {
    return std::make_unique<GenesisSystem<GenesisVariant::GENESIS>>();
});

REGISTER_SYSTEM(megadrive_descriptor, [] {
    return std::make_unique<GenesisSystem<GenesisVariant::MEGADRIVE>>();
});
