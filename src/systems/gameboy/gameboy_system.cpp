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
#include <algorithm>

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
    video_port_->set_palette(board_.ppu.system_palette(),
                             board_.ppu.palette_size());

    // Wire PPU video output to the composite video port
    board_.ppu.set_video_out(&video_port_->output());

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(gb_constants::CPU_FREQ_HZ,
                           gb_constants::DEFAULT_SAMPLE_RATE);

    // Default MBC (ROM only) — replaced when a cartridge is loaded
    mbc_ = std::make_unique<GbMbcNone>();

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
    ie_ = 0;
    joypad_buttons_ = 0x0F;
    joypad_dpad_ = 0x0F;
    std::memset(io_regs_, 0, sizeof(io_regs_));
    std::memset(hram_, 0, sizeof(hram_));
    if (mbc_) mbc_->reset();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<GameBoyVariant V>
void GameBoySystem<V>::tick() {
    // PPU dot tick — returns interrupt request bits
    uint8_t ppu_irq = board_.ppu.tick();
    io_regs_[gb_constants::IO_IF] |= ppu_irq;

    // APU tick — generate audio sample
    float apu_sample = board_.apu.tick();
    if (audio_port_) audio_port_->drive(apu_sample);

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

        if (addr < 0x8000) {
            // ROM area ($0000–$7FFF): MBC handles banking
            if (BUS_GET_BIT(pins_, BUS_RW_BIT)) {
                BUS_SET_DATA(pins_, mbc_->rom_read(addr));
            } else {
                mbc_->rom_write(addr, BUS_GET_DATA(pins_));
            }
        } else if (addr < 0xA000) {
            // VRAM ($8000–$9FFF): also accessible by PPU
            uint16_t vram_addr = addr - 0x8000;
            if (BUS_GET_BIT(pins_, BUS_RW_BIT)) {
                BUS_SET_DATA(pins_, board_.ppu.vram_[vram_addr]);
            } else {
                board_.ppu.vram_[vram_addr] = BUS_GET_DATA(pins_);
            }
        } else if (addr < 0xC000) {
            // External RAM ($A000–$BFFF): MBC handles banking
            if (BUS_GET_BIT(pins_, BUS_RW_BIT)) {
                BUS_SET_DATA(pins_, mbc_->ram_read(addr));
            } else {
                mbc_->ram_write(addr, BUS_GET_DATA(pins_));
            }
        } else if (addr < 0xFE00) {
            // WRAM ($C000–$DFFF) + Echo RAM ($E000–$FDFF)
            uint16_t wram_addr = addr;
            if (addr >= 0xE000) wram_addr -= 0x2000;  // Echo mirror
            wram_addr -= 0xC000;
            if (wram_addr < gb_constants::WRAM_SIZE) {
                uint8_t* wram = board_.wram.data();
                if (BUS_GET_BIT(pins_, BUS_RW_BIT)) {
                    BUS_SET_DATA(pins_, wram[wram_addr]);
                } else {
                    wram[wram_addr] = BUS_GET_DATA(pins_);
                }
            }
        } else if (addr < 0xFEA0) {
            // OAM ($FE00–$FE9F)
            uint8_t oam_offset = addr - 0xFE00;
            if (BUS_GET_BIT(pins_, BUS_RW_BIT)) {
                BUS_SET_DATA(pins_, board_.ppu.oam_[oam_offset]);
            } else {
                board_.ppu.oam_[oam_offset] = BUS_GET_DATA(pins_);
            }
        } else if (addr < 0xFF00) {
            // Unusable area ($FEA0–$FEFF)
            if (BUS_GET_BIT(pins_, BUS_RW_BIT))
                BUS_SET_DATA(pins_, 0xFF);
        } else {
            // High page: I/O, HRAM, IE ($FF00–$FFFF)
            // Resolve address → chip-select, then let chips self-dispatch
            pins_ = bus_.resolve(pins_);
            pins_ = bus_.service(pins_);
            pins_ = board_.ppu.tick_mmio(pins_);
            pins_ = board_.apu.tick_mmio(pins_);
            if (!ChipBase::is_cs_serviced(pins_))
                pins_ = io_tick(pins_);
        }
    }

    // Timer: 16-bit internal divider, TIMA counter
    uint16_t old_div = div_counter_;
    div_counter_++;
    io_regs_[gb_constants::IO_DIV] = static_cast<uint8_t>(div_counter_ >> 8);

    // TIMA increment: TAC selects which bit of the divider falling edge clocks TIMA
    uint8_t tac = io_regs_[gb_constants::IO_TAC];
    if (tac & 0x04) {  // Timer enable
        // TAC bits 0-1 select divider bit: 0=bit9(4096Hz), 1=bit3(262144Hz),
        //                                   2=bit5(65536Hz), 3=bit7(16384Hz)
        static constexpr uint8_t tac_bits[4] = { 9, 3, 5, 7 };
        uint8_t bit = tac_bits[tac & 0x03];
        // Falling edge detection on the selected divider bit
        bool old_bit = (old_div >> bit) & 1;
        bool new_bit = (div_counter_ >> bit) & 1;
        if (old_bit && !new_bit) {
            io_regs_[gb_constants::IO_TIMA]++;
            if (io_regs_[gb_constants::IO_TIMA] == 0) {
                // TIMA overflow: reload from TMA and request timer interrupt
                io_regs_[gb_constants::IO_TIMA] = io_regs_[gb_constants::IO_TMA];
                io_regs_[gb_constants::IO_IF] |= 0x04;  // Timer interrupt (IF bit 2)
            }
        }
    }

    // OAM DMA transfer (160 bytes, takes 160 M-cycles = 640 T-states)
    // Simplified: instant transfer on write to DMA register
    if (board_.ppu.dma_pending_) {
        board_.ppu.dma_pending_ = false;
        uint16_t src = board_.ppu.dma_source_;
        for (int i = 0; i < 160; i++) {
            uint16_t addr = src + i;
            uint8_t val = 0xFF;
            if (addr < 0x8000) {
                val = mbc_->rom_read(addr);
            } else if (addr < 0xA000) {
                val = board_.ppu.vram_[addr - 0x8000];
            } else if (addr < 0xC000) {
                val = mbc_->ram_read(addr);
            } else if (addr < 0xE000) {
                uint8_t* wram = board_.wram.data();
                val = wram[addr - 0xC000];
            }
            board_.ppu.oam_[i] = val;
        }
    }

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

    // $FF04 — DIV: any write resets the 16-bit divider to 0
    if (offset == gb_constants::IO_DIV) {
        if (is_read) {
            BUS_SET_DATA(pins, static_cast<uint8_t>(div_counter_ >> 8));
        } else {
            div_counter_ = 0;
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

    // Load the entire ROM file
    size_t file_size = 0;
    VfsData file_data(vfs_read_file(filepath, &file_size));
    if (!file_data || file_size < 0x150) {
        log_info("Game Boy: Failed to open or file too small: %s\n", filepath);
        return false;
    }

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(file_data.get());

    // Parse cartridge header
    uint8_t cart_type = raw[0x0147];
    uint8_t rom_code  = raw[0x0148];
    uint8_t ram_code  = raw[0x0149];

    uint32_t expected_rom_size = rom_size_from_header(rom_code);
    uint32_t expected_ram_size = ram_size_from_header(ram_code);

    // Use actual file size if larger than expected (some ROMs have padding)
    uint32_t actual_rom_size = static_cast<uint32_t>(std::max(file_size,
                                                     static_cast<size_t>(expected_rom_size)));

    // Allocate ROM buffer (round up to power of 2 for clean masking)
    uint32_t rom_alloc = 32768;  // Minimum 32KB
    while (rom_alloc < actual_rom_size) rom_alloc <<= 1;
    cart_rom_.resize(rom_alloc, 0xFF);
    std::memcpy(cart_rom_.data(), raw, file_size);

    // Mirror if file is smaller than allocation
    if (file_size < rom_alloc) {
        for (size_t offset = file_size; offset < rom_alloc; offset += file_size)
            std::memcpy(cart_rom_.data() + offset, raw,
                        std::min(file_size, static_cast<size_t>(rom_alloc) - offset));
    }

    // Allocate external RAM
    cart_ram_.resize(expected_ram_size, 0);

    // Create MBC from cartridge type
    mbc_ = create_mbc(cart_type);
    mbc_->bind_cartridge(cart_rom_.data(), rom_alloc,
                         cart_ram_.empty() ? nullptr : cart_ram_.data(),
                         static_cast<uint32_t>(cart_ram_.size()));

    // Extract title from header ($0134–$0143)
    char title[17] = {};
    std::memcpy(title, raw + 0x0134, 16);
    title[16] = '\0';
    // Trim trailing nulls/spaces
    for (int i = 15; i >= 0; i--) {
        if (title[i] == '\0' || title[i] == ' ') title[i] = '\0';
        else break;
    }
    program_title_ = title;

    log_info("Game Boy: Loaded \"%s\" — type=$%02X, ROM=%uKB, RAM=%uKB, MBC=%s\n",
             title, cart_type, rom_alloc / 1024, expected_ram_size / 1024,
             cart_type == 0 ? "None" :
             cart_type <= 3 ? "MBC1" :
             cart_type <= 6 ? "MBC2" :
             cart_type <= 0x13 ? "MBC3" : "MBC5");

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
