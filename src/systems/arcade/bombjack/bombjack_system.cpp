/*
 * bombjack_system.cpp — Bomb Jack arcade system implementation
 */

#include "core/cermu.hpp"
#include "systems/arcade/bombjack/bombjack_system.hpp"
#include "core/rom_set.hpp"
#include "core/vfs/vfs.hpp"
#include "core/config/path_discovery.hpp"
#include "core/system_registry.hpp"
#include <cstring>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor bombjack_descriptor = {
    "Bomb Jack", "BombJack",
    "Tehkan Bomb Jack — dual Z80A, 3× AY-3-8910, 256×224 sprites+tiles (1984)",
    "bombjack", {"BombJack", "Bomb Jack"},
    nullptr, {}, nullptr,
    "Tehkan", 1984, "Z80A (x2)", SystemType::Arcade
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

BombJackSystem::BombJackSystem()
    : System()
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
    traits.display.rotation        = DisplayRotation::CW90;
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

BombJackSystem::~BombJackSystem() {}

const SystemDescriptor& BombJackSystem::get_descriptor() const { return bombjack_descriptor; }
bool BombJackSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool BombJackSystem::apply_configuration() { return true; }

bool BombJackSystem::initialize() {
    log_info("Bomb Jack: Initializing arcade system\n");
    register_board(&main_board_);

    // ── Bind value-typed chips, then create remaining ─────────────────
    bind_all(main_board_, main_board_.components_, kBombJackMainManifest);
    main_board_.create_chips(&main_pins_);
    main_board_.apply(main_bus_);

    bind_all(sound_board_, sound_board_.components_, kBombJackSoundManifest);
    sound_board_.create_chips(&sound_pins_);
    sound_board_.apply(sound_bus_);

    // ── Set port manifest (from main board manifest) ────────────────────
    port_manifest_       = kBombJackMainManifest.port_slots;
    port_manifest_count_ = kBombJackMainManifest.port_count;

    // ── Init chips ─────────────────────────────────────────────────────
    main_pins_  = main_board_.cpu.init();
    sound_pins_ = sound_board_.cpu.init();
    for (int i = 0; i < 3; i++) {
        ay_[i].init();
        // AY clock = sound CPU / 2 = 1.5 MHz
        ay_[i].set_clock_frequency(1500000);
        ay_[i].set_audio_sample_rate(bombjack_constants::DEFAULT_SAMPLE_RATE);
    }

    // Wire 3× AY to audio thread — cpu_cycles_per_tick=2 (AY = sound CPU / 2)
    for (int i = 0; i < 3; i++) {
        ay_adapter_[i] = std::make_unique<WriteOnlySynthAdapter<AY_3_8910, true>>(
            &ay_[i], 2);
        audio_thread_.register_engine(ay_adapter_[i].get());
        // Wire each AY to its own audio signal port
        audio_port_[i] = std::make_unique<AudioPort>();
        ay_[i].set_audio_port(audio_port_[i].get());
    }
    audio_thread_.start();

    load_roms();

    // ── GPU indexed palette rendering ───────────────────────────────
    palette_ = PaletteTable(bombjack_constants::PALETTE_ENTRIES);
    decode_palette();

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();

    // Video generator — models TTL video rendering hardware (all 3 layers)
    video_gen_.set_video_out(&video_port_->output());

    // Wire graphics ROMs to video generator (set once at init)
    if (!char_rom_.empty()) {
        video_gen_.set_char_rom(char_rom_.data(), static_cast<int>(char_rom_.size()));
    }
    if (!bg_tile_rom_.empty()) {
        int plane_size = static_cast<int>(bg_tile_rom_.size()) / 3;
        video_gen_.set_bg_tile_rom(
            bg_tile_rom_.data(),
            bg_tile_rom_.data() + plane_size,
            bg_tile_rom_.data() + plane_size * 2);
    }
    if (!bg_map_rom_.empty()) {
        video_gen_.set_bg_map_rom(bg_map_rom_.data());
    }
    if (!sprite_rom_.empty()) {
        int plane_size = static_cast<int>(sprite_rom_.size()) / 3;
        video_gen_.set_sprite_rom(
            sprite_rom_.data(),
            sprite_rom_.data() + plane_size,
            sprite_rom_.data() + plane_size * 2);
    }

    // Auto-reconstruct signal → framebuffer (palette is dynamically decoded)
    video_port_->bind_display(nullptr, palette_.data(),
                              bombjack_constants::FB_WIDTH, 0);
    video_port_->set_palette(palette_.data(), bombjack_constants::PALETTE_ENTRIES);
    video_port_->bind_frame_output(&last_frame_data_);

    // ── Register chips for Hardware menu ─────────────────────────────────

    register_bus_chips(main_board_);
    register_bus_chips(sound_board_);

    system_ready_ = true;
    return true;
}

void BombJackSystem::shutdown() {
    audio_thread_.stop();
    system_ready_ = false;
}

void BombJackSystem::reset() {
    main_pins_  = main_board_.cpu.reset(main_pins_);
    sound_pins_ = sound_board_.cpu.reset(sound_pins_);
    audio_thread_.stop();
    for (int i = 0; i < 3; i++) {
        ay_[i].reset();
        if (ay_adapter_[i]) ay_adapter_[i]->reset();
    }
    audio_thread_.start();
    sound_latch_ = 0;
    sound_nmi_ = false;
    sound_cycles_ = 0;
}

// ── Input ────────────────────────────────────────────────────────────────────

void BombJackSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    namespace bc = bombjack_constants;
    auto set_clear = [](uint8_t& reg, uint8_t mask, bool set) {
        if (set) reg |= mask; else reg &= ~mask;
    };

    switch (key) {
        // Player 1 — arrow keys + Z
        case SDLK_RIGHT: set_clear(input_p1_, bc::INPUT_RIGHT,   pressed); break;
        case SDLK_LEFT:  set_clear(input_p1_, bc::INPUT_LEFT,    pressed); break;
        case SDLK_UP:    set_clear(input_p1_, bc::INPUT_UP,      pressed); break;
        case SDLK_DOWN:  set_clear(input_p1_, bc::INPUT_DOWN,    pressed); break;
        case SDLK_z:     set_clear(input_p1_, bc::INPUT_BUTTON1, pressed); break;

        // Player 2 — WASD + X
        case SDLK_d:     set_clear(input_p2_, bc::INPUT_RIGHT,   pressed); break;
        case SDLK_a:     set_clear(input_p2_, bc::INPUT_LEFT,    pressed); break;
        case SDLK_w:     set_clear(input_p2_, bc::INPUT_UP,      pressed); break;
        case SDLK_s:     set_clear(input_p2_, bc::INPUT_DOWN,    pressed); break;
        case SDLK_x:     set_clear(input_p2_, bc::INPUT_BUTTON1, pressed); break;

        // System — coins and start
        case SDLK_5:     set_clear(input_system_, bc::SYSTEM_COIN1,  pressed); break;
        case SDLK_6:     set_clear(input_system_, bc::SYSTEM_COIN2,  pressed); break;
        case SDLK_1:     set_clear(input_system_, bc::SYSTEM_START1, pressed); break;
        case SDLK_2:     set_clear(input_system_, bc::SYSTEM_START2, pressed); break;

        default: break;
    }
}

void BombJackSystem::tick() {

    // Main CPU tick
    main_pins_ = main_board_.cpu.tick(main_pins_);

    // Main CPU bus dispatch — memory-mapped only (no IORQ for main CPU)
    if (!BUS_GET_BIT(main_pins_, Z80_MREQ_BIT)) {
        uint16_t addr = BUS_GET_ADDR(main_pins_);

        if ((addr & 0xF000) == 0xB000) {
            main_pins_ = main_io_tick(main_pins_);
        } else if (addr == bombjack_constants::BG_SELECT && !BUS_GET_BIT(main_pins_, BUS_RW_BIT)) {
            // $9E00 write — background image select (intercepted before bus dispatch)
            bg_image_select_ = BUS_GET_DATA(main_pins_) & 0x1F;
        } else {
            main_pins_ = main_bus_.tick(main_pins_);
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

        sound_pins_ = sound_board_.cpu.tick(sound_pins_);

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
                sound_pins_ = sound_bus_.tick(sound_pins_);
            }
        } else if (!BUS_GET_BIT(sound_pins_, Z80_IORQ_BIT)) {
            sound_pins_ = sound_io_tick(sound_pins_);
        }

        // AY synthesis is driven by the audio thread — track sound CPU cycles
        // for timestamping register writes.
        sound_cycles_++;
    }

    // VBLANK NMI to main CPU (edge-triggered, once per frame, gated by nmi_mask_)
    // Real hardware holds NMI low for the entire VBLANK period (~128 CPU cycles).
    // The Z80's edge-triggered NMI flip-flop latches on the falling edge.
    uint32_t frame_cycle = total_cycles_ % bombjack_constants::MAIN_CYCLES_PER_FRAME;
    if (frame_cycle == 0 && total_cycles_ > 0 && nmi_mask_) {
        BUS_CLR_BIT(main_pins_, BUS_NMI_BIT);  // Assert NMI (falling edge)
    } else if (frame_cycle == 128) {
        BUS_SET_BIT(main_pins_, BUS_NMI_BIT);  // Deassert NMI after VBLANK
    }

    total_cycles_++;
}

void BombJackSystem::run_frame() {
    for (uint32_t i = 0; i < bombjack_constants::MAIN_CYCLES_PER_FRAME; ++i) tick();
    audio_thread_.signal_progress(sound_cycles_);
    decode_palette();
    render_frame();
    if (video_port_) video_port_->swap_frame();

}

uint32_t BombJackSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    // AudioPort path: read from each AY's port and mix
    if (audio_port_[0]) {
        uint32_t avail = std::min({audio_port_[0]->available(),
                                   audio_port_[1]->available(),
                                   audio_port_[2]->available()});
        uint32_t count = std::min(avail, max_samples);
        if (count == 0) return 0;

        count = static_cast<uint32_t>(audio_port_[0]->read_samples(buffer, static_cast<int>(count)));

        constexpr uint32_t MIX_CHUNK = 256;
        float tmp[MIX_CHUNK];
        for (int chip = 1; chip < 3; chip++) {
            uint32_t remaining = count;
            uint32_t offset = 0;
            while (remaining > 0) {
                uint32_t n = std::min(MIX_CHUNK, remaining);
                audio_port_[chip]->read_samples(tmp, static_cast<int>(n));
                for (uint32_t i = 0; i < n; i++)
                    buffer[offset + i] += tmp[i];
                offset += n;
                remaining -= n;
            }
        }

        constexpr float inv3 = 1.0f / 3.0f;
        for (uint32_t i = 0; i < count; i++)
            buffer[i] *= inv3;

        return count;
    }

    // Legacy path: read from chip ring buffers
    // Read minimum available from all 3 AYs for synchronization
    uint32_t avail = std::min({ay_[0].audio_available(),
                               ay_[1].audio_available(),
                               ay_[2].audio_available()});
    uint32_t count = std::min(avail, max_samples);
    if (count == 0) return 0;

    // Read first AY into output buffer
    count = ay_[0].audio_read(buffer, count);

    // Mix in remaining AYs
    constexpr uint32_t MIX_CHUNK = 256;
    float tmp[MIX_CHUNK];
    for (int chip = 1; chip < 3; chip++) {
        uint32_t remaining = count;
        uint32_t offset = 0;
        while (remaining > 0) {
            uint32_t n = std::min(MIX_CHUNK, remaining);
            ay_[chip].audio_read(tmp, n);
            for (uint32_t i = 0; i < n; i++)
                buffer[offset + i] += tmp[i];
            offset += n;
            remaining -= n;
        }
    }

    // Normalize: average of 3 channels
    constexpr float inv3 = 1.0f / 3.0f;
    for (uint32_t i = 0; i < count; i++)
        buffer[i] *= inv3;

    return count;
}
void BombJackSystem::set_audio_sample_rate(int hz) {
    audio_sample_rate_ = hz;
    for (int i = 0; i < 3; i++) ay_[i].set_audio_sample_rate(hz);
}

// ============================================================================
// PALETTE DECODE — rebuild RGBA palette from palette RAM each frame
// ============================================================================
//
// Palette RAM at $9C00: 128 colors × 2 bytes = 256 bytes.
// Even byte: GGGGRRRR (green high nibble, red low nibble)
// Odd byte:  xxxxBBBB (blue low nibble, upper 4 bits unused)
// Each 4-bit channel maps through a resistor ladder DAC.
//
// The resistor weights produce the following 4-bit → 8-bit mapping:
//   val = (bit3 * 0x80) + (bit2 * 0x40) + (bit1 * 0x20) + (bit0 * 0x10)
// Approximated as: (nibble << 4) | nibble  (full-range expansion).

void BombJackSystem::decode_palette() {
    const uint8_t* pal = main_board_.palette.data();
    for (int i = 0; i < bombjack_constants::PALETTE_ENTRIES; i++) {
        uint8_t lo = pal[i * 2];       // GGGGRRRR
        uint8_t hi = pal[i * 2 + 1];   // xxxxBBBB
        uint8_t r4 = lo & 0x0F;
        uint8_t g4 = (lo >> 4) & 0x0F;
        uint8_t b4 = hi & 0x0F;
        uint8_t r = (r4 << 4) | r4;
        uint8_t g = (g4 << 4) | g4;
        uint8_t b = (b4 << 4) | b4;
        // PaletteTable format: 0xAABBGGRR (GL_RGBA byte order on little-endian)
        palette_.set_entry(i, 0xFF000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | r);
    }
}

// ============================================================================
// VIDEO RENDERING — delegate to BombJackVideo (3-layer TTL pipeline)
// ============================================================================

void BombJackSystem::render_frame() {
    if (char_rom_.empty()) return;

    video_gen_.set_tilemap(main_board_.fg_map.data());
    video_gen_.set_attr_map(main_board_.fg_attr.data());
    video_gen_.set_sprite_ram(main_board_.sprites.data());
    video_gen_.set_bg_image_select(bg_image_select_);
    video_gen_.render_frame();
}

// ============================================================================
// I/O DISPATCH — Main CPU ($B000-$BFFF memory-mapped registers)
// ============================================================================

bus_state_t BombJackSystem::main_io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);

    if (addr & 0x0800) {
        // $B800-$BFFF: Sound latch (write-only, mirrored)
        if (!BUS_GET_BIT(pins, BUS_RW_BIT)) {
            sound_latch_ = BUS_GET_DATA(pins);
            sound_nmi_ = true;
        }
        return pins;
    }

    // $B000-$B7FF: I/O registers (mirrored every 8 bytes, low 3 bits select register)
    uint8_t reg = addr & 0x07;

    if (BUS_GET_BIT(pins, BUS_RW_BIT)) {
        // Reads: input ports and DIP switches
        uint8_t data = 0xFF;
        switch (reg) {
            case 0: data = input_p1_; break;                        // P1
            case 1: data = input_p2_; break;                        // P2
            case 2: data = input_system_; break;                    // SYSTEM (coins/start)
            case 3: break;                                          // Watchdog (read resets timer)
            case 4: data = main_board_.dsw1.bank.value; break;      // SW1
            case 5: data = main_board_.dsw2.bank.value; break;      // SW2
            default: break;                                         // 6,7: unused
        }
        BUS_SET_DATA(pins, data);
    } else {
        // Writes: NMI mask, flip screen
        uint8_t data = BUS_GET_DATA(pins);
        switch (reg) {
            case 0: nmi_mask_ = data & 0x01; break;
            case 4: /* flip screen — ignored for now */ break;      // Cocktail mode
            default: break;
        }
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
                // Shadow write for immediate readback, enqueue for audio thread
                ay_[ay_idx].write_register_shadow(data);
                ay_adapter_[ay_idx]->cmd_queue().push_write(
                    sound_cycles_, ay_latch_[ay_idx], data);
            }
        } else {
            if (!is_read) {
                ay_latch_[ay_idx] = data & 0x0F;
                ay_[ay_idx].latch_address(data);
            }
        }
    }

    return pins;
}
bool BombJackSystem::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("bombjack", rom_root, sizeof(rom_root)))
        return false;

    auto descriptors = get_rom_set_descriptors();
    if (descriptors.empty()) return false;

    // Scan the ROM directory for archive files containing a valid ROM set
    auto entries = vfs_list_entries(rom_root);
    for (const auto& entry : entries) {
        if (entry.type != VfsEntryType::Archive) continue;
        auto match = rom_set_scan_and_match(
            entry.full_path.c_str(), descriptors.data(),
            static_cast<int>(descriptors.size()));
        if (match.matched)
            return load_rom_set(match);
    }
    return false;
}

// ============================================================================
// ROM SET — Bomb Jack MAME ROM set definitions
// ============================================================================
//
// bombjack.zip (Set 1):
//   EPROMs 09–12 (suffix b): main CPU $0000–$7FFF (4×8KB)
//   EPROM 13 (13.1r):       main CPU $C000–$DFFF (8KB)
//   EPROM 01 (suffix t):     sound CPU $0000–$1FFF (8KB)
//   EPROMs 03–05 (suffix t): foreground character ROMs, 3 planes ×4KB
//   EPROMs 06–08 (suffix t): background tile ROMs, 3 planes ×8KB
//   EPROM 02 (suffix t):     background map ROM (4KB)
//   EPROMs 14–16 (suffix b): sprite ROMs, 3 planes ×8KB
//
// bombjac2.zip (Set 2):
//   Identical to Set 1 except EPROM 13 is named 13_r01b.bin

// ROM entry indices (for the write callback)
enum BjRomEntry {
    BJ_MAIN_1 = 0,   // 09_j01b.bin   $0000
    BJ_MAIN_2,       // 10_l01b.bin   $2000
    BJ_MAIN_3,       // 11_m01b.bin   $4000
    BJ_MAIN_4,       // 12_n01b.bin   $6000
    BJ_MAIN_5,       // 13.1r         $C000
    BJ_SOUND,        // 01_h03t.bin
    BJ_CHAR_P0,      // 03_e08t.bin
    BJ_CHAR_P1,      // 04_h08t.bin
    BJ_CHAR_P2,      // 05_k08t.bin
    BJ_TILE_P0,      // 06_l08t.bin
    BJ_TILE_P1,      // 07_n08t.bin
    BJ_TILE_P2,      // 08_r08t.bin
    BJ_MAP,          // 02_p04t.bin
    BJ_SPR_P0,       // 16_m07b.bin
    BJ_SPR_P1,       // 15_l07b.bin
    BJ_SPR_P2,       // 14_j07b.bin
    BJ_ENTRY_COUNT
};

static const RomEntryDescriptor bj_set1_entries[] = {
    // Main CPU program ROMs ($0000–$7FFF loaded into 32KB ROMChip)
    { {"09_j01b"},   0x0000, 8192, true  },
    { {"10_l01b"},   0x2000, 8192, true  },
    { {"11_m01b"},   0x4000, 8192, true  },
    { {"12_n01b"},   0x6000, 8192, true  },
    { {"13.1r"},     0xC000, 8192, true  },  // Set 1 name
    // Sound CPU ROM
    { {"01_h03t"},   0x10000, 8192, true },  // address > $FFFF = sound CPU
    // FG character ROMs (3 planes × 4KB)
    { {"03_e08t"},   0x20000, 4096, true },
    { {"04_h08t"},   0x20000, 4096, true },
    { {"05_k08t"},   0x20000, 4096, true },
    // BG tile ROMs (3 planes × 8KB)
    { {"06_l08t"},   0x30000, 8192, true },
    { {"07_n08t"},   0x30000, 8192, true },
    { {"08_r08t"},   0x30000, 8192, true },
    // BG map ROM
    { {"02_p04t"},   0x40000, 4096, true },
    // Sprite ROMs (3 planes × 8KB)
    { {"16_m07b"},   0x50000, 8192, true },
    { {"15_l07b"},   0x50000, 8192, true },
    { {"14_j07b"},   0x50000, 8192, true },
};

static const RomSetDescriptor bj_romset_1 = {
    "Bomb Jack (Set 1)", "BombJack",
    bj_set1_entries, BJ_ENTRY_COUNT
};

static const RomEntryDescriptor bj_set2_entries[] = {
    { {"09_j01b"},   0x0000, 8192, true  },
    { {"10_l01b"},   0x2000, 8192, true  },
    { {"11_m01b"},   0x4000, 8192, true  },
    { {"12_n01b"},   0x6000, 8192, true  },
    { {"13_r01b"},   0xC000, 8192, true  },  // Set 2 name
    { {"01_h03t"},   0x10000, 8192, true },
    { {"03_e08t"},   0x20000, 4096, true },
    { {"04_h08t"},   0x20000, 4096, true },
    { {"05_k08t"},   0x20000, 4096, true },
    { {"06_l08t"},   0x30000, 8192, true },
    { {"07_n08t"},   0x30000, 8192, true },
    { {"08_r08t"},   0x30000, 8192, true },
    { {"02_p04t"},   0x40000, 4096, true },
    { {"16_m07b"},   0x50000, 8192, true },
    { {"15_l07b"},   0x50000, 8192, true },
    { {"14_j07b"},   0x50000, 8192, true },
};

static const RomSetDescriptor bj_romset_2 = {
    "Bomb Jack (Set 2)", "BombJack",
    bj_set2_entries, BJ_ENTRY_COUNT
};

std::vector<const RomSetDescriptor*> BombJackSystem::get_rom_set_descriptors() const {
    return { &bj_romset_1, &bj_romset_2 };
}

bool BombJackSystem::load_file(const char* filepath) {
    if (!filepath) return false;

    // Determine the archive path — either the filepath itself is a .zip,
    // or it's an archive-internal path (e.g. "bombjack.zip!/01_h03t.bin")
    // from the main startup's scan_archive() resolution.
    std::string archive_path;
    if (vfs_is_archive_path(filepath)) {
        // Extract parent archive: everything before "!/"
        archive_path = vfs_parent_path(filepath);
    } else {
        std::string ext = vfs_extension(filepath);
        if (!ext.empty() && vfs_is_archive_extension(ext.c_str()))
            archive_path = filepath;
    }

    if (!archive_path.empty()) {
        auto descriptors = get_rom_set_descriptors();
        auto match = rom_set_scan_and_match(
            archive_path.c_str(), descriptors.data(), static_cast<int>(descriptors.size()));
        if (match.matched)
            return load_rom_set(match);
    }
    return false;
}

bool BombJackSystem::load_rom_set(const RomSetMatch& match) {
    if (!match.matched) return false;
    if (!system_ready_) { if (!initialize()) return false; }

    // Prepare buffers for graphics ROMs (concatenated planes)
    char_rom_.resize(4096 * 3);     // 3 planes × 4KB
    bg_tile_rom_.resize(8192 * 3);  // 3 planes × 8KB
    bg_map_rom_.resize(4096);       // 1 × 4KB
    sprite_rom_.resize(8192 * 3);   // 3 planes × 8KB

    bool ok = rom_set_load_matched(match, [this](uint32_t load_address,
                                                  const uint8_t* data,
                                                  size_t size,
                                                  int entry_index) -> bool {
        switch (entry_index) {
            case BJ_MAIN_1: case BJ_MAIN_2: case BJ_MAIN_3: case BJ_MAIN_4: {
                // Main CPU program ROM ($0000–$7FFF) — 4×8KB into 32KB ROMChip
                uint32_t offset = load_address;
                if (offset + size <= main_board_.rom.size_bytes()) {
                    std::memcpy(main_board_.rom.data() + offset, data, size);
                }
                return true;
            }
            case BJ_MAIN_5: {
                // Main CPU program ROM 2 ($C000–$DFFF)
                std::memcpy(main_board_.rom2.data(), data, std::min(size, main_board_.rom2.size_bytes()));
                return true;
            }
            case BJ_SOUND: {
                // Sound CPU ROM
                std::memcpy(sound_board_.rom.data(), data, std::min(size, sound_board_.rom.size_bytes()));
                return true;
            }
            case BJ_CHAR_P0: std::memcpy(char_rom_.data(),              data, std::min(size, (size_t)4096)); return true;
            case BJ_CHAR_P1: std::memcpy(char_rom_.data() + 4096,      data, std::min(size, (size_t)4096)); return true;
            case BJ_CHAR_P2: std::memcpy(char_rom_.data() + 8192,      data, std::min(size, (size_t)4096)); return true;
            case BJ_TILE_P0: std::memcpy(bg_tile_rom_.data(),           data, std::min(size, (size_t)8192)); return true;
            case BJ_TILE_P1: std::memcpy(bg_tile_rom_.data() + 8192,   data, std::min(size, (size_t)8192)); return true;
            case BJ_TILE_P2: std::memcpy(bg_tile_rom_.data() + 16384,  data, std::min(size, (size_t)8192)); return true;
            case BJ_MAP:     std::memcpy(bg_map_rom_.data(),            data, std::min(size, (size_t)4096)); return true;
            case BJ_SPR_P0:  std::memcpy(sprite_rom_.data(),            data, std::min(size, (size_t)8192)); return true;
            case BJ_SPR_P1:  std::memcpy(sprite_rom_.data() + 8192,    data, std::min(size, (size_t)8192)); return true;
            case BJ_SPR_P2:  std::memcpy(sprite_rom_.data() + 16384,   data, std::min(size, (size_t)8192)); return true;
            default: return true;
        }
    });

    if (!ok) {
        log_error("Bomb Jack: Failed to load ROM set '%s'\n", match.rom_set->name);
        return false;
    }

    // Wire graphics ROMs to video generator
    video_gen_.set_char_rom(char_rom_.data(), static_cast<int>(char_rom_.size()));
    int tile_plane = static_cast<int>(bg_tile_rom_.size()) / 3;
    video_gen_.set_bg_tile_rom(
        bg_tile_rom_.data(),
        bg_tile_rom_.data() + tile_plane,
        bg_tile_rom_.data() + tile_plane * 2);
    video_gen_.set_bg_map_rom(bg_map_rom_.data());
    int spr_plane = static_cast<int>(sprite_rom_.size()) / 3;
    video_gen_.set_sprite_rom(
        sprite_rom_.data(),
        sprite_rom_.data() + spr_plane,
        sprite_rom_.data() + spr_plane * 2);

    // Reset CPUs with new ROM data
    main_pins_  = main_board_.cpu.reset(main_pins_);
    sound_pins_ = sound_board_.cpu.reset(sound_pins_);

    program_title_ = match.rom_set->name;
    log_info("Bomb Jack: Loaded ROM set '%s'\n", match.rom_set->name);
    return true;
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bombjack_descriptor, [] { return std::make_unique<BombJackSystem>(); });
