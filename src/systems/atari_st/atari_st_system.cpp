/*
 * atari_st_system.cpp — Atari ST system implementation
 *
 * Tick loop:
 *   MC68000 runs at 8 MHz via memory callbacks.
 *   MFP timers count down each tick.
 *   YM2149 PSG ticks at CPU/4.
 *   Shifter increments DMA counter (framebuffer scan).
 *
 * Address decoding (GLUE function, implemented here):
 *   $000000–$3FFFFF : RAM (mirrored within size)
 *   $FA0000–$FBFFFF : Cartridge ROM
 *   $FC0000–$FEFFFF : TOS ROM
 *   $FF8200–$FF827F : Shifter
 *   $FF8240–$FF825F : Palette (within Shifter)
 *   $FF8800–$FF8803 : YM2149 PSG
 *   $FF8604–$FF860B : WD1772 FDC / DMA
 *   $FFFA01–$FFFA2F : MK68901 MFP
 *   $FFFC00–$FFFC06 : ACIAs (keyboard, MIDI)
 */

#include "core/cermu.hpp"
#include "systems/atari_st/atari_st_system.hpp"
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

template<AtariSTVariant V>
static HardwareTraits create_atari_st_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = atari_st_constants::DISPLAY_WIDTH_LOW;
    traits.display.native_height   = atari_st_constants::DISPLAY_HEIGHT_LOW;
    traits.display.visible_width   = atari_st_constants::DISPLAY_WIDTH_LOW;
    traits.display.visible_height  = atari_st_constants::DISPLAY_HEIGHT_LOW;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = atari_st_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "YM2149";

    traits.timing.cpu_frequency_hz   = atari_st_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = atari_st_constants::TARGET_FPS_50HZ;
    traits.timing.cycles_per_frame   = atari_st_constants::CYCLES_PER_FRAME_50;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor atari_st_descriptor = {
    "Atari ST", "AtariST",
    "Atari ST — MC68000 8 MHz, Shifter, YM2149 (1985)",
    "atari_st", {"AtariST", "Atari ST", "ST", "520ST", "1040ST"},
    nullptr,
    create_atari_st_hardware_traits<AtariSTVariant::ST>(),
    nullptr,
    "Atari", 1985, "Motorola 68000", SystemType::Home
};

static SystemDescriptor atari_ste_descriptor = {
    "Atari STe", "AtariSTe",
    "Atari STe — MC68000 8 MHz, Enhanced Shifter, DMA Sound (1989)",
    "atari_st", {"AtariSTe", "Atari STe", "STe", "1040STe"},
    nullptr,
    create_atari_st_hardware_traits<AtariSTVariant::STE>(),
    nullptr,
    "Atari", 1989, "Motorola 68000", SystemType::Home
};

static SystemDescriptor atari_mega_st_descriptor = {
    "Atari Mega ST", "MegaST",
    "Atari Mega ST — MC68000 8 MHz, Blitter, RTC (1987)",
    "atari_st", {"MegaST", "Atari Mega ST", "Mega ST"},
    nullptr,
    create_atari_st_hardware_traits<AtariSTVariant::MEGA_ST>(),
    nullptr,
    "Atari", 1987, "Motorola 68000", SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<AtariSTVariant V>
AtariSTSystem<V>::AtariSTSystem()
    : System()
    , m68k_pins_(ATARI_ST_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_atari_st_hardware_traits<V>();
}

template<AtariSTVariant V>
AtariSTSystem<V>::~AtariSTSystem() = default;

template<AtariSTVariant V>
const SystemDescriptor& AtariSTSystem<V>::get_descriptor() const {
    if constexpr (V == AtariSTVariant::ST)   return atari_st_descriptor;
    else if constexpr (V == AtariSTVariant::STE) return atari_ste_descriptor;
    else return atari_mega_st_descriptor;
}

template<AtariSTVariant V>
bool AtariSTSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<AtariSTVariant V>
bool AtariSTSystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<AtariSTVariant V>
bool AtariSTSystem<V>::initialize() {
    log_info("Atari ST: Initializing system (%s)\n",
             V == AtariSTVariant::STE ? "STe" :
             V == AtariSTVariant::MEGA_ST ? "Mega ST" : "ST");
    register_board(&board_);

    bind_all(board_, board_.components_, kAtariSTManifest<V>);

    port_manifest_       = kAtariSTManifest<V>.port_slots;
    port_manifest_count_ = kAtariSTManifest<V>.port_count;

    // MC68000 memory callbacks
    board_.cpu.set_memory_callbacks(mem_read, mem_write, this);
    m68k_pins_ = board_.cpu.init();

    board_.shifter.reset();
    board_.psg.init();
    board_.mfp.reset();
    board_.fdc.reset();

    board_.psg.set_clock_frequency(atari_st_constants::PSG_FREQ_HZ);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);

    if (!load_roms()) {
        log_info("Atari ST: Warning — TOS ROM not loaded\n");
    }

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atari_st_constants::PSG_FREQ_HZ,
                           atari_st_constants::DEFAULT_SAMPLE_RATE);
    board_.psg.set_audio_port(audio_port_.get());

    system_ready_ = true;
    log_info("Atari ST: System initialized\n");
    return true;
}

template<AtariSTVariant V>
void AtariSTSystem<V>::shutdown() {
    system_ready_ = false;
}

template<AtariSTVariant V>
void AtariSTSystem<V>::reset() {
    board_.reset_chips();
    board_.cpu.set_memory_callbacks(mem_read, mem_write, this);
    m68k_pins_ = board_.cpu.reset(m68k_pins_);
    frame_counter_ = 0;
    acia_kbd_status_ = 0x02;
    acia_kbd_data_ = 0;
    acia_midi_status_ = 0x02;
    acia_midi_data_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<AtariSTVariant V>
void AtariSTSystem<V>::tick() {
    // CPU tick (one clock cycle)
    m68k_pins_ = board_.cpu.tick(m68k_pins_);

    // MFP tick
    bus_state_t mfp_bus = 0;
    board_.mfp.tick(mfp_bus);

    // MFP IRQ → 68000 IPL (active-low, directly wired)
    if (board_.mfp.timer_irq_pending())
        BUS_CLR_BIT(m68k_pins_, M68K_IPL0_BIT);
    else
        BUS_SET_BIT(m68k_pins_, M68K_IPL0_BIT);

    // PSG tick (CPU / 4)
    if ((frame_counter_ & 0x03) == 0) {
        board_.psg.tick();
    }

    // Shifter — advance video counter
    bus_state_t sh_bus = 0;
    board_.shifter.tick(sh_bus);

    frame_counter_++;
    total_cycles_++;
}

template<AtariSTVariant V>
void AtariSTSystem<V>::run_frame() {
    uint32_t start = total_cycles_;
    uint32_t cpf = atari_st_constants::CYCLES_PER_FRAME_50;
    while (total_cycles_ - start < cpf) {
        tick();
    }
    board_.shifter.start_of_frame();
}

// ============================================================================
// MC68000 MEMORY CALLBACKS (word-level)
// ============================================================================

template<AtariSTVariant V>
uint16_t AtariSTSystem<V>::mem_read(void* ctx, uint32_t addr) {
    auto* self = static_cast<AtariSTSystem<V>*>(ctx);
    addr &= 0x00FFFFFF;  // 24-bit address bus
    uint8_t hi = self->read_byte(addr);
    uint8_t lo = self->read_byte(addr + 1);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

template<AtariSTVariant V>
void AtariSTSystem<V>::mem_write(void* ctx, uint32_t addr, uint16_t data) {
    auto* self = static_cast<AtariSTSystem<V>*>(ctx);
    addr &= 0x00FFFFFF;
    self->write_byte(addr, (data >> 8) & 0xFF);
    self->write_byte(addr + 1, data & 0xFF);
}

// ============================================================================
// BYTE-LEVEL ADDRESS DECODING
// ============================================================================

template<AtariSTVariant V>
uint8_t AtariSTSystem<V>::read_byte(uint32_t addr) noexcept {
    // RAM: $000000–$3FFFFF
    uint32_t ram_size = board_.ram.size_bytes();
    if (addr < 0x400000 && addr < ram_size) {
        return board_.ram.data()[addr];
    }

    // TOS ROM: $FC0000–$FEFFFF
    if (addr >= atari_st_constants::TOS_ROM_BASE &&
        addr < atari_st_constants::TOS_ROM_BASE + atari_st_constants::TOS_ROM_SIZE) {
        return board_.tos_rom.data()[addr - atari_st_constants::TOS_ROM_BASE];
    }

    // TOS ROM mirror at $000000-$000007 during reset (first 4 long words)
    // (handled by CPU reading SSP and PC from ROM)

    // Cartridge ROM: $FA0000–$FBFFFF
    if (addr >= atari_st_constants::CART_ROM_BASE &&
        addr < atari_st_constants::CART_ROM_BASE + atari_st_constants::CART_ROM_SIZE) {
        return board_.cart_rom.data()[addr - atari_st_constants::CART_ROM_BASE];
    }

    // Shifter: $FF8200–$FF827F (includes palette at $FF8240–$FF825F)
    if (addr >= atari_st_constants::SHIFTER_BASE && addr < atari_st_constants::SHIFTER_BASE + 0x80) {
        bus_state_t sh_bus = 0;
        BUS_SET_ADDR(sh_bus, addr - atari_st_constants::SHIFTER_BASE);
        BUS_SET_BIT(sh_bus, BUS_RW_BIT);
        sh_bus = board_.shifter.on_bus_read(sh_bus);
        return BUS_GET_DATA(sh_bus);
    }

    // YM2149 PSG: $FF8800–$FF8803
    if (addr >= atari_st_constants::PSG_BASE && addr < atari_st_constants::PSG_BASE + 4) {
        // PSG data read (address latch → data)
        return board_.psg.read_register();
    }

    // WD1772 FDC: $FF8604–$FF860F
    if (addr >= atari_st_constants::FDC_BASE && addr < atari_st_constants::FDC_BASE + 8) {
        bus_state_t fdc_bus = 0;
        BUS_SET_ADDR(fdc_bus, (addr - atari_st_constants::FDC_BASE) & 0x07);
        BUS_SET_BIT(fdc_bus, BUS_RW_BIT);
        fdc_bus = board_.fdc.on_bus_read(fdc_bus);
        return BUS_GET_DATA(fdc_bus);
    }

    // MK68901 MFP: $FFFA01–$FFFA2F (odd bytes)
    if (addr >= atari_st_constants::MFP_BASE && addr < atari_st_constants::MFP_BASE + 0x30) {
        if (addr & 1) {
            bus_state_t mfp_bus = 0;
            BUS_SET_ADDR(mfp_bus, addr - atari_st_constants::MFP_BASE);
            BUS_SET_BIT(mfp_bus, BUS_RW_BIT);
            mfp_bus = board_.mfp.on_bus_read(mfp_bus);
            return BUS_GET_DATA(mfp_bus);
        }
        return 0xFF;
    }

    // ACIA keyboard: $FFFC00–$FFFC02
    if (addr >= atari_st_constants::ACIA_KBD_BASE && addr < atari_st_constants::ACIA_KBD_BASE + 4) {
        if (addr & 1) {
            acia_kbd_status_ &= ~0x01;  // Clear RX full on data read
            return acia_kbd_data_;
        }
        return acia_kbd_status_;
    }

    // ACIA MIDI: $FFFC04–$FFFC06
    if (addr >= atari_st_constants::ACIA_MIDI_BASE && addr < atari_st_constants::ACIA_MIDI_BASE + 4) {
        if (addr & 1) return acia_midi_data_;
        return acia_midi_status_;
    }

    return 0xFF;  // Open bus
}

template<AtariSTVariant V>
void AtariSTSystem<V>::write_byte(uint32_t addr, uint8_t data) noexcept {
    // RAM
    uint32_t ram_size = board_.ram.size_bytes();
    if (addr < 0x400000 && addr < ram_size) {
        board_.ram.data()[addr] = data;
        return;
    }

    // Shifter
    if (addr >= atari_st_constants::SHIFTER_BASE && addr < atari_st_constants::SHIFTER_BASE + 0x80) {
        bus_state_t sh_bus = 0;
        BUS_SET_ADDR(sh_bus, addr - atari_st_constants::SHIFTER_BASE);
        BUS_SET_DATA(sh_bus, data);
        board_.shifter.on_bus_write(sh_bus);
        return;
    }

    // YM2149 PSG: $FF8800 = address latch, $FF8802 = data write
    if (addr >= atari_st_constants::PSG_BASE && addr < atari_st_constants::PSG_BASE + 4) {
        uint8_t port = (addr - atari_st_constants::PSG_BASE) & 0x03;
        if (port == 0)
            board_.psg.latch_address(data);
        else if (port == 2)
            board_.psg.write_register(data);
        return;
    }

    // WD1772 FDC
    if (addr >= atari_st_constants::FDC_BASE && addr < atari_st_constants::FDC_BASE + 8) {
        bus_state_t fdc_bus = 0;
        BUS_SET_ADDR(fdc_bus, (addr - atari_st_constants::FDC_BASE) & 0x07);
        BUS_SET_DATA(fdc_bus, data);
        board_.fdc.on_bus_write(fdc_bus);
        return;
    }

    // MK68901 MFP (odd bytes only)
    if (addr >= atari_st_constants::MFP_BASE && addr < atari_st_constants::MFP_BASE + 0x30) {
        if (addr & 1) {
            bus_state_t mfp_bus = 0;
            BUS_SET_ADDR(mfp_bus, addr - atari_st_constants::MFP_BASE);
            BUS_SET_DATA(mfp_bus, data);
            board_.mfp.on_bus_write(mfp_bus);
        }
        return;
    }

    // ACIA keyboard
    if (addr >= atari_st_constants::ACIA_KBD_BASE && addr < atari_st_constants::ACIA_KBD_BASE + 4) {
        if (addr & 1) acia_kbd_data_ = data;
        return;
    }

    // ACIA MIDI
    if (addr >= atari_st_constants::ACIA_MIDI_BASE && addr < atari_st_constants::ACIA_MIDI_BASE + 4) {
        if (addr & 1) acia_midi_data_ = data;
        return;
    }
}

// ============================================================================
// FILE / AUDIO / INPUT
// ============================================================================

template<AtariSTVariant V>
bool AtariSTSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    // Atari ST program formats: .prg, .tos, .st (disk image)
    // For .prg: load into RAM at base address from header, set PC
    // For now: load raw into cartridge ROM area
    uint8_t* cart = board_.cart_rom.data();
    if (!cart) return false;

    if (!load_raw_rom_mirrored(filepath, cart, atari_st_constants::CART_ROM_SIZE, 0,
                               get_descriptor().name, program_title_))
        return false;

    reset();
    return true;
}

template<AtariSTVariant V>
uint32_t AtariSTSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<AtariSTVariant V>
void AtariSTSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

template<AtariSTVariant V>
void AtariSTSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Atari ST keyboard: ACIA sends make (scancode) / break (scancode | 0x80)
    // Map SDL keycodes to Atari ST hardware scancodes
    uint8_t scancode = 0;
    switch (key) {
        case SDLK_ESCAPE:    scancode = 0x01; break;
        case SDLK_1:         scancode = 0x02; break;
        case SDLK_2:         scancode = 0x03; break;
        case SDLK_3:         scancode = 0x04; break;
        case SDLK_4:         scancode = 0x05; break;
        case SDLK_5:         scancode = 0x06; break;
        case SDLK_6:         scancode = 0x07; break;
        case SDLK_7:         scancode = 0x08; break;
        case SDLK_8:         scancode = 0x09; break;
        case SDLK_9:         scancode = 0x0A; break;
        case SDLK_0:         scancode = 0x0B; break;
        case SDLK_MINUS:     scancode = 0x0C; break;
        case SDLK_EQUALS:    scancode = 0x0D; break;
        case SDLK_BACKSPACE: scancode = 0x0E; break;
        case SDLK_TAB:       scancode = 0x0F; break;
        case SDLK_q:         scancode = 0x10; break;
        case SDLK_w:         scancode = 0x11; break;
        case SDLK_e:         scancode = 0x12; break;
        case SDLK_r:         scancode = 0x13; break;
        case SDLK_t:         scancode = 0x14; break;
        case SDLK_y:         scancode = 0x15; break;
        case SDLK_u:         scancode = 0x16; break;
        case SDLK_i:         scancode = 0x17; break;
        case SDLK_o:         scancode = 0x18; break;
        case SDLK_p:         scancode = 0x19; break;
        case SDLK_LEFTBRACKET:  scancode = 0x1A; break;
        case SDLK_RIGHTBRACKET: scancode = 0x1B; break;
        case SDLK_RETURN:    scancode = 0x1C; break;
        case SDLK_LCTRL:     scancode = 0x1D; break;
        case SDLK_a:         scancode = 0x1E; break;
        case SDLK_s:         scancode = 0x1F; break;
        case SDLK_d:         scancode = 0x20; break;
        case SDLK_f:         scancode = 0x21; break;
        case SDLK_g:         scancode = 0x22; break;
        case SDLK_h:         scancode = 0x23; break;
        case SDLK_j:         scancode = 0x24; break;
        case SDLK_k:         scancode = 0x25; break;
        case SDLK_l:         scancode = 0x26; break;
        case SDLK_SEMICOLON: scancode = 0x27; break;
        case SDLK_QUOTE:     scancode = 0x28; break;
        case SDLK_BACKQUOTE: scancode = 0x29; break;
        case SDLK_LSHIFT:    scancode = 0x2A; break;
        case SDLK_BACKSLASH: scancode = 0x2B; break;
        case SDLK_z:         scancode = 0x2C; break;
        case SDLK_x:         scancode = 0x2D; break;
        case SDLK_c:         scancode = 0x2E; break;
        case SDLK_v:         scancode = 0x2F; break;
        case SDLK_b:         scancode = 0x30; break;
        case SDLK_n:         scancode = 0x31; break;
        case SDLK_m:         scancode = 0x32; break;
        case SDLK_COMMA:     scancode = 0x33; break;
        case SDLK_PERIOD:    scancode = 0x34; break;
        case SDLK_SLASH:     scancode = 0x35; break;
        case SDLK_RSHIFT:    scancode = 0x36; break;
        case SDLK_SPACE:     scancode = 0x39; break;
        case SDLK_CAPSLOCK:  scancode = 0x3A; break;
        case SDLK_F1:        scancode = 0x3B; break;
        case SDLK_F2:        scancode = 0x3C; break;
        case SDLK_F3:        scancode = 0x3D; break;
        case SDLK_F4:        scancode = 0x3E; break;
        case SDLK_F5:        scancode = 0x3F; break;
        case SDLK_F6:        scancode = 0x40; break;
        case SDLK_F7:        scancode = 0x41; break;
        case SDLK_F8:        scancode = 0x42; break;
        case SDLK_F9:        scancode = 0x43; break;
        case SDLK_F10:       scancode = 0x44; break;
        case SDLK_HOME:      scancode = 0x47; break;  // CLR/HOME
        case SDLK_UP:        scancode = 0x48; break;
        case SDLK_LEFT:      scancode = 0x4B; break;
        case SDLK_RIGHT:     scancode = 0x4D; break;
        case SDLK_DOWN:      scancode = 0x50; break;
        case SDLK_INSERT:    scancode = 0x52; break;
        case SDLK_DELETE:    scancode = 0x53; break;
        case SDLK_LALT:      scancode = 0x38; break;  // ALTERNATE
        default: return;
    }

    // ACIA make/break protocol: make = scancode, break = scancode | 0x80
    acia_kbd_data_ = pressed ? scancode : (scancode | 0x80);
    acia_kbd_status_ |= 0x01;  // Set RX full
}

template<AtariSTVariant V>
bool AtariSTSystem<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("atari_st", rom_root, sizeof(rom_root))) {
        log_info("Atari ST: Could not find ROM root folder\n");
        return false;
    }

    uint8_t* tos = board_.tos_rom.data();
    if (!tos) return false;

    // Try to load TOS ROM using rom_loader
    if (rom_loader_load_from_root(rom_root,
            "tos.img|tos104.img|tos102.img|TOS.IMG",
            atari_st_constants::TOS_ROM_SIZE, tos,
            atari_st_constants::TOS_ROM_SIZE)) {
        log_info("Atari ST: Loaded TOS ROM\n");
        return true;
    }
    return false;
}

// ============================================================================
// TEMPLATE INSTANTIATION
// ============================================================================

template class AtariSTSystem<AtariSTVariant::ST>;
template class AtariSTSystem<AtariSTVariant::STE>;
template class AtariSTSystem<AtariSTVariant::MEGA_ST>;

REGISTER_SYSTEM(atari_st_descriptor, [] {
    return std::make_unique<AtariSTSystem<AtariSTVariant::ST>>();
});

REGISTER_SYSTEM(atari_ste_descriptor, [] {
    return std::make_unique<AtariSTSystem<AtariSTVariant::STE>>();
});

REGISTER_SYSTEM(atari_mega_st_descriptor, [] {
    return std::make_unique<AtariSTSystem<AtariSTVariant::MEGA_ST>>();
});
