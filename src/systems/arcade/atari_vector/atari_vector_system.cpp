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
    { {"035127.01"},                0x5000, 2048, true  },
    // Program ROMs — three 2 KB chips covering $6800-$7FFF
    { {"035145.01"},                0x6800, 2048, true  },   // socket ef2
    { {"035144.01"},                0x7000, 2048, true  },   // socket h2
    { {"035143.01"},                0x7800, 2048, true  },   // socket j2 (reset vector)
};

static const RomSetDescriptor ast_v1_romset = {
    "Asteroids Rev 1", "Asteroids",
    ast_v1_entries, 4
};

// ── Asteroids Rev 2 ──────────────────────────────────────────────────────────

static const RomEntryDescriptor ast_v2_entries[] = {
    { {"035127.02"},                0x5000, 2048, true  },
    { {"035145.02"},                0x6800, 2048, true  },
    { {"035144.02"},                0x7000, 2048, true  },
    { {"035143.02"},                0x7800, 2048, true  },
};

static const RomSetDescriptor ast_v2_romset = {
    "Asteroids Rev 2", "Asteroids",
    ast_v2_entries, 4
};

// ── Lunar Lander Rev 1 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v1_entries[] = {
    // Vector ROM 0 at $5000
    { {"034598.01", "LLVROM0"},     0x5000, 2048, true  },
    // Program ROMs — four 2 KB chips covering $6000-$7FFF
    { {"034572.01"},                0x6000, 2048, true  },   // socket c1
    { {"034571.01", "LLPROM2"},     0x6800, 2048, true  },   // socket de1
    { {"034570.01", "LLPROM1"},     0x7000, 2048, true  },   // socket f1
    { {"034569.01", "LLPROM0"},     0x7800, 2048, true  },   // socket j1 (reset vector)
    // Vector ROM 1 at $5800 (optional — system chip manifest may not map this yet)
    { {"034599.01", "LLVROM1"},     0x5800, 2048, false },
    // Language PROM (optional)
    { {"034597.01", "034597-01"},   0x0000, 2048, false },
};

static const RomSetDescriptor ll_v1_romset = {
    "Lunar Lander Rev 1", "LunarLander",
    ll_v1_entries, 7
};

// ── Lunar Lander Rev 2 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v2_entries[] = {
    // Vector ROMs unchanged from v1
    { {"034598.01", "LLVROM0"},     0x5000, 2048, true  },
    // Program ROMs — rev 2 chips
    { {"034572.02"},                0x6000, 2048, true  },
    { {"034571.02"},                0x6800, 2048, true  },
    { {"034570.02"},                0x7000, 2048, true  },
    { {"034569.02"},                0x7800, 2048, true  },
    { {"034599.01", "LLVROM1"},     0x5800, 2048, false },
    { {"034597.01", "034597-01"},   0x0000, 2048, false },
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
    { {"036800.01"},                0x4800, 2048, true  },   // DVG offset $0800
    { {"036799.01"},                0x5000, 2048, true  },   // DVG offset $1000
    // Program ROMs
    { {"036430.01"},                0x6000, 2048, true  },   // socket c1
    { {"036431.01"},                0x6800, 2048, true  },   // socket de1
    { {"036432.01"},                0x7000, 2048, true  },   // socket f1
    { {"036433.02"},                0x7800, 2048, true  },   // socket j1 (reset vector)
};

static const RomSetDescriptor ad_v1_romset = {
    "Asteroids Deluxe Rev 1", "AsteroidsDeluxe",
    ad_v1_entries, 6
};

// ── Asteroids Deluxe Rev 2 ───────────────────────────────────────────────

static const RomEntryDescriptor ad_v2_entries[] = {
    // Vector ROMs (036800 updated, 036799 same as v1)
    { {"036800.02"},                0x4800, 2048, true  },
    { {"036799.01"},                0x5000, 2048, true  },
    // Program ROMs
    { {"036430.02"},                0x6000, 2048, true  },
    { {"036431.02"},                0x6800, 2048, true  },
    { {"036432.02"},                0x7000, 2048, true  },
    { {"036433.03"},                0x7800, 2048, true  },
};

static const RomSetDescriptor ad_v2_romset = {
    "Asteroids Deluxe Rev 2", "AsteroidsDeluxe",
    ad_v2_entries, 6
};

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<AtariVectorVariant V>
static HardwareTraits create_vector_hardware_traits() {
    using Traits = AtariVectorTraits<V>;

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
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor asteroids_descriptor = {
    "Asteroids",
    "Asteroids",
    "Atari Asteroids (1979) — 6502 CPU, DVG vector display",
    "asteroids",
    {"Asteroids", "ASTEROIDS"},
    nullptr,
    create_vector_hardware_traits<AtariVectorVariant::ASTEROIDS>(),
    [](const format_descriptor_t*, const char* filepath,
       const uint8_t*, size_t size) -> SystemProbeResult {
        SystemProbeResult result = { 0.0f, {} };
        const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
        if (!ext) return result;
        // Match common Asteroids ROM extensions
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            // Asteroids ROMs are typically 6 KB (program) + 2 KB (vector) = 8 KB
            // or raw 6 KB program ROM
            if (size == 6144 || size == 8192)
                result.confidence = 0.3f;
        }
        return result;
    }
};

static SystemDescriptor lunar_lander_descriptor = {
    "Lunar Lander",
    "LunarLander",
    "Atari Lunar Lander (1979) — 6502 CPU, DVG vector display",
    "lunar_lander",
    {"LunarLander", "Lunar Lander", "LUNARLANDER"},
    nullptr,
    create_vector_hardware_traits<AtariVectorVariant::LUNAR_LANDER>(),
    [](const format_descriptor_t*, const char* filepath,
       const uint8_t*, size_t size) -> SystemProbeResult {
        SystemProbeResult result = { 0.0f, {} };
        const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
        if (!ext) return result;
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            // Lunar Lander: 8 KB program + 2 KB vector = 10 KB, or 8 KB program alone
            if (size == 8192 || size == 10240)
                result.confidence = 0.3f;
        }
        return result;
    }
};

static SystemDescriptor asteroids_deluxe_descriptor = {
    "Asteroids Deluxe",
    "AsteroidsDeluxe",
    "Atari Asteroids Deluxe (1980) — 6502 CPU, DVG, POKEY sound",
    "asteroids_deluxe",
    {"AsteroidsDeluxe", "Asteroids Deluxe", "ASTEROIDSDELUXE"},
    nullptr,
    create_vector_hardware_traits<AtariVectorVariant::ASTEROIDS_DELUXE>(),
    [](const format_descriptor_t*, const char* filepath,
       const uint8_t*, size_t size) -> SystemProbeResult {
        SystemProbeResult result = { 0.0f, {} };
        const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
        if (!ext) return result;
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            // AD: 8 KB program + 4 KB vector = 12 KB
            if (size == 12288)
                result.confidence = 0.3f;
        }
        return result;
    }
};
template<>
const SystemDescriptor& AsteroidsSystem::get_descriptor() const {
    return asteroids_descriptor;
}

template<>
const SystemDescriptor& AsteroidsDeluxeSystem::get_descriptor() const {
    return asteroids_deluxe_descriptor;
}

template<>
const SystemDescriptor& LunarLanderSystem::get_descriptor() const {
    return lunar_lander_descriptor;
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

    // Factory-create all chips from the manifest
    board_.create_chips(&pins_);
    cpu_      = board_.template cpu<MOS6502>();
    vec_ram_  = board_.template find<RAMChip>(1);   // 2nd RAMChip = vector RAM
    vec_rom_  = board_.template find<ROMChip>(0);   // 1st ROMChip = vector ROM
    prog_rom_ = board_.template find<ROMChip>(1);   // 2nd ROMChip = program ROM

    // Wire MemoryBus page tables
    board_.apply(bus_);

    // Initialize CPU
    board_.cpu_chip()->init();
    board_.cpu_chip()->reset();

    // Initialize DVG
    dvg_.init();

    // Register all manifest-created chips for the Hardware menu
    register_bus_chips(board_);

    // Register DVG as a non-bus chip for the Hardware menu
    register_chip(&dvg_, "DVG", "DVG", "Video");

    // Initialize POKEY (Asteroids Deluxe)
    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        pokey_.init();
        register_chip(&pokey_, "POKEY", "POKEY", "Sound");
    }

    // Video port — VectorVideoPort for signal-based rendering
    video_port_ = std::make_unique<VectorVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire DVG to the video stream
    dvg_.set_stream(&video_port_->stream());

    // Wire DVG vector memory — pointers are stable after board_.create_chips()
    if (vec_ram_ && vec_rom_) {
        dvg_.set_vector_memory(vec_ram_->data(), atv::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atv::CPU_FREQ_HZ, atv::DEFAULT_SAMPLE_RATE);

    // Wire POKEY audio output
    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        pokey_.set_audio_port(audio_port_.get());
    }

    // Default DIP switches (factory defaults)
    dsw1_ = 0x00;
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

    if (board_.cpu_chip()) {
        board_.cpu_chip()->reset();
    }

    pins_ = MOS6502::default_bus_state();
    total_cycles_ = 0;

    dvg_.reset();
    nmi_counter_ = atv::NMI_PERIOD_CYCLES;

    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        pokey_.reset();
    }

    in0_ = 0x00;
    in1_ = 0x00;
    thrust_ = 0x00;
    snd_latch_ = 0x00;
}

// ============================================================================
// EXECUTION
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick() {
    // CPU tick
    tick_cpu();

    // DVG tick — runs at the same frequency as the CPU
    dvg_.tick();

    // POKEY tick (Asteroids Deluxe — runs at CPU clock)
    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        pokey_.tick(0);  // Arcade POKEY: no bus-driven memory access
    }

    // NMI timer — periodic pulse model (matches MAME set_periodic_int).
    // The NMI is edge-triggered on the 6502.  We assert NMI for one cycle
    // every NMI_PERIOD_CYCLES, then de-assert.  The 6502 detects the
    // falling edge and vectors to the NMI handler.
    if (nmi_counter_ > 0) {
        --nmi_counter_;
        BUS_SET_BIT(pins_, BUS_NMI_BIT);   // NMI inactive (high)
    } else {
        nmi_counter_ = atv::NMI_PERIOD_CYCLES;
        BUS_CLR_BIT(pins_, BUS_NMI_BIT);   // NMI active (low) — 1-cycle pulse
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
    if (!cpu_) return;

    pins_ = cpu_->tick<MOS6502::Phase::PHI2>(pins_);

    // Asteroids/Lunar Lander only decode 15 address lines (A0-A14).
    // A15 is not connected to the address decoder, so $8000-$FFFF
    // mirrors $0000-$7FFF.  The CPU still drives A15 on the bus (e.g.
    // $FFFC for reset vector), but the board ignores it — mask locally
    // for dispatch without modifying the bus state the CPU sees.
    uint16_t addr = BUS_GET_ADDR(pins_) & 0x7FFF;
    bool is_write = !BUS_GET_BIT(pins_, BUS_RW_BIT);

    // I/O region: $2000-$3FFF — manual dispatch (not on MemoryBus)
    if (addr >= 0x2000 && addr < 0x4000) {
        if (is_write) {
            pins_ = io_write(addr, BUS_GET_DATA(pins_), pins_);
        } else {
            pins_ = io_read(addr, pins_);
        }
    } else {
        // All other addresses: RAM, vector RAM/ROM, program ROM via MemoryBus.
        // Present the masked address to the bus for page-table lookup.
        bus_state_t bus = pins_;
        BUS_SET_ADDR(bus, addr);
        pins_ = bus_.tick(bus);
    }

    pins_ = cpu_->tick<MOS6502::Phase::PHI1>(pins_);
    cpu_->sample_nmi_pin(pins_);
}

// ============================================================================
// I/O READ
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_read(uint16_t addr, bus_state_t pins) {
    uint8_t data = 0x00;

    if constexpr (V == AtariVectorVariant::ASTEROIDS ||
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

            // bit 2: DVG HALT (IP_ACTIVE_LOW: halted=0, running=1)
            if (!dvg_.is_halted())
                port_val |= atv::AST_IN0_HALT;
            else
                port_val &= ~atv::AST_IN0_HALT;

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

    } else {
        // ── Lunar Lander I/O reads ───────────────────────────────────────
        //
        // Lunar Lander has a different I/O layout (MAME llander_map):
        //   $2000        IN0   (direct full-byte read, NOT multiplexed)
        //   $2400-$2407  IN1   (multiplexed, 8 bits)
        //   $2800-$2803  DSW1  (multiplexed, 4 bits)
        //   $2C00        THRUST (direct ADC value)

        uint16_t port_base = addr & 0x2C00;

        if (port_base == 0x2000 && (addr & 0x03FF) == 0x0000) {
            // IN0: direct read (non-multiplexed), full byte
            data = in0_;

            // bit 0: DVG HALT (IP_ACTIVE_HIGH in LL: done_r → bit set when halted)
            if (dvg_.is_halted())
                data |= atv::LL_IN0_HALT;
            else
                data &= ~atv::LL_IN0_HALT;

            // bit 6: 3 KHz clock
            if (total_cycles_ & 0x100)
                data |= atv::LL_IN0_CLOCK;
            else
                data &= ~atv::LL_IN0_CLOCK;

        } else if (port_base == 0x2400) {
            // IN1: multiplexed
            uint8_t offset = addr & 0x07;
            data = (in1_ & (1 << offset)) ? 0x80 : 0x7F;

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
        case atv::VGGO_ADDR: {
            // $3000 — VGGO: Start DVG vector state machine
            dvg_.trigger_go();
            break;
        }

        case atv::VGRST_ADDR:
            // $3200 — VGRST: Reset DVG
            dvg_.trigger_reset();
            break;

        case atv::WDCLR_ADDR:
            // $3400 — WD CLR: Watchdog clear (no-op in emulation)
            break;

        case atv::SND_BASE_ADDR:
            // $3600 — Sound triggers (game-specific discrete circuits)
            snd_latch_ = data;
            break;

        case 0x3800:
        case 0x3A00:
            // $3800 — EAROM control (AD) / sound latches (other games)
            if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
                earom_ctrl_ = data;
            }
            break;

        case atv::COIN_CTR_ADDR:
            // $3C00 — Coin counter (no-op in emulation)
            break;

        case atv::NMI_ACK_ADDR:
            // $3E00 — Noise reset (asteroid_noise_reset_w in MAME).
            // Resets the LFSR noise generator.  No-op for now.
            break;

        default:
            break;
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

    if (!cpu_) {
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

    // Wire DVG to vector memory
    if (vec_ram_ && vec_rom_) {
        dvg_.set_vector_memory(vec_ram_->data(), atv::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

    if (cold_boot && cpu_) {
        cpu_->set(REG_A, 0);
        cpu_->set(REG_X, 0);
        cpu_->set(REG_Y, 0);
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
    } else {
        return { &ll_v1_romset, &ll_v2_romset };
    }
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_rom_set(const RomSetMatch& match) {
    using Traits = AtariVectorTraits<V>;

    if (!match.matched) return false;

    if (!cpu_) {
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

        if (load_address >= AtariVectorTraits<V>::PROGROM_BASE &&
            load_address < AtariVectorTraits<V>::PROGROM_BASE + AtariVectorTraits<V>::PROGROM_SIZE) {
            // Program ROM ($6000/$6800-$7FFF)
            if (!prog_rom_) return false;
            uint32_t offset = load_address - AtariVectorTraits<V>::PROGROM_BASE;
            size_t to_copy = std::min(size, static_cast<size_t>(AtariVectorTraits<V>::PROGROM_SIZE - offset));
            std::memcpy(prog_rom_->data() + offset, data, to_copy);
            printf("  Program ROM: %zu bytes at $%04X (offset $%04X)\n",
                   to_copy, load_address, offset);
            return true;
        }

        printf("  WARNING: Unhandled ROM address $%04X (%zu bytes) — skipped\n",
               load_address, size);
        return true;  // Not a fatal error
    });

    if (!ok) {
        printf("%s: Failed to load ROM set\n", Traits::NAME);
        return false;
    }

    // Set program title from ROM set name
    if (match.rom_set && match.rom_set->name)
        program_title_ = match.rom_set->name;

    // Wire DVG to vector memory
    if (vec_ram_ && vec_rom_) {
        dvg_.set_vector_memory(vec_ram_->data(), atv::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

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

    if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
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
    } else {
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

template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS_DELUXE>;
template class AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(asteroids_descriptor, [] { return std::make_unique<AsteroidsSystem>(); });
REGISTER_SYSTEM(asteroids_deluxe_descriptor, [] { return std::make_unique<AsteroidsDeluxeSystem>(); });
REGISTER_SYSTEM(lunar_lander_descriptor, [] { return std::make_unique<LunarLanderSystem>(); });
