#pragma once
/*
 * pce_system.hpp — NEC PC Engine / TurboGrafx-16 Emulated System
 *
 * The PC Engine (1987) was NEC's entry into the console market, designed
 * by Hudson Soft.  It uses a custom 65C02-based CPU (HuC6280) with
 * integrated 6-channel PSG, paired with the HuC6270 VDC and HuC6260 VCE.
 *
 * Variants:
 *   PCE     (1987): PC Engine (Japan), composite video, HuCard slot
 *   TG16    (1989): TurboGrafx-16 (USA), same hardware, different form factor
 *   SGX     (1989): SuperGrafx — dual VDC (HuC6202 arbitrates two HuC6270s)
 *
 * The HuC6280 CPU uses 8KB memory pages mapped via 8 MPR registers,
 * giving a 21-bit logical address space.  For this stub, we use WDC_65C02
 * as a stand-in until the HuC6280 is fully instantiated in fam65xx.
 */

#include "systems/pce/pce_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/fam65xx/wdc65c02.hpp"
#include "chip/video/huc6270/huc6270.hpp"
#include "chip/video/huc6260/huc6260.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

enum class PCEVariant { PCE, TG16, SGX };

// ============================================================================
// PC Engine default bus state
// ============================================================================

#define PCE_BUS_DEFAULT_STATE (WDC_65C02::default_bus_state())

// ============================================================================
// PC Engine Manifest  (using WDC_65C02 as HuC6280 stand-in)
// ============================================================================

inline constexpr auto kPCEManifest = make_manifest(
    // Chips
    Slot<WDC_65C02>{.base_addr = 0x0000, .label = "HuC6280 (65C02 stand-in)"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = pce_constants::MAX_ROM_SIZE,
                  .label = "HuCard ROM"},
    Slot<RAMChip>{.base_addr = 0x2000, .size_bytes = pce_constants::RAM_SIZE,
                  .label = "Work RAM"},
    Slot<huc6270_t>{.base_addr = pce_constants::IO_VDC, .addr_mask = 0x0003,
                    .label = "HuC6270 VDC"},
    Slot<huc6260_t>{.base_addr = pce_constants::IO_VCE, .addr_mask = 0x0007,
                    .label = "HuC6260 VCE"},
    // Ports
    Slot<PortControlDB9>{.name = "Controller Port", .port_number = 1,
                         .default_device = "joystick"},
    Slot<PortExpansion>{.name = "HuCard Slot"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv"},
    Slot<PortAudioStereo>{.name = "Audio Out"}
);

using PCEBusSpec = ManifestBusSpec<kPCEManifest, 21, 8>;

struct PCEBoard : Board<PCEBusSpec> {
    using ComponentTuple = decltype(kPCEManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    WDC_65C02&   cpu  = std::get<0>(components_);
    ROMChip&     rom  = std::get<1>(components_);
    RAMChip&     ram  = std::get<2>(components_);
    huc6270_t&   vdc  = std::get<3>(components_);
    huc6260_t&   vce  = std::get<4>(components_);

    // Port aliases
    PortControlDB9&     ctrl_port    = std::get<5>(components_);
    PortExpansion&      card_slot    = std::get<6>(components_);
    PortCompositeVideo& video_port   = std::get<7>(components_);
    PortAudioStereo&    audio_port   = std::get<8>(components_);

    PCEBoard() : Board(kPCEManifest) {}
};

// ============================================================================
// PC Engine System
// ============================================================================

template<PCEVariant V>
class PCEngineSystem : public System {
public:
    PCEngineSystem();
    ~PCEngineSystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    PCEBoard board_;

    std::unique_ptr<CompositeVideoPort> video_port_;
    uint32_t audio_sample_rate_ = pce_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // MPR banking (8 × 8KB pages → 21-bit physical address)
    uint8_t mpr_[8] = {0xFF, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Timer
    uint8_t  timer_reload_ = 0;
    uint8_t  timer_value_  = 0;
    bool     timer_enable_ = false;
    uint16_t timer_counter_ = 0;  // Prescaler (÷1024)

    // IRQ
    uint8_t  irq_disable_ = 0;    // $1402 write mask
    uint8_t  irq_pending_ = 0;    // $1403 read
    static constexpr uint8_t IRQ2_BIT   = 0x01;  // External IRQ (VDC)
    static constexpr uint8_t IRQ1_BIT   = 0x02;  // VDC IRQ line
    static constexpr uint8_t TIMER_BIT  = 0x04;  // Timer IRQ

    // Joypad
    uint8_t joypad_state_ = 0xFF;   // Active-low buttons (active buttons = 0)
    uint8_t joypad_sel_ = 0;        // Select line (bit 0 of $1000 write)
    uint8_t joypad_clr_ = 0;        // Clear line (bit 1)

    bus_state_t pins_ = PCE_BUS_DEFAULT_STATE;
    uint32_t frame_counter_ = 0;

    uint32_t map_address(uint16_t logical) const noexcept;
    void io_write(uint16_t addr, uint8_t data) noexcept;
    uint8_t io_read(uint16_t addr) noexcept;
    bool load_roms();
};
