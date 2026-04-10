/*
 * msx_system.cpp — MSX1 / MSX2 / MSX2+ system implementation
 *
 * Tick loop:
 *   Each call to tick() advances the Z80 by one T-state.
 *   The VDP is clocked per dot (342 dots/line).
 *   The AY-3-8910 is clocked at CPU_FREQ / 16 internally.
 *
 * I/O map:
 *   $98:  VDP data read/write
 *   $99:  VDP status read / control write
 *   $9A:  V9938+ palette write
 *   $9B:  V9938+ indirect register access
 *   $A0:  PSG address latch (write)
 *   $A1:  PSG data write
 *   $A2:  PSG data read
 *   $A8:  PPI Port A — primary slot select
 *   $A9:  PPI Port B — keyboard column data
 *   $AA:  PPI Port C — keyboard row select, cassette, caps LED
 *   $AB:  PPI control word
 */

#include "core/cermu.hpp"
#include "systems/msx/msx_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_registry.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/vfs/vfs.hpp"
#include "utils/keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"
#include <cstring>
#include <cstdio>
#include <vector>



// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<MSXVariant V>
static HardwareTraits create_msx_hardware_traits() {
    using Traits = MSXVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = Traits::display_w;
    traits.display.native_height   = Traits::display_h;
    traits.display.visible_width   = Traits::display_w;
    traits.display.visible_height  = Traits::display_h;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = msx_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8910";

    traits.timing.cpu_frequency_hz   = msx_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = msx_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = msx_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = msx_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor msx1_descriptor = {
    "MSX1", "MSX1",
    "MSX1 — Z80A, TMS9918A, AY-3-8910, i8255 PPI (1983)",
    "msx", {"MSX", "MSX1"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX1>(),
    nullptr,
    "Microsoft", 1983, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor msx2_descriptor = {
    "MSX2", "MSX2",
    "MSX2 — Z80A, V9938, AY-3-8910, i8255 PPI (1985)",
    "msx", {"MSX2"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX2>(),
    nullptr,
    "Microsoft", 1985, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor msx2p_descriptor = {
    "MSX2+", "MSX2+",
    "MSX2+ — Z80A, V9958, AY-3-8910, i8255 PPI (1988)",
    "msx", {"MSX2+", "MSX2Plus"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX2P>(),
    nullptr,
    "Microsoft", 1988, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<MSXVariant V>
MSXSystem<V>::MSXSystem()
    : System()
    , pins_(MSX_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_msx_hardware_traits<V>();
}

template<MSXVariant V>
MSXSystem<V>::~MSXSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<MSXVariant V>
const SystemDescriptor& MSXSystem<V>::get_descriptor() const {
    if constexpr (V == MSXVariant::MSX1) return msx1_descriptor;
    else if constexpr (V == MSXVariant::MSX2) return msx2_descriptor;
    else return msx2p_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<MSXVariant V>
bool MSXSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // Bind value-typed Chips members, then factory-create remaining (RAM/ROM)
    bind_all(board_, board_.components_, BT::kManifest);
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Set port manifest ───────────────────────────────────────────────
    port_manifest_       = BT::kManifest.port_slots;
    port_manifest_count_ = BT::kManifest.port_count;

    // Configure memory map
    configure_bus_memory_map();

    // Init chips
    pins_ = board_.z80.init();
    board_.psg.init();
    board_.ppi.init();

    // AY clock: PSG runs at CPU_FREQ / 16 internally, but we tick it
    // at CPU rate and let the chip handle internal division
    board_.psg.set_clock_frequency(msx_constants::CPU_FREQ_HZ / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);

    // Audio setup
    audio_sample_period_ = msx_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Keyboard init — all keys released (active-low)
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    // PPI Port B read callback — returns keyboard column data
    board_.ppi.set_port_b_read_callback(
        [](void* ctx, uint8_t /*port_a*/) -> uint8_t {
            auto* sys = static_cast<MSXSystem*>(ctx);
            uint8_t row = sys->board_.ppi.get_port_c_output() & 0x0F;
            if (row < msx_constants::KEYBOARD_ROWS)
                return sys->keyboard_matrix_[row];
            return 0xFF;
        }, this);

    // Load ROMs
    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    // Register chips for Hardware menu
    register_bus_chips(board_);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(msx_constants::DEFAULT_SAMPLE_RATE,
                           msx_constants::DEFAULT_SAMPLE_RATE);

    log_info("%s: System initialized (RAM: %dKB)\n", Traits::name,
           Traits::ram_size / 1024);
    system_ready_ = true;
    return true;
}

template<MSXVariant V>
void MSXSystem<V>::shutdown() {
    system_ready_ = false;
}

template<MSXVariant V>
void MSXSystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    slot_select_ = 0xF0;  // pages 0-1: BIOS, pages 2-3: RAM
    bus_.load_snapshot(0, slot_snapshots_[slot_select_]);
    frame_tstate_counter_ = 0;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    board_.ppi.init();
    board_.vdp.reset();
    board_.psg.init();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::tick() {
    // VDP tick — dot clock is ~3× CPU clock, but for simplicity
    // we tick the VDP once per CPU T-state (approximate)
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.vdp.tick(vdp_bus);

    // Check VDP interrupt
    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);  // Assert INT (active-low)
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick — AY runs at CPU/16, but we tick at CPU rate
    // and let generate_sample handle downsampling
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        board_.psg.tick();
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = board_.psg.get_sample();
        audio_ring_buf_.write(&sample, 1);
        if (audio_port_) audio_port_->drive_sample(sample);
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

template<MSXVariant V>
void MSXSystem<V>::run_frame() {
    if (!video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::configure_bus_memory_map() {
    // Register all chips with the bus (slot records, flat_mem allocation).
    // This maps every slot at its base_addr; we immediately override the
    // page tables with precalculated snapshots.
    board_.apply(bus_);

    // Build 256 slot snapshots (one per PPI Port A value)
    generate_slot_snapshots();

    // Default MSX slot layout:
    //   Pages 0-1 ($0000-$7FFF): BIOS ROM (slot 0)
    //   Pages 2-3 ($8000-$FFFF): Main RAM  (slot 3)
    slot_select_ = 0xF0;   // pp0=0, pp1=0, pp2=3, pp3=3
    bus_.load_snapshot(0, slot_snapshots_[slot_select_]);
}

// ============================================================================
// SLOT SNAPSHOT GENERATION
// ============================================================================
//
// Precalculate one ModeSnapshot per possible PPI Port A value (0-255).
// Each snapshot is a frozen page table mapping every 256-byte page to the
// correct chip ID.  Switching the slot register at runtime is a single
// load_snapshot() call — O(1) memcpy, no per-page branching.
//
// Slot mapping:
//   Slot 0: BIOS+BASIC ROM (32 KB, pages 0-1 only)
//   Slot 1: Cartridge ROM  (up to 64 KB, loaded at runtime)
//   Slot 2: Expansion      (open bus for now)
//   Slot 3: Main RAM       (full address space, writable)

template<MSXVariant V>
void MSXSystem<V>::generate_slot_snapshots() {
    using ChipId      = typename Bus::ChipId;
    using WriteChipId = typename Bus::WriteChipId;

    constexpr auto& manifest = BT::kManifest;
    constexpr size_t kPgBits       = BT::Spec::PageBits;   // 8
    constexpr size_t kPagesPerSlot = 16384 >> kPgBits;      // 64 pages per 16 KB page

    // Chip base IDs (manifest indices: 0=Z80, 1=BIOS, 2=Cart, 3=RAM)
    constexpr size_t bios_base = manifest.base_id(1, kPgBits);
    constexpr size_t cart_base = manifest.base_id(2, kPgBits);
    constexpr size_t ram_base  = manifest.base_id(3, kPgBits);

    // BIOS ROM coverage: 32 KB = 2 × 16 KB pages
    constexpr size_t bios_16k_pages = manifest.chips[1].size_bytes >> 14;

    for (uint32_t ppi = 0; ppi < 256; ppi++) {
        bus_.reset_viewer(0);

        for (int page = 0; page < 4; page++) {
            uint8_t slot     = (ppi >> (page * 2)) & 0x03;
            size_t  first_pg = static_cast<size_t>(page) * kPagesPerSlot;

            switch (slot) {
            case 0: // BIOS+BASIC ROM (32 KB — pages 0-1 only)
                if (static_cast<size_t>(page) < bios_16k_pages) {
                    bus_.fill_read_pages(0, first_pg, kPagesPerSlot,
                        ChipId(bios_base + page * kPagesPerSlot));
                }
                // Pages beyond 32 KB: open bus (no-chip from reset_viewer)
                break;

            case 1: // Cartridge ROM
                if (cart_loaded_ &&
                    page >= cart_start_page_ && page < cart_end_page_) {
                    size_t rom_offset = static_cast<size_t>(
                        page - cart_start_page_) * kPagesPerSlot;
                    bus_.fill_read_pages(0, first_pg, kPagesPerSlot,
                        ChipId(cart_base + rom_offset));
                }
                // No cart / page outside cart: open bus
                break;

            case 2: // Expansion slot (open bus)
                break;

            case 3: { // Main RAM
                constexpr auto& ram_slot = manifest.chips[3];
                if constexpr (ram_slot.bank_size > 0) {
                    // Banked RAM (MSX2: 128 KB / 8 × 16 KB banks)
                    // Identity mapping: page N → bank N
                    auto bank_id = ChipId(ram_base + page);
                    bus_.fill_read_constant(0, first_pg, kPagesPerSlot, bank_id);
                    bus_.fill_write_constant(0, first_pg, kPagesPerSlot,
                        WriteChipId(ram_base + page));
                } else {
                    // Flat RAM — each 256-byte page gets its own chip ID
                    bus_.fill_pages(0, first_pg, kPagesPerSlot,
                        ChipId(ram_base + page * kPagesPerSlot),
                        WriteChipId(ram_base + page * kPagesPerSlot));
                }
                break;
            }
            }
        }
        bus_.save_snapshot(0, slot_snapshots_[ppi]);
    }
    log_info("%s: Generated 256 slot snapshots (cart=%s)\n",
             Traits::name, cart_loaded_ ? "yes" : "no");
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<MSXVariant V>
bus_state_t MSXSystem<V>::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // VDP ports $98-$9B
    if (port >= msx_constants::VDP_DATA_PORT && port <= msx_constants::VDP_INDIRECT_PORT) {
        // Set up a bus word with port offset in address for VDP static dispatch
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, port - msx_constants::VDP_DATA_PORT);
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = VDP::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            VDP::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // PSG ports $A0-$A2
    if (port == msx_constants::PSG_ADDR_PORT && !is_read) {
        board_.psg.latch_address(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == msx_constants::PSG_DATA_WRITE_PORT && !is_read) {
        board_.psg.write_register(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == msx_constants::PSG_DATA_READ_PORT && is_read) {
        BUS_SET_DATA(pins, board_.psg.read_register());
        return pins;
    }

    // PPI ports $A8-$AB
    if (port >= msx_constants::PPI_PORT_A && port <= msx_constants::PPI_CONTROL) {
        if (is_read) {
            BUS_SET_DATA(pins, board_.ppi.read(port - msx_constants::PPI_PORT_A));
        } else {
            board_.ppi.write(port - msx_constants::PPI_PORT_A, BUS_GET_DATA(pins));
            // Any PPI write can potentially change Port A output
            // (direct write to $A8, or mode change via $AB control word)
            uint8_t new_slot = board_.ppi.get_port_a_output();
            if (new_slot != slot_select_) {
                slot_select_ = new_slot;
                bus_.load_snapshot(0, slot_snapshots_[slot_select_]);
            }
        }
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::load_file(const char* filepath) {
    if (!filepath) return false;

    // Try format registry first (handles CAS tape files)
    {
        format_apply_config_t cfg{};
        cfg.ram         = board_.main_ram.data();
        cfg.ram_size    = 0x10000;  // 64 KB
        cfg.cpu         = &board_.z80;
        cfg.system_name = Traits::name;

        format_load_result_t result;
        if (format_load_file(filepath, &result)) {
            bool ok = format_apply_program(result, cfg);
            result.release();
            if (ok) return true;
        }
    }

    // Fall through to ROM cartridge loading

    size_t file_size = 0;
    VfsData file_data(vfs_read_file(filepath, &file_size));
    if (!file_data) {
        log_info("%s: Cannot open file: %s\n", Traits::name, filepath);
        return false;
    }

    if (file_size == 0 || file_size > 65536) {
        log_info("%s: Invalid ROM size (%zu bytes): %s\n",
                 Traits::name, file_size, filepath);
        return false;
    }

    // Determine cartridge page placement based on size:
    //   ≤32 KB ROMs → $4000 (pages 1-2)
    //   >32 KB ROMs → $0000 (pages 0-3)
    cart_size_ = static_cast<uint32_t>(file_size);
    if (cart_size_ <= 32768) {
        cart_start_page_ = 1;
    } else {
        cart_start_page_ = 0;
    }
    uint32_t pages_needed = (cart_size_ + 16383) / 16384;
    cart_end_page_ = cart_start_page_ + static_cast<uint8_t>(pages_needed);
    if (cart_end_page_ > 4) cart_end_page_ = 4;

    // Copy ROM data into the cartridge ROM chip buffer
    uint8_t* cart_buf = board_.cart_rom.data();
    if (!cart_buf) {
        log_info("%s: Cart ROM chip has no data buffer\n", Traits::name);
        return false;
    }
    std::memset(cart_buf, 0xFF, 65536);  // fill unused space with $FF
    std::memcpy(cart_buf, file_data.get(), file_size);
    cart_loaded_ = true;

    // Regenerate snapshots with cartridge mapped, reload current config
    generate_slot_snapshots();
    bus_.load_snapshot(0, slot_snapshots_[slot_select_]);

    log_info("%s: Loaded %u byte ROM at page %d-%d: %s\n",
             Traits::name, cart_size_, cart_start_page_, cart_end_page_ - 1, filepath);

    reset();
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<MSXVariant V>
uint32_t MSXSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

template<MSXVariant V>
void MSXSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = msx_constants::CPU_FREQ_HZ / audio_sample_rate_;
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // MSX keyboard matrix: 11 rows × 8 columns, active-low
    // Row selected by PPI Port C bits 0-3
    static const KeyMatrixEntry entries[] = {
        // Row 0: 0-7
        { 0, 0, '0', 0 }, { 0, 1, '1', 0 }, { 0, 2, '2', 0 }, { 0, 3, '3', 0 },
        { 0, 4, '4', 0 }, { 0, 5, '5', 0 }, { 0, 6, '6', 0 }, { 0, 7, '7', 0 },
        // Row 1: 8-9, -, =, etc
        { 1, 0, '8', 0 }, { 1, 1, '9', 0 }, { 1, 2, '-', 0 }, { 1, 3, '=', 0 },
        { 1, 4, '\\', 0 }, { 1, 5, '[', 0 }, { 1, 6, ']', 0 },
        { 1, 7, ';', 0 },
        // Row 2: A-H
        { 2, 0, 'a', 0 }, { 2, 1, 'b', 0 }, { 2, 2, 'c', 0 }, { 2, 3, 'd', 0 },
        { 2, 4, 'e', 0 }, { 2, 5, 'f', 0 }, { 2, 6, 'g', 0 }, { 2, 7, 'h', 0 },
        // Row 3: I-P
        { 3, 0, 'i', 0 }, { 3, 1, 'j', 0 }, { 3, 2, 'k', 0 }, { 3, 3, 'l', 0 },
        { 3, 4, 'm', 0 }, { 3, 5, 'n', 0 }, { 3, 6, 'o', 0 }, { 3, 7, 'p', 0 },
        // Row 4: Q-X
        { 4, 0, 'q', 0 }, { 4, 1, 'r', 0 }, { 4, 2, 's', 0 }, { 4, 3, 't', 0 },
        { 4, 4, 'u', 0 }, { 4, 5, 'v', 0 }, { 4, 6, 'w', 0 }, { 4, 7, 'x', 0 },
        // Row 5: Y, Z
        { 5, 0, 'y', 0 }, { 5, 1, 'z', 0 },
        // Row 6: SHIFT, CTRL, GRAPH, CAPS, F1-F3
        { 6, 0, UKEY_SHIFT_L, 0 },
        { 6, 1, UKEY_CTRL_L, 0 },
        { 6, 2, UKEY_ALT_L, 0 },     // GRAPH
        { 6, 3, UKEY_CAPS_LOCK, 0 },
        { 6, 5, UKEY_F1, 0 }, { 6, 6, UKEY_F2, 0 }, { 6, 7, UKEY_F3, 0 },
        // Row 7: F4-F5, ESC, TAB, STOP, BS, SELECT, ENTER
        { 7, 0, UKEY_F4, 0 }, { 7, 1, UKEY_F5, 0 },
        { 7, 2, '\x1B', 0 }, { 7, 3, '\t', 0 },
        { 7, 4, UKEY_END, 0 },       // STOP
        { 7, 5, '\b', 0 },
        { 7, 6, UKEY_HOME, 0 },      // SELECT
        { 7, 7, '\r', 0 },
        // Row 8: SPACE, cursor keys
        { 8, 0, ' ', 0 },
        { 8, 5, UKEY_CURSOR_UP, 0 }, { 8, 6, UKEY_CURSOR_DOWN, 0 },
        { 8, 7, UKEY_CURSOR_LEFT, 0 },
        // Row 9
        { 9, 0, UKEY_CURSOR_RIGHT, 0 },
        { 9, 1, UKEY_INSERT, 0 }, { 9, 2, '\x7F', 0 },
        // Row 10: period, comma, slash, quote, backquote
        { 10, 0, '.', 0 }, { 10, 1, ',', 0 },
        { 10, 2, '/', 0 }, { 10, 3, '\'', 0 },
        { 10, 4, '`', 0 },
    };

    static const HostKeyBinding bindings[] = {
        { SDLK_LSHIFT,   UKEY_SHIFT_L      },
        { SDLK_RSHIFT,   UKEY_SHIFT_L      },
        { SDLK_LCTRL,    UKEY_CTRL_L       },
        { SDLK_RCTRL,    UKEY_CTRL_L       },
        { SDLK_LALT,     UKEY_ALT_L        },  // GRAPH
        { SDLK_CAPSLOCK, UKEY_CAPS_LOCK    },
        { SDLK_F1,       UKEY_F1           },
        { SDLK_F2,       UKEY_F2           },
        { SDLK_F3,       UKEY_F3           },
        { SDLK_F4,       UKEY_F4           },
        { SDLK_F5,       UKEY_F5           },
        { SDLK_END,      UKEY_END          },  // STOP
        { SDLK_HOME,     UKEY_HOME         },  // SELECT
        { SDLK_UP,       UKEY_CURSOR_UP    },
        { SDLK_DOWN,     UKEY_CURSOR_DOWN  },
        { SDLK_LEFT,     UKEY_CURSOR_LEFT  },
        { SDLK_RIGHT,    UKEY_CURSOR_RIGHT },
        { SDLK_DELETE,   '\x7F'            },
        { SDLK_INSERT,   UKEY_INSERT       },
    };

    keyboard_matrix_apply(entries, bindings, keyboard_matrix_, key, pressed);
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root(Traits::data_folder, rom_root, sizeof(rom_root))) {
        log_info("%s: Could not find ROM root folder\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class MSXSystem<MSXVariant::MSX1>;
template class MSXSystem<MSXVariant::MSX2>;
template class MSXSystem<MSXVariant::MSX2P>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(msx1_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX1>>();
});

REGISTER_SYSTEM(msx2_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX2>>();
});

REGISTER_SYSTEM(msx2p_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX2P>>();
});
