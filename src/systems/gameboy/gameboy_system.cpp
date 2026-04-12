/*
 * gameboy_system.cpp — Game Boy / Game Boy Color system implementation
 *
 * Tick loop:
 *   SM83 CPU runs at 4.194 MHz (T-states).
 *   PPU runs at 1 dot per T-state (456 dots per line, 154 lines per frame).
 *   APU frame sequencer at 512 Hz (CPU_FREQ / 8192).
 *   Timer increments at configurable rate.
 *
 * I/O dispatch:
 *   $FF00       : Joypad (P1/JOYP)
 *   $FF01-$FF02 : Serial
 *   $FF04-$FF07 : Timer (DIV, TIMA, TMA, TAC)
 *   $FF0F       : Interrupt Flag (IF)
 *   $FF10-$FF3F : APU registers + Wave RAM
 *   $FF40-$FF4B : PPU registers
 *   $FF80-$FFFE : HRAM
 *   $FFFF       : Interrupt Enable (IE)
 */

#include "core/cermu.hpp"
#include "systems/gameboy/gameboy_system.hpp"
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

template<GameBoyVariant V>
static HardwareTraits create_gb_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = gb_constants::DISPLAY_WIDTH;
    traits.display.native_height   = gb_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = gb_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = gb_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = (V == GameBoyVariant::GBC) ? 32768 : 4;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = gb_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 2;
    traits.audio.chip_name         = "SM83 APU";

    traits.timing.cpu_frequency_hz   = gb_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = gb_constants::TARGET_FPS;
    traits.timing.cycles_per_frame   = gb_constants::CYCLES_PER_FRAME;
    traits.timing.standard           = VideoStandard::CUSTOM;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor dmg_descriptor = {
    "Game Boy", "DMG",
    "Nintendo Game Boy — Sharp SM83, 4 shades of green (1989)",
    "gameboy", {"Game Boy", "DMG", "GB", "GameBoy"},
    nullptr,
    create_gb_hardware_traits<GameBoyVariant::DMG>(),
    nullptr,
    "Nintendo", 1989, "Sharp SM83 (LR35902)", SystemType::Console
};

static SystemDescriptor gbc_descriptor = {
    "Game Boy Color", "GBC",
    "Nintendo Game Boy Color — Sharp SM83, 32768 colors (1998)",
    "gameboy", {"Game Boy Color", "GBC", "CGB", "GameBoyColor"},
    nullptr,
    create_gb_hardware_traits<GameBoyVariant::GBC>(),
    nullptr,
    "Nintendo", 1998, "Sharp SM83 (LR35902)", SystemType::Console
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<GameBoyVariant V>
GameBoySystem<V>::GameBoySystem()
    : System()
    , pins_(GB_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_gb_hardware_traits<V>();
    std::memset(io_regs_, 0, sizeof(io_regs_));
    std::memset(hram_, 0, sizeof(hram_));
}

template<GameBoyVariant V>
GameBoySystem<V>::~GameBoySystem() = default;

template<GameBoyVariant V>
const SystemDescriptor& GameBoySystem<V>::get_descriptor() const {
    if constexpr (V == GameBoyVariant::DMG) return dmg_descriptor;
    else return gbc_descriptor;
}

template<GameBoyVariant V>
bool GameBoySystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<GameBoyVariant V>
bool GameBoySystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<GameBoyVariant V>
bool GameBoySystem<V>::initialize() {
    log_info("Game Boy: Initializing (%s)\n",
             V == GameBoyVariant::DMG ? "DMG" : "GBC");
    register_board(&board_);

    bind_all(board_, board_.components_, kGameBoyManifest<V>);

    port_manifest_       = kGameBoyManifest<V>.port_slots;
    port_manifest_count_ = kGameBoyManifest<V>.port_count;
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.cpu.init();

    // Post-boot ROM register state (DMG/GBC)
    // When no boot ROM is present, the system initializes CPU registers
    // to match the state left by the boot ROM after completion.
    board_.cpu.set(z80::reg::AF, static_cast<uint16_t>(0x01B0));
    board_.cpu.set(z80::reg::BC, static_cast<uint16_t>(0x0013));
    board_.cpu.set(z80::reg::DE, static_cast<uint16_t>(0x00D8));
    board_.cpu.set(z80::reg::HL, static_cast<uint16_t>(0x014D));
    board_.cpu.set(z80::reg::SP, static_cast<uint16_t>(0xFFFE));
    board_.cpu.set_pc(0x0100);  // Entry point after boot ROM
    board_.ppu.reset();
    board_.apu.reset();

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(gb_constants::DEFAULT_SAMPLE_RATE,
                           gb_constants::DEFAULT_SAMPLE_RATE);

    system_ready_ = true;
    log_info("Game Boy: Initialized\n");
    return true;
}

template<GameBoyVariant V>
void GameBoySystem<V>::shutdown() {
    system_ready_ = false;
}

template<GameBoyVariant V>
void GameBoySystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.cpu.reset(pins_);
    frame_counter_ = 0;
    div_counter_ = 0;
    timer_counter_ = 0;
    ie_ = 0;
    joypad_buttons_ = 0x0F;
    joypad_dpad_ = 0x0F;
    std::memset(io_regs_, 0, sizeof(io_regs_));
    std::memset(hram_, 0, sizeof(hram_));
}

// ============================================================================
// EXECUTION
// ============================================================================

template<GameBoyVariant V>
void GameBoySystem<V>::tick() {
    // PPU dot tick
    bus_state_t ppu_bus = 0;
    ppu_bus = board_.ppu.tick(ppu_bus);

    // SM83 interrupt model: drive INT pin based on IF & IE
    // Active-low: assert INT (clear bit) when any enabled interrupt is pending
    uint8_t if_reg = io_regs_[gb_constants::IO_IF];
    uint8_t pending = if_reg & ie_;
    if (pending) {
        BUS_CLR_BIT(pins_, Z80_INT_BIT);  // Assert INT (active-low)
        // Provide interrupt vector on data bus for CPU to read during INT ack.
        // Priority: bit 0 (VBlank) highest → bit 4 (Joypad) lowest.
        // Vectors: VBlank=$0040, LCD STAT=$0048, Timer=$0050, Serial=$0058, Joypad=$0060
        static constexpr uint8_t vectors[5] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
        for (int i = 0; i < 5; i++) {
            if (pending & (1 << i)) {
                BUS_SET_DATA(pins_, vectors[i]);
                // Clear the serviced IF bit when INT is acknowledged
                io_regs_[gb_constants::IO_IF] &= ~(1 << i);
                break;
            }
        }
    } else {
        BUS_SET_BIT(pins_, Z80_INT_BIT);  // Deassert INT
    }

    // CPU tick
    pins_ = board_.cpu.tick(pins_);

    // SM83 memory bus dispatch (no IORQ — all I/O is memory-mapped via MREQ)
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);

    if (mreq) {
        uint16_t addr = BUS_GET_ADDR(pins_);

        if (addr >= 0xFF00) {
            // High page: I/O, HRAM, IE
            pins_ = io_tick(pins_);
        } else {
            // Echo RAM ($E000–$FDFF): mirror of $C000–$DDFF
            if (addr >= 0xE000 && addr < 0xFE00) {
                BUS_SET_ADDR(pins_, addr - 0x2000);
            }
            pins_ = bus_.tick(pins_);
        }
    }

    // Timer / divider
    div_counter_++;

    frame_counter_++;
    total_cycles_++;
}

template<GameBoyVariant V>
void GameBoySystem<V>::run_frame() {
    uint32_t start = total_cycles_;
    while (total_cycles_ - start < gb_constants::CYCLES_PER_FRAME) {
        tick();
    }
}

// ============================================================================
// I/O DISPATCH ($FF00–$FFFF)
// ============================================================================

template<GameBoyVariant V>
bus_state_t GameBoySystem<V>::io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t offset = addr & 0xFF;

    // $FFFF — Interrupt Enable
    if (addr == 0xFFFF) {
        if (is_read) BUS_SET_DATA(pins, ie_);
        else         ie_ = BUS_GET_DATA(pins);
        return pins;
    }

    // $FF80–$FFFE — HRAM
    if (offset >= 0x80) {
        if (is_read) BUS_SET_DATA(pins, hram_[offset - 0x80]);
        else         hram_[offset - 0x80] = BUS_GET_DATA(pins);
        return pins;
    }

    // $FF00 — Joypad
    if (offset == gb_constants::IO_JOYP) {
        if (is_read) {
            uint8_t result = 0xCF;  // Bits 6-7 unused, set high
            if (!(joypad_select_ & 0x10))
                result = (result & 0xF0) | (joypad_dpad_ & 0x0F);
            if (!(joypad_select_ & 0x20))
                result = (result & 0xF0) | (joypad_buttons_ & 0x0F);
            BUS_SET_DATA(pins, result);
        } else {
            joypad_select_ = BUS_GET_DATA(pins) & 0x30;
        }
        return pins;
    }

    // $FF10–$FF3F — APU (delegated)
    if (offset >= 0x10 && offset <= 0x3F) {
        bus_state_t apu_bus = 0;
        BUS_SET_ADDR(apu_bus, offset);
        BUS_SET_DATA(apu_bus, BUS_GET_DATA(pins));
        if (is_read) {
            apu_bus = board_.apu.on_bus_read(apu_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(apu_bus));
        } else {
            board_.apu.on_bus_write(apu_bus);
        }
        return pins;
    }

    // $FF40–$FF4B — PPU (delegated)
    if (offset >= 0x40 && offset <= 0x4B) {
        bus_state_t ppu_bus = 0;
        BUS_SET_ADDR(ppu_bus, offset - 0x40);
        BUS_SET_DATA(ppu_bus, BUS_GET_DATA(pins));
        if (is_read) {
            ppu_bus = board_.ppu.on_bus_read(ppu_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(ppu_bus));
        } else {
            board_.ppu.on_bus_write(ppu_bus);
        }
        return pins;
    }

    // Generic I/O register shadow
    if (is_read) BUS_SET_DATA(pins, io_regs_[offset]);
    else         io_regs_[offset] = BUS_GET_DATA(pins);

    return pins;
}

// ============================================================================
// BUS / FILE / AUDIO / INPUT
// ============================================================================

template<GameBoyVariant V>
void GameBoySystem<V>::configure_bus_memory_map() {
    board_.apply(bus_);
}

template<GameBoyVariant V>
bool GameBoySystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* rom = board_.rom.data();
    if (!rom) return false;

    // Game Boy ROMs: 32KB minimum (bank 0 + bank 1)
    // Larger ROMs use MBC banking (not yet implemented)
    if (!load_raw_rom_mirrored(filepath, rom, 0x8000, 0x0000,
                               get_descriptor().name, program_title_))
        return false;

    reset();
    return true;
}

template<GameBoyVariant V>
uint32_t GameBoySystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<GameBoyVariant V>
void GameBoySystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
}

template<GameBoyVariant V>
void GameBoySystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // D-pad: Right=bit0, Left=bit1, Up=bit2, Down=bit3 (active-low)
    // Buttons: A=bit0, B=bit1, Select=bit2, Start=bit3 (active-low)
    auto set_bit = [](uint8_t& reg, int bit, bool p) {
        if (p) reg &= ~(1 << bit); else reg |= (1 << bit);
    };

    switch (key) {
        case SDLK_RIGHT:  set_bit(joypad_dpad_, 0, pressed); break;
        case SDLK_LEFT:   set_bit(joypad_dpad_, 1, pressed); break;
        case SDLK_UP:     set_bit(joypad_dpad_, 2, pressed); break;
        case SDLK_DOWN:   set_bit(joypad_dpad_, 3, pressed); break;
        case SDLK_z:      set_bit(joypad_buttons_, 0, pressed); break;  // A
        case SDLK_x:      set_bit(joypad_buttons_, 1, pressed); break;  // B
        case SDLK_BACKSPACE: set_bit(joypad_buttons_, 2, pressed); break;  // Select
        case SDLK_RETURN: set_bit(joypad_buttons_, 3, pressed); break;  // Start
        default: break;
    }
}

template<GameBoyVariant V>
bool GameBoySystem<V>::load_roms() {
    // Game Boy has optional boot ROM ($0000–$00FF, disabled after boot)
    char rom_root[1024];
    if (!system_config_discover_rom_root("gameboy", rom_root, sizeof(rom_root)))
        return false;
    return board_.load_roms(rom_root, "Game Boy");
}

// ============================================================================
// TEMPLATE INSTANTIATION
// ============================================================================

template class GameBoySystem<GameBoyVariant::DMG>;
template class GameBoySystem<GameBoyVariant::GBC>;

REGISTER_SYSTEM(dmg_descriptor, [] {
    return std::make_unique<GameBoySystem<GameBoyVariant::DMG>>();
});

REGISTER_SYSTEM(gbc_descriptor, [] {
    return std::make_unique<GameBoySystem<GameBoyVariant::GBC>>();
});
