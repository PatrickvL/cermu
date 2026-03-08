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

    // Create CPU
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();

    // Initialize ULA
    ula_.init();

    // Initialize AY (128K only, but always present)
    if constexpr (Traits::has_ay_sound) {
        ay_.init();
    }

    // Allocate memory
    ram_.resize(Traits::ram_size_kb * 1024, 0x00);
    rom_.resize(Traits::rom_count * spectrum_constants::ROM_SIZE_48K, 0xFF);

    // Audio setup
    audio_sample_period_ = spectrum_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Load ROMs
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    system_ready_ = true;
    return true;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::shutdown() {
    delete cpu_;
    cpu_ = nullptr;
    ram_.clear();
    rom_.clear();
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
    uint16_t addr = BUS_GET_ADDR(pins_);
    bool is_read = BUS_GET_BIT(pins_, BUS_RW_BIT);

    // Check for I/O request vs memory request
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = mem_tick(pins_);
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
// MEMORY DISPATCH
// ============================================================================

template<SpectrumVariant V>
bus_state_t SpectrumSystem<V>::mem_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (is_read) {
        uint8_t data;
        if (addr < 0x4000) {
            // ROM
            if constexpr (Traits::has_banking) {
                uint16_t rom_bank = (bank_select_ & 0x10) ? 1 : 0;
                data = rom_[rom_bank * 0x4000 + addr];
            } else {
                data = rom_[addr];
            }
        } else {
            // RAM
            if constexpr (Traits::has_banking) {
                if (addr < 0x8000) {
                    data = ram_[5 * 0x4000 + (addr - 0x4000)];  // Bank 5
                } else if (addr < 0xC000) {
                    data = ram_[2 * 0x4000 + (addr - 0x8000)];  // Bank 2
                } else {
                    uint8_t bank = bank_select_ & 0x07;
                    data = ram_[bank * 0x4000 + (addr - 0xC000)];
                }
            } else {
                data = ram_[addr - 0x4000];
            }
        }
        BUS_SET_DATA(pins, data);
    } else {
        uint8_t data = BUS_GET_DATA(pins);
        if (addr >= 0x4000) {
            // RAM write (ROM is write-protected)
            if constexpr (Traits::has_banking) {
                if (addr < 0x8000) {
                    ram_[5 * 0x4000 + (addr - 0x4000)] = data;
                } else if (addr < 0xC000) {
                    ram_[2 * 0x4000 + (addr - 0x8000)] = data;
                } else {
                    uint8_t bank = bank_select_ & 0x07;
                    ram_[bank * 0x4000 + (addr - 0xC000)] = data;
                }
            } else {
                ram_[addr - 0x4000] = data;
            }
        }
    }
    return pins;
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
