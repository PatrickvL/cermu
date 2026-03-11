/*
 * spectrum_system.cpp — ZX Spectrum 48K / 128K system implementation
 *
 * Tick loop:
 *   Each call to tick() advances the Z80 by one T-state.
 *   The ULA generates video at the same clock rate (3.5 MHz).
 *   The AY-3-8912 (128K only) is clocked at CPU_FREQ / 2 = 1.75 MHz.
 *
 * Memory map (48K):
 *   $0000-$3FFF: 16KB ROM
 *   $4000-$FFFF: 48KB RAM (screen at $4000-$5AFF)
 *
 * Memory map (128K):
 *   $0000-$3FFF: ROM bank (ROM 0 or ROM 1, selected by port $7FFD bit 4)
 *   $4000-$7FFF: RAM bank 5 (always mapped)
 *   $8000-$BFFF: RAM bank 2 (always mapped)
 *   $C000-$FFFF: Switchable RAM bank (0-7, selected by port $7FFD bits 0-2)
 *
 * I/O decoding:
 *   A0=0:      ULA port $FE (keyboard/border/tape)
 *   $7FFD:     128K banking (A1=0, A15=0 — active when bits match)
 *   $FFFD:     AY register select
 *   $BFFD:     AY data write
 */

#include "spectrum_system.h"
#include "../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<SpectrumVariant V>
static HardwareTraits create_spectrum_hardware_traits() {
    using Traits = SpectrumVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = spectrum_constants::TOTAL_WIDTH;
    traits.display.native_height   = spectrum_constants::TOTAL_HEIGHT;
    traits.display.visible_width   = spectrum_constants::TOTAL_WIDTH;
    traits.display.visible_height  = spectrum_constants::TOTAL_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = spectrum_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = Traits::has_ay_sound ? "AY-3-8912 + Beeper" : "Beeper";

    traits.timing.cpu_frequency_hz   = spectrum_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = spectrum_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = spectrum_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = spectrum_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor spectrum48k_descriptor = {
    "ZX Spectrum 48K",
    "Spectrum48K",
    "Sinclair ZX Spectrum 48K — Z80A, ULA, 48KB RAM (1982)",
    "spectrum",
    {"Spectrum", "Spectrum48K", "ZXSpectrum", "ZX48K", "Speccy"},
    nullptr,
    create_spectrum_hardware_traits<SpectrumVariant::ZX48K>(),
    nullptr  // probe
};

static SystemDescriptor spectrum128k_descriptor = {
    "ZX Spectrum 128K",
    "Spectrum128K",
    "Sinclair ZX Spectrum 128K — Z80A, ULA, AY sound, 128KB RAM (1985)",
    "spectrum",
    {"Spectrum128K", "ZX128K", "Spectrum128"},
    nullptr,
    create_spectrum_hardware_traits<SpectrumVariant::ZX128K>(),
    nullptr  // probe
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<SpectrumVariant V>
SpectrumSystem<V>::SpectrumSystem()
    : EmulatedSystem()
    , ay_(Traits::has_ay_sound ? AYVariant::AY_3_8912 : AYVariant::AY_3_8910)
    , pins_(SPECTRUM_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_spectrum_hardware_traits<V>();
}

template<SpectrumVariant V>
SpectrumSystem<V>::~SpectrumSystem() {
    delete cpu_;
    cpu_ = nullptr;
}

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<SpectrumVariant V>
const SystemDescriptor& SpectrumSystem<V>::get_descriptor() const {
    if constexpr (V == SpectrumVariant::ZX48K) return spectrum48k_descriptor;
    else return spectrum128k_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<SpectrumVariant V>
bool SpectrumSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);

    // ── Create RAMChip wrappers ──────────────────────────────────────
    constexpr size_t ram_bytes = Traits::ram_size_kb * 1024;
    constexpr size_t rom_bytes = Traits::rom_count * spectrum_constants::ROM_SIZE_48K;

    // RAM: power-of-2 allocation (64KB for 48K, 128KB for 128K)
    auto ram_chip = std::make_unique<RAMChip>(
        ChipInfo{"DRAM", "Various"},
        (V == SpectrumVariant::ZX48K) ? 65536 : ram_bytes,
        RAMChip::RAM, &pins_, "RAM", 0x0000);
    ram_ = ram_chip.get();

    // ROM: 16KB (48K) or 32KB (128K)
    auto rom_chip = std::make_unique<ROMChip>(
        ChipInfo{"ROM", "Sinclair"}, rom_bytes,
        ROMChip::ROM, &pins_, "ROM", 0x0000);
    rom_ = rom_chip.get();

    // ── Bind manifest slots, wire the bus ───────────────────────────────
    bus_mem_.initialize(bus_, ram_, rom_);

    // ── Configure page tables for this variant ──────────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();
    ula_.init();
    if constexpr (Traits::has_ay_sound) {
        ay_.init();
    }

    // Audio setup
    audio_sample_period_ = spectrum_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Load ROMs
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────
    register_chip(static_cast<ChipBase*>(cpu_),
        "Zilog Z80A CPU", "Z80A", "CPU", 0x0000);
    register_chip(&ula_,
        "Ferranti ULA", "ULA", "Video", spectrum_constants::SCREEN_BASE);
    if constexpr (Traits::has_ay_sound) {
        register_chip(&ay_,
            "AY-3-8912 Sound", "AY-3-8912", "Sound", 0);
    }
    register_chip(std::move(ram_chip));
    register_chip(std::move(rom_chip));

    printf("%s: System initialized (%dKB RAM)\n", Traits::name, Traits::ram_size_kb);
    system_ready_ = true;
    return true;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::shutdown() {
    delete cpu_;
    cpu_ = nullptr;
    system_ready_ = false;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    ula_.reset();
    if constexpr (Traits::has_ay_sound) {
        ay_.reset();
    }
    bank_select_ = 0;
    bank_locked_ = false;
    frame_tstate_counter_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::tick() {
    if (!cpu_) return;

    // ULA tick (same clock as CPU — one T-state)
    ula_.tick();

    // Frame interrupt: ULA asserts INT at start of frame (held for 32 T-states)
    if (ula_.check_frame_interrupt()) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);  // Assert INT (active-low)
    }

    // Memory contention (ULA stalls CPU during screen fetch)
    // TODO: Implement contention pattern

    // CPU tick — one T-state
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    // Check for I/O request vs memory request
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // AY tick (128K: clocked at CPU/2)
    if constexpr (Traits::has_ay_sound) {
        if (frame_tstate_counter_ & 1) {
            ay_.tick();
        }
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = ula_.get_ear_output() ? 0.5f : 0.0f;
        if constexpr (Traits::has_ay_sound) {
            sample += ay_.get_sample() * 0.5f;
        }
        audio_buffer_.push_back(sample);
    }

    // Frame counter
    frame_tstate_counter_++;
    if (frame_tstate_counter_ >= spectrum_constants::TSTATES_PER_FRAME) {
        frame_tstate_counter_ = 0;
        update_framebuffer();
    }

    total_cycles_++;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::run_frame() {
    for (uint32_t i = 0; i < spectrum_constants::TSTATES_PER_FRAME; ++i) {
        tick();
    }
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::configure_bus_memory_map() {
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;

    // apply() establishes the default map from the manifest:
    //   48K:  RAM pages 0-255 (read+write), ROM overlays read pages 0-63
    //   128K: RAM pages 0-255 (read+write from first 64KB), ROM overlays read pages 0-63
    bus_mem_.apply(bus_);

    // ROM region ($0000-$3FFF): no writes — unmap write pages
    for (size_t page = 0; page < 64; ++page)
        bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);

    if constexpr (Traits::has_banking) {
        // 128K: remap fixed banks and current paging state.
        // RAM base_id is always 0 (slot 0), 64 pages per 16KB bank.
        //
        //   $4000-$7FFF: always bank 5
        //   $8000-$BFFF: always bank 2
        //   $C000-$FFFF: selected by bank_select_ bits 0-2
        //   $0000-$3FFF: ROM bank selected by bank_select_ bit 4
        constexpr size_t kPagesPerBank = 64;  // 16384 / 256

        // Bank 5 at $4000
        bus_.fill_read_pages (0, 0x40, kPagesPerBank, ChipId(5 * kPagesPerBank));
        bus_.fill_write_pages(0, 0x40, kPagesPerBank, WriteChipId(5 * kPagesPerBank));

        // Bank 2 at $8000
        bus_.fill_read_pages (0, 0x80, kPagesPerBank, ChipId(2 * kPagesPerBank));
        bus_.fill_write_pages(0, 0x80, kPagesPerBank, WriteChipId(2 * kPagesPerBank));

        // Switchable bank at $C000 + ROM bank at $0000
        update_banking();
    }

    // Screen RAM pointer — bank 5 for 128K (default), $4000 offset for 48K
    if constexpr (Traits::has_banking) {
        constexpr size_t kPagesPerBank = 64;
        bool use_bank7 = (bank_select_ & 0x08) != 0;
        size_t screen_bank = use_bank7 ? 7 : 5;
        screen_ram_ptr_ = bus_mem_.chip_buffer(ChipId(screen_bank * kPagesPerBank));
    } else {
        // 48K: screen starts at $4000 = page $40 = chip ID 64
        screen_ram_ptr_ = bus_mem_.chip_buffer(ChipId(0x40));
    }
}

template<SpectrumVariant V>
void SpectrumSystem<V>::update_banking() {
    if constexpr (!Traits::has_banking) return;

    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;

    constexpr size_t kPagesPerBank = 64;  // 16384 / 256

    // Switchable RAM bank at $C000-$FFFF (bits 0-2 of bank_select_)
    uint8_t ram_bank = bank_select_ & 0x07;
    bus_.fill_read_pages (0, 0xC0, kPagesPerBank, ChipId(ram_bank * kPagesPerBank));
    bus_.fill_write_pages(0, 0xC0, kPagesPerBank, WriteChipId(ram_bank * kPagesPerBank));

    // ROM bank at $0000-$3FFF (bit 4 of bank_select_)
    constexpr size_t rom_base = kSpectrum128KChips.base_id(
        spectrum_chips::kRomSlot, SpectrumBusTraits<SpectrumVariant::ZX128K>::Spec::PageBits);
    uint8_t rom_bank = (bank_select_ & 0x10) ? 1 : 0;
    bus_.fill_read_pages(0, 0x00, kPagesPerBank, ChipId(rom_base + rom_bank * kPagesPerBank));

    // Screen bank: bit 3 selects bank 5 or 7
    bool use_bank7 = (bank_select_ & 0x08) != 0;
    size_t screen_bank = use_bank7 ? 7 : 5;
    screen_ram_ptr_ = bus_mem_.chip_buffer(ChipId(screen_bank * kPagesPerBank));
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<SpectrumVariant V>
bus_state_t SpectrumSystem<V>::io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (!(addr & 0x01)) {
        // ULA port ($FE) — selected when A0=0
        if (is_read) {
            uint8_t data = ula_.read_port_fe(static_cast<uint8_t>(addr >> 8));
            BUS_SET_DATA(pins, data);
        } else {
            ula_.write_port_fe(BUS_GET_DATA(pins));
        }
    }

    if constexpr (Traits::has_banking) {
        // 128K banking port $7FFD (A1=0, A15=0)
        if (!is_read && !(addr & 0x8002)) {
            if (!bank_locked_) {
                bank_select_ = BUS_GET_DATA(pins);
                bank_locked_ = (bank_select_ & 0x20) != 0;
                update_banking();
            }
        }

        // AY register select $FFFD (A1=0, A14=1, A15=1)
        if ((addr & 0xC002) == 0xC000) {
            if (is_read) {
                BUS_SET_DATA(pins, ay_.read_register());
            } else {
                ay_.latch_address(BUS_GET_DATA(pins));
            }
        }

        // AY data write $BFFD (A1=0, A14=1, A15=0)
        if (!is_read && (addr & 0xC002) == 0x8000) {
            ay_.write_register(BUS_GET_DATA(pins));
        }
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::load_file(const char* filepath) {
    // TODO: Support .tap, .tzx, .sna, .z80, .szx snapshot formats
    (void)filepath;
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================

template<SpectrumVariant V>
uint32_t* SpectrumSystem<V>::get_framebuffer() {
    return framebuffer_;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::get_display_dimensions(int* width, int* height) const {
    *width = spectrum_constants::TOTAL_WIDTH;
    *height = spectrum_constants::TOTAL_HEIGHT;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::set_framebuffer(uint32_t* buffer, int width, int height) {
    (void)buffer; (void)width; (void)height;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::update_framebuffer() {
    // TODO: Render Spectrum display from screen RAM + attributes
    // Border → ula_.border_color()
    // Bitmap at $4000-$57FF (interleaved: lines 0,8,16..., 1,9,17..., etc.)
    // Attributes at $5800-$5AFF (32×24 cells, INK/PAPER/BRIGHT/FLASH)
}

// ============================================================================
// AUDIO
// ============================================================================

template<SpectrumVariant V>
uint32_t SpectrumSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    uint32_t count = static_cast<uint32_t>(audio_buffer_.size());
    if (count > max_samples) count = max_samples;
    if (count > 0) {
        std::memcpy(buffer, audio_buffer_.data(), count * sizeof(float));
        audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.begin() + count);
    }
    return count;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = spectrum_constants::CPU_FREQ_HZ / audio_sample_rate_;
}

// ============================================================================
// INPUT
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // TODO: Map SDL keycodes to Spectrum keyboard matrix half-rows
    (void)key; (void)pressed;
}

// ============================================================================
// GUI
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::render_system_menu_items() {}

template<SpectrumVariant V>
void SpectrumSystem<V>::render_configuration_ui() {}

template<SpectrumVariant V>
void SpectrumSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::load_roms() {
    // TODO: Load ROM from data/spectrum/ directory
    return false;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class SpectrumSystem<SpectrumVariant::ZX48K>;
template class SpectrumSystem<SpectrumVariant::ZX128K>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(spectrum48k_descriptor, [] {
    return std::make_unique<SpectrumSystem<SpectrumVariant::ZX48K>>();
});

REGISTER_SYSTEM(spectrum128k_descriptor, [] {
    return std::make_unique<SpectrumSystem<SpectrumVariant::ZX128K>>();
});
