/*
 * vtech_vz_system.cpp — VTech VZ200 / VZ300 system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "core/cermu.hpp"
#include "systems/vtech_vz/vtech_vz_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<VZVariant V>
static HardwareTraits create_vz_hardware_traits() {
    using Traits = VZVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = vtech_vz_constants::FB_WIDTH;
    traits.display.native_height   = vtech_vz_constants::FB_HEIGHT;
    traits.display.visible_width   = vtech_vz_constants::FB_WIDTH;
    traits.display.visible_height  = vtech_vz_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = vtech_vz_constants::COLOR_COUNT;

    // No sound chip — 1-bit speaker via Z80 port
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = vtech_vz_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "1-bit Speaker";

    traits.timing.cpu_frequency_hz = vtech_vz_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = vtech_vz_constants::TSTATES_PER_FRAME_PAL;
    traits.timing.standard         = VideoStandard::PAL;

    // Memory options
    traits.memory_options.push_back({
        V == VZVariant::VZ200 ? "8KB RAM (VZ200)" : "16KB RAM (VZ300)",
        Traits::ram_size, 0, true
    });

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor vz200_descriptor = {
    "VTech VZ200", "VZ200",
    "VTech VZ200 / Laser 200 (1983) — Z80A @ 3.58MHz, MC6847, 8KB RAM",
    "vtech_vz", {"VZ200", "Laser200", "Laser 200"},
    nullptr,
    create_vz_hardware_traits<VZVariant::VZ200>(),
    nullptr,
    "VTech", 1983, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor vz300_descriptor = {
    "VTech VZ300", "VZ300",
    "VTech VZ300 / Laser 310 (1985) — Z80A @ 3.58MHz, MC6847, 16KB RAM",
    "vtech_vz", {"VZ300", "Laser310", "Laser 310"},
    nullptr,
    create_vz_hardware_traits<VZVariant::VZ300>(),
    nullptr,
    "VTech", 1985, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<VZVariant V>
VTechVZSystem<V>::VTechVZSystem()
    : System()
    , pins_(VZ_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_vz_hardware_traits<V>();
}

template<VZVariant V>
VTechVZSystem<V>::~VTechVZSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<VZVariant V>
const SystemDescriptor& VTechVZSystem<V>::get_descriptor() const {
    if constexpr (V == VZVariant::VZ200) return vz200_descriptor;
    else return vz300_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<VZVariant V>
bool VTechVZSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<VZVariant V>
bool VTechVZSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<VZVariant V>
bool VTechVZSystem<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);
    { size_t slot_idx_ = 0;
      VZ200_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);

    video_ram_ptr_ = board_.vram.data();

    pins_ = board_.z80.init();
    board_.vdg.init();

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    configure_bus_memory_map();
    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    register_bus_chips(board_);

    // Video output — composite video from MC6847 VDG
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdg.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    system_ready_ = true;
    log_info("%s: System initialized (RAM: %dKB)\n", Traits::name,
           Traits::ram_size / 1024);
    return true;
}

template<VZVariant V>
void VTechVZSystem<V>::shutdown() { system_ready_ = false; }

template<VZVariant V>
void VTechVZSystem<V>::reset() {
    if (!system_ready_) return;
    pins_ = board_.z80.reset(pins_);
    board_.reset_chips();
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// EXECUTION (stub — to be filled in)
// ============================================================================

template<VZVariant V>
void VTechVZSystem<V>::tick() {
    bus_state_t s = pins_;

    // ---- Z80 CPU tick ----
    s = board_.z80.tick(s);

    // ---- Bus dispatch: MREQ → memory, IORQ → port I/O ----
    bool mreq = !BUS_GET_BIT(s, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(s, Z80_IORQ_BIT);

    if (mreq) {
        uint16_t addr = BUS_GET_ADDR(s);

        // Memory-mapped I/O at $6800-$6FFF
        if (addr >= vtech_vz_constants::IO_BASE &&
            addr <= vtech_vz_constants::IO_END) {
            if (BUS_GET_BIT(s, BUS_RW_BIT)) {
                // Read: return keyboard matrix row selected by address bits
                uint8_t row = addr & 0x07;
                BUS_SET_DATA(s, (row < vtech_vz_constants::KEYBOARD_ROWS)
                                ? keyboard_matrix_[row] : 0xFF);
            } else {
                // Write: display mode / cassette / speaker control
                uint8_t data = BUS_GET_DATA(s);
                // Bit 3: speaker toggle
                spkr_state_ = (data >> 0) & 1;
                // Bits 3-4 control MC6847 mode pins
                board_.vdg.set_ag((data >> 3) & 1);   // text/graphics
                board_.vdg.set_css((data >> 5) & 1);  // color set
            }
        } else {
            // Normal memory access via page table
            s = bus_.tick(s);
        }
    } else if (iorq) {
        io_tick(s);
    }

    // ---- MC6847 VDG tick (pixel clock) ----
    board_.vdg.tick();

    pins_ = s;
    total_cycles_++;
}

template<VZVariant V>
void VTechVZSystem<V>::run_frame() {
    if (!system_ready_) return;
    for (uint32_t i = 0; i < static_cast<uint32_t>(vtech_vz_constants::TSTATES_PER_FRAME_PAL); ++i)
        tick();

    // Render current frame from video RAM
    board_.vdg.render_frame(video_ram_ptr_);

    if (video_port_) video_port_->swap_frame();
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<VZVariant V>
bool VTechVZSystem<V>::load_file(const char* /*filepath*/) {
    // TODO: support VZ tape format (.vz)
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<VZVariant V>
uint32_t VTechVZSystem<V>::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    // TODO: 1-bit speaker synthesis
    return 0;
}

template<VZVariant V>
void VTechVZSystem<V>::set_audio_sample_rate(int rate) {
    audio_sample_rate_ = rate;
}

// ============================================================================
// INPUT
// ============================================================================

template<VZVariant V>
void VTechVZSystem<V>::handle_keyboard_event(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: VZ keyboard matrix mapping
}


// ============================================================================
// INTERNAL HELPERS (stubs)
// ============================================================================

template<VZVariant V>
void VTechVZSystem<V>::configure_bus_memory_map() {
    // apply() establishes the default linear map from the manifest:
    //   $00-$3F: ROM (read)
    //   $70-$77: Video RAM (read+write)
    //   $78-$B7: User RAM (read+write)
    // The gap at $4000-$6FFF and $B800+ is unmapped (open bus).
    board_.apply(bus_);
}

template<VZVariant V>
bool VTechVZSystem<V>::load_roms() {
    return board_.load_roms("vtech_vz");
}

template<VZVariant V>
void VTechVZSystem<V>::io_tick(bus_state_t& bus) {
    uint8_t port = BUS_GET_ADDR(bus) & 0xFF;
    bool is_read = BUS_GET_BIT(bus, BUS_RW_BIT);

    if (is_read) {
        // Port reads — keyboard rows
        if (port < vtech_vz_constants::KEYBOARD_ROWS) {
            BUS_SET_DATA(bus, keyboard_matrix_[port]);
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
    } else {
        // Port writes — speaker control
        if ((port & 0xF0) == vtech_vz_constants::PORT_SPEAKER) {
            spkr_state_ = BUS_GET_DATA(bus) & 1;
        }
    }
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class VTechVZSystem<VZVariant::VZ200>;
template class VTechVZSystem<VZVariant::VZ300>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(vz200_descriptor, [] {
    return std::make_unique<VTechVZSystem<VZVariant::VZ200>>();
});

REGISTER_SYSTEM(vz300_descriptor, [] {
    return std::make_unique<VTechVZSystem<VZVariant::VZ300>>();
});
