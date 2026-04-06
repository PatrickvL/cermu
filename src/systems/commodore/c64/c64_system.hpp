#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/port.hpp"
#include "core/device_registry.hpp"
#include "core/formats/format_handler.hpp"
#include "core/formats/sid_format.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
// Chip headers (previously included via c64.h)
#include "chip/memory/memory_chip.hpp"
#include "chip/memory/mos2114.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/io/mos6526.hpp"
#include "chip/video/vic_ii/vicii_common.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "systems/commodore/c64/c64_manifest.hpp"
#include "systems/commodore/c64/c64_config.hpp"
#include <memory>
#include <string>
#include <vector>

#include "chip/cpu/fam65xx/mos6510.hpp"

class LightpenDevice;

// C64Board — value-typed chip and port fields via TypedManifest component_tuple.
// Buffer-backed chips (RAMChip, ROMChip) get their flat-memory buffer via
// Board::bind_chip() → on_bind_buffer().  MMIO chips (size==0) are bound
// without a buffer.  Ports are initialized via bind_all().
struct C64Board : Board<C64BusSpec> {
    using ComponentTuple = decltype(kC64Chips)::component_tuple;
    ComponentTuple components_;

    // Chip aliases [0..11]
    MOS6510&      cpu      = std::get<0>(components_);
    RAMChip&      ram      = std::get<1>(components_);
    ROMChip&      roml     = std::get<2>(components_);
    ROMChip&      basic    = std::get<3>(components_);
    ROMChip&      romh     = std::get<4>(components_);
    vicii_base_t& vicii    = std::get<5>(components_);
    ROMChip&      charrom  = std::get<6>(components_);
    mos6581_t&    sid      = std::get<7>(components_);
    MOS2114&      colorram = std::get<8>(components_);
    mos6526_t&    cia1     = std::get<9>(components_);
    mos6526_t&    cia2     = std::get<10>(components_);
    ROMChip&      kernal   = std::get<11>(components_);

    // Port aliases [12..20]
    PortControlDB9&     control1_port  = std::get<12>(components_);
    PortControlDB9&     control2_port  = std::get<13>(components_);
    PortIecSerial&      iec_port       = std::get<14>(components_);
    PortCassette&       cassette_port  = std::get<15>(components_);
    PortUserPort&       user_port      = std::get<16>(components_);
    PortExpansion&      expansion_port = std::get<17>(components_);
    PortCompositeVideo& video_port     = std::get<18>(components_);
    PortAudioMono&      audio_port     = std::get<19>(components_);
    PortCustom&         keyboard_port  = std::get<20>(components_);

    C64Board() : Board(kC64Chips) {}
};

/**
 * C64System — Commodore 64 system emulation.
 *
 * Owns all chip instances (CPU, VIC-II, SID, CIAs, RAM, ROMs) and drives
 * the per-cycle tick loop.
 */
class C64System : public CommodoreSystem {
public:
    C64System();
    ~C64System() override;
    
    // System interface - system-specific overrides
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    void tick() override;
    void run_frame() override;
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // Video port access — enables DisplayPipeline connection from host
    void* get_video_port_ptr() override { return video_port_.get(); }

    // GUI rendering overrides
    void render_system_menu_items() override;
    void render_debug_windows(void* gui_state, std::mutex& emu_mutex) override;

    // Window title metadata
    const char* get_mode_label() const override;
    std::string get_subtitle_info() const override;
    
    // Configuration interface
    bool apply_configuration() override;
    void render_configuration_ui() override;

    // Audio output — drains SID ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

    // Update SID sample rate to match actual audio device rate
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Apply KERNAL RAMTAS patch to skip the memory test during boot.
    // Returns true if the patch was applied.
    bool patch_skip_memtest();

    // --- Connector Port Access -----------------------------------------
    //
    // Connector ports, owned devices, attach/detach, and the generic
    // peripheral connector UI are all provided by the System base
    // class.  Ports are declared in kC64Chips (the TypedManifest) and
    // created by bind_all() during initialize().
    //

    // =========================================================================
    // CHIP ACCESS — zero-cost references into board_ component tuple.
    //
    // All manifest-declared chips and ports are owned by board_ and accessed
    // via its named std::get<N> references (e.g. board_.cpu, board_.vicii).
    // The following members are NOT board components:
    // =========================================================================
public:
    commodore_keyboard_t* keyboard = nullptr; // Keyboard matrix (connected to CIA1)
    void* io1 = nullptr;               // Cartridge I/O 1 ($DE00-$DEFF)
    void* io2 = nullptr;               // Cartridge I/O 2 ($DF00-$DFFF)

    // =========================================================================
    // PLA debug accessors (for PLA906114 system-specific GUI)
    // =========================================================================
    uint8_t get_pla_banking_mode() const { return pla_banking_mode_; }
    bus_state_t get_bus_state() const { return bus_state_; }

    // Cartridge signal accessors
    void set_exrom_signal(bool active);
    void set_game_signal(bool active);
    void set_cartridge_signals(bool exrom_active, bool game_active);
    bool get_exrom_signal() const;
    bool get_game_signal() const;

    // Debug memory access (reads/writes through current PLA configuration)
    uint8_t read_memory(uint16_t addr);
    void write_memory(uint16_t addr, uint8_t value);

    // Banking change — also used by test framework
    void on_banking_change(uint8_t banking_state);

    // ==========================================================================
    // MANIFEST-DRIVEN BUS
    // =========================================================================
    C64Bus   bus_;                       // MemoryBus<C64BusSpec> — page-table dispatch
    C64Board board_;                     // Board — owns flat mem, chip binding

    // PLA banking — 32 modes × 2 viewers (CPU + VIC-II)
    std::array<C64Snapshot, kC64NumPlaModes> cpu_snapshots_;    // Viewer 0
    std::array<C64Snapshot, kC64NumPlaModes> vicii_snapshots_;  // Viewer 1
    uint8_t pla_banking_mode_ = 0;      // Current PLA mode (0-31)
    uint8_t system_lines_     = 0;      // EXROM/GAME cartridge signals
    bus_state_t default_state_ = 0;     // Pull-up defaults for each cycle
    bus_state_t bus_state_     = 0;     // Current bus state (end of previous tick)

    // PLA debug tables — PLA chip ID per mode per 4KB bank (for PLA GUI)
    C64PlaChipId pla_cpu_read_chip_[32][16];   // CPU read chip per [mode][bank]
    C64PlaChipId pla_cpu_write_chip_[32][16];  // CPU write chip per [mode][bank]
    C64PlaChipId pla_vicii_read_chip_[32][16]; // VIC-II read chip per [mode][bank]

private:
    bool initialized_ = false;          // True when initialize() has succeeded
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    vicii_standard_t created_vicii_standard_ = VIC_PAL;  // Actual VIC-II standard at creation time
    sid_revision_t pending_sid_revision_ = SID_REVISION_6581_R4AR;  // Applied after SID creation

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    bool is_system_initialized() const override { return initialized_; }
    bool on_file_parsed(format_load_result_t& result, const char* filepath) override;
    bool pre_apply_pending_load() override;
    int get_iec_port_index() const override { return PORT_IEC_SERIAL; }
    int get_cassette_port_index() const override { return PORT_CASSETTE; }

    /** Single system tick — ticks all chips in correct phase order. */
    void system_tick();

    /** Derive VIC-II standard from SystemConfiguration region index. */
    vicii_standard_t get_vicii_standard() const;

    /** Generate PLA memory maps and set initial banking mode. */
    bool pla_maps_generate();

    /** Switch to a PLA banking mode — loads snapshots for both viewers. */
    void mode_switch(uint8_t mode);

    /** Compute PLA mode from CPU port bits + cartridge signals. */
    uint8_t generate_pla_mode(uint8_t cpu_port_bits) const;

    /** Wire the IndexedSubTable and MMIO handlers for $D000-$DFFF. */
    void init_io_dispatch();

    /** Initialize RAM, color RAM, and load ROMs from configured paths. */
    void memory_init();

    /**
     * Ensure the C64 is configured compatibly for a SID file's requirements.
     *
     * Checks the SID header's video standard (PAL/NTSC) and SID model
     * (6581/8580) against the current system configuration.  If the region
     * doesn't match, the C64 is destroyed and recreated with the correct
     * VIC-II standard.  If only the SID revision differs, it is updated
     * in place.  In all cases a reset is performed and the RAMTAS memory
     * test is patched out for fast boot.
     */
    void ensure_compatible_for_sid(const sid_header_t* sid);

    // =========================================================================
    // SID PLAYER STATE
    // =========================================================================
    // When a SID file is loaded, we keep a copy of its header and payload
    // so the user can switch subtunes interactively (digits 0-9 for direct
    // selection, left/right cursor keys for prev/next with wrapping).
    // =========================================================================
    bool sid_player_active_ = false;            ///< True while a SID file is playing
    sid_header_t active_sid_header_{};           ///< Copy of the loaded SID header
    std::vector<uint8_t> active_sid_data_;       ///< Copy of original payload bytes
    uint16_t active_subtune_ = 0;               ///< Current 0-based subtune index

    /** Handle SID player keyboard shortcuts (subtune selection).
     *  Returns true if the key was consumed (should not be forwarded to C64). */
    bool handle_sid_player_key(SDL_Keycode key);

    // Note: hardware_traits_, config_ (SystemConfiguration), speed_multiplier_,
    // total_cycles_ are now stored in System base class
    
    // =========================================================================
    // CONNECTOR PORTS — C64-SPECIFIC LAYOUT
    // =========================================================================
public:
    // Indices into get_port() for quick access (public for CIA1 callbacks)
    static constexpr int PORT_CONTROL1   = 0;
    static constexpr int PORT_CONTROL2   = 1;
    static constexpr int PORT_IEC_SERIAL = 2;
    static constexpr int PORT_CASSETTE   = 3;
    static constexpr int PORT_USER       = 4;
    static constexpr int PORT_EXPANSION  = 5;
    static constexpr int PORT_VIDEO      = 6;
    static constexpr int PORT_AUDIO      = 7;
    static constexpr int PORT_KEYBOARD   = 8;

    /// Cached lightpen pointer (used by LP pin callback for zero-overhead access).
    LightpenDevice* get_cached_lightpen() const { return cached_lightpen_; }

    /// Update cached lightpen pointer when devices change on Control Port 1.
    void on_port_device_changed(int port_index) override;

private:
    /// Pass the current display rect to any lightpen on Control Port 1 (once per frame).
    void update_lightpen_display_rect();

    /// Cached pointer to lightpen device on Control Port 1 (nullptr if none).
    /// Updated by on_port_device_changed() to avoid per-cycle lookups.
    LightpenDevice* cached_lightpen_ = nullptr;

    /// True when at least one drive is attached to PORT_IEC_SERIAL.

    // =========================================================================
    // KERNAL SERIAL TRAPS — dispatch (uses shared handlers from CommodoreSystem)
    // =========================================================================

    /// Check if PC matches a C64 KERNAL serial trap address; if so, handle it.
    bool check_serial_traps(uint16_t pc);
};