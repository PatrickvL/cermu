#pragma once

#include "../commodore/commodore_system.h"
#include "../../core/connector.h"
#include "../../core/device_registry.h"
#include "../../core/formats/format_handler.h"
#include "../../core/formats/sid_format.h"
// Chip headers (previously included via c64.h)
#include "../../chip/memory/memory_chip.h"
#include "../../chip/memory/mos2114.h"
#include "../../chip/sound/mos6581.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/input/commodore_keyboard.h"
#include "c64_bus.h"
#include "c64_config.h"
#include <string>
#include <vector>

#include "../../chip/cpu/fam65xx/mos6510.h"

class LightpenDevice;

/**
 * C64System — Commodore 64 system emulation.
 *
 * CONSOLIDATION STATUS (see docs/C64_CONSOLIDATION_PLAN.md):
 * ==========================================================
 * Phases 1a-1c, 2, 3, 4a, 4c, 5, 6 are complete:
 *   - c64_t embedded directly (no heap allocation)
 *   - tick/reset/init/shutdown/framebuffer absorbed as methods
 *   - PLA generation, memory init, CPU banking callback absorbed
 *   - Bus back-pointer typed to C64SystemData*, container_of removed
 *   - gui_state_t eliminated; chip debug uses generic ChipInfo
 *   - Legacy chip registry (system_8bit_t) eliminated
 *   - Class renamed C64SystemWrapper → C64System
 *   - SimpleSystemGUI → SystemGUI
 *   - Dead legacy GUI code removed (imgui_interface, c64_main, c64_main_gui)
 *   - Residual "wrapper" terminology cleaned up
 *
 * Remaining:
 *   - Phase 4b: Merge c64_config_t into SystemConfiguration
 *
 * c64.cpp still provides c64_system_create/init/tick/reset for the test
 * framework's independent code path.
 */
class C64System : public CommodoreSystem {
public:
    C64System();
    ~C64System() override;
    
    // EmulatedSystem interface - system-specific overrides
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    void tick() override;
    void run_frame() override;
    bool load_file(const char* filepath) override;
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
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

    // Auto-detect region (PAL/NTSC) from file contents
    SystemConfiguration detect_optimal_configuration(
        const char* filepath, const uint8_t* data, size_t size) override;

    // Audio output — drains SID ring buffer
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

    // Update SID sample rate to match actual audio device rate
    void set_audio_sample_rate(int sample_rate_hz) override;
    // Apply KERNAL RAMTAS patch to skip the memory test during boot.
    // Returns true if the patch was applied.
    bool patch_skip_memtest();

    // Access to internal C64 configuration (needed by test framework)
    c64_config_t& get_c64_config() { return c64_config_; }
    const c64_config_t& get_c64_config() const { return c64_config_; }

    // --- Connector Port Access -----------------------------------------
    //
    // Connector ports, owned devices, attach/detach, and the generic
    // peripheral connector UI are all provided by the EmulatedSystem base
    // class.  The C64 only defines its port layout constants and the
    // system-specific setup_connector_ports() initializer below.
    //

    // =========================================================================
    // CHIP INSTANCES — formerly in C64SystemData struct
    // =========================================================================
public:
    c64_bus_t bus{};                    // C64 bus controller (embedded, not heap-allocated)
    MOS6510* mos6510 = nullptr;         // MOS6510 CPU instance
    MemoryChip* ram = nullptr;          // RAM memory $0000-$FFFF (64KB)
    MemoryChip* cartridge_roml = nullptr; // Cartridge ROM Low $8000-$9FFF (8KB)
    MemoryChip* cartridge_romh = nullptr; // Cartridge ROM High $A000-$BFFF (8KB)
    MemoryChip* basic = nullptr;        // Basic ROM $A000-$BFFF (8KB)
    MemoryChip* charrom = nullptr;      // Character ROM $D000-$DFFF (4KB) when CHAREN=0
    vicii_t* vicii = nullptr;           // mos6567_t (NTSC) or mos6569_t (PAL) ($D000-$DFFF, 4KB)
    mos6581_t* sid = nullptr;           // MOS6581 SID sound chip ($D400-$D7FF, 1KB)
    mos2114_t* colorram = nullptr;      // Color RAM (1KB at $D800-$DBFF)
    mos6526_t* cia1 = nullptr;          // MOS6526 CIA 1 (BUS_MASK_IRQ) ($DC00-$DDFF, 256 bytes)
    mos6526_t* cia2 = nullptr;          // MOS6526 CIA 2 (BUS_MASK_NMI) ($DD00-$DFFF, 256 bytes)
    commodore_keyboard_t* keyboard = nullptr; // Keyboard matrix (connected to CIA1)
    void* io1 = nullptr;               // Cartridge I/O 1 ($DE00-$DEFF)
    void* io2 = nullptr;               // Cartridge I/O 2 ($DF00-$DFFF)
    MemoryChip* kernal = nullptr;       // Kernal ROM $E000-$FFFF (8KB)

private:
    bool initialized_ = false;          // True when initialize() has succeeded
    c64_config_t c64_config_;  // Renamed to avoid conflict with base class config_
    vicii_standard_t created_vicii_standard_ = VIC_PAL;  // Actual VIC-II standard at creation time
    sid_revision_t pending_sid_revision_ = SID_REVISION_6581_R4AR;  // Applied after SID creation

    // =========================================================================
    // DEFERRED LOADING
    // =========================================================================
    // File loading is deferred until KERNAL/BASIC boot completes. This avoids
    // the problem where BASIC's cold-start NEW routine zeros $0801/$0802,
    // corrupting program data loaded before boot. The system stores the
    // parsed format result and applies it only after BASIC reaches its READY
    // state (warm-start vector set, keyboard buffer empty).
    //
    // MEDIA ATTACHMENT: For D64 and TAP files, the media is inserted into the
    // appropriate storage device (1541 drive or datasette). D64 files also
    // extract the first PRG for fast direct-load (hybrid approach: disk is
    // available for directory listing AND the first program auto-runs). TAP
    // files are loaded into the datasette; full tape loading requires KERNAL
    // cassette I/O integration (CASS_READ signal wiring to CPU I/O port).
    //
    // FUTURE OPTIMIZATION: Some files (e.g. raw ML at $C000, or programs
    // that never touch KERNAL/BASIC-initialized memory) could be loaded
    // earlier — even before BASIC or KERNAL init completes. This would
    // reduce the perceived startup latency. Combined with techniques like
    // patching out KERNAL's memory test/clear loops, this could allow
    // near-instant startup for many programs. Not yet implemented; the
    // current approach prioritizes correctness over speed.
    // =========================================================================

    /** How the deferred load should be applied after BASIC READY. */
    enum class LoadMode {
        DIRECT,         ///< Standard: write program data to RAM, inject RUN
        DISK_FAST,      ///< D64: disk inserted in 1541 + fast PRG extraction to RAM
        TAPE_INSERTED   ///< TAP: tape loaded in datasette, inject LOAD + press play
    };

    struct PendingLoad {
        format_load_result_t result;  // Parsed file data (owns heap allocations)
        std::string filepath;         // Original filepath for SYS-from-filename
        bool active = false;          // Whether a deferred load is pending
        LoadMode mode = LoadMode::DIRECT;  // How to apply the load
    };
    PendingLoad pending_load_;
    bool boot_completed_ = false;  // Set after first deferred load; skips VARTAB check

    /** Check if BASIC has reached its READY state (safe to inject program). */
    bool is_basic_ready() const;

    /** Apply the pending load result to RAM and inject auto-run. */
    void apply_pending_load();

    /** Single system tick — ticks all chips in correct phase order. */
    void system_tick();

    /** Generate PLA memory maps and set initial banking mode. */
    bool pla_maps_generate();

    /** Initialize RAM, color RAM, and load ROMs from configured paths. */
    void memory_init(const c64_config_t* config);

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
    // total_cycles_ are now stored in EmulatedSystem base class
    
    // =========================================================================
    // CONNECTOR PORTS — C64-SPECIFIC LAYOUT
    // =========================================================================
public:
    // Indices into connector_ports_ for quick access (public for CIA1 callbacks)
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
    void setup_connector_ports();

    /// Register all C64 chips into registered_chips_ for the Hardware menu.
    void register_c64_chips();

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