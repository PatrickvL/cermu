#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/video/ted/ted7360.hpp"
#include "chip/cpu/fam65xx/mos7501.hpp"
#include "chip/io/mos6529.hpp"
#include "systems/commodore/c16/c264_io_decoder.hpp"
#include <memory>

class Datasette1530Device;

// C264 series (C16/C116/Plus4) default bus state — derived from CPU.
// CSG7501 provides: RW, RDY, IRQ, AEC.  (No NMI — NO_NMI_LINE flag.)
#define C264_BUS_DEFAULT_STATE  CSG7501::default_bus_state()

#include "core/formats/format_handler.hpp"
#include <string>
#include <cstdint>

// ============================================================================
// TED-based system variant (compile-time template parameter)
// ============================================================================

enum class C264SeriesVariant { C16, C116, PLUS4 };

// ============================================================================
// Compile-time variant traits
// ============================================================================

template<C264SeriesVariant V> struct C264SeriesVariantTraits;

template<> struct C264SeriesVariantTraits<C264SeriesVariant::C16> {
    static constexpr bool has_user_port  = false;
    static constexpr size_t default_ram  = 16384;
    static constexpr const char* name    = "C16";
    static constexpr const char* full_name = "Commodore 16";
    static constexpr const char* short_id = "C16";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"C16"}; }
    static constexpr const char* description =
        "Commodore 16 (1984) - 16KB RAM, TED 7360 graphics and sound";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::C116> {
    static constexpr bool has_user_port  = false;
    static constexpr size_t default_ram  = 16384;
    static constexpr const char* name    = "C116";
    static constexpr const char* full_name = "Commodore 116";
    static constexpr const char* short_id = "C116";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"C116"}; }
    static constexpr const char* description =
        "Commodore 116 (1984) - 16KB RAM, TED 7360, chiclet keyboard variant of C16";
};

template<> struct C264SeriesVariantTraits<C264SeriesVariant::PLUS4> {
    static constexpr bool has_user_port  = true;
    static constexpr size_t default_ram  = 65536;
    static constexpr const char* name    = "Plus/4";
    static constexpr const char* full_name = "Commodore Plus/4";
    static constexpr const char* short_id = "PLUS4";
    static constexpr const char* data_folder = "c16";
    static std::vector<const char*> get_aliases() { return {"Plus4", "Plus/4", "Plus-4"}; }
    static constexpr const char* description =
        "Commodore Plus/4 (1984) - 64KB RAM, TED 7360, built-in 3-PLUS-1 software";
};

// ============================================================================
// C264 chip manifest — declarative memory layout
// ============================================================================
//
// Memory map (CPU view):
//   $0000-$7FFF  RAM (always)
//   $8000-$BFFF  BASIC ROM (read, when ROM enabled) / RAM
//   $C000-$FCFF  KERNAL ROM (read, when ROM enabled) / RAM
//   $FD00-$FDFF  I/O area — c264_io_decoder_t dispatches via 74LS139:
//                  $FD10-$FD1F  PIO1 (MOS 6529B) — user port / tape sense
//                  $FD30-$FD3F  PIO2 (MOS 6529B) — keyboard row select
//                  $FDD0-$FDDF  74LS175 ROM bank latch — write-only
//                  Other $FD addresses → open bus (no chip)
//   $FE00-$FEFF  KERNAL ROM (cont.) / RAM
//   $FF00-$FF3F  TED registers — MaskedSubTable sub-page MMIO
//   $FF40-$FFFF  KERNAL ROM (cont.) / RAM — MaskedSubTable base
//
// ROM banking is controlled by TED latch writes ($FF3E/$FF3F).
// ROM bank pair selected by writes to $FDD0-$FDDF.
// RAM size varies: 16 KB (C16/C116) with mirroring, 64 KB (Plus/4).
//
//                                ctx   type                    chip       base    size    mask    ovl  label              rom


// MaskedSubTable indices (determined by manifest slot order during apply())
namespace c264_sub {
    inline constexpr size_t kTedPage = 0;    // page $FF — TED registers
    inline constexpr size_t kIoPage  = 1;    // page $FD — I/O decoder (74LS139 + 74LS175)
}

inline constexpr auto kC264Manifest = make_manifest(
    // Chips — bank_size = size_bytes for all buffer chips (ROM banking via TED latch)
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x00010000, .label = "RAM", .bank_size = 0x00010000},
    Slot<ROMChip>{.base_addr = 0x8000, .size_bytes = 0x4000, .label = "BASIC ROM", .bank_size = 0x4000, .overlay_group = 1, .rom = {"basic.318006-01.bin|basic.rom|318006-01.bin"}},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x4000, .label = "KERNAL ROM", .bank_size = 0x4000, .overlay_group = 1, .rom = {"kernal.318004-05.bin|kernal.rom|318004-05.bin"}},
    Slot<CSG7501>{.label = "CSG 7501"},
    Slot<ted7360_t>{.base_addr = 0xFF00, .addr_mask = 0xFFC0, .label = "TED 7360"},
    Slot<mos6529_t>{.label = "MOS 6529B PIO1"},
    Slot<mos6529_t>{.label = "MOS 6529B PIO2"},
    Slot<c264_io_decoder_t>{.base_addr = 0xFD00, .addr_mask = 0xFF00, .label = "C264 I/O Decoder"},
    // Ports
    Slot<PortControlDB9>{.name = "Joystick Port 1", .port_number = 1, .default_device = "joystick"},
    Slot<PortControlDB9>{.name = "Joystick Port 2", .port_number = 2, .default_device = "joystick"},
    Slot<PortIecSerial>{.name = "IEC Serial Bus", .is_bus = true, .default_device = "1541"},
    Slot<PortCassette>{.name = "Cassette Port", .default_device = "datasette"},
    Slot<PortUserPort>{.name = "User Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_1702"},
    Slot<PortAudioMono>{.name = "Audio Out"},
    Slot<PortCustom>{.name = "Keyboard", .is_internal = true}
);

inline constexpr size_t kC264ChipCount = decltype(kC264Manifest)::chip_count;


struct C264BusTraits {
    static constexpr const auto& kManifest = kC264Manifest;
    using Spec = ManifestBusSpec<kC264Manifest, 16, 8, 2, true>;  // 2 viewers: CPU + TED video, CS-enabled
};

struct C264Board : Board<C264BusTraits::Spec> {
    using ComponentTuple = decltype(kC264Manifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    RAMChip&           ram        = std::get<0>(components_);
    ROMChip&           basic_rom  = std::get<1>(components_);
    ROMChip&           kernal_rom = std::get<2>(components_);
    CSG7501&           csg7501    = std::get<3>(components_);
    ted7360_t&         ted        = std::get<4>(components_);
    mos6529_t&         pio1       = std::get<5>(components_);
    mos6529_t&         pio2       = std::get<6>(components_);
    c264_io_decoder_t& io_dec     = std::get<7>(components_);

    // Port aliases
    PortControlDB9&     joy1_port       = std::get<8>(components_);
    PortControlDB9&     joy2_port       = std::get<9>(components_);
    PortIecSerial&      iec_serial_port = std::get<10>(components_);
    PortCassette&       cassette_port   = std::get<11>(components_);
    PortUserPort&       user_port       = std::get<12>(components_);
    PortExpansion&      expansion_port  = std::get<13>(components_);
    PortCompositeVideo& video_port      = std::get<14>(components_);
    PortAudioMono&      audio_port      = std::get<15>(components_);
    PortCustom&         keyboard_port   = std::get<16>(components_);

    C264Board() : Board(kC264Manifest) {}
};


// Viewer IDs for the C264 bus
namespace c264_viewer {
    inline constexpr size_t kCpu      = 0;  // CPU memory map (controlled by rom_enabled)
    inline constexpr size_t kTedVideo = 1;  // TED video fetches (controlled by video_romsel)
}

// ============================================================================
// Commodore264System — Commodore 264 Series Emulator (C16, C116, Plus/4)
// ============================================================================
/**
 * Template-based system for the Commodore 264 series (TED 7360 family).
 *
 * The "264 series" was Commodore's internal project codename for the
 * budget computer line released in 1984–1985.  All three machines share
 * the same CPU (MOS 7501), video/sound/IO chip (TED 7360), memory map,
 * and BASIC 3.5 ROM.  The only hardware-level differences are:
 *   - C16:    16 KB RAM, full-travel keyboard, no User Port
 *   - C116:   16 KB RAM, chiclet (rubber) keyboard, no User Port
 *   - Plus/4: 64 KB RAM, full-travel keyboard, User Port, built-in
 *             "3-PLUS-1" productivity ROM
 *
 * These differences are captured by C264SeriesVariantTraits<V> and selected at
 * compile time via the template parameter.
 *
 * Inherits from CommodoreSystem which provides shared Commodore 8-bit
 * infrastructure (keyboard mapper, configuration, speed control).
 */
template<C264SeriesVariant V>
class Commodore264System : public CommodoreSystem {
    using Traits = C264SeriesVariantTraits<V>;

public:
    Commodore264System();
    ~Commodore264System() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration management
    bool apply_configuration() override;

    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

    // Hardware traits (compile-time variant-specific)
    static HardwareTraits create_hardware_traits();

    // --- Test / debug accessors ---
    CSG7501&           cpu()       { return board_.csg7501; }
    ted7360_t&         ted()       { return board_.ted; }
    RAMChip&           ram()       { return board_.ram; }
    const CSG7501&     cpu() const { return board_.csg7501; }
    const ted7360_t&   ted() const { return board_.ted; }
    const RAMChip&     ram() const { return board_.ram; }

    // File probe — returns confidence + optimal configuration for this TED variant
    static SystemProbeResult probe_file_static(
        const format_descriptor_t* matched_format,
        const char* filepath,
        const uint8_t* data, size_t size);

    // Static descriptor accessor (usable without an instance, e.g. for REGISTER_SYSTEM)
    static const SystemDescriptor& static_descriptor();

private:
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output
    std::unique_ptr<AudioPort> audio_port_;            // Audio signal output
    bus_state_t bus_state_;

    // ── Memory bus (declarative manifest + page-pointer dispatch) ────────
    using Bus = MemoryBus<C264BusTraits::Spec>;
    using MainBoard = C264Board;
    Bus bus_;
    MainBoard board_;

    size_t  ram_size_ = 16384;           // Cached configured RAM size (updated in apply_configuration)

    // System state
    bool initialized_;

    /// Cached pointer to datasette on the cassette port (nullptr if none).
    /// Updated by on_port_device_changed() to avoid per-cycle lookups.
    Datasette1530Device* cached_datasette_ = nullptr;

    // ---- CommodoreSystem loading hooks ----
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    bool is_system_initialized() const override { return initialized_; }
    int get_iec_port_index() const override { return 2; }   // IEC Serial Bus
    int get_cassette_port_index() const override { return 3; }  // Cassette Port

    /// Update cached peripheral pointers when devices are attached/detached.
    void on_port_device_changed(int port_index) override;

    // Helper methods
    bool load_roms();
    void setup_ram_mirroring();                 // configure chip_info_ mask for current ram_size_
    void build_banking_snapshots();             // populate overlay snapshots via generic builder
    void apply_cpu_banking();                   // load CPU viewer snapshot for current rom_enabled
    void apply_ted_video_banking();             // load TED viewer snapshot for current video_romsel
    static uint8_t io_port_in(void* user_data);
    static void io_port_out(uint8_t data, void* user_data);
    static uint8_t ted_keyboard_scan(void* user_data, uint8_t column);
    static uint8_t ted_mem_read(void* user_data, uint16_t address);
    static void ted_banking_changed(void* user_data, uint8_t changes);
    static void set_cpu_pc(void* user_data, uint16_t addr);

    // Pre-computed banking mode snapshots for instant mode switching.
    // Indexed as snapshots_[viewer][mode]: mode 0 = RAM only, mode 1 = ROM overlaid.
    // Generated by Board::build_overlay_snapshots() from manifest overlay groups.
    static constexpr size_t kNumViewers = 2;
    static constexpr size_t kNumModes   = kC264Manifest.overlay_mode_count();  // 2
    std::array<std::array<Bus::Snapshot, kNumModes>, kNumViewers> snapshots_;
};

// Convenience type aliases
using C16System   = Commodore264System<C264SeriesVariant::C16>;
using C116System  = Commodore264System<C264SeriesVariant::C116>;
using Plus4System = Commodore264System<C264SeriesVariant::PLUS4>;
