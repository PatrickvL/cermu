/*
 * atari_vector_system.cpp — Atari vector arcade system implementation
 *
 * Shared implementation for Asteroids and Lunar Lander.
 *
 * Tick loop (per CPU cycle at 1.512 MHz):
 *   1. CPU PHI2 → address + data on bus
 *   2. Address decode:
 *      - $0000-$03FF → Work RAM (via MemoryBus)
 *      - $2000-$2FFF → I/O reads (manual dispatch)
 *      - $3000-$3FFF → I/O writes (manual dispatch)
 *      - $4000-$47FF → Vector RAM (via MemoryBus)
 *      - $5000-$57FF → Vector ROM (via MemoryBus)
 *      - $6000/$6800+ → Program ROM (via MemoryBus)
 *      - Unmapped → open bus
 *   3. CPU PHI1
 *   4. DVG tick (runs concurrently)
 *   5. NMI timer (fires every ~6048 CPU cycles ≈ 250 Hz)
 *
 * ROM file format:
 *   Raw binary ROM dumps.  The system expects the full program ROM
 *   concatenated with vector ROM if separate.
 */

#include "systems/arcade/atari_vector/atari_vector_system.hpp"
#include "core/rom_set.hpp"
#include "core/system_registry.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

namespace atv = atari_vector_constants;

// ============================================================================
// ROM SET DESCRIPTORS
// ============================================================================
//
// Each Atari vector game shipped with multiple ROM chips.  These descriptors
// map the individual ROM files (identified by part-number substrings) to their
// memory addresses so the generic rom_set loader can place them correctly.
//
// Asteroids:  3 × 2 KB program ROMs ($6800-$7FFF) + 1 × 2 KB vector ROM ($5000)
// Lunar Lander: 4 × 2 KB program ROMs ($6000-$7FFF) + 1–2 × 2 KB vector ROMs ($5000+)

// ── Asteroids Rev 1 ──────────────────────────────────────────────────────────

static const RomEntryDescriptor ast_v1_entries[] = {
    // Vector ROM — DVG display list ROM at $5000
    { {"035127.01", "035127-01"},   0x5000, 2048, true  },
    // Program ROMs — three 2 KB chips covering $6800-$7FFF
    { {"035145.01", "035145-01"},   0x6800, 2048, true  },   // socket ef2
    { {"035144.01", "035144-01"},   0x7000, 2048, true  },   // socket h2
    { {"035143.01", "035143-01"},   0x7800, 2048, true  },   // socket j2 (reset vector)
};

static const RomSetDescriptor ast_v1_romset = {
    "Asteroids Rev 1", "Asteroids",
    ast_v1_entries, 4
};

// ── Asteroids Rev 2 ──────────────────────────────────────────────────────────

static const RomEntryDescriptor ast_v2_entries[] = {
    { {"035127.02", "035127-02"},   0x5000, 2048, true  },
    { {"035145.02", "035145-02"},   0x6800, 2048, true  },
    { {"035144.02", "035144-02"},   0x7000, 2048, true  },
    { {"035143.02", "035143-02"},   0x7800, 2048, true  },
};

static const RomSetDescriptor ast_v2_romset = {
    "Asteroids Rev 2", "Asteroids",
    ast_v2_entries, 4
};

// ── Lunar Lander Rev 1 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v1_entries[] = {
    // Vector ROMs — 034599 at $4800, 034598 at $5000
    { {"034599.01", "034599-01", "LLVROM1"},  0x4800, 2048, true  },
    { {"034598.01", "034598-01", "LLVROM0"},  0x5000, 2048, true  },
    // Program ROMs — four 2 KB chips covering $6000-$7FFF
    { {"034572.01", "034572-01"},             0x6000, 2048, true  },   // socket c1
    { {"034571.01", "034571-01", "LLPROM2"},  0x6800, 2048, true  },   // socket de1
    { {"034570.01", "034570-01", "LLPROM1"},  0x7000, 2048, true  },   // socket f1
    { {"034569.01", "034569-01", "LLPROM0"},  0x7800, 2048, true  },   // socket j1 (reset vector)
    // Language PROM (optional)
    { {"034597.01", "034597-01"},             0x0000, 2048, false },
};

static const RomSetDescriptor ll_v1_romset = {
    "Lunar Lander Rev 1", "LunarLander",
    ll_v1_entries, 7
};

// ── Lunar Lander Rev 2 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v2_entries[] = {
    // Vector ROMs — same layout as v1: 034599 at $4800, 034598 at $5000
    { {"034599.01", "034599-01", "LLVROM1"},  0x4800, 2048, true  },
    { {"034598.01", "034598-01", "LLVROM0"},  0x5000, 2048, true  },
    // Program ROMs — rev 2 chips
    { {"034572.02", "034572-02"},             0x6000, 2048, true  },
    { {"034571.02", "034571-02"},             0x6800, 2048, true  },
    { {"034570.02", "034570-02"},             0x7000, 2048, true  },
    { {"034569.02", "034569-02"},             0x7800, 2048, true  },
    { {"034597.01", "034597-01"},             0x0000, 2048, false },
};

static const RomSetDescriptor ll_v2_romset = {
    "Lunar Lander Rev 2", "LunarLander",
    ll_v2_entries, 7
};

// ── Asteroids Deluxe Rev 1 ───────────────────────────────────────────────
//
// Program ROMs: 4 × 2 KB at $6000-$7FFF
// Vector ROMs:  2 × 2 KB
//   036800 at DVG offset $0800 → CPU $4800 (load_address $4800)
//   036799 at DVG offset $1000 → CPU $5000 (load_address $5000)

static const RomEntryDescriptor ad_v1_entries[] = {
    // Vector ROMs
    { {"036800.01", "036800-01"},   0x4800, 2048, true  },   // DVG offset $0800
    { {"036799.01", "036799-01"},   0x5000, 2048, true  },   // DVG offset $1000
    // Program ROMs
    { {"036430.01", "036430-01"},   0x6000, 2048, true  },   // socket c1
    { {"036431.01", "036431-01"},   0x6800, 2048, true  },   // socket de1
    { {"036432.01", "036432-01"},   0x7000, 2048, true  },   // socket f1
    { {"036433.02", "036433-02"},   0x7800, 2048, true  },   // socket j1 (reset vector)
};

static const RomSetDescriptor ad_v1_romset = {
    "Asteroids Deluxe Rev 1", "AsteroidsDeluxe",
    ad_v1_entries, 6
};

// ── Asteroids Deluxe Rev 2 ───────────────────────────────────────────────

static const RomEntryDescriptor ad_v2_entries[] = {
    // Vector ROMs (036800 updated, 036799 same as v1)
    { {"036800.02", "036800-02"},   0x4800, 2048, true  },
    { {"036799.01", "036799-01"},   0x5000, 2048, true  },
    // Program ROMs
    { {"036430.02", "036430-02"},   0x6000, 2048, true  },
    { {"036431.02", "036431-02"},   0x6800, 2048, true  },
    { {"036432.02", "036432-02"},   0x7000, 2048, true  },
    { {"036433.03", "036433-03"},   0x7800, 2048, true  },
};

static const RomSetDescriptor ad_v2_romset = {
    "Asteroids Deluxe Rev 2", "AsteroidsDeluxe",
    ad_v2_entries, 6
};

// ── Battlezone Rev 1 ─────────────────────────────────────────────────────
//
// Battlezone: 6 × 2 KB program ROMs ($5000-$7FFF), 2 × 2 KB vector ROMs ($3000-$3FFF)

static const RomEntryDescriptor bz_v1_entries[] = {
    // Vector ROMs
    { {"036422.01", "036422-01"},   0x3000, 2048, true  },   // vector ROM 1
    { {"036421.01", "036421-01"},   0x3800, 2048, true  },   // vector ROM 2
    // Program ROMs ($5000-$7FFF)
    { {"036414.01", "036414-01"},   0x5000, 2048, true  },
    { {"036413.01", "036413-01"},   0x5800, 2048, true  },
    { {"036412.01", "036412-01"},   0x6000, 2048, true  },
    { {"036411.01", "036411-01"},   0x6800, 2048, true  },
    { {"036410.01", "036410-01"},   0x7000, 2048, true  },
    { {"036409.01", "036409-01"},   0x7800, 2048, true  },
};

static const RomSetDescriptor bz_v1_romset = {
    "Battlezone Rev 1", "Battlezone",
    bz_v1_entries, 8
};

// ── Battlezone Rev 2 ─────────────────────────────────────────────────────

static const RomEntryDescriptor bz_v2_entries[] = {
    { {"036422.01", "036422-01"},   0x3000, 2048, true  },
    { {"036421.01", "036421-01"},   0x3800, 2048, true  },
    { {"036414.02", "036414-02"},   0x5000, 2048, true  },
    { {"036413.02", "036413-02"},   0x5800, 2048, true  },
    { {"036412.02", "036412-02"},   0x6000, 2048, true  },
    { {"036411.02", "036411-02"},   0x6800, 2048, true  },
    { {"036410.02", "036410-02"},   0x7000, 2048, true  },
    { {"036409.02", "036409-02"},   0x7800, 2048, true  },
};

static const RomSetDescriptor bz_v2_romset = {
    "Battlezone Rev 2", "Battlezone",
    bz_v2_entries, 8
};

// ── Red Baron ────────────────────────────────────────────────────────────
//
// Red Baron: 6 × 2 KB program ROMs ($5000-$7FFF), 2 × 2 KB vector ROMs ($3000-$3FFF)

static const RomEntryDescriptor rb_entries[] = {
    // Vector ROMs
    { {"037006.01", "037006-01"},   0x3000, 2048, true  },   // vector ROM 1
    { {"037007.01", "037007-01"},   0x3800, 2048, true  },   // vector ROM 2
    // Program ROMs ($5000-$7FFF)
    { {"037001.01", "037001-01"},   0x5000, 2048, true  },
    { {"037000.01", "037000-01"},   0x5800, 2048, true  },
    { {"036999.01", "036999-01"},   0x6000, 2048, true  },
    { {"036998.01", "036998-01"},   0x6800, 2048, true  },
    { {"036997.01", "036997-01"},   0x7000, 2048, true  },
    { {"036996.01", "036996-01"},   0x7800, 2048, true  },
};

static const RomSetDescriptor rb_romset = {
    "Red Baron", "RedBaron",
    rb_entries, 8
};

// ── Tempest Rev 3 ────────────────────────────────────────────────────────
//
// Tempest: 10 × 2 KB program ROMs + 2 × 2 KB vector ROMs.
// MAME confirms part numbers for V3 (tempest3): most chips shared with
// V1/V2, with V3-specific replacements: -217 (J1), -222 (R1), -316 (H1).
//
// The real hardware maps program ROM at $9000-$DFFF (20 KB) using A15
// for bank select.  Our 15-bit address scheme maps only $4000-$7FFF
// (16 KB), so the first 2 program ROMs ($9000-$9FFF → 15-bit $1000-$1FFF)
// overflow the program ROM chip and are skipped by the loader with a
// warning.  Full 16-bit address decode is needed for correct execution.
//
// ROM order matches MAME: lowest CPU address first.

static const RomEntryDescriptor tempest_v3_entries[] = {
    // Vector ROM (AVG display list ROM at $3000)
    { {"136002.111", "136002-111"},  0x3000, 0, true  },
    // Program ROMs — MAME loads at $9000-$CFFF for V3 (only 8 chips, not 10).
    // V3 uses different part numbers from V1/V2.
    // The zip contains: .133,.134,.235,.316,.217,.138,.136,.237
    { {"136002.133", "136002-133"},  0x9000, 0, true  },   // D1
    { {"136002.134", "136002-134"},  0x9800, 0, true  },   // E1
    { {"136002.235", "136002-235"},  0xA000, 0, true  },   // F1 (V3)
    { {"136002.316", "136002-316"},  0xA800, 0, true  },   // H1 (V3)
    { {"136002.217", "136002-217"},  0xB000, 0, true  },   // J1 (V2/V3)
    { {"136002.138", "136002-138"},  0xB800, 0, true  },   // K1
    { {"136002.136", "136002-136"},  0xC000, 0, true  },   // L/M1
    { {"136002.237", "136002-237"},  0xC800, 0, true  },   // M/N1 (V3)
};

static const RomSetDescriptor tempest_v3_romset = {
    "Tempest Rev 3", "Tempest",
    tempest_v3_entries, 9
};

// ── Gravitar Rev 2 ───────────────────────────────────────────────────────
//
// Gravitar: program ROMs are 4 KB chips (4096 bytes), not 2 KB.
// The actual ROM files in distribution zips (e.g. 136010.201) are 4096 bytes.
// TODO: Restructure entries with 4 KB sizes and 4 KB address stride once
//       the full MAME-to-address mapping is verified.
// Patterns include both dot-separated (actual files) and dash-separated
// (MAME naming convention) forms for flexible matching.

static const RomEntryDescriptor gravitar_v2_entries[] = {
    // Vector ROMs at $3000 (MAME: 136010-101, 136010-102 — 2 KB each)
    { {"136010.101", "136010-101"},  0x3000, 2048, true  },
    { {"136010.102", "136010-102"},  0x3800, 2048, true  },
    // Program ROMs — MAME loads at $5000-$9FFF (5 × 4 KB, full 16-bit).
    // expected_size=0 allows matching regardless of file size.
    { {"136010.210", "136010-210"},  0x5000, 0, true  },
    { {"136010.207", "136010-207"},  0x6000, 0, true  },
    { {"136010.208", "136010-208"},  0x7000, 0, true  },
    { {"136010.209", "136010-209"},  0x8000, 0, true  },
    { {"136010.201", "136010-201"},  0x9000, 0, true  },
};

static const RomSetDescriptor gravitar_v2_romset = {
    "Gravitar Rev 2", "Gravitar",
    gravitar_v2_entries, 7
};

// ── Gravitar Rev 3 ───────────────────────────────────────────────────────
//
// Rev 3 uses different part numbers: 136010.301-309 + shared 136010.210.
// Vector ROM: 136010.302-306 (5 × 4 KB), program ROM: 136010.301,309,307-308,210.
// Based on the actual zip contents and MAME naming conventions.

static const RomEntryDescriptor gravitar_v3_entries[] = {
    // Vector ROMs (in the avgdvg region — mapped to $3000+)
    { {"136010.302", "136010-302"},  0x3000, 0, false },
    // Program ROMs — same MAME layout: $5000-$9FFF.
    // V3 replaces .209→.309 and .201→.301; others (.210,.207,.208) are shared with V2.
    { {"136010.210", "136010-210"},  0x5000, 0, true  },
    { {"136010.207", "136010-207"},  0x6000, 0, true  },
    { {"136010.208", "136010-208"},  0x7000, 0, true  },
    { {"136010.309", "136010-309"},  0x8000, 0, true  },
    { {"136010.301", "136010-301"},  0x9000, 0, true  },
};

static const RomSetDescriptor gravitar_v3_romset = {
    "Gravitar Rev 3", "Gravitar",
    gravitar_v3_entries, 6
};

// ── Space Duel ───────────────────────────────────────────────────────────
//
// Space Duel: program ROMs are 4 KB chips. Actual files match 136006.NNN format.
// TODO: Restructure with 4 KB sizes/stride once MAME mapping is verified.

static const RomEntryDescriptor spaceduel_entries[] = {
    // Vector ROMs — .108 optional (not always present in dumps)
    { {"136006.107", "136006-107"},  0x3000, 0, true  },
    { {"136006.108", "136006-108"},  0x3800, 0, false },
    // Program ROMs — MAME loads at $4000-$9FFF (6 × 4 KB, full 16-bit).
    // expected_size=0 for flexible matching.
    { {"136006.201", "136006-201"},  0x4000, 0, true  },
    { {"136006.102", "136006-102"},  0x5000, 0, true  },
    { {"136006.103", "136006-103"},  0x6000, 0, true  },
    { {"136006.104", "136006-104"},  0x7000, 0, true  },
    { {"136006.105", "136006-105"},  0x8000, 0, true  },
    { {"136006.106", "136006-106"},  0x9000, 0, true  },
};

static const RomSetDescriptor spaceduel_romset = {
    "Space Duel", "SpaceDuel",
    spaceduel_entries, 8
};

// ── Black Widow ──────────────────────────────────────────────────────────
//
// Black Widow: program ROMs are 4 KB chips. Actual files match 136017.NNN format.
// TODO: Restructure with 4 KB sizes/stride once MAME mapping is verified.

static const RomEntryDescriptor blackwidow_entries[] = {
    // Vector ROMs — expected_size=0 for flexible matching (files may be 2 KB or 4 KB)
    { {"136017.107", "136017-107"},  0x3000, 0, true  },
    { {"136017.108", "136017-108"},  0x3800, 0, false },
    // Program ROMs — MAME loads at $4000-$9FFF (bwidow board, full 16-bit).
    // expected_size=0 for flexible matching.
    { {"136017.101", "136017-101"},  0x4000, 0, true  },
    { {"136017.102", "136017-102"},  0x5000, 0, true  },
    { {"136017.103", "136017-103"},  0x6000, 0, true  },
    { {"136017.104", "136017-104"},  0x7000, 0, true  },
    { {"136017.105", "136017-105"},  0x8000, 0, true  },
    { {"136017.106", "136017-106"},  0x9000, 0, true  },
};

static const RomSetDescriptor blackwidow_romset = {
    "Black Widow", "BlackWidow",
    blackwidow_entries, 8
};

// ── Major Havoc Rev 3 ────────────────────────────────────────────────────
//
// Major Havoc: uses 16 KB ROM chips (actual files are 16384 bytes).
// The memory architecture is more complex than other AVG games, with bank
// switching for extra program ROM and a separate gamma CPU.
// TODO: Restructure with 16 KB sizes and proper bank-switched layout.

static const RomEntryDescriptor majorhavoc_v3_entries[] = {
    // Vector ROMs
    { {"136025.110", "136025-110"},  0x3000, 0, true  },
    { {"136025.111", "136025-111"},  0x3800, 0, true  },
    // Program ROMs ($4000-$7FFF) — expected_size=0 for flexible matching
    { {"136025.215", "136025-215"},  0x4000, 0, true  },
    { {"136025.216", "136025-216"},  0x4800, 0, true  },
    { {"136025.217", "136025-217"},  0x5000, 0, true  },
    { {"136025.218", "136025-218"},  0x5800, 0, true  },
};

static const RomSetDescriptor majorhavoc_v3_romset = {
    "Major Havoc Rev 3", "MajorHavoc",
    majorhavoc_v3_entries, 6
};

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<AtariVectorVariant V>
static HardwareTraits create_vector_hardware_traits() {

    HardwareTraits ht = {};

    // Display
    ht.display.native_width    = atv::DISPLAY_WIDTH;
    ht.display.native_height   = atv::DISPLAY_HEIGHT;
    ht.display.visible_width   = atv::DISPLAY_WIDTH;
    ht.display.visible_height  = atv::DISPLAY_HEIGHT;
    ht.display.format          = FramebufferFormat::RGBA8888;
    ht.display.palette_size    = 0;  // Vector display — no palette
    ht.display.pixel_aspect_ratio = 1.0f;
    ht.display.has_overscan    = false;

    // Audio
    ht.audio.format             = AudioFormat::CUSTOM;
    ht.audio.sample_rate_hz     = atv::DEFAULT_SAMPLE_RATE;
    ht.audio.channels           = 1;
    ht.audio.chip_name          = "Discrete";

    // Timing
    ht.timing.cpu_frequency_hz   = atv::CPU_FREQ_HZ;
    ht.timing.video_frequency_hz = atv::CPU_FREQ_HZ;
    ht.timing.audio_sample_rate_hz = atv::DEFAULT_SAMPLE_RATE;
    ht.timing.target_fps         = atv::TARGET_FPS;
    ht.timing.cycles_per_frame   = atv::CYCLES_PER_FRAME;
    ht.timing.standard           = VideoStandard::CUSTOM;

    return ht;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<AtariVectorVariant V>
AtariVectorSystem<V>::AtariVectorSystem()
    : System()
    , pins_(MOS6502::default_bus_state())
    , nmi_counter_(atv::NMI_PERIOD_CYCLES)
{
    hardware_traits_ = create_vector_hardware_traits<V>();
    current_palette_ = hardware_traits_.display.default_palette;
}

template<AtariVectorVariant V>
AtariVectorSystem<V>::~AtariVectorSystem() = default;

// ============================================================================
// SYSTEM DESCRIPTORS — compile-time traits → runtime descriptor
// ============================================================================

template<AtariVectorVariant V>
static SystemDescriptor create_system_descriptor() {
    using T = AtariVectorTraits<V>;
    return {
        T::NAME, T::SHORT_NAME, T::DESCRIPTION, T::DATA_FOLDER,
        std::vector<const char*>(std::begin(T::ALIASES), std::end(T::ALIASES)),
        nullptr,
        create_vector_hardware_traits<V>(),
        [](const format_descriptor_t*, const char* filepath,
           const uint8_t*, size_t size) -> SystemProbeResult {
            SystemProbeResult result = { 0.0f, {} };
            const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
            if (!ext) return result;
            if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
                for (auto expected : T::PROBE_ROM_SIZES) {
                    if (size == expected) {
                        result.confidence = 0.3f;
                        break;
                    }
                }
            }
            return result;
        }
    };
}

template<AtariVectorVariant V>
static SystemDescriptor& descriptor_instance() {
    static SystemDescriptor desc = create_system_descriptor<V>();
    return desc;
}

template<AtariVectorVariant V>
const SystemDescriptor& AtariVectorSystem<V>::get_descriptor() const {
    return descriptor_instance<V>();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::initialize() {
    using Traits = AtariVectorTraits<V>;
    printf("%s: Initializing system\n", Traits::NAME);

    register_board(&board_);

    // Bind value-typed CPU from ChipSet, then factory-create remaining chips
    board_.bind_chipset();
    board_.create_chips(&pins_);
    vec_ram_  = board_.template find<RAMChip>(1);   // 2nd RAMChip = vector RAM
    vec_rom_  = board_.template find<ROMChip>(0);   // 1st ROMChip = vector ROM
    prog_rom_ = board_.template find<ROMChip>(1);   // 2nd ROMChip = program ROM

    // 16-bit address games have a 3rd ROMChip for upper address space ($8000-$FFFF)
    if constexpr (!Traits::USES_15BIT_ADDR) {
        prog_rom_hi_ = board_.template find<ROMChip>(2);
    }

    // Wire MemoryBus page tables
    board_.apply(bus_);

    // Initialize CPU
    board_.cpu().init();
    board_.cpu().reset();

    // Initialize vector generator (DVG or AVG via ChipSet)
    vg().init();

    // Register all manifest-created chips for the Hardware menu
    register_bus_chips(board_);

    // Register vector generator for the Hardware menu
    register_chip(&vg(), Traits::VIDEO_CHIP_NAME, Traits::VIDEO_CHIP_NAME, "Video");

    // Initialize POKEY (for games that have it)
    if constexpr (Traits::HAS_POKEY) {
        pokey_.init();
        register_chip(&pokey_, "POKEY", "POKEY", "Sound");
    }

    // Video port — VectorVideoPort for signal-based rendering
    video_port_ = std::make_unique<VectorVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire vector generator to the video stream
    vg().set_stream(&video_port_->stream());

    // Wire vector generator memory — pointers are stable after board_.create_chips()
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atv::CPU_FREQ_HZ, atv::DEFAULT_SAMPLE_RATE);

    // Wire POKEY audio output
    if constexpr (Traits::HAS_POKEY) {
        pokey_.set_audio_port(audio_port_.get());
    }

    // Default DIP switches (MAME factory defaults)
    // Asteroids: English, 3 lives, 1 coin/1 credit = 0x84
    // Lunar Lander: 0 bonus fuel, free play off, English = game-specific
    dsw1_ = 0x84;
    dsw2_ = 0x00;

    printf("%s: System initialized\n", Traits::NAME);
    return true;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::shutdown() {
    printf("%s: Shutting down\n", AtariVectorTraits<V>::NAME);
    System::shutdown();
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::reset() {
    printf("%s: Reset\n", AtariVectorTraits<V>::NAME);

    board_.reset_chips();
    board_.cpu().reset();

    pins_ = MOS6502::default_bus_state();
    total_cycles_ = 0;

    vg().reset();
    nmi_counter_ = atv::NMI_PERIOD_CYCLES;

    if constexpr (Traits::HAS_POKEY) {
        pokey_.reset();
    }

    // Internal button state uses active-HIGH convention (1=pressed, 0=not pressed).
    in0_ = 0x00;
    in1_ = 0x00;
    thrust_ = 0x00;
    snd_latch_ = 0x00;
    nmi_enabled_ = false;  // NMI gated off until ROM enables it
}

// ============================================================================
// EXECUTION
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick() {
    // CPU tick
    tick_cpu();

    // Vector generator tick — runs at the same frequency as the CPU
    vg().tick();

    // POKEY tick (runs at CPU clock for games with POKEY)
    if constexpr (Traits::HAS_POKEY) {
        pokey_.tick(0);  // Arcade POKEY: no bus-driven memory access
    }

    // NMI timer — periodic pulse model (matches MAME set_periodic_int).
    // The NMI is edge-triggered on the 6502.  We assert NMI for one cycle
    // every NMI_PERIOD_CYCLES, then de-assert.  The 6502 detects the
    // falling edge and vectors to the NMI handler.
    //
    // Most games: NMI fires unconditionally every period.
    // Asteroids Deluxe: NMI is gated by the 74LS259 output latch Q4
    //   ($3C04, D0).  When NMI is disabled, IRQ is asserted instead
    //   (level-sensitive, held until NMI is re-enabled).
    if (nmi_counter_ > 0) {
        --nmi_counter_;
        BUS_SET_BIT(pins_, BUS_NMI_BIT);   // NMI inactive (high)
    } else {
        nmi_counter_ = atv::NMI_PERIOD_CYCLES;
        if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
            if (nmi_enabled_)
                BUS_CLR_BIT(pins_, BUS_NMI_BIT);   // NMI pulse (gated)
        } else {
            BUS_CLR_BIT(pins_, BUS_NMI_BIT);       // NMI pulse (unconditional)
        }
    }

    // AD: IRQ line mirrors "NMI disabled" state (level-sensitive).
    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        if (nmi_enabled_)
            BUS_SET_BIT(pins_, BUS_IRQ_BIT);   // IRQ inactive
        else
            BUS_CLR_BIT(pins_, BUS_IRQ_BIT);   // IRQ active (HOLD_LINE)
    }

    total_cycles_++;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::run_frame() {
    if (!video_port_) return;

    if (!system_ready_) {
        // No ROM loaded — nothing to draw
        video_port_->swap_frame();
        return;
    }

    // Run one frame's worth of CPU cycles
    for (uint32_t i = 0; i < atv::CYCLES_PER_FRAME; ++i) {
        tick();
    }

    // Swap frame — produces FrameData with VideoSignalType::Vector for the GPU
    video_port_->swap_frame();

    // Tick peripherals
    tick_peripherals();
}

// ============================================================================
// CPU TICK
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick_cpu() {
    auto& cpu = board_.cpu();

    pins_ = cpu.template tick<MOS6502::Phase::PHI2>(pins_);

    uint16_t raw_addr = BUS_GET_ADDR(pins_);
    uint16_t addr;
    if constexpr (Traits::USES_15BIT_ADDR) {
        // DVG games + BZ/RB: A15 not connected, $8000-$FFFF mirrors $0000-$7FFF.
        addr = raw_addr & 0x7FFF;
    } else {
        // Tempest, Gravitar, BW, SD: full 16-bit address space.
        addr = raw_addr;
    }
    bool is_write = !BUS_GET_BIT(pins_, BUS_RW_BIT);

    // I/O region varies by game family:
    //   Asteroids/LL/AD:  $2000-$3FFF
    //   Battlezone/RB:    $0800-$1FFF (MAME bzone.cpp)
    //   Gravitar/BW:      $0800-$1FFF (MAME bwidow.cpp — $8800 masked to $0800)
    //   Space Duel:       $0800-$1FFF (MAME spacduel_map)
    //   Major Havoc:      $0800-$1FFF (MAME mhavoc.cpp)
    //   Tempest:          $0800-$1FFF (inputs) + $6000-$60FF (POKEY, VGGO, etc.)
    bool is_io = false;

    if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                  V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                  V == AtariVectorVariant::LUNAR_LANDER) {
        // Original DVG games: I/O at $2000-$3FFF
        is_io = (addr >= 0x2000 && addr < 0x4000);
    } else if constexpr (V == AtariVectorVariant::TEMPEST) {
        // Tempest: I/O at $0800-$1FFF (input ports, color RAM)
        //          $4000-$5FFF (coin, VGGO, WD, VGRST)
        //          $6000-$60FF (EAROM, mathbox, POKEY, LED)
        is_io = (addr >= 0x0800 && addr < 0x2000) ||
                (addr >= 0x4000 && addr < 0x6100);
    } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                         V == AtariVectorVariant::BLACK_WIDOW) {
        // Gravitar/BW (bwidow board): full 16-bit addressing.
        //   $6000-$6FFF: POKEY1/2
        //   $7000-$7FFF: EAROM, IN0 ($7800)
        //   $8000-$8FFF: IN3, IN4, VGGO, VGRST, IRQ ack, EAROM ctrl, WD
        is_io = (addr >= 0x6000 && addr < 0x9000);
    } else {
        // BZ, RB, SD, MH: I/O at $0800-$1FFF
        is_io = (addr >= 0x0800 && addr < 0x2000);
    }

    if (is_io) {
        if (is_write) {
            pins_ = io_write(addr, BUS_GET_DATA(pins_), pins_);
        } else {
            pins_ = io_read(addr, pins_);
        }
    } else {
        // All other addresses: RAM, vector RAM/ROM, program ROM via MemoryBus.
        bus_state_t bus = pins_;
        BUS_SET_ADDR(bus, addr);
        pins_ = bus_.tick(bus);
    }

    pins_ = cpu.template tick<MOS6502::Phase::PHI1>(pins_);
    cpu.sample_nmi_pin(pins_);
}

// ============================================================================
// I/O READ
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_read(uint16_t addr, bus_state_t pins) {
    uint8_t data = 0x00;

    if constexpr (V == AtariVectorVariant::BATTLEZONE ||
                  V == AtariVectorVariant::RED_BARON) {
        // ── Battlezone / Red Baron I/O reads ────────────────────────────
        //
        // I/O at $0800-$1FFF (MAME bzone.cpp / redbaron_map):
        //   $0800         IN0  (direct, full byte — HALT, clock, coins, start)
        //   $0A00         DSW0 (DIP switches)
        //   $0C00         DSW1 (DIP switches)
        //   $1810-$181F   POKEY (Red Baron only)

        if constexpr (V == AtariVectorVariant::RED_BARON) {
            if (addr >= atv::RB_POKEY_BASE && addr < atv::RB_POKEY_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
        }

        if (addr >= atv::BZ_IN0_ADDR && addr < atv::BZ_IN0_ADDR + 0x0200) {
            // IN0 — full byte with live HW signals
            // XOR converts internal active-HIGH → hardware active-LOW for
            // coin, self-test, diagnostic step bits.
            data = in0_ ^ atv::BZ_IN0_ACTIVE_LOW_MASK;
            // bit 6: VG HALT
            if (vg().is_halted())
                data |= atv::BZ_IN0_HALT;
            else
                data &= ~atv::BZ_IN0_HALT;
            // bit 1: 3 KHz clock
            if (total_cycles_ & 0x100)
                data |= atv::BZ_IN0_CLOCK;
            else
                data &= ~atv::BZ_IN0_CLOCK;

        } else if (addr >= atv::BZ_DSW0_ADDR && addr < atv::BZ_DSW0_ADDR + 0x0200) {
            data = dsw1_;

        } else if (addr >= atv::BZ_DSW1_ADDR && addr < atv::BZ_DSW1_ADDR + 0x0200) {
            data = dsw2_;

        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                  V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        // ── Asteroids / Asteroids Deluxe I/O reads ──────────────────────
        //
        // Both use MULTIPLEXED input reads via 74LS244 buffers.
        // Reading address $200X returns bit X of the IN0 port placed at D7.
        //
        // Address decode:
        //   $2000-$2007   IN0   (multiplexed, 8 bits)
        //   $2400-$2407   IN1   (multiplexed, 8 bits)
        //   $2600-$260F   POKEY (Asteroids Deluxe only)
        //   $2800-$2803   DSW1  (multiplexed, 4 bits)
        //   $2C00-$2C3F   EAROM (Asteroids Deluxe only)

        // AD-specific peripherals: POKEY and EAROM
        if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
            // POKEY at $2600-$260F
            if (addr >= atv::AD_POKEY_BASE && addr < atv::AD_POKEY_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
            // EAROM at $2C00-$2C3F
            if (addr >= atv::AD_EAROM_BASE && addr < atv::AD_EAROM_BASE + atv::AD_EAROM_SIZE) {
                data = earom_[addr & 0x3F];
                BUS_SET_DATA(pins, data);
                return pins;
            }
        }

        // Multiplexed input port reads (shared Asteroids / AD)
        uint16_t port_base = addr & 0x2C00;  // A13, A11, A10 select port
        uint8_t offset = addr & 0x07;        // A0-A2 select which bit

        uint8_t port_val = 0x00;

        if (port_base == 0x2000) {
            // IN0: build live value from button state + hw signals
            port_val = in0_;

            // bit 1: 3 KHz clock — toggle every 256 CPU cycles (matches MAME clock_r)
            if (total_cycles_ & 0x100)
                port_val |= atv::AST_IN0_CLOCK;
            else
                port_val &= ~atv::AST_IN0_CLOCK;

            // bit 2: DVG done_r (IP_ACTIVE_LOW: halted=0, running=1)
            if (vg().is_halted())
                port_val &= ~atv::AST_IN0_HALT;
            else
                port_val |= atv::AST_IN0_HALT;

        } else if (port_base == 0x2400) {
            // IN1: player controls, coins, start
            port_val = in1_;

        } else if (port_base == 0x2800) {
            // DSW1: DIP switches
            port_val = dsw1_;
            offset &= 0x03;  // only 4 switches via this multiplexer

        } else {
            // Unmapped read
            data = 0xFF;
            BUS_SET_DATA(pins, data);
            return pins;
        }

        // Multiplexed read: extract bit[offset], return at D7
        data = (port_val & (1 << offset)) ? 0x80 : 0x7F;

    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        // ── Lunar Lander I/O reads ───────────────────────────────────────
        //
        // Lunar Lander has a different I/O layout (MAME llander_map):
        //   $2000        IN0   (direct full-byte read, NOT multiplexed)
        //   $2400-$2407  IN1   (multiplexed, 8 bits)
        //   $2800-$2803  DSW1  (multiplexed, 4 bits)
        //   $2C00        THRUST (direct ADC value)

        uint16_t port_base = addr & 0x2C00;

        if (port_base == 0x2000 && (addr & 0x03FF) == 0x0000) {
            // IN0: direct read (non-multiplexed), full byte.
            // XOR converts internal active-HIGH state → hardware active-LOW
            // for IP_ACTIVE_LOW bits (self-test, tilt, diag step, etc.).
            // HALT (bit 0) and CLOCK (bit 6) are active-HIGH → unaffected.
            data = in0_ ^ atv::LL_IN0_ACTIVE_LOW_MASK;

            // bit 0: DVG HALT (IP_ACTIVE_HIGH in LL: done_r → bit set when halted)
            if (vg().is_halted())
                data |= atv::LL_IN0_HALT;
            else
                data &= ~atv::LL_IN0_HALT;

            // bit 6: 3 KHz clock
            if (total_cycles_ & 0x100)
                data |= atv::LL_IN0_CLOCK;
            else
                data &= ~atv::LL_IN0_CLOCK;

        } else if (port_base == 0x2400) {
            // IN1: multiplexed. XOR for active-LOW polarity before bit extract.
            uint8_t offset = addr & 0x07;
            uint8_t port_val = in1_ ^ atv::LL_IN1_ACTIVE_LOW_MASK;
            data = (port_val & (1 << offset)) ? 0x80 : 0x7F;

        } else if (port_base == 0x2800) {
            // DSW1: multiplexed (4 bits)
            uint8_t offset = addr & 0x03;
            data = (dsw1_ & (1 << offset)) ? 0x80 : 0x7F;

        } else if (port_base == 0x2C00) {
            // Thrust lever ADC
            data = thrust_;

        } else {
            data = 0xFF;
        }

    } else {
        // ── AVG-based game I/O reads ─────────────────────────────────────
        //
        // Tempest/Gravitar/Space Duel/Black Widow/Major Havoc share the
        // Atari "AVG board" layout.  I/O addresses vary per game but the
        // general scheme is:
        //   IN0:    full-byte read (VG halt, 3KHz clock, coins)
        //   IN1:    player controls
        //   DSW1/2: DIP switch banks
        //   POKEY:  sound/input multiplexer
        //   EAROM:  high-score storage (some games)
        //
        // The addresses differ per game but the data format is similar.
        // We use Traits constants where available, with fallback to
        // Tempest-style layout for shared code.

        // POKEY reads (all AVG games have at least one POKEY)
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            if (addr >= atv::TEMP_POKEY1_BASE && addr < atv::TEMP_POKEY1_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
            if (addr == atv::TEMP_EAROM_READ_ADDR) {
                data = earom_[0];  // TODO: proper ER2055 address latch
                BUS_SET_DATA(pins, data);
                return pins;
            }
            if (addr == atv::TEMP_EAROM_CTRL_ADDR) {
                // Mathbox status register (read side of $6040)
                data = 0x00;  // TODO: mathbox status
                BUS_SET_DATA(pins, data);
                return pins;
            }
        } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                             V == AtariVectorVariant::BLACK_WIDOW) {
            if (addr >= atv::GRAV_POKEY1_BASE && addr < atv::GRAV_POKEY1_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
        } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            if (addr >= atv::SD_POKEY1_BASE && addr < atv::SD_POKEY1_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
        } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
            if (addr >= atv::MH_POKEY1_BASE && addr < atv::MH_POKEY1_BASE + 0x10) {
                data = pokey_.read(addr & 0x0F);
                BUS_SET_DATA(pins, data);
                return pins;
            }
        }

        // Input port reads — per-game addresses and VG halt bit positions.
        // All AVG games use active-LOW IN0 bits (0 = active, 1 = idle).
        // Default 0xFF = all idle/inactive.
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            // Tempest IN0 at $0C00: bit 6 = VG done_r (IP_ACTIVE_HIGH)
            if (addr >= 0x0C00 && addr < 0x0D00) {
                data = 0xFF;
                if (!vg().is_halted()) data &= ~0x40;
            } else if (addr >= 0x0D00 && addr < 0x0E00) {
                data = in1_;
            } else if (addr >= 0x0E00 && addr < 0x0F00) {
                data = dsw1_;
            } else {
                data = 0xFF;
            }
        } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                             V == AtariVectorVariant::BLACK_WIDOW) {
            // Gravitar/BW (bwidow board, MAME bwidow.cpp bwidow_map)
            //   $7800: IN0 — bit 5 = VG done_r (IP_ACTIVE_HIGH), coins, self-test
            //   $8000: IN3 — player controls (joystick, fire, shield, start)
            //   $8800: IN4 — DIP switches / P2 controls
            if (addr == atv::GRAV_IN0_ADDR) {
                data = 0xFF;
                if (!vg().is_halted()) data &= ~0x20;  // bit 5 = VG done_r
            } else if (addr == atv::GRAV_IN3_ADDR) {
                data = in1_;
            } else if (addr == atv::GRAV_IN4_ADDR) {
                data = dsw1_;
            } else {
                data = 0xFF;
            }
        } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            // Space Duel (MAME bwidow.cpp spacduel_map)
            //   $0800: IN0 — bit 5 = VG done_r (IP_ACTIVE_HIGH), coins
            //   $0900: IN3 — player controls
            if (addr == atv::SD_IN0_ADDR) {
                data = 0xFF;
                if (!vg().is_halted()) data &= ~0x20;  // bit 5 = VG done_r
            } else if (addr >= 0x0900 && addr < 0x0A00) {
                data = in1_;
            } else {
                data = 0xFF;
            }
        } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
            // MH alpha reads IN0 at $0800; bit 1 = VG done_r (IP_ACTIVE_HIGH)
            if (addr >= 0x0800 && addr < 0x0900) {
                data = 0xFF;
                if (!vg().is_halted()) data &= ~0x02;
            } else {
                data = 0xFF;
            }
        }
    }

    BUS_SET_DATA(pins, data);
    return pins;
}

// ============================================================================
// I/O WRITE
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_write(uint16_t addr, uint8_t data, bus_state_t pins) {
    // I/O writes are decode-by-address — the upper address bits select the register.
    // The data byte on the bus is sometimes ignored (trigger-only writes).

    if constexpr (V == AtariVectorVariant::BATTLEZONE ||
                  V == AtariVectorVariant::RED_BARON) {
        // ── BZ/RB I/O writes at $0800-$1FFF ────────────────────────────
        //
        // MAME bzone.cpp:
        //   $1000: coin counters   $1200: VGGO
        //   $1400: WD clear        $1600: VGRST
        //   $1840: sound latch
        if constexpr (V == AtariVectorVariant::RED_BARON) {
            if (addr >= atv::RB_POKEY_BASE && addr < atv::RB_POKEY_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
        }

        if (addr >= atv::BZ_COIN_CTR_ADDR && addr < atv::BZ_COIN_CTR_ADDR + 0x0200) {
            // Coin counters / output latch
        } else if (addr >= atv::BZ_SND_ADDR && addr < atv::BZ_SND_ADDR + 0x0200) {
            snd_latch_ = data;
        } else if (addr >= atv::BZ_VGGO_ADDR && addr < atv::BZ_VGGO_ADDR + 0x0200) {
            vg().trigger_go();
        } else if (addr >= atv::BZ_VGRST_ADDR && addr < atv::BZ_VGRST_ADDR + 0x0200) {
            vg().trigger_reset();
        } else if (addr >= atv::BZ_WDCLR_ADDR && addr < atv::BZ_WDCLR_ADDR + 0x0200) {
            // Watchdog clear — no-op
        }

    } else if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                         V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                         V == AtariVectorVariant::LUNAR_LANDER) {
        // ── Asteroids / AD / Lunar Lander I/O writes at $3000-$3FFF ────

        // AD-specific write-capable peripherals in the $2000-$2FFF range
        if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
            // POKEY write at $2600-$260F
            if (addr >= atv::AD_POKEY_BASE && addr < atv::AD_POKEY_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
            // EAROM write at $2C00-$2C3F
            if (addr >= atv::AD_EAROM_BASE && addr < atv::AD_EAROM_BASE + atv::AD_EAROM_SIZE) {
                earom_[addr & 0x3F] = data;
                return pins;
            }
        }

        uint16_t reg = addr & 0x3E00;

        switch (reg) {
            case atv::VGGO_ADDR:
                vg().trigger_go();
                break;

            case atv::VGRST_ADDR:
                vg().trigger_reset();
                break;

            case atv::WDCLR_ADDR:
                // Watchdog clear — no-op in emulation
                break;

            case atv::SND_BASE_ADDR:
                snd_latch_ = data;
                break;

            case 0x3800:
            case 0x3A00:
                if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
                    earom_ctrl_ = data;
                }
                break;

            case atv::COIN_CTR_ADDR: {
                uint8_t latch_bit = addr & 0x07;
                if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
                    if (latch_bit == 4)
                        nmi_enabled_ = (data & 1) != 0;
                }
                break;
            }

            case atv::NMI_ACK_ADDR:
                break;

            default:
                break;
        }

    } else {
        // ── AVG-based game I/O writes ────────────────────────────────────
        //
        // Each AVG game has its own I/O address decode.
        // VGGO/VGRST/POKEY/WD addresses vary per game.

        if constexpr (V == AtariVectorVariant::TEMPEST) {
            // Tempest (MAME tempest.cpp)
            // POKEY1 $60C0, POKEY2 $60D0, VGGO $4800, VGRST $5800, WD $5000
            if (addr >= atv::TEMP_POKEY1_BASE && addr < atv::TEMP_POKEY1_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
            if (addr >= atv::TEMP_EAROM_BASE && addr < atv::TEMP_EAROM_BASE + atv::TEMP_EAROM_SIZE) {
                earom_[addr & 0x3F] = data;
                return pins;
            }
            if (addr == atv::TEMP_VGGO_ADDR)  { vg().trigger_go(); return pins; }
            if (addr == atv::TEMP_VGRST_ADDR) { vg().trigger_reset(); return pins; }
            if (addr == atv::TEMP_WDCLR_ADDR) { return pins; }

        } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                             V == AtariVectorVariant::BLACK_WIDOW) {
            // Gravitar / Black Widow board (MAME bwidow.cpp bwidow_map)
            // POKEY1 $6000, POKEY2 $6800, VGGO $8840, VGRST $8880, WD $8980
            if (addr >= atv::GRAV_POKEY1_BASE && addr < atv::GRAV_POKEY1_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
            if (addr >= atv::GRAV_POKEY2_BASE && addr < atv::GRAV_POKEY2_BASE + 0x10) {
                // TODO: second POKEY
                return pins;
            }
            if (addr == atv::GRAV_VGGO_ADDR)  { vg().trigger_go(); return pins; }
            if (addr == atv::GRAV_VGRST_ADDR) { vg().trigger_reset(); return pins; }
            if (addr == atv::GRAV_WDCLR_ADDR) { return pins; }

        } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            // Space Duel (MAME bwidow.cpp spacduel_map)
            // POKEY1 $1000, POKEY2 $1400, VGGO $0C80, VGRST $0D80, WD $0D00
            if (addr >= atv::SD_POKEY1_BASE && addr < atv::SD_POKEY1_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
            if (addr >= atv::SD_POKEY2_BASE && addr < atv::SD_POKEY2_BASE + 0x10) {
                // TODO: second POKEY
                return pins;
            }
            if (addr == atv::SD_VGGO_ADDR)  { vg().trigger_go(); return pins; }
            if (addr == atv::SD_VGRST_ADDR) { vg().trigger_reset(); return pins; }
            if (addr == atv::SD_WDCLR_ADDR) { return pins; }

        } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
            // Major Havoc (MAME mhavoc.cpp)
            // POKEY1 $1200, VGGO $1400, VGRST $1600, WD $1800
            if (addr >= atv::MH_POKEY1_BASE && addr < atv::MH_POKEY1_BASE + 0x10) {
                pokey_.write(addr & 0x0F, data);
                return pins;
            }
            if (addr == atv::MH_VGGO_ADDR)  { vg().trigger_go(); return pins; }
            if (addr == atv::MH_VGRST_ADDR) { vg().trigger_reset(); return pins; }
            if (addr == atv::MH_WDCLR_ADDR) { return pins; }
        }
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_file(const char* filepath) {
    using Traits = AtariVectorTraits<V>;

    bool cold_boot = system_ready_;

    if (!system_ready_) {
        if (!initialize()) return false;
    }

    printf("%s: Loading file: %s\n", Traits::NAME, filepath);

    // Archive files (zip, 7z, …) may contain a multi-file ROM set.
    // Try ROM set matching before falling back to single-blob loading.
    std::string ext = vfs_extension(filepath);
    if (!ext.empty() && vfs_is_archive_extension(ext.c_str())) {
        auto descriptors = get_rom_set_descriptors();
        if (!descriptors.empty()) {
            auto match = rom_set_scan_and_match(
                filepath, descriptors.data(), static_cast<int>(descriptors.size()));
            if (match.matched) {
                printf("%s: Archive contains ROM set '%s'\n",
                       Traits::NAME, match.rom_set ? match.rom_set->name : "?");
                return load_rom_set(match);
            }
        }
        // Archive didn't match any ROM set — cannot load raw ZIP as ROM data
        printf("%s: Archive '%s' did not match any known ROM set\n",
               Traits::NAME, filepath);
        return false;
    }

    // Read the ROM file
    size_t file_size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &file_size);
    if (!file_data) {
        printf("%s: Failed to open file: %s\n", Traits::NAME, filepath);
        return false;
    }

    if (file_size == 0) {
        printf("%s: Empty file\n", Traits::NAME);
        free(file_data);
        return false;
    }

    printf("%s: ROM file is %zu bytes\n", Traits::NAME, file_size);

    // Determine ROM layout:
    // The file may contain:
    //   a) Just the program ROM
    //   b) Program ROM + vector ROM concatenated
    //   c) A combined ROM image with everything

    if (file_size >= Traits::PROGROM_ACTUAL + Traits::VECROM_SIZE) {
        // File contains both program ROM and vector ROM
        // Layout: program ROM first, then vector ROM at the end.
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
        if (vec_rom_) {
            std::memcpy(vec_rom_->data(), file_data + Traits::PROGROM_ACTUAL,
                        Traits::VECROM_SIZE);
        }
    } else if (file_size >= Traits::PROGROM_ACTUAL) {
        // Just the program ROM — vector ROM must be loaded separately
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
    } else {
        // Unknown size — load as much as fits into program ROM
        size_t to_copy = std::min(file_size, static_cast<size_t>(Traits::PROGROM_ACTUAL));
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, to_copy);
        }
    }

    free(file_data);

    // Set program title from filename
    const char* name = strrchr(filepath, '/');
    if (!name) name = strrchr(filepath, '\\');
    program_title_ = name ? (name + 1) : filepath;

    // Wire vector generator to vector memory
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

    // Show reset vector for diagnostic — confirms ROM data is present and mapped
    if (prog_rom_) {
        uint16_t rst_offset = Traits::PROGROM_SIZE - 4;  // $FFFC relative
        uint16_t rst_lo = prog_rom_->data()[rst_offset];
        uint16_t rst_hi = prog_rom_->data()[rst_offset + 1];
        printf("%s: Reset vector = $%04X (chip offset $%04X)\n",
               Traits::NAME, rst_lo | (rst_hi << 8), rst_offset);
    }

    if (cold_boot) {
        board_.cpu().set(REG_A, 0);
        board_.cpu().set(REG_X, 0);
        board_.cpu().set(REG_Y, 0);
    }

    printf("%s: ROM loaded, system ready\n", Traits::NAME);
    return true;
}

// ============================================================================
// ROM SET LOADING
// ============================================================================

template<AtariVectorVariant V>
std::vector<const RomSetDescriptor*> AtariVectorSystem<V>::get_rom_set_descriptors() const {
    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        return { &ast_v1_romset, &ast_v2_romset };
    } else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        return { &ad_v1_romset, &ad_v2_romset };
    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        return { &ll_v1_romset, &ll_v2_romset };
    } else if constexpr (V == AtariVectorVariant::BATTLEZONE) {
        return { &bz_v1_romset, &bz_v2_romset };
    } else if constexpr (V == AtariVectorVariant::RED_BARON) {
        return { &rb_romset };
    } else if constexpr (V == AtariVectorVariant::TEMPEST) {
        return { &tempest_v3_romset };
    } else if constexpr (V == AtariVectorVariant::GRAVITAR) {
        return { &gravitar_v2_romset, &gravitar_v3_romset };
    } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
        return { &spaceduel_romset };
    } else if constexpr (V == AtariVectorVariant::BLACK_WIDOW) {
        return { &blackwidow_romset };
    } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
        return { &majorhavoc_v3_romset };
    } else {
        return {};
    }
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_rom_set(const RomSetMatch& match) {
    using Traits = AtariVectorTraits<V>;

    if (!match.matched) return false;

    if (!system_ready_) {
        if (!initialize()) return false;
    }

    printf("%s: Loading ROM set '%s' (%zu entries)\n",
           Traits::NAME, match.rom_set ? match.rom_set->name : "?",
           match.entries.size());

    bool ok = rom_set_load_matched(match, [this](uint32_t load_address,
                                                  const uint8_t* data,
                                                  size_t size,
                                                  int /*entry_index*/) -> bool {
        // Route data to the correct chip based on load address
        if (load_address >= Traits::VECROM_BASE &&
            load_address < Traits::VECROM_BASE + Traits::VECROM_SIZE) {
            // Vector ROM
            if (!vec_rom_) return false;
            uint32_t offset = load_address - Traits::VECROM_BASE;
            size_t to_copy = std::min(size, static_cast<size_t>(Traits::VECROM_SIZE - offset));
            std::memcpy(vec_rom_->data() + offset, data, to_copy);
            printf("  Vector ROM: %zu bytes at $%04X\n", to_copy, load_address);
            return true;
        }

        // Program ROM — lower chip ($4000-$7FFF for most games)
        if (load_address >= Traits::PROGROM_BASE &&
            load_address < Traits::PROGROM_BASE + Traits::PROGROM_SIZE) {
            if (!prog_rom_) return false;
            uint32_t offset = load_address - Traits::PROGROM_BASE;
            size_t to_copy = std::min(size, static_cast<size_t>(Traits::PROGROM_SIZE - offset));
            std::memcpy(prog_rom_->data() + offset, data, to_copy);
            printf("  Program ROM: %zu bytes at $%04X (offset $%04X)\n",
                   to_copy, load_address, offset);
            return true;
        }

        // 16-bit games: upper ROM chip ($8000-$FFFF)
        if constexpr (!Traits::USES_15BIT_ADDR) {
            if (prog_rom_hi_ && load_address >= 0x8000) {
                uint32_t offset = load_address - 0x8000;
                size_t to_copy = std::min(size, static_cast<size_t>(0x8000u - offset));
                std::memcpy(prog_rom_hi_->data() + offset, data, to_copy);
                printf("  Program ROM (high): %zu bytes at $%04X (offset $%04X)\n",
                       to_copy, load_address, offset);
                return true;
            }
        }

        printf("  WARNING: Unhandled ROM address $%04X (%zu bytes) — skipped\n",
               load_address, size);
        return true;  // Not a fatal error
    });

    if (!ok) {
        printf("%s: Failed to load ROM set\n", Traits::NAME);
        return false;
    }

    // 16-bit games: create reset vector mirrors
    if constexpr (!Traits::USES_15BIT_ADDR) {
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            // Tempest: prog_rom_ IS the $8000-$FFFF chip (32KB).
            // Mirror upper ROM region for reset vector coverage.
            // V3 ROM goes to $CFFF, so mirror $C000-$CFFF repeating through $D000-$FFFF.
            // V1/V2 ROM goes to $DFFF, so mirror $C000-$DFFF at $E000-$FFFF.
            if (prog_rom_) {
                // Fill $D000-$DFFF with copy of $C000-$CFFF
                std::memcpy(prog_rom_->data() + 0x5000,
                            prog_rom_->data() + 0x4000, 0x1000);
                // Fill $E000-$FFFF with copy of $C000-$DFFF (now includes the mirror)
                std::memcpy(prog_rom_->data() + 0x6000,
                            prog_rom_->data() + 0x4000, 0x2000);
                printf("  Reset vector mirror: ROM mirrored to $E000-$FFFF\n");
            }
        } else if (prog_rom_hi_) {
            if constexpr (V == AtariVectorVariant::GRAVITAR ||
                          V == AtariVectorVariant::BLACK_WIDOW) {
                // Mirror last 4 KB ROM ($9000) at $F000 (chip offset $1000 → $7000)
                std::memcpy(prog_rom_hi_->data() + 0x7000,
                            prog_rom_hi_->data() + 0x1000, 0x1000);
                printf("  Reset vector mirror: $9000 → $F000\n");
            } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
                // Mirror $4000-$9FFF at $A000-$FFFF.
                // $A000 in high chip = offset $2000.
                if (prog_rom_) {
                    std::memcpy(prog_rom_hi_->data() + 0x2000,
                                prog_rom_->data(), 0x4000);   // $A000-$DFFF ← $4000-$7FFF
                }
                std::memcpy(prog_rom_hi_->data() + 0x6000,
                            prog_rom_hi_->data(), 0x2000);    // $E000-$FFFF ← $8000-$9FFF
                printf("  Reset vector mirror: $4000-$9FFF → $A000-$FFFF\n");
            }
        }
    }

    // Set program title from ROM set name
    if (match.rom_set && match.rom_set->name)
        program_title_ = match.rom_set->name;

    // Wire vector generator to vector memory
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

    // Show reset vector for diagnostic — confirms ROM data is present and mapped
    if (prog_rom_) {
        uint16_t rst_offset = Traits::PROGROM_SIZE - 4;  // $FFFC relative
        uint16_t rst_lo = prog_rom_->data()[rst_offset];
        uint16_t rst_hi = prog_rom_->data()[rst_offset + 1];
        printf("%s: Reset vector = $%04X (chip offset $%04X)\n",
               Traits::NAME, rst_lo | (rst_hi << 8), rst_offset);
    }

    printf("%s: ROM set loaded, system ready\n", Traits::NAME);
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::get_display_dimensions(int* width, int* height) const {
    if (width)  *width  = atv::DISPLAY_WIDTH;
    if (height) *height = atv::DISPLAY_HEIGHT;
}

// ============================================================================
// AUDIO
// ============================================================================

template<AtariVectorVariant V>
uint32_t AtariVectorSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    if constexpr (Traits::HAS_POKEY) {
        // Read from POKEY audio ring buffer
        if (audio_port_) {
            int got = audio_port_->ring_.pop(buffer, static_cast<int>(max_samples));
            // Pad remainder with silence if ring didn't have enough
            if (got < static_cast<int>(max_samples))
                std::memset(buffer + got, 0, (max_samples - got) * sizeof(float));
            return max_samples;
        }
    }

    // Discrete sound (Asteroids, Lunar Lander) — silence for now
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = sample_rate_hz;
}

// ============================================================================
// INPUT
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        // Asteroids controls (active-HIGH: pressed = set bit):
        //   Arrow keys = rotate left/right, thrust
        //   Space = fire
        //   H = hyperspace
        //   1/2 = 1P/2P start
        //   5 = coin
        //
        // In the MAME mapping, player controls are on IN1 ($2400-$2407),
        // fire/hyperspace are on IN0 ($2000-$2007).
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  atv::AST_IN1_ROT_LEFT;
                else         in1_ &= ~atv::AST_IN1_ROT_LEFT;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  atv::AST_IN1_ROT_RIGHT;
                else         in1_ &= ~atv::AST_IN1_ROT_RIGHT;
                break;
            case SDLK_UP:
                if (pressed) in1_ |=  atv::AST_IN1_THRUST;
                else         in1_ &= ~atv::AST_IN1_THRUST;
                break;
            case SDLK_SPACE:
                // Fire is on IN0 bit 4 in the MAME mapping
                if (pressed) in0_ |=  atv::AST_IN0_FIRE;
                else         in0_ &= ~atv::AST_IN0_FIRE;
                break;
            case SDLK_h:
                // Hyperspace is on IN0 bit 3 in the MAME mapping
                if (pressed) in0_ |=  atv::AST_IN0_HYPERSPACE;
                else         in0_ &= ~atv::AST_IN0_HYPERSPACE;
                break;
            case SDLK_1:
                if (pressed) in1_ |=  atv::AST_IN1_1P_START;
                else         in1_ &= ~atv::AST_IN1_1P_START;
                break;
            case SDLK_2:
                if (pressed) in1_ |=  atv::AST_IN1_2P_START;
                else         in1_ &= ~atv::AST_IN1_2P_START;
                break;
            case SDLK_5:
                // Coin insert (IN1 bit 0)
                if (pressed) in1_ |=  atv::AST_IN1_COIN1;
                else         in1_ &= ~atv::AST_IN1_COIN1;
                break;
            default:
                break;
        }
    } else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        // Asteroids Deluxe controls:
        //   Arrow left/right = rotate (IN1 bits 5/6)
        //   Up = thrust (IN0 bit 4)
        //   Space = fire (IN1 bit 7)
        //   H = shields (IN0 bit 3)
        //   1/2 = 1P/2P start
        //   5 = coin
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  atv::AD_IN1_ROT_LEFT;
                else         in1_ &= ~atv::AD_IN1_ROT_LEFT;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  atv::AD_IN1_ROT_RIGHT;
                else         in1_ &= ~atv::AD_IN1_ROT_RIGHT;
                break;
            case SDLK_UP:
                if (pressed) in0_ |=  atv::AD_IN0_THRUST;
                else         in0_ &= ~atv::AD_IN0_THRUST;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  atv::AD_IN1_FIRE;
                else         in1_ &= ~atv::AD_IN1_FIRE;
                break;
            case SDLK_h:
                if (pressed) in0_ |=  atv::AD_IN0_SHIELDS;
                else         in0_ &= ~atv::AD_IN0_SHIELDS;
                break;
            case SDLK_1:
                if (pressed) in1_ |=  atv::AST_IN1_1P_START;
                else         in1_ &= ~atv::AST_IN1_1P_START;
                break;
            case SDLK_2:
                if (pressed) in1_ |=  atv::AST_IN1_2P_START;
                else         in1_ &= ~atv::AST_IN1_2P_START;
                break;
            case SDLK_5:
                if (pressed) in1_ |=  atv::AST_IN1_COIN1;
                else         in1_ &= ~atv::AST_IN1_COIN1;
                break;
            default:
                break;
        }
    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        // Lunar Lander controls (active-HIGH: pressed = set bit):
        //   Up/Down = thrust (adjusts ADC value)
        //   Left/Right = rotate
        //   Space = abort (IN1 bit 5)
        //   1 = start (IN1 bit 0)
        //   5 = coin (IN1 bit 1)
        switch (key) {
            case SDLK_UP:
                if (pressed) {
                    thrust_ = std::min(255, thrust_ + 32);
                }
                break;
            case SDLK_DOWN:
                if (pressed) {
                    thrust_ = std::max(0, thrust_ - 32);
                }
                break;
            case SDLK_SPACE:
                // Abort button (IN1 bit 5)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_1:
                // Start (IN1 bit 0)
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_5:
                // Coin (IN1 bit 1, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_LEFT:
                // Rotate left (IN1 bit 7, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x80;
                else         in1_ &= ~0x80;
                break;
            case SDLK_RIGHT:
                // Rotate right (IN1 bit 6, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x40;
                else         in1_ &= ~0x40;
                break;
            default:
                break;
        }

    } else if constexpr (V == AtariVectorVariant::BATTLEZONE) {
        // Battlezone — twin-stick tank controls
        //   W/S = left stick forward/reverse
        //   I/K = right stick forward/reverse
        //   Space = fire
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_w:
                if (pressed) in1_ |=  0x01;  // left forward
                else         in1_ &= ~0x01;
                break;
            case SDLK_s:
                if (pressed) in1_ |=  0x02;  // left reverse
                else         in1_ &= ~0x02;
                break;
            case SDLK_i:
                if (pressed) in1_ |=  0x04;  // right forward
                else         in1_ &= ~0x04;
                break;
            case SDLK_k:
                if (pressed) in1_ |=  0x08;  // right reverse
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;  // fire
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                // Start is IN3 bit 5 in MAME (joystick register, not IN0)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  atv::BZ_IN0_COIN1;
                else         in0_ &= ~atv::BZ_IN0_COIN1;
                break;
            default:
                break;
        }

    } else if constexpr (V == AtariVectorVariant::RED_BARON) {
        // Red Baron — yoke controls
        //   Arrow keys = up/down/left/right
        //   Space = fire
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_UP:
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_DOWN:
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_LEFT:
                if (pressed) in1_ |=  0x04;
                else         in1_ &= ~0x04;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  0x08;
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;  // fire
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                // Start is IN3 bit 5 in MAME (joystick register, not IN0)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  atv::BZ_IN0_COIN1;
                else         in0_ &= ~atv::BZ_IN0_COIN1;
                break;
            default:
                break;
        }

    } else {
        // ── AVG-based games — generic controls ──────────────────────────
        //   Arrow keys = directional
        //   Space = fire / primary action
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_UP:
                if (pressed) in1_ |=  0x04;
                else         in1_ &= ~0x04;
                break;
            case SDLK_DOWN:
                if (pressed) in1_ |=  0x08;
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                if (pressed) in0_ |=  0x80;  // start
                else         in0_ &= ~0x80;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  0x10;  // coin
                else         in0_ &= ~0x10;
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// GUI
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    // Future: DIP switch editor, display options
#endif
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    // Future: DIP switch configuration, phosphor color selection
#endif
}
// ============================================================================
// SPEED CONTROL
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATION
// ============================================================================

// DVG-based
template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS_DELUXE>;
template class AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;
template class AtariVectorSystem<AtariVectorVariant::BATTLEZONE>;
template class AtariVectorSystem<AtariVectorVariant::RED_BARON>;

// AVG-based
template class AtariVectorSystem<AtariVectorVariant::TEMPEST>;
template class AtariVectorSystem<AtariVectorVariant::GRAVITAR>;
template class AtariVectorSystem<AtariVectorVariant::SPACE_DUEL>;
template class AtariVectorSystem<AtariVectorVariant::BLACK_WIDOW>;
template class AtariVectorSystem<AtariVectorVariant::MAJOR_HAVOC>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

// DVG-based
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::ASTEROIDS>(), [] { return std::make_unique<AsteroidsSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::ASTEROIDS_DELUXE>(), [] { return std::make_unique<AsteroidsDeluxeSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::LUNAR_LANDER>(), [] { return std::make_unique<LunarLanderSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::BATTLEZONE>(), [] { return std::make_unique<BattlezoneSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::RED_BARON>(), [] { return std::make_unique<RedBaronSystem>(); });

// AVG-based
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::TEMPEST>(), [] { return std::make_unique<TempestSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::GRAVITAR>(), [] { return std::make_unique<GravitarSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::SPACE_DUEL>(), [] { return std::make_unique<SpaceDuelSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::BLACK_WIDOW>(), [] { return std::make_unique<BlackWidowSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::MAJOR_HAVOC>(), [] { return std::make_unique<MajorHavocSystem>(); });
