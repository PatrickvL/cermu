#pragma once

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/audio_port.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/video/fam6845/mc6845.hpp"
#include "chip/video/bbc_vidproc/bbc_vidproc.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "core/audio_thread.hpp"
#include "utils/write_only_synth_adapter.hpp"
#include "systems/bbc/bbc_micro_constants.hpp"
#include <cstdint>
#include <memory>

// BBC Micro bus is derived from 6502 — shares address, data, and control signals.
#define BBC_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

// =============================================================================
// BBC Micro chip manifest — declarative memory layout
// =============================================================================
//
// Slot 1: RAM — 32 KB at $0000-$7FFF
// Slot 2: Paged ROM pool — 256 KB at $8000 (16 × 16 KB sideways ROM slots)
//         Only one 16 KB bank visible at $8000-$BFFF; selected by rom_select_.
//         apply() clips to available pages; configure_bus_memory_map() remaps.
// Slot 3: OS ROM — 16 KB at $C000-$FFFF
//         $FC00-$FEFF (FRED/JIM/SHEILA) handled by sheila_tick(), not the bus.
//


inline constexpr auto kBBCMicroManifest = make_manifest(
    // Chips
    Slot<MOS6502>{.label = "MOS 6502"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x8000, .label = "RAM"},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x00040000, .label = "Paged ROM", .bank_size = 16384},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "MOS ROM", .rom = {"os12.rom|OS12.ROM|os.rom|OS-1.20.rom|MOS120.rom|bbc_os.rom|os1.2.rom"}},
    Slot<mc6845_t>{.base_addr = 0xFE00, .addr_mask = 0xFFF8, .label = "MC6845 CRTC"},
    Slot<sn76489_t>{.label = "SN76489 PSG"},
    Slot<mos6522_t>{.base_addr = 0xFE40, .addr_mask = 0xFFF0, .label = "System VIA"},
    Slot<mos6522_t>{.base_addr = 0xFE60, .addr_mask = 0xFFF0, .label = "User VIA"},
    Slot<bbc_vidproc_t>{.base_addr = 0xFE20, .addr_mask = 0xFFF0, .label = "Video ULA"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortUserPort>{.name = "User Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv_pal"},
    Slot<PortRgb>{.name = "Video Out (RGB)", .default_device = "crt_cub1431"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kBBCMicroChipCount = decltype(kBBCMicroManifest)::chip_count;

using BBCMicroBusSpec = ManifestBusSpec<kBBCMicroManifest, 16, 8, 1, true>;

struct BBCMicroBoard : Board<BBCMicroBusSpec> {
    using ComponentTuple = decltype(kBBCMicroManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MOS6502&       m6502     = std::get<0>(components_);
    RAMChip&       ram       = std::get<1>(components_);
    ROMChip&       paged_rom = std::get<2>(components_);
    ROMChip&       os_rom    = std::get<3>(components_);
    mc6845_t&      crtc      = std::get<4>(components_);
    sn76489_t&     psg       = std::get<5>(components_);
    mos6522_t&     sys_via   = std::get<6>(components_);
    mos6522_t&     user_via  = std::get<7>(components_);
    bbc_vidproc_t& vidproc   = std::get<8>(components_);

    // Port aliases
    PortCassette&       cassette_port  = std::get<9>(components_);
    PortUserPort&       user_port      = std::get<10>(components_);
    PortCompositeVideo& video_port     = std::get<11>(components_);
    PortRgb&            video_rgb_port = std::get<12>(components_);
    PortAudioMono&      audio_port     = std::get<13>(components_);

    BBCMicroBoard() : Board(kBBCMicroManifest) {}
};
/**
 * BBC Micro Model B System Implementation
 *
 * Acorn BBC Micro Model B (1981):
 *   - MOS 6502A CPU @ 2 MHz
 *   - 32 KB RAM
 *   - 16 KB MOS (OS) ROM + up to 16 sideways ROM slots (16 KB each)
 *   - MC6845 CRTC for display timing
 *   - SN76489 sound chip (3 tone + 1 noise)
 *   - 2x MOS 6522 VIA (System VIA + User VIA)
 *   - Video ULA for pixel generation
 *   - SHEILA I/O page at $FE00-$FEFF
 *
 * Display modes: text (Mode 7 teletext — 40×25) and bitmap (Modes 0-6).
 * Initial implementation: Mode 7 text + bitmap Modes 0-6 for booting the MOS.
 */
class BBCMicroSystem : public System {
public:
    BBCMicroSystem();
    ~BBCMicroSystem() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // File loading
    bool load_file(const char* filepath) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // Audio thread — SN76489 synthesis runs off the emulation thread
    AudioThread audio_thread_;
    std::unique_ptr<WriteOnlySynthAdapter<sn76489_t>> psg_adapter_;
    std::unique_ptr<AudioPort> audio_port_;  // Audio signal output

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<BBCMicroBusSpec>;
    using PT  = PackingTraits<BBCMicroBusSpec>;
    using MainBoard = BBCMicroBoard;
    Bus bus_;
    MainBoard board_;

    // Convenience pointer into flat RAM for rendering hot path
    uint8_t*    memory_ = nullptr;       // → board_.ram.data()

    // Paged ROM state
    uint8_t     rom_select_ = 0;         // Currently selected paged ROM bank (0-15)

    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output

    // Keyboard matrix (10 columns × 8 rows)
    // Each element: true = key pressed
    bool        key_matrix_[10][8]{};
    bool        any_key_pressed_ = false;

    // Addressable latch (accent accent accent accent accent accent accent)
    // Written via System VIA PB0-PB3.  8 bits:
    //   D0: sound chip /WE
    //   D1: speech processor /RS and /WS
    //   D2: speech processor enable
    //   D3: keyboard auto-scan enable
    //   D4: shift lock LED
    //   D5: caps lock LED
    //   D6: not used
    //   D7: not used
    uint8_t     addressable_latch_ = 0;

    // System state
    bus_state_t pins_;
    uint32_t    cycles_per_frame_;
    uint32_t    crtc_divider_ = 0;       // CPU runs at 2 MHz, CRTC at 1 MHz

    // Helper methods
    void configure_bus_memory_map();          // Post-apply() page table fixups
    void update_paged_rom();                  // Remap $8000-$BFFF after rom_select_ change

    // System VIA Port B write callback — addressable latch + SN76489 trigger
    static void sys_via_port_b_write(void* context, uint8_t data);

    // CRTC display callbacks — forward to VIDPROC
    void crtc_display_char(uint16_t ma, uint8_t ra, bool cursor);
    void crtc_vsync();
    void crtc_hsync();

    // VIA callbacks (System VIA)
    static uint8_t sys_via_port_a_read(void* ctx, uint8_t output);
    static uint8_t sys_via_port_b_read(void* ctx, uint8_t output);

    // ROM loading
    bool load_roms();

    // Keyboard mapping
    void update_key_matrix(SDL_Keycode key, bool pressed);
    uint8_t scan_keyboard(uint8_t column) const;
};
