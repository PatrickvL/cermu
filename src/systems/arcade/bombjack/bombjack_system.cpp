/*
 * bombjack_system.cpp — Bomb Jack arcade system implementation
 */

#include "bombjack_system.h"
#include "../../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor bombjack_descriptor = {
    "Bomb Jack", "BombJack",
    "Tehkan Bomb Jack — dual Z80A, 3× AY-3-8910, 256×224 sprites+tiles (1984)",
    "bombjack", {"BombJack", "Bomb Jack"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

BombJackSystem::BombJackSystem()
    : EmulatedSystem()
    , main_pins_(BOMBJACK_BUS_DEFAULT_STATE)
    , sound_pins_(BOMBJACK_BUS_DEFAULT_STATE)
{
    HardwareTraits traits = {};
    traits.display.native_width    = bombjack_constants::FB_WIDTH;
    traits.display.native_height   = bombjack_constants::FB_HEIGHT;
    traits.display.visible_width   = bombjack_constants::FB_WIDTH;
    traits.display.visible_height  = bombjack_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = bombjack_constants::PALETTE_ENTRIES;
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = bombjack_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "3× AY-3-8910";
    traits.timing.cpu_frequency_hz = bombjack_constants::MAIN_CPU_FREQ_HZ;
    traits.timing.target_fps       = bombjack_constants::REFRESH_HZ;
    traits.timing.cycles_per_frame = bombjack_constants::MAIN_CYCLES_PER_FRAME;
    traits.timing.standard         = VideoStandard::NTSC;
    hardware_traits_ = traits;
    bombjack_descriptor.hardware_traits = traits;
}

BombJackSystem::~BombJackSystem() { delete main_cpu_; delete sound_cpu_; }

const SystemDescriptor& BombJackSystem::get_descriptor() const { return bombjack_descriptor; }
bool BombJackSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool BombJackSystem::apply_configuration() { return true; }

bool BombJackSystem::initialize() {
    printf("Bomb Jack: Initializing arcade system\n");

    // ── Create MemoryChip wrappers (main CPU) ────────────────────────────
    auto main_rom = std::make_unique<ROMChip>(
        ChipInfo{"ROM", "Tehkan"}, bombjack_constants::MAIN_ROM_SIZE,
        ROMChip::ROM, &main_pins_,
        "Program ROM", bombjack_constants::MAIN_ROM_BASE);
    main_rom_chip_ = main_rom.get();

    auto main_ram = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, bombjack_constants::MAIN_RAM_SIZE,
        RAMChip::SRAM, &main_pins_,
        "Work RAM", bombjack_constants::MAIN_RAM_BASE);
    main_ram_chip_ = main_ram.get();

    auto fg_tilemap = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, bombjack_constants::FG_TILEMAP_SIZE,
        RAMChip::SRAM, &main_pins_,
        "FG Tilemap", bombjack_constants::FG_TILEMAP_BASE);
    fg_tilemap_chip_ = fg_tilemap.get();

    auto fg_attr = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, bombjack_constants::FG_TILEMAP_SIZE,
        RAMChip::SRAM, &main_pins_,
        "FG Attributes", bombjack_constants::FG_ATTR_BASE);
    fg_attr_chip_ = fg_attr.get();

    auto sprite_area = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, 256,
        RAMChip::SRAM, &main_pins_,
        "Sprite Area", 0x9800);
    sprite_area_chip_ = sprite_area.get();

    auto palette_ram = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, bombjack_constants::PALETTE_RAM_SIZE,
        RAMChip::SRAM, &main_pins_,
        "Palette RAM", bombjack_constants::PALETTE_RAM_BASE);
    palette_ram_chip_ = palette_ram.get();

    // ── Create MemoryChip wrappers (sound CPU) ───────────────────────────
    auto sound_rom = std::make_unique<ROMChip>(
        ChipInfo{"ROM", "Tehkan"}, bombjack_constants::SOUND_ROM_SIZE,
        ROMChip::ROM, &sound_pins_,
        "Sound ROM", bombjack_constants::SOUND_ROM_BASE);
    sound_rom_chip_ = sound_rom.get();

    auto sound_ram = std::make_unique<RAMChip>(
        ChipInfo{"SRAM", "Various"}, bombjack_constants::SOUND_RAM_SIZE,
        RAMChip::SRAM, &sound_pins_,
        "Sound RAM", bombjack_constants::SOUND_RAM_BASE);
    sound_ram_chip_ = sound_ram.get();

    // ── Initialize buses ─────────────────────────────────────────────────
    main_bus_mem_.initialize(main_bus_,
        main_rom_chip_, main_ram_chip_, fg_tilemap_chip_,
        fg_attr_chip_, sprite_area_chip_, palette_ram_chip_);

    sound_bus_mem_.initialize(sound_bus_,
        sound_rom_chip_, sound_ram_chip_);

    // ── Init chips ───────────────────────────────────────────────────────
    main_cpu_  = new ZilogZ80A();
    sound_cpu_ = new ZilogZ80A();
    main_pins_  = main_cpu_->init();
    sound_pins_ = sound_cpu_->init();
    for (auto& ay : ay_) ay.init();

    load_roms();

    // ── Register chips for Hardware menu ─────────────────────────────────
    register_chip(static_cast<ChipBase*>(main_cpu_),
        "Main Z80A CPU", "Z80A", "CPU", 0x0000);
    register_chip(static_cast<ChipBase*>(sound_cpu_),
        "Sound Z80A CPU", "Z80A", "CPU", 0x0000);
    register_chip(std::move(main_rom));
    register_chip(std::move(main_ram));
    register_chip(std::move(fg_tilemap));
    register_chip(std::move(fg_attr));
    register_chip(std::move(sprite_area));
    register_chip(std::move(palette_ram));
    register_chip(std::move(sound_rom));
    register_chip(std::move(sound_ram));

    system_ready_ = true;
    return true;
}

void BombJackSystem::shutdown() {
    delete main_cpu_;  main_cpu_  = nullptr;
    delete sound_cpu_; sound_cpu_ = nullptr;
    system_ready_ = false;
}

void BombJackSystem::reset() {
    if (!main_cpu_ || !sound_cpu_) return;
    main_pins_  = main_cpu_->reset(main_pins_);
    sound_pins_ = sound_cpu_->reset(sound_pins_);
    for (auto& ay : ay_) ay.reset();
    sound_latch_ = 0;
    sound_nmi_ = false;
}

void BombJackSystem::tick() {
    if (!main_cpu_ || !sound_cpu_) return;

    // Main CPU tick
    main_pins_ = main_cpu_->tick(main_pins_);

    // Main CPU bus dispatch — memory-mapped only (no IORQ for main CPU)
    if (!BUS_GET_BIT(main_pins_, Z80_MREQ_BIT)) {
        uint16_t addr = BUS_GET_ADDR(main_pins_);
        if ((addr & 0xF000) == 0xB000) {
            main_pins_ = main_io_tick(main_pins_);
        } else {
            main_pins_ = main_bus_.tick(0, main_pins_);
        }
    }

    // Sound CPU runs at 3/4 speed (3 MHz vs 4 MHz main)
    // Tick sound CPU on 3 out of every 4 main cycles
    if (total_cycles_ % 4 != 3) {
        // Manage sound NMI line (edge-triggered: assert when latch written)
        if (sound_nmi_) {
            BUS_CLR_BIT(sound_pins_, BUS_NMI_BIT);
        } else {
            BUS_SET_BIT(sound_pins_, BUS_NMI_BIT);
        }

        sound_pins_ = sound_cpu_->tick(sound_pins_);

        // Sound CPU bus dispatch
        if (!BUS_GET_BIT(sound_pins_, Z80_MREQ_BIT)) {
            uint16_t addr = BUS_GET_ADDR(sound_pins_);
            if (addr == 0x6000) {
                // Sound latch read — clear NMI
                if (BUS_GET_BIT(sound_pins_, BUS_RW_BIT)) {
                    BUS_SET_DATA(sound_pins_, sound_latch_);
                    sound_nmi_ = false;
                }
            } else {
                sound_pins_ = sound_bus_.tick(0, sound_pins_);
            }
        } else if (!BUS_GET_BIT(sound_pins_, Z80_IORQ_BIT)) {
            sound_pins_ = sound_io_tick(sound_pins_);
        }

        // AY chips tick at ~1.5 MHz (sound CPU / 2)
        if (total_cycles_ & 1) {
            for (auto& ay : ay_) ay.tick();
        }
    }

    // VBLANK NMI to main CPU (edge-triggered, once per frame)
    uint32_t frame_cycle = total_cycles_ % bombjack_constants::MAIN_CYCLES_PER_FRAME;
    if (frame_cycle == 0 && total_cycles_ > 0) {
        BUS_CLR_BIT(main_pins_, BUS_NMI_BIT);  // Assert NMI (falling edge)
    } else if (frame_cycle == 1) {
        BUS_SET_BIT(main_pins_, BUS_NMI_BIT);  // Deassert NMI
    }

    total_cycles_++;
}

void BombJackSystem::run_frame() {
    for (uint32_t i = 0; i < bombjack_constants::MAIN_CYCLES_PER_FRAME; ++i) tick();
}

bool BombJackSystem::load_file(const char*) { return false; }
uint32_t* BombJackSystem::get_framebuffer() { return framebuffer_; }
void BombJackSystem::get_display_dimensions(int* w, int* h) const {
    *w = bombjack_constants::FB_WIDTH; *h = bombjack_constants::FB_HEIGHT;
}
void BombJackSystem::set_framebuffer(uint32_t*, int, int) {}
uint32_t BombJackSystem::get_audio_samples(float*, uint32_t) { return 0; }
void BombJackSystem::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void BombJackSystem::handle_keyboard_event(SDL_Keycode, bool) {}
void BombJackSystem::render_system_menu_items() {}
void BombJackSystem::render_configuration_ui() {}
void BombJackSystem::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// I/O DISPATCH — Main CPU ($B000-$BFFF memory-mapped registers)
// ============================================================================

bus_state_t BombJackSystem::main_io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);

    if (BUS_GET_BIT(pins, BUS_RW_BIT)) {
        // Reads: input ports and DIP switches
        uint8_t data = 0xFF;
        if (addr == bombjack_constants::INPUT_P1)         data = input_p1_;
        else if (addr == bombjack_constants::INPUT_P2)    data = input_p2_;
        else if (addr == bombjack_constants::INPUT_SYSTEM) data = input_system_;
        else if (addr == bombjack_constants::DSW1)        data = dsw1_;
        else if (addr == bombjack_constants::DSW2)        data = dsw2_;
        BUS_SET_DATA(pins, data);
    } else {
        // Writes: sound latch, background select, watchdog
        uint8_t data = BUS_GET_DATA(pins);
        if (addr == bombjack_constants::SOUND_LATCH) {
            sound_latch_ = data;
            sound_nmi_ = true;
        } else if (addr == bombjack_constants::BG_SELECT) {
            bg_image_select_ = data & 0x07;
        }
        // Watchdog write at $B800 is intentionally ignored
    }

    return pins;
}
bus_state_t BombJackSystem::sound_io_tick(bus_state_t pins) {
    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // Determine which AY chip and whether address or data register
    int ay_idx = -1;
    bool is_data = false;

    if ((port & 0xFE) == bombjack_constants::AY1_ADDR) {
        ay_idx = 0; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY2_ADDR) {
        ay_idx = 1; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY3_ADDR) {
        ay_idx = 2; is_data = port & 0x01;
    }

    if (ay_idx >= 0) {
        if (is_data) {
            if (is_read) {
                BUS_SET_DATA(pins, ay_[ay_idx].read_register());
            } else {
                ay_[ay_idx].write_register(data);
            }
        } else {
            if (!is_read) {
                ay_[ay_idx].latch_address(data);
            }
        }
    }

    return pins;
}
bool BombJackSystem::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bombjack_descriptor, [] { return std::make_unique<BombJackSystem>(); });
