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

// Value-typed chips: CPU + SID + CIA1 + Color RAM + CIA2.
// VIC-II stays factory-created (polymorphic vicii_base_t, PAL/NTSC variant).
struct C64ChipSet : StandardChips<MOS6510, NoChip, mos6581_t, mos6526_t> {
    MOS2114    colorram;
    mos6526_t  cia2;

    template<typename Board> void bind_extras(Board& board) {
        board.bind_chip(board.template find_index<MOS2114>(),    &colorram);
        board.bind_chip(board.template find_index<mos6526_t>(1), &cia2);
    }
    template<typename Board> void register_extras(Board& board) {
        board.register_component(&colorram);
        board.register_component(&cia2);
    }
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
    // class.  The C64 only defines its port layout constants and the
    // system-specific setup_ports() initializer below.
    //

    // =========================================================================
    // CHIP INSTANCES
    // =========================================================================
public:
    MOS6510* mos6510 = nullptr;         // MOS6510 CPU instance
    RAMChip* ram = nullptr;          // RAM memory $0000-$FFFF (64KB)
    ROMChip* cartridge_roml = nullptr; // Cartridge ROM Low $8000-$9FFF (8KB)
    ROMChip* cartridge_romh = nullptr; // Cartridge ROM High $A000-$BFFF (8KB)
    ROMChip* basic = nullptr;        // Basic ROM $A000-$BFFF (8KB)
    ROMChip* charrom = nullptr;      // Character ROM $D000-$DFFF (4KB) when CHAREN=0
    vicii_base_t* vicii = nullptr;      // VIC-II base (traits set at init for PAL/NTSC)
    mos6581_t* sid = nullptr;           // MOS6581 SID sound chip ($D400-$D7FF, 1KB)
    MOS2114* colorram = nullptr;        // Color RAM (1KB at $D800-$DBFF)
    mos6526_t* cia1 = nullptr;          // MOS6526 CIA 1 (BUS_MASK_IRQ) ($DC00-$DDFF, 256 bytes)
    mos6526_t* cia2 = nullptr;          // MOS6526 CIA 2 (BUS_MASK_NMI) ($DD00-$DFFF, 256 bytes)
    commodore_keyboard_t* keyboard = nullptr; // Keyboard matrix (connected to CIA1)
    void* io1 = nullptr;               // Cartridge I/O 1 ($DE00-$DEFF)
    void* io2 = nullptr;               // Cartridge I/O 2 ($DF00-$DFFF)
    ROMChip* kernal = nullptr;       // Kernal ROM $E000-$FFFF (8KB)

    // =========================================================================
    // PLA debug accessors (for PlaChip GUI)
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
    Board<C64BusSpec, C64ChipSet> board_{kC64Chips}; // Board — owns flat mem, chip binding

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
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video stream output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    vicii_standard_t created_vicii_standard_ = VIC_PAL;  // Actual VIC-II standard at creation time
    sid_revision_t pending_sid_revision_ = SID_REVISION_6581_R4AR;  // Applied after SID creation

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
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

    /// Cached lightpen pointer (used by LP pin callback for zero-overhead access).
    LightpenDevice* get_cached_lightpen() const { return cached_lightpen_; }

    /// Update cached lightpen pointer when devices change on Control Port 1.
    void on_port_device_changed(int port_index) override;

private:
    /// Create and wire up all C64 connector ports (CIA1 joystick callbacks etc).
    void setup_ports();
    std::vector<DefaultPeripheral> get_default_peripherals() const override;

    /// Pass the current display rect to any lightpen on Control Port 1 (once per frame).
    void update_lightpen_display_rect();

    /// Cached pointer to lightpen device on Control Port 1 (nullptr if none).
    /// Updated by on_port_device_changed() to avoid per-cycle lookups.
    LightpenDevice* cached_lightpen_ = nullptr;

    /// True when at least one drive is attached to PORT_IEC_SERIAL.
    /// Updated by on_port_device_changed() to skip the per-cycle opdone()
    /// + PC range check when no drive is present.
    bool serial_traps_enabled_ = false;

    // =========================================================================
    // KERNAL SERIAL TRAPS
    // =========================================================================
    // Intercept KERNAL ROM serial bus routines to provide instant IEC I/O.
    // This is the standard approach (same as VICE's serial-trap.c) — when the
    // CPU reaches specific KERNAL addresses, the C++ trap handler executes
    // the operation directly on the Drive1541Device channel buffers instead
    // of bit-banging the IEC bus protocol.
    //
    // Trap addresses for KERNAL 901227-03:
    //   $ED24: SerialListen      → serial_trap_attention()
    //   $ED37: SerialSaListen    → serial_trap_attention()
    //   $ED41: SerialSendByte    → serial_trap_send()
    //   $EE14: SerialReceiveByte → serial_trap_receive()
    //   $EEA9: SerialReady       → serial_trap_ready()
    // Resume address: $EDAB (RTS)
    // =========================================================================

    struct SerialTrapState {
        uint8_t trap_device = 0;      ///< LISTEN/TALK command (0x20+dev or 0x40+dev)
        uint8_t trap_secondary = 0;   ///< Secondary address command byte
        int active_device = -1;       ///< Device number currently addressed (-1 = none)
    } serial_trap_;

    /// Find a 1541 drive for a given device number on the IEC bus.
    class Drive1541Device* find_iec_drive(int device_number);

    /// Check if PC matches a serial trap address; if so, handle it.
    bool check_serial_traps(uint16_t pc);

    /// Trap handlers (return true if handled).
    bool serial_trap_attention();
    bool serial_trap_send();
    bool serial_trap_receive();
    bool serial_trap_ready();
};