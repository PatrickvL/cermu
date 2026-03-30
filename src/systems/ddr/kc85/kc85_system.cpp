/*
 * kc85_system.cpp — KC 85/2, /3, /4 system implementation
 */

#include "systems/ddr/kc85/kc85_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

using namespace z80::reg;  // PC, IX, etc.

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor kc85_2_descriptor = {
    "KC 85/2", "KC85/2",
    "VEB Mühlhausen KC 85/2 (HC 900) — U880 @ 1.77MHz, 16KB RAM, CAOS 2.2 (1984)",
    "kc85", {"KC85/2", "HC900", "HC-900"},
    nullptr, {}, nullptr,
    "Robotron", 1984, z80::U880Traits.display_name, SystemType::Home
};

static SystemDescriptor kc85_3_descriptor = {
    "KC 85/3", "KC85/3",
    "VEB Mühlhausen KC 85/3 — U880 @ 1.77MHz, 16KB RAM, BASIC, CAOS 3.1 (1986)",
    "kc85", {"KC85/3"},
    nullptr, {}, nullptr,
    "Robotron", 1986, z80::U880Traits.display_name, SystemType::Home
};

static SystemDescriptor kc85_4_descriptor = {
    "KC 85/4", "KC85/4",
    "VEB Mühlhausen KC 85/4 — U880 @ 1.77MHz, 64KB RAM, dual-plane video, CAOS 4.2 (1989)",
    "kc85", {"KC85/4"},
    nullptr, {}, nullptr,
    "Robotron", 1989, z80::U880Traits.display_name, SystemType::Home
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

template<KC85Variant V>
KC85System<V>::KC85System() : System(), pins_(KC85_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = kc85_constants::FB_WIDTH;
    traits.display.native_height   = kc85_constants::FB_HEIGHT;
    traits.display.visible_width   = kc85_constants::FB_WIDTH;
    traits.display.visible_height  = kc85_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = kc85_constants::COLOR_COUNT;
    traits.timing.cpu_frequency_hz = kc85_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = kc85_constants::TSTATES_PER_FRAME;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
}

template<KC85Variant V>
KC85System<V>::~KC85System() {}

template<KC85Variant V>
const SystemDescriptor& KC85System<V>::get_descriptor() const {
    if constexpr (V == KC85Variant::KC85_2) return kc85_2_descriptor;
    else if constexpr (V == KC85Variant::KC85_3) return kc85_3_descriptor;
    else return kc85_4_descriptor;
}

template<KC85Variant V> bool KC85System<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    auto it = config.custom_settings.find("keyboard_mode");
    if (it != config.custom_settings.end()) {
        if (it->second == "serial_pio")
            kbd_mode_ = KC85KeyboardMode::SERIAL_PIO;
        else
            kbd_mode_ = KC85KeyboardMode::MEMORY_INJECT;
    }
    return true;
}
template<KC85Variant V> bool KC85System<V>::apply_configuration() { return true; }

template<KC85Variant V>
bool KC85System<V>::initialize() {
    printf("%s: Initializing system (CAOS %s)\n", Traits::name, Traits::caos_version);
    register_board(&board_);

    // ── Bind value-typed chips, then factory-create remaining ──────────
    if constexpr (V == KC85Variant::KC85_2) {
        size_t slot_idx_ = 0;
        KC852_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_)
    } else if constexpr (V == KC85Variant::KC85_3) {
        size_t slot_idx_ = 0;
        KC853_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_)
    } else {
        size_t slot_idx_ = 0;
        KC854_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_)
    }
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // Typed pointers for memory chips accessed after initialize()
    if constexpr (Traits::has_basic_rom) {
        basic_rom_chip_ = &board_.basic_rom;
    }
    caos_rom_chip_ = &board_.caos_rom;
    irm_chip_  = &board_.irm;
    modules_   = &board_.modules;

    // ── KC85/2,3: fill RAM and IRM with pseudo-random noise ─────────────
    // Real hardware powers up with random bit patterns in DRAM.  KC85/4 RAM
    // is cleared to zero by firmware, so only /2 and /3 get the noise fill.
    if constexpr (!Traits::has_extended_video) {
        auto* ram = &board_.ram;
        auto xorshift32 = [](uint32_t x) -> uint32_t {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x;
        };
        uint32_t r = 0x6D98302B;  // same seed as floooh reference
        if (ram) {
            uint8_t* p = ram->data();
            for (uint32_t i = 0; i < Traits::ram_size; ++i) {
                r = xorshift32(r); p[i] = static_cast<uint8_t>(r);
            }
        }
        if (irm_chip_) {
            uint8_t* p = irm_chip_->data();
            for (uint32_t i = 0; i < 16384; ++i) {
                r = xorshift32(r); p[i] = static_cast<uint8_t>(r);
            }
        }
    }

    // ── Set initial banking state ───────────────────────────────────────
    caos_rom_on_  = true;
    irm_enabled_  = true;
    basic_rom_on_ = false;
    bank_ctrl_    = 0;
    bank_ctrl2_   = 0;
    active_plane_ = 0;
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.z80.set(PC, 0xF000);   // CAOS cold-start entry (real HW forces this via address latch)
    board_.pio1.init();
    board_.pio2.init();
    board_.ctc.init();
    modules_->init();

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    kbd_encoder_.reset();
    ktab_valid_ = false;
    std::memset(reverse_ktab_, 0xFF, sizeof(reverse_ktab_));

    load_roms();

    // ── Register chips for Hardware menu ──────────────────────────────
    register_bus_chips(board_);

    // ── GPU indexed palette rendering ───────────────────────────────
    palette_.set(kc85_constants::PALETTE, kc85_constants::COLOR_COUNT);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              kc85_constants::FB_WIDTH, 1);
    video_port_->set_palette(palette_.data(), kc85_constants::COLOR_COUNT);
    video_port_->bind_frame_output(&last_frame_data_);

    // Video generator — models KC85 TTL / U82720 video circuitry
    video_gen_.set_video_out(&video_port_->output());
    if constexpr (Traits::has_extended_video) {
        video_gen_.set_mode(KC85VideoMode::Extended);
    } else {
        video_gen_.set_mode(KC85VideoMode::Standard);
    }

    // Audio stream output
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(audio_sample_rate_, audio_sample_rate_);

    // ── Audio beeper ────────────────────────────────────────────────
    audio_sample_period_ = kc85_constants::CPU_FREQ_HZ / audio_sample_rate_;
    audio_sample_counter_ = 0;
    beeper1_state_ = false;
    beeper2_state_ = false;

    printf("%s: System initialized (RAM: %d KB, IRM: %d KB)\n",
           Traits::name, Traits::ram_size / 1024,
           Traits::has_extended_video ? 64 : 16);
    system_ready_ = true;
    return true;
}

template<KC85Variant V> void KC85System<V>::shutdown() { system_ready_ = false; }
template<KC85Variant V> void KC85System<V>::reset() {
    if (!system_ready_) return;
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    board_.z80.set(PC, 0xF000);   // CAOS cold-start entry
    modules_->init();
    bank_ctrl_ = 0;
    bank_ctrl2_ = 0;
    caos_rom_on_ = true;
    basic_rom_on_ = false;
    irm_enabled_ = true;
    active_plane_ = 0;
    kbd_encoder_.reset();
    ktab_valid_ = false;
    configure_bus_memory_map();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<KC85Variant V>
void KC85System<V>::configure_bus_memory_map() {
    // apply() maps base-layer chips (RAM, plus IRM bank 0 for KC85/4) and
    // skips overlay_group > 0 slots (IRM for /2-/3, ROMs for all variants).
    // KC85/4 IRM effective_size=16384 limits Phase 1 to one 16 KB bank;
    // the full 64 KB buffer is still available for select_bank_at().
    board_.apply(bus_);

    // Build overlay snapshots from the manifest's overlay_group tags.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);

    // Apply initial banking state.
    apply_banking();
}

// ============================================================================
// DYNAMIC BANKING
// ============================================================================

// ── Apply combined banking state ─────────────────────────────────────────
// Loads the pre-computed overlay snapshot for the current ROM/IRM enable
// flags, then manually adjusts IRM on KC85/4 (bank selection).
template<KC85Variant V>
void KC85System<V>::apply_banking() {
    // Compute overlay mode index from enable flags.
    // KC85/2: bit 0=IRM, bit 1=CAOS                     (groups 1,2 → 4 modes)
    // KC85/3: bit 0=IRM, bit 1=BASIC, bit 2=CAOS        (groups 1,2,3 → 8 modes)
    // KC85/4: bit 0=BASIC, bit 1=CAOS                   (groups 1,2 → 4 modes)
    size_t mode = 0;
    if constexpr (Traits::has_extended_video) {
        // KC85/4: IRM and RAM4 are manual (not in overlay groups)
        if (basic_rom_on_)       mode |= 1;  // Group 1: BASIC at $C000
        if (caos_rom_on_)        mode |= 2;  // Group 2: CAOS-E at $E000
        if (bank_ctrl2_ & 0x80)  mode |= 4;  // Group 3: CAOS-C 4 KB at $C000
    } else if constexpr (Traits::has_basic_rom) {
        // KC85/3: IRM=group 1, BASIC=group 2, CAOS=group 3
        if (irm_enabled_)  mode |= 1;
        if (basic_rom_on_) mode |= 2;
        if (caos_rom_on_)  mode |= 4;
    } else {
        // KC85/2: IRM=group 1, CAOS=group 2
        if (irm_enabled_)  mode |= 1;
        if (caos_rom_on_)  mode |= 2;
    }
    bus_.load_snapshot(0, snapshots_[0][mode]);

    // ── KC85/4: manual RAM4 and IRM handling ────────────────────────────
    if constexpr (Traits::has_extended_video) {
        // RAM4 at $4000-$7FFF — controlled by port $86 bits 0-1
        if (!(bank_ctrl2_ & 0x01)) {
            // RAM4 disabled — unmap $4000-$7FFF (pages $40-$7F)
            bus_.map_no_chip_selected(0, 0x40, 0x40);
        } else if (!(bank_ctrl2_ & 0x02)) {
            // RAM4 enabled, write-protected — block writes to $4000-$7FFF
            bus_.map_write_no_chip_selected(0, 0x40, 0x40);
        }
        // else: RAM4 enabled + writable — snapshot already mapped it correctly

        // IRM bank selection
        if (irm_enabled_) {
            constexpr size_t kIrmSlot = 1;
            uint8_t bank = (bank_ctrl_ >> 1) & 0x03;  // io84 bits [2:1] select CPU bank
            board_.select_bank_at(bus_, 0, kIrmSlot, bank, 0x80);
        } else {
            // IRM disabled — unmap $80-$BF
            bus_.map_no_chip_selected(0, 0x80, 0x40);
        }
        active_plane_ = bank_ctrl_ & 0x01;
    }
}

// ── PIO B banking update ─────────────────────────────────────────────────
// Reads current PIO B output, updates enable flags, and re-applies banking
// if any flag changed.
template<KC85Variant V>
void KC85System<V>::update_bank_state() {
    // Banking is controlled by PIO Port A (not Port B)
    uint8_t pio_a = board_.pio1.get_output(0);

    bool new_caos = (pio_a & 0x01) != 0;   // PIO-A bit 0: CAOS ROM at E000
    bool new_irm  = (pio_a & 0x04) != 0;   // PIO-A bit 2: IRM at 8000
    bool new_basic = basic_rom_on_;
    if constexpr (Traits::has_basic_rom)
        new_basic = (pio_a & 0x80) != 0;   // PIO-A bit 7: BASIC ROM at C000

    if (new_irm != irm_enabled_ || new_caos != caos_rom_on_ ||
        new_basic != basic_rom_on_) {
        irm_enabled_  = new_irm;
        caos_rom_on_  = new_caos;
        basic_rom_on_ = new_basic;
        apply_banking();
    }
}

// ============================================================================
// TICK
// ============================================================================

template<KC85Variant V>
void KC85System<V>::tick() {
    if (!system_ready_) return;

    // ── 1. Tick peripherals BEFORE CPU ──────────────────────────────────
    // CTC must tick before cpu so CTC3 timer measures pulse intervals correctly.
    board_.ctc.tick();

    // CTC channel 0 and 1 zero-count toggle audio beepers
    if (board_.ctc.check_zero_count(0)) {
        beeper1_state_ = !beeper1_state_;
    }
    if (board_.ctc.check_zero_count(1)) {
        beeper2_state_ = !beeper2_state_;
    }

    // CTC channel 2 zero-count toggles the foreground blink flag
    if (board_.ctc.check_zero_count(2)) {
        blink_flag_ = !blink_flag_;
    }

    // In SERIAL_PIO mode, tick the keyboard encoder and trigger PIO-B
    // strobe on each pulse (which fires the PIO-B interrupt).
    if (kbd_mode_ == KC85KeyboardMode::SERIAL_PIO) {
        if (kbd_encoder_.tick()) {
            // Pulse from U807 → PIO Port B strobe (BSTB)
            // This triggers the PIO-B interrupt service routine at $E199
            // which reads CTC3 to measure the interval between pulses.
            board_.pio1.strobe(1, true);
            board_.pio1.strobe(1, false);
        }
    }

    // ── 2. Drive INT pin on bus (active-low, level-sensitive) ──────────
    // Daisy chain priority: CTC > PIO-A > PIO-B.
    // The CPU samples INT at the start of each M1 cycle.
    if (board_.ctc.interrupt_pending() || board_.pio1.any_interrupt_pending()) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);   // Assert INT (active-low)
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);   // Deassert INT
    }

    // ── 3. CPU tick (one T-state) ─────────────────────────────────────
    pins_ = board_.z80.tick(pins_);

    // ── 4. Bus dispatch ───────────────────────────────────────────────
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // ── 5. Video timing counters ─────────────────────────────────────────
    constexpr int h_width = Traits::has_extended_video
        ? kc85_constants::KC4_H_TICKS : kc85_constants::KC23_H_TICKS;
    h_count_++;
    if (h_count_ >= static_cast<uint32_t>(h_width)) {
        h_count_ = 0;
        v_count_++;
        if (v_count_ >= static_cast<uint32_t>(kc85_constants::SCANLINES_PER_FRAME)) {
            v_count_ = 0;
            // Vertical sync: trigger CTC channel 2 for blink frequency
            board_.ctc.trigger(2, true);
        } else {
            board_.ctc.trigger(2, false);
        }
    }

    // ── 6. Audio sample generation ──────────────────────────────────────
    if (audio_sample_period_ > 0) {
        audio_sample_counter_++;
        if (audio_sample_counter_ >= audio_sample_period_) {
            audio_sample_counter_ = 0;
            float sample = (beeper1_state_ ? audio_volume_ : 0.0f)
                         + (beeper2_state_ ? audio_volume_ : 0.0f);
            audio_ring_buf_.write(&sample, 1);
            if (audio_port_) audio_port_->drive_sample(sample);
        }
    }

    total_cycles_++;
}

template<KC85Variant V> void KC85System<V>::run_frame() {
    for (uint32_t i = 0; i < kc85_constants::TSTATES_PER_FRAME; ++i) tick();
    if (kbd_mode_ == KC85KeyboardMode::MEMORY_INJECT) {
        handle_keyboard();
    }
    render_frame();
    if (video_port_) video_port_->swap_frame();
}

// ============================================================================
// KEYBOARD — CAOS memory patching
// ============================================================================
//
// Simplified version of the PIO-B interrupt service routine.
// Instead of emulating the serial keyboard encoder hardware, we patch
// the key code directly into CAOS OS variables at offsets from IX.
// See: https://github.com/floooh/yakc/blob/master/misc/kc85_3_kbdint.md
//
//   IX+$08: key status (bit 0=ready, bit 3=timeout, bit 4=repeat)
//   IX+$0A: repeat counter
//   IX+$0D: current key code

template<KC85Variant V>
void KC85System<V>::handle_keyboard() {
    if (!system_ready_ || !board_.z80.iff1()) return;

    RAMChip* ram = &board_.ram;
    if (!ram) return;
    uint8_t* mem = ram->data();
    const uint32_t ram_size = Traits::ram_size;

    auto r8 = [&](uint16_t addr) -> uint8_t {
        return (addr < ram_size) ? mem[addr] : 0xFF;
    };
    auto w8 = [&](uint16_t addr, uint8_t val) {
        if (addr < ram_size) mem[addr] = val;
    };

    const uint16_t ix = board_.z80.get(IX);
    const uint16_t addr_status  = ix + 0x08;
    const uint16_t addr_repeat  = ix + 0x0A;
    const uint16_t addr_keycode = ix + 0x0D;

    constexpr uint8_t READY_BIT   = 1 << 0;
    constexpr uint8_t TIMEOUT_BIT = 1 << 3;
    constexpr uint8_t REPEAT_BIT  = 1 << 4;
    constexpr uint8_t SHORT_REPEAT = 8;
    constexpr uint8_t LONG_REPEAT  = 60;

    // Decrement sticky counter
    if (key_sticky_count_ > 0) {
        key_sticky_count_ = (key_sticky_count_ > kc85_constants::TSTATES_PER_FRAME)
                          ? key_sticky_count_ - kc85_constants::TSTATES_PER_FRAME : 0;
    }

    const uint8_t key = (key_sticky_count_ > 0) ? cur_key_code_ : 0;

    if (key == 0) {
        // No key — timeout
        w8(addr_status, r8(addr_status) | TIMEOUT_BIT);
        w8(addr_keycode, 0);
    } else {
        // Key pressed
        w8(addr_status, r8(addr_status) & ~TIMEOUT_BIT);

        if (key != r8(addr_keycode)) {
            // New key
            w8(addr_keycode, key);
            w8(addr_status, (r8(addr_status) & ~REPEAT_BIT) | READY_BIT);
            w8(addr_repeat, 0);
        } else {
            // Same key held — handle repeat
            w8(addr_repeat, r8(addr_repeat) + 1);
            if (r8(addr_status) & REPEAT_BIT) {
                // Short repeat
                if (r8(addr_repeat) < SHORT_REPEAT) return;
            } else {
                // First long repeat
                if (r8(addr_repeat) < LONG_REPEAT) return;
                w8(addr_status, r8(addr_status) | REPEAT_BIT);
            }
            w8(addr_status, r8(addr_status) | READY_BIT);
            w8(addr_repeat, 0);
        }
    }
}

// ============================================================================
// KEYBOARD — Reverse KTAB lookup (for SERIAL_PIO mode)
// ============================================================================
//
// The CAOS ROM contains a KTAB (key table) that maps scancodes to keycodes
// (ASCII). For SERIAL_PIO mode, we need the reverse mapping: given a
// keycode (what the host user typed), find the scancode that KTAB maps to
// that keycode, so the keyboard encoder can serialize it.
//
// KTAB address is stored in RAM at IX+$0E (low byte), IX+$0F (high byte).
// The table has ~128 entries (one per possible scancode).

template<KC85Variant V>
void KC85System<V>::build_reverse_ktab() {
    std::memset(reverse_ktab_, 0xFF, sizeof(reverse_ktab_));

    if (!system_ready_ || !board_.z80.iff1()) return;

    // Read KTAB pointer from CAOS OS variables (IX+$0E, IX+$0F)
    RAMChip* ram = &board_.ram;
    if (!ram) return;
    uint8_t* mem = ram->data();
    const uint32_t ram_size = Traits::ram_size;

    auto r8 = [&](uint16_t addr) -> uint8_t {
        return (addr < ram_size) ? mem[addr] : 0xFF;
    };

    const uint16_t ix = board_.z80.get(IX);
    uint16_t ktab_addr = r8(ix + 0x0E) | (static_cast<uint16_t>(r8(ix + 0x0F)) << 8);

    if (ktab_addr == 0 || ktab_addr == 0xFFFF) return;

    // Read KTAB from ROM/RAM — need to use direct chip reads since KTAB
    // is typically in CAOS ROM space
    ROMChip* caos = caos_rom_chip_;
    if (!caos) return;

    // KTAB is typically at an address in the E000-FFFF ROM range
    // Read up to 128 scancodes (8×8 matrix = 64, but with caps-lock variants = 128)
    for (uint16_t scancode = 0; scancode < 128; ++scancode) {
        uint16_t addr = ktab_addr + scancode;
        uint8_t keycode = 0xFF;

        // Try to read from the memory bus (handles banking correctly)
        // For ROM addresses (E000+), read from CAOS ROM directly
        if (addr >= 0xE000 && caos) {
            uint16_t rom_offset = addr - 0xE000;
            if (rom_offset < caos->size_bytes()) {
                keycode = caos->data()[rom_offset];
            }
        } else if (addr < ram_size) {
            keycode = mem[addr];
        }

        // Build reverse mapping: first scancode wins for each keycode
        if (keycode != 0 && keycode != 0xFF && reverse_ktab_[keycode] == 0xFF) {
            reverse_ktab_[keycode] = static_cast<uint8_t>(scancode);
        }
    }

    ktab_valid_ = true;
    printf("%s: Built reverse KTAB (keycode→scancode) from CAOS at $%04X\n",
           Traits::name, ktab_addr);
}

template<KC85Variant V> uint32_t KC85System<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (audio_port_) {
        uint32_t n = audio_port_->read_samples(buffer, max_samples);
        if (n > 0) return n;
    }
    return audio_ring_buf_.read(buffer, max_samples);
}
template<KC85Variant V> void KC85System<V>::set_audio_sample_rate(int hz) {
    audio_sample_rate_ = hz;
    audio_sample_period_ = kc85_constants::CPU_FREQ_HZ / audio_sample_rate_;
}
template<KC85Variant V>
void KC85System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Map SDL keycodes to KC85 key codes (CAOS encoding).
    // The KC85 uses its own key encoding which is roughly ASCII for
    // printable characters, with special codes for cursor/control keys.
    uint8_t kc85_key = 0;

    if (pressed) {
        switch (key) {
        // Letters A-Z → uppercase ASCII
        case SDLK_a: kc85_key = 'A'; break;
        case SDLK_b: kc85_key = 'B'; break;
        case SDLK_c: kc85_key = 'C'; break;
        case SDLK_d: kc85_key = 'D'; break;
        case SDLK_e: kc85_key = 'E'; break;
        case SDLK_f: kc85_key = 'F'; break;
        case SDLK_g: kc85_key = 'G'; break;
        case SDLK_h: kc85_key = 'H'; break;
        case SDLK_i: kc85_key = 'I'; break;
        case SDLK_j: kc85_key = 'J'; break;
        case SDLK_k: kc85_key = 'K'; break;
        case SDLK_l: kc85_key = 'L'; break;
        case SDLK_m: kc85_key = 'M'; break;
        case SDLK_n: kc85_key = 'N'; break;
        case SDLK_o: kc85_key = 'O'; break;
        case SDLK_p: kc85_key = 'P'; break;
        case SDLK_q: kc85_key = 'Q'; break;
        case SDLK_r: kc85_key = 'R'; break;
        case SDLK_s: kc85_key = 'S'; break;
        case SDLK_t: kc85_key = 'T'; break;
        case SDLK_u: kc85_key = 'U'; break;
        case SDLK_v: kc85_key = 'V'; break;
        case SDLK_w: kc85_key = 'W'; break;
        case SDLK_x: kc85_key = 'X'; break;
        case SDLK_y: kc85_key = 'Y'; break;
        case SDLK_z: kc85_key = 'Z'; break;
        // Digits 0-9
        case SDLK_0: kc85_key = '0'; break;
        case SDLK_1: kc85_key = '1'; break;
        case SDLK_2: kc85_key = '2'; break;
        case SDLK_3: kc85_key = '3'; break;
        case SDLK_4: kc85_key = '4'; break;
        case SDLK_5: kc85_key = '5'; break;
        case SDLK_6: kc85_key = '6'; break;
        case SDLK_7: kc85_key = '7'; break;
        case SDLK_8: kc85_key = '8'; break;
        case SDLK_9: kc85_key = '9'; break;
        // Space and Return
        case SDLK_SPACE:  kc85_key = 0x20; break;
        case SDLK_RETURN: kc85_key = 0x0D; break;
        // Cursor keys
        case SDLK_RIGHT:  kc85_key = 0x09; break;  // cursor right / TAB
        case SDLK_LEFT:   kc85_key = 0x08; break;  // cursor left / BS
        case SDLK_DOWN:   kc85_key = 0x0A; break;  // cursor down / LF
        case SDLK_UP:     kc85_key = 0x0B; break;  // cursor up / VT
        // Editing
        case SDLK_BACKSPACE: kc85_key = 0x01; break;  // DEL (rubout)
        case SDLK_ESCAPE:    kc85_key = 0x03; break;  // BRK / STOP
        case SDLK_DELETE:    kc85_key = 0x7F; break;  // INS/DEL
        case SDLK_HOME:      kc85_key = 0x10; break;  // CLR (home)
        // Punctuation
        case SDLK_PERIOD:    kc85_key = '.'; break;
        case SDLK_COMMA:     kc85_key = ','; break;
        case SDLK_SEMICOLON: kc85_key = ';'; break;
        case SDLK_MINUS:     kc85_key = '-'; break;
        case SDLK_EQUALS:    kc85_key = '='; break;
        case SDLK_SLASH:     kc85_key = '/'; break;
        case SDLK_PLUS:      kc85_key = '+'; break;
        case SDLK_ASTERISK:  kc85_key = '*'; break;
        // Function keys → F1-F6 mapped to CAOS function keys
        case SDLK_F1: kc85_key = 0xF1; break;
        case SDLK_F2: kc85_key = 0xF2; break;
        case SDLK_F3: kc85_key = 0xF3; break;
        case SDLK_F4: kc85_key = 0xF4; break;
        case SDLK_F5: kc85_key = 0xF5; break;
        case SDLK_F6: kc85_key = 0xF6; break;
        default: break;
        }
    }

    if (kc85_key != 0) {
        cur_key_code_ = kc85_key;
        // Hold key for ~2 frames to give CAOS time to sample it
        key_sticky_count_ = 2 * kc85_constants::TSTATES_PER_FRAME;

        // SERIAL_PIO mode: convert keycode to scancode via reverse KTAB,
        // then set the keyboard encoder matrix position so the encoder
        // transmits the scancode as timed serial pulses to PIO-B.
        if (kbd_mode_ == KC85KeyboardMode::SERIAL_PIO) {
            if (!ktab_valid_) build_reverse_ktab();
            uint8_t scancode = reverse_ktab_[kc85_key];
            if (scancode != 0xFF) {
                kbd_encoder_.clear_all_keys();
                kbd_encoder_.set_key_by_scancode(scancode, true);
            }
        }
    } else if (!pressed) {
        cur_key_code_ = 0;
        key_sticky_count_ = 0;

        if (kbd_mode_ == KC85KeyboardMode::SERIAL_PIO) {
            kbd_encoder_.clear_all_keys();
        }
    }
}
template<KC85Variant V> void KC85System<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("Keyboard Emulation:");
    static const char* kbd_names[] = { "Memory inject (fast)", "Serial PIO (accurate)" };
    int selected = (kbd_mode_ == KC85KeyboardMode::SERIAL_PIO) ? 1 : 0;
    if (ImGui::Combo("##kbd_mode", &selected, kbd_names, 2)) {
        SystemConfiguration new_config = config_;
        new_config.custom_settings["keyboard_mode"] = (selected == 1) ? "serial_pio" : "memory_inject";
        set_configuration(new_config);
    }
#endif
}

// ============================================================================
// VIDEO RENDERING — delegate to KC85VideoGenerator
// ============================================================================

template<KC85Variant V>
void KC85System<V>::render_frame() {
    if (!irm_chip_) return;

    const uint8_t pio_b = board_.pio1.get_output(1);
    video_gen_.set_irm(irm_chip_->data());
    video_gen_.set_blink_bg(blink_flag_ && (pio_b & 0x80));
    if constexpr (Traits::has_extended_video) {
        video_gen_.set_active_plane(active_plane_);
    }
    video_gen_.render_frame();
}

// ============================================================================
// I/O BUS DISPATCH
// ============================================================================

template<KC85Variant V>
bus_state_t KC85System<V>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1 (both active-low)
    // Daisy chain priority: CTC > PIO-A > PIO-B.
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        if (board_.ctc.interrupt_pending()) return board_.ctc.inta(pins);
        if (board_.pio1.any_interrupt_pending()) return board_.pio1.inta(pins);
        BUS_SET_DATA(pins, 0xFF);
        return pins;
    }

    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));

    // PIO 1 at $88-$8B (system + keyboard)
    if ((port & 0xFC) == kc85_constants::PIO_A_DATA) {
        bool is_write = !BUS_GET_BIT(pins, BUS_RW_BIT);
        bool is_data_reg = !((port >> 1) & 0x01);
        pins = board_.pio1.io_tick(pins);
        // PIO 1 data writes control memory banking
        if (is_write && is_data_reg) {
            update_bank_state();
        }
        return pins;
    }

    // CTC at $8C-$8F (4 channels)
    if ((port & 0xFC) == kc85_constants::CTC_CH0) {
        return board_.ctc.io_tick(pins);
    }

    // Module system at $80
    // Upper 8 bits of the 16-bit port address encode the slot address:
    //   0x08 = internal slot 0 (right), 0x0C = internal slot 1 (left)
    if (port == kc85_constants::MODULE_PORT) {
        uint16_t full_addr = static_cast<uint16_t>(BUS_GET_ADDR(pins));
        uint8_t slot_addr = static_cast<uint8_t>(full_addr >> 8);
        int slot_idx = (slot_addr == 0x08) ? 0 : (slot_addr == 0x0C) ? 1 : -1;

        if (BUS_GET_BIT(pins, BUS_RW_BIT)) {
            // Read: return module structure byte (ID) for selected slot
            uint8_t id = (slot_idx >= 0) ? modules_->read_slot_status(slot_idx) : 0xFF;
            BUS_SET_DATA(pins, id);
        } else {
            // Write: set control byte for selected slot, update memory mapping
            if (slot_idx >= 0) {
                modules_->write_slot_control(slot_idx, BUS_GET_DATA(pins));
                // TODO: map/unmap module memory based on active bit
            }
        }
        return pins;
    }

    // KC85/4: additional banking control ports
    if constexpr (Traits::has_extended_video) {
        if (port == kc85_constants::KC4_CTRL_PORT && !BUS_GET_BIT(pins, BUS_RW_BIT)) {
            uint8_t data = BUS_GET_DATA(pins);
            uint8_t old_bank = (bank_ctrl_ >> 1) & 0x03;
            bank_ctrl_ = data;
            uint8_t new_bank = (bank_ctrl_ >> 1) & 0x03;

            if (new_bank != old_bank && irm_enabled_) {
                constexpr size_t kIrmSlot = 1;
                board_.select_bank_at(bus_, 0, kIrmSlot, new_bank, 0x80);
            }
            active_plane_ = bank_ctrl_ & 0x01;
            return pins;
        }
        if (port == kc85_constants::KC4_CTRL2_PORT && !BUS_GET_BIT(pins, BUS_RW_BIT)) {
            bank_ctrl2_ = BUS_GET_DATA(pins);
            apply_banking();
            return pins;
        }
    }

    return pins;
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<KC85Variant V>
bool KC85System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"kc85", "KC85", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        printf("%s: ROM path not found\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class KC85System<KC85Variant::KC85_2>;
template class KC85System<KC85Variant::KC85_3>;
template class KC85System<KC85Variant::KC85_4>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(kc85_2_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_2>>(); });
REGISTER_SYSTEM(kc85_3_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_3>>(); });
REGISTER_SYSTEM(kc85_4_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_4>>(); });
