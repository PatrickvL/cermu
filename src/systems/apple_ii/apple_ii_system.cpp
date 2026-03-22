/*
 * apple_ii_system.cpp — Apple II family system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "systems/apple_ii/apple_ii_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<AppleIIVariant V>
static HardwareTraits create_apple_ii_hardware_traits() {
    using Traits = AppleIIVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = apple_ii_constants::DISPLAY_WIDTH;
    traits.display.native_height   = apple_ii_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = apple_ii_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = apple_ii_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    // No sound chip — 1-bit speaker toggle
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = apple_ii_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "1-bit Speaker";

    traits.timing.cpu_frequency_hz = apple_ii_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 60;
    traits.timing.cycles_per_frame = apple_ii_constants::CYCLES_PER_FRAME;
    traits.timing.standard         = VideoStandard::NTSC;

    // Memory options
    if constexpr (V == AppleIIVariant::APPLE_II) {
        traits.memory_options.push_back({"16KB RAM", 16384, 0, false});
        traits.memory_options.push_back({"48KB RAM", 49152, 0, true});
    } else {
        traits.memory_options.push_back({"64KB RAM", 65536, 0, false});
        traits.memory_options.push_back({"128KB RAM (Aux)", 131072, 0, true});
    }

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor apple_ii_descriptor = {
    "Apple II", "AppleII",
    "Apple II (1977) — MOS 6502 @ 1.023MHz, 48KB RAM, soft-switch video",
    "apple_ii", {"AppleII", "Apple 2", "Apple2", "A2"},
    nullptr,
    create_apple_ii_hardware_traits<AppleIIVariant::APPLE_II>(),
    nullptr
};

static SystemDescriptor apple_iie_descriptor = {
    "Apple IIe", "AppleIIe",
    "Apple IIe (1983) — 65C02 @ 1.023MHz, 128KB, 80-col, double hi-res",
    "apple_ii", {"AppleIIe", "Apple 2e", "Apple2e", "A2e"},
    nullptr,
    create_apple_ii_hardware_traits<AppleIIVariant::APPLE_IIE>(),
    nullptr
};

static SystemDescriptor apple_iic_descriptor = {
    "Apple IIc", "AppleIIc",
    "Apple IIc (1984) — 65C02 @ 1.023MHz, 128KB, built-in floppy, portable",
    "apple_ii", {"AppleIIc", "Apple 2c", "Apple2c", "A2c"},
    nullptr,
    create_apple_ii_hardware_traits<AppleIIVariant::APPLE_IIC>(),
    nullptr
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<AppleIIVariant V>
AppleIISystem<V>::AppleIISystem()
    : System()
    , pins_(APPLE_II_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_apple_ii_hardware_traits<V>();
}

template<AppleIIVariant V>
AppleIISystem<V>::~AppleIISystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<AppleIIVariant V>
const SystemDescriptor& AppleIISystem<V>::get_descriptor() const {
    if constexpr (V == AppleIIVariant::APPLE_II) return apple_ii_descriptor;
    else if constexpr (V == AppleIIVariant::APPLE_IIE) return apple_iie_descriptor;
    else return apple_iic_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<AppleIIVariant V>
bool AppleIISystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<AppleIIVariant V>
bool AppleIISystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<AppleIIVariant V>
bool AppleIISystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);
    board_.bind_chipset();
    board_.create_chips(&pins_);

    rom_      = board_.template find<ROMChip>();
    ram_ptr_  = board_.template find<RAMChip>()->data();

    pins_ = board_.cpu().init();

    configure_bus_memory_map();
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    register_bus_chips(board_);

    palette_.set(apple_ii_constants::PALETTE, 16);

    // Video stream output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              apple_ii_constants::DISPLAY_WIDTH, 1);
    video_port_->set_palette(palette_.data(), 16);
    video_port_->bind_frame_output(&last_frame_data_);

    system_ready_ = true;
    printf("%s: System initialized\n", Traits::name);
    return true;
}

template<AppleIIVariant V>
void AppleIISystem<V>::shutdown() { system_ready_ = false; }

template<AppleIIVariant V>
void AppleIISystem<V>::reset() {
    if (!system_ready_) return;
    pins_ = board_.cpu().reset(pins_);
    board_.reset_chips();
    sw_text_ = true; sw_mixed_ = false; sw_page2_ = false; sw_hires_ = false;
    kbd_data_ = 0; kbd_strobe_ = false;
}

// ============================================================================
// EXECUTION (stub — to be filled in)
// ============================================================================

template<AppleIIVariant V>
void AppleIISystem<V>::tick() {
    // TODO: implement tick — CPU tick + soft-switch I/O dispatch + video scan
}

template<AppleIIVariant V>
void AppleIISystem<V>::run_frame() {
    if (!system_ready_) return;
    for (uint32_t i = 0; i < apple_ii_constants::CYCLES_PER_FRAME; ++i)
        tick();
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<AppleIIVariant V>
bool AppleIISystem<V>::load_file(const char* /*filepath*/) {
    // TODO: support DSK, NIB, 2MG, BIN formats
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<AppleIIVariant V>
uint32_t AppleIISystem<V>::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    // TODO: 1-bit speaker synthesis
    return 0;
}

template<AppleIIVariant V>
void AppleIISystem<V>::set_audio_sample_rate(int rate) {
    audio_sample_rate_ = rate;
}

// ============================================================================
// INPUT
// ============================================================================

template<AppleIIVariant V>
void AppleIISystem<V>::handle_keyboard_event(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: map SDL keys to Apple II keyboard codes
}

template<AppleIIVariant V>
void AppleIISystem<V>::handle_text_input(const char* text) {
    if (text && text[0]) {
        kbd_data_ = static_cast<uint8_t>(text[0]) | 0x80;
        kbd_strobe_ = true;
    }
}

// ============================================================================
// INTERNAL HELPERS (stubs)
// ============================================================================

template<AppleIIVariant V>
void AppleIISystem<V>::configure_bus_memory_map() {
    // TODO: setup page tables, ROM overlay, soft-switch regions
}

template<AppleIIVariant V>
void AppleIISystem<V>::render_frame() {
    // TODO: render current video mode to pixel_buffer_
}

template<AppleIIVariant V>
bool AppleIISystem<V>::load_roms() {
    return board_.load_roms("apple_ii");
}

template<AppleIIVariant V>
uint8_t AppleIISystem<V>::soft_switch_read(uint16_t /*addr*/) {
    // TODO: implement soft switch read dispatch
    return 0;
}

template<AppleIIVariant V>
void AppleIISystem<V>::soft_switch_write(uint16_t /*addr*/, uint8_t /*data*/) {
    // TODO: implement soft switch write dispatch
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class AppleIISystem<AppleIIVariant::APPLE_II>;
template class AppleIISystem<AppleIIVariant::APPLE_IIE>;
template class AppleIISystem<AppleIIVariant::APPLE_IIC>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(apple_ii_descriptor, [] {
    return std::make_unique<AppleIISystem<AppleIIVariant::APPLE_II>>();
});

REGISTER_SYSTEM(apple_iie_descriptor, [] {
    return std::make_unique<AppleIISystem<AppleIIVariant::APPLE_IIE>>();
});

REGISTER_SYSTEM(apple_iic_descriptor, [] {
    return std::make_unique<AppleIISystem<AppleIIVariant::APPLE_IIC>>();
});
