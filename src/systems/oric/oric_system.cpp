/*
 * oric_system.cpp — Oric-1 / Oric Atmos system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "core/cermu.hpp"
#include "systems/oric/oric_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_registry.hpp"
#include "core/formats/format_load_helpers.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<OricVariant V>
static HardwareTraits create_oric_hardware_traits() {
    using Traits = OricVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = oric_constants::FB_WIDTH;
    traits.display.native_height   = oric_constants::FB_HEIGHT;
    traits.display.visible_width   = oric_constants::FB_WIDTH;
    traits.display.visible_height  = oric_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = oric_constants::COLOR_COUNT;

    // Default 8-color palette
    for (int i = 0; i < oric_constants::COLOR_COUNT; ++i) {
        uint32_t c = oric_constants::PALETTE[i];
        traits.display.default_palette.push_back(
            PaletteColor(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF)
        );
    }

    // Audio — AY-3-8912 (3 channels)
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = oric_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8912";

    traits.timing.cpu_frequency_hz = oric_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = oric_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard         = VideoStandard::PAL;

    // Memory options
    if constexpr (!Traits::is_atmos) {
        traits.memory_options.push_back({"16KB RAM", oric_constants::RAM_16K, 0, false});
        traits.memory_options.push_back({"48KB RAM", oric_constants::RAM_48K, 0, true});
    } else {
        traits.memory_options.push_back({"48KB RAM", oric_constants::RAM_48K, 0, true});
    }

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor oric1_descriptor = {
    "Oric-1", "Oric1",
    "Oric-1 (1983) — MOS 6502 @ 1MHz, AY-3-8912, 48KB RAM",
    "oric", {"Oric1", "Oric-1", "Oric"},
    nullptr,
    create_oric_hardware_traits<OricVariant::ORIC_1>(),
    nullptr,
    "Oric", 1983, fam65xx::MOS6502Traits.display_name, SystemType::Home
};

static SystemDescriptor oric_atmos_descriptor = {
    "Oric Atmos", "OricAtmos",
    "Oric Atmos (1984) — MOS 6502 @ 1MHz, AY-3-8912, 48KB RAM, BASIC 1.1",
    "oric", {"OricAtmos", "Atmos"},
    nullptr,
    create_oric_hardware_traits<OricVariant::ORIC_ATMOS>(),
    nullptr,
    "Oric", 1984, fam65xx::MOS6502Traits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<OricVariant V>
OricSystem<V>::OricSystem()
    : System()
    , pins_(ORIC_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_oric_hardware_traits<V>();
}

template<OricVariant V>
OricSystem<V>::~OricSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<OricVariant V>
const SystemDescriptor& OricSystem<V>::get_descriptor() const {
    if constexpr (V == OricVariant::ORIC_1) return oric1_descriptor;
    else return oric_atmos_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<OricVariant V>
bool OricSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<OricVariant V>
bool OricSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<OricVariant V>
bool OricSystem<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // Bind and create value-typed chips before factory-creating other chips
    bind_all(board_, board_.components_, BTraits::kManifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = BTraits::kManifest.port_slots;
    port_manifest_count_ = BTraits::kManifest.port_count;
    ram_ptr_ = board_.ram.data();

    pins_ = board_.cpu.init();
    board_.via.reset();
    board_.via.interrupt_bit = BUS_IRQ_BIT;

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    configure_bus_memory_map();
    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    register_bus_chips(board_);

    palette_.set(oric_constants::PALETTE, oric_constants::COLOR_COUNT);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              oric_constants::FB_WIDTH, 1);
    video_port_->set_palette(palette_.data(), oric_constants::COLOR_COUNT);
    video_port_->bind_frame_output(&last_frame_data_);

    system_ready_ = true;
    log_info("%s: System initialized\n", Traits::name);
    return true;
}

template<OricVariant V>
void OricSystem<V>::shutdown() { system_ready_ = false; }

template<OricVariant V>
void OricSystem<V>::reset() {
    if (!system_ready_) return;
    pins_ = board_.cpu.reset(pins_);
    board_.reset_chips();
    board_.via.reset();
    board_.via.interrupt_bit = BUS_IRQ_BIT;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    hires_mode_ = false;
}

// ============================================================================
// EXECUTION (stub — to be filled in)
// ============================================================================

template<OricVariant V>
void OricSystem<V>::tick() {
    bus_state_t s = pins_;

    // ---- Propagate VIA interrupt state from previous cycle ----
    if (board_.via.ifr & board_.via.ier & 0x7F) {
        if (board_.via.interrupt_bit != 0)
            BUS_CLR_BIT(s, board_.via.interrupt_bit);
    }

    // ---- CPU PHI2 — address/R#W valid on bus ----
    s = board_.cpu.template tick<MOS6502::Phase::PHI2>(s);

    // ---- Address decode + flat-mem service + VIA CS dispatch ----
    s = bus_.resolve(s);
    s = bus_.service(s);
    s = board_.via.tick(s);

    // ---- CPU PHI1 ----
    s = board_.cpu.template tick<MOS6502::Phase::PHI1>(s);

    BUS_SET_BIT(s, BUS_RW_BIT);
    pins_ = s;
    total_cycles_++;
}

template<OricVariant V>
void OricSystem<V>::run_frame() {
    if (!system_ready_) return;
    for (uint32_t i = 0; i < oric_constants::CYCLES_PER_FRAME_PAL; ++i)
        tick();
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<OricVariant V>
bool OricSystem<V>::load_file(const char* filepath) {
    if (!filepath) return false;

    format_apply_config_t cfg{};
    cfg.ram         = board_.ram.data();
    cfg.ram_size    = 0xC000;  // RAM below ROM
    cfg.cpu         = &board_.cpu;
    cfg.system_name = Traits::name;

    return format_load_and_apply(filepath, cfg);
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<OricVariant V>
uint32_t OricSystem<V>::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    // TODO: AY-3-8912 audio output via VIA port control
    return 0;
}

template<OricVariant V>
void OricSystem<V>::set_audio_sample_rate(int rate) {
    audio_sample_rate_ = rate;
}

// ============================================================================
// INPUT
// ============================================================================

template<OricVariant V>
void OricSystem<V>::handle_keyboard_event(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: Oric keyboard matrix mapping (accent via VIA)
}


// ============================================================================
// INTERNAL HELPERS (stubs)
// ============================================================================

template<OricVariant V>
void OricSystem<V>::configure_bus_memory_map() {
    // apply() establishes the default linear map from the manifest:
    //   $00-$FF: RAM (read+write)
    //   $C0-$FF: ROM overlay (read)
    // With EnableCs=true, Phase 3 creates a MaskedSubTable on page $03
    // for the VIA ($0300-$030F, mask 0xFFF0).
    board_.apply(bus_);
}

template<OricVariant V>
void OricSystem<V>::render_frame() {
    // TODO: ULA rendering — text and hi-res modes from RAM
}

template<OricVariant V>
bool OricSystem<V>::load_roms() {
    return board_.load_roms("oric");
}

template<OricVariant V>
void OricSystem<V>::via_port_a_write(void* /*context*/, uint8_t /*data*/) {
    // TODO: forward AY-3-8912 data writes
}

template<OricVariant V>
uint8_t OricSystem<V>::via_port_b_read(void* /*context*/, uint8_t /*output*/) {
    // TODO: keyboard matrix scan + AY control
    return 0xFF;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class OricSystem<OricVariant::ORIC_1>;
template class OricSystem<OricVariant::ORIC_ATMOS>;

REGISTER_SYSTEM(oric1_descriptor, [] {
    return std::make_unique<OricSystem<OricVariant::ORIC_1>>();
});

REGISTER_SYSTEM(oric_atmos_descriptor, [] {
    return std::make_unique<OricSystem<OricVariant::ORIC_ATMOS>>();
});
