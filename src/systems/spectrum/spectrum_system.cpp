/*
 * spectrum_system.cpp — ZX Spectrum 48K / 128K system implementation
 *
 * Tick loop:
 *   Each call to tick() advances the Z80 by one T-state.
 *   The ULA generates video at the same clock rate (3.5 MHz).
 *   The AY-3-8912 (128K only) is clocked at CPU_FREQ / 2 = 1.75 MHz.
 *
 * Memory map (48K):
 *   $0000-$3FFF: 16KB ROM
 *   $4000-$FFFF: 48KB RAM (screen at $4000-$5AFF)
 *
 * Memory map (128K):
 *   $0000-$3FFF: ROM bank (ROM 0 or ROM 1, selected by port $7FFD bit 4)
 *   $4000-$7FFF: RAM bank 5 (always mapped)
 *   $8000-$BFFF: RAM bank 2 (always mapped)
 *   $C000-$FFFF: Switchable RAM bank (0-7, selected by port $7FFD bits 0-2)
 *
 * I/O decoding:
 *   A0=0:      ULA port $FE (keyboard/border/tape)
 *   $7FFD:     128K banking (A1=0, A15=0 — active when bits match)
 *   $FFFD:     AY register select
 *   $BFFD:     AY data write
 */

#include "systems/spectrum/spectrum_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_registry.hpp"
#include "core/formats/sna_format.hpp"
#include "core/formats/z80_snapshot_format.hpp"
#include "core/formats/spectrum_tap_format.hpp"
#include "core/formats/scl_format.hpp"
#include "core/formats/trd_format.hpp"
#include <cstring>
#include <cstdio>

using namespace z80::reg;  // PC, AF, BC, etc.

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/palette_selector.hpp"
#endif

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<SpectrumVariant V>
static HardwareTraits create_spectrum_hardware_traits() {
    using Traits = SpectrumVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = spectrum_constants::TOTAL_WIDTH;
    traits.display.native_height   = spectrum_constants::TOTAL_HEIGHT;
    traits.display.visible_width   = spectrum_constants::TOTAL_WIDTH;
    traits.display.visible_height  = spectrum_constants::TOTAL_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = spectrum_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = Traits::has_ay_sound ? "AY-3-8912 + Beeper" : "Beeper";

    traits.timing.cpu_frequency_hz   = spectrum_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = spectrum_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = spectrum_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = spectrum_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

// ── Supported format descriptors ─────────────────────────────────────────────

static const format_descriptor_t* const spectrum_formats[] = {
    &SNA_FORMAT_DESCRIPTOR,
    &Z80_SNAPSHOT_FORMAT_DESCRIPTOR,
    &SPECTRUM_TAP_FORMAT_DESCRIPTOR,
    &SCL_FORMAT_DESCRIPTOR,
    &TRD_FORMAT_DESCRIPTOR,
    nullptr
};

// ── Probe callback — inspects matched format content for system confidence ───

static SystemProbeResult spectrum_probe_file(
    const format_descriptor_t* matched_format,
    const char* /*filepath*/,
    const uint8_t* data, size_t size) {

    SystemProbeResult result;
    result.confidence = 0.0f;

    if (!matched_format) return result;

    // SNA and Z80 snapshots are always Spectrum files
    if (matched_format == &SNA_FORMAT_DESCRIPTOR ||
        matched_format == &Z80_SNAPSHOT_FORMAT_DESCRIPTOR) {
        result.confidence = 1.0f;

        // Detect 128K from format metadata
        if (matched_format == &SNA_FORMAT_DESCRIPTOR && size > 49179) {
            result.configuration.custom_settings["variant"] = "128K";
        } else if (matched_format == &Z80_SNAPSHOT_FORMAT_DESCRIPTOR && data && size >= 30) {
            z80_snapshot_header_t hdr;
            if (z80_snapshot_parse_header(data, size, &hdr) && hdr.is_128k)
                result.configuration.custom_settings["variant"] = "128K";
        }
        return result;
    }

    // Spectrum TAP — high confidence (already identified as Spectrum TAP, not Commodore)
    if (matched_format == &SPECTRUM_TAP_FORMAT_DESCRIPTOR) {
        result.confidence = 0.95f;
        return result;
    }

    return result;
}

// ── System descriptors ──────────────────────────────────────────────────────

static SystemDescriptor spectrum48k_descriptor = {
    "ZX Spectrum 48K",
    "Spectrum48K",
    "Sinclair ZX Spectrum 48K — Z80A, ULA, 48KB RAM (1982)",
    "spectrum",
    {"Spectrum", "Spectrum48K", "ZXSpectrum", "ZX48K", "Speccy"},
    spectrum_formats,
    create_spectrum_hardware_traits<SpectrumVariant::ZX48K>(),
    spectrum_probe_file,
    "Sinclair", 1982, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor spectrum128k_descriptor = {
    "ZX Spectrum 128K",
    "Spectrum128K",
    "Sinclair ZX Spectrum 128K — Z80A, ULA, AY sound, 128KB RAM (1985)",
    "spectrum",
    {"Spectrum128K", "ZX128K", "Spectrum128"},
    spectrum_formats,
    create_spectrum_hardware_traits<SpectrumVariant::ZX128K>(),
    spectrum_probe_file,
    "Sinclair", 1985, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<SpectrumVariant V>
SpectrumSystem<V>::SpectrumSystem()
    : System()
    , pins_(SPECTRUM_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_spectrum_hardware_traits<V>();
}

template<SpectrumVariant V>
SpectrumSystem<V>::~SpectrumSystem() {
}

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<SpectrumVariant V>
const SystemDescriptor& SpectrumSystem<V>::get_descriptor() const {
    if constexpr (V == SpectrumVariant::ZX48K) return spectrum48k_descriptor;
    else return spectrum128k_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<SpectrumVariant V>
bool SpectrumSystem<V>::apply_configuration() {
    // Apply display palette selection
    apply_display_palette_();
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Pre-bind stack-member chips, then factory-create all chips ─────
    { size_t slot_idx_ = 0;
      SPECTRUM48K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Configure page tables for this variant ──────────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.ula.init();
    if constexpr (Traits::has_ay_sound) {
        board_.psg.init();
    }

    // Audio setup
    audio_sample_period_ = spectrum_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Load ROMs
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────

    register_bus_chips(board_);

    // Video output — composite video from Ferranti ULA
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.ula.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio port — system mixes beeper + AY, uses drive_sample()
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(spectrum_constants::DEFAULT_SAMPLE_RATE,
                           spectrum_constants::DEFAULT_SAMPLE_RATE);

    printf("%s: System initialized (%dKB RAM)\n", Traits::name, Traits::ram_size_kb);
    system_ready_ = true;
    return true;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::shutdown() {
    system_ready_ = false;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    bank_select_ = 0;
    bank_locked_ = false;
    frame_tstate_counter_ = 0;
    int_counter_ = 0;
    std::memset(keyboard_rows_, 0xFF, sizeof(keyboard_rows_));
}

// ============================================================================
// EXECUTION
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::tick() {

    // ULA tick (same clock as CPU — one T-state)
    board_.ula.tick();

    // Frame interrupt: ULA asserts INT at start of frame, held for 32 T-states
    if (board_.ula.check_frame_interrupt()) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);  // Assert INT (active-low)
        int_counter_ = 32;
    }
    if (int_counter_ > 0) {
        if (--int_counter_ == 0) {
            BUS_SET_BIT(pins_, BUS_IRQ_BIT);  // Deassert INT
        }
    }

    // Memory contention (ULA stalls CPU during screen fetch)
    // TODO: Implement contention pattern

    // CPU tick — one T-state
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    // Check for I/O request vs memory request
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // AY tick (128K: clocked at CPU/2)
    if constexpr (Traits::has_ay_sound) {
        if (frame_tstate_counter_ & 1) {
            board_.psg.tick();
        }
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = board_.ula.get_ear_output() ? 0.5f : 0.0f;
        if constexpr (Traits::has_ay_sound) {
            sample += board_.psg.get_sample() * 0.5f;
        }
        audio_ring_buf_.write(&sample, 1);
        if (audio_port_) audio_port_->drive_sample(sample);
    }

    // Frame counter
    frame_tstate_counter_++;
    if (frame_tstate_counter_ >= spectrum_constants::TSTATES_PER_FRAME) {
        frame_tstate_counter_ = 0;
        board_.ula.render_frame(screen_ram_ptr_);
    }

    total_cycles_++;
}

template<SpectrumVariant V>
void SpectrumSystem<V>::run_frame() {
    if (!video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::configure_bus_memory_map() {
    using ChipId = typename PT::ChipId;

    // apply() establishes the default map from the manifest:
    //   48K:  RAM pages 0-255 (read+write), ROM overlays read pages 0-63
    //   128K: RAM banks 0-3 mapped linearly, ROM bank 0 overlays reads
    board_.apply(bus_);

    if constexpr (Traits::has_banking) {
        // 128K: remap fixed banks and current paging state.
        //   $4000-$7FFF: always bank 5
        //   $8000-$BFFF: always bank 2
        //   $C000-$FFFF: selected by bank_select_ bits 0-2
        //   $0000-$3FFF: ROM bank selected by bank_select_ bit 4
        board_.select_bank_at(bus_, 0, BT::kManifest.template find<RAMChip>(), 5, 0x40);
        board_.select_bank_at(bus_, 0, BT::kManifest.template find<RAMChip>(), 2, 0x80);

        // Switchable bank at $C000 + ROM bank at $0000
        update_banking();
    }

    // Screen RAM pointer — bank 5 for 128K (default), $4000 offset for 48K
    if constexpr (Traits::has_banking) {
        bool use_bank7 = (bank_select_ & 0x08) != 0;
        size_t screen_bank = use_bank7 ? 7 : 5;
        screen_ram_ptr_ = board_.chip_buffer(
            ChipId(size_t(board_.slot(BT::kManifest.template find<RAMChip>()).base_id) + screen_bank));
    } else {
        // 48K: screen starts at $4000 = page $40 = chip ID 64
        screen_ram_ptr_ = board_.chip_buffer(ChipId(0x40));
    }
}

template<SpectrumVariant V>
void SpectrumSystem<V>::update_banking() {
    if constexpr (!Traits::has_banking) return;

    using ChipId = typename PT::ChipId;

    // Switchable RAM bank at $C000-$FFFF (bits 0-2 of bank_select_)
    uint8_t ram_bank = bank_select_ & 0x07;
    board_.select_bank_at(bus_, 0, BT::kManifest.template find<RAMChip>(), ram_bank, 0xC0);

    // ROM bank at $0000-$3FFF (bit 4 of bank_select_)
    uint8_t rom_bank = (bank_select_ & 0x10) ? 1 : 0;
    board_.select_bank_at(bus_, 0, BT::kManifest.template find<ROMChip>(), rom_bank, 0x00);

    // Screen bank: bit 3 selects bank 5 or 7
    bool use_bank7 = (bank_select_ & 0x08) != 0;
    size_t screen_bank = use_bank7 ? 7 : 5;
    screen_ram_ptr_ = board_.chip_buffer(
        ChipId(size_t(board_.slot(BT::kManifest.template find<RAMChip>()).base_id) + screen_bank));
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<SpectrumVariant V>
bus_state_t SpectrumSystem<V>::io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (!(addr & 0x01)) {
        // ULA port ($FE) — selected when A0=0
        if (is_read) {
            uint8_t data = board_.ula.read_port_fe(static_cast<uint8_t>(addr >> 8));
            BUS_SET_DATA(pins, data);
        } else {
            board_.ula.write_port_fe(BUS_GET_DATA(pins));
        }
    }

    if constexpr (Traits::has_banking) {
        // 128K banking port $7FFD (A1=0, A15=0)
        if (!is_read && !(addr & 0x8002)) {
            if (!bank_locked_) {
                bank_select_ = BUS_GET_DATA(pins);
                bank_locked_ = (bank_select_ & 0x20) != 0;
                update_banking();
            }
        }

        // AY register select $FFFD (A1=0, A14=1, A15=1)
        if ((addr & 0xC002) == 0xC000) {
            if (is_read) {
                BUS_SET_DATA(pins, board_.psg.read_register());
            } else {
                board_.psg.latch_address(BUS_GET_DATA(pins));
            }
        }

        // AY data write $BFFD (A1=0, A14=1, A15=0)
        if (!is_read && (addr & 0xC002) == 0x8000) {
            board_.psg.write_register(BUS_GET_DATA(pins));
        }
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    format_load_result_t result;
    if (!format_load_file(filepath, &result)) {
        printf("%s: Failed to load file: %s\n", Traits::name, result.error_msg);
        return false;
    }

    using ChipId = typename PT::ChipId;
    uint8_t* ram = board_.chip_buffer(ChipId(0));

    // ── SNA snapshot ────────────────────────────────────────────────────
    if (result.format == &SNA_FORMAT_DESCRIPTOR && result.metadata_size >= sizeof(sna_header_t)) {
        const auto& hdr = *reinterpret_cast<const sna_header_t*>(result.metadata);

        // Reset system first
        reset();

        // Copy 48K RAM to $4000-$FFFF (page $40 = chip offset $4000)
        if (result.program.data && result.program.data_size >= 49152) {
            std::memcpy(ram + 0x4000, result.program.data, 49152);
        }

        // Restore Z80 registers
        board_.z80.set(I, hdr.i_reg);
        board_.z80.set(R, hdr.r_reg);
        board_.z80.set(AF, hdr.af);
        board_.z80.set(BC, hdr.bc);
        board_.z80.set(DE, hdr.de);
        board_.z80.set(HL, hdr.hl);
        board_.z80.set(IX, hdr.ix);
        board_.z80.set(IY, hdr.iy);
        board_.z80.set(AF_, hdr.af_prime);
        board_.z80.set(BC_, hdr.bc_prime);
        board_.z80.set(DE_, hdr.de_prime);
        board_.z80.set(HL_, hdr.hl_prime);
        board_.z80.set(SP, hdr.sp);
        board_.z80.set_im(hdr.int_mode);
        board_.z80.set_iff1((hdr.iff2 & 0x04) != 0);
        board_.z80.set_iff2((hdr.iff2 & 0x04) != 0);

        // SNA 48K: PC is on the stack — pop it
        uint16_t sp = hdr.sp;
        uint16_t pc_lo = ram[sp];
        uint16_t pc_hi = ram[(uint16_t)(sp + 1)];
        board_.z80.set(PC, pc_lo | (pc_hi << 8));
        board_.z80.set(SP, sp + 2);

        // Restore border color
        board_.ula.set_border_color(hdr.border & 0x07);

        printf("%s: SNA loaded — PC=$%04X SP=$%04X\n", Traits::name,
               board_.z80.get(PC), board_.z80.get(SP));
        result.release();
        return true;
    }

    // ── Z80 snapshot ────────────────────────────────────────────────────
    if (result.format == &Z80_SNAPSHOT_FORMAT_DESCRIPTOR &&
        result.metadata_size >= sizeof(z80_snapshot_header_t)) {
        const auto& hdr = *reinterpret_cast<const z80_snapshot_header_t*>(result.metadata);

        reset();

        // Copy RAM
        if (hdr.is_128k && result.program.data_size >= 131072) {
            // 128K: 8 banks × 16KB stored sequentially in program.data
            if constexpr (Traits::has_banking) {
                constexpr size_t BANK_SIZE = 16384;
                constexpr size_t kPagesPerBank = 64;
                for (int bank = 0; bank < 8; ++bank) {
                    uint8_t* dst = board_.chip_buffer(ChipId(bank * kPagesPerBank));
                    std::memcpy(dst, result.program.data + bank * BANK_SIZE, BANK_SIZE);
                }
                bank_select_ = hdr.port_7ffd;
                bank_locked_ = false;
                update_banking();
            } else {
                // 48K system loading 128K snapshot — take banks 5, 2, 0
                std::memcpy(ram + 0x4000, result.program.data + 5 * 16384, 16384); // bank 5 → $4000
                std::memcpy(ram + 0x8000, result.program.data + 2 * 16384, 16384); // bank 2 → $8000
                std::memcpy(ram + 0xC000, result.program.data + 0 * 16384, 16384); // bank 0 → $C000
            }
        } else if (result.program.data && result.program.data_size >= 49152) {
            // 48K: direct RAM copy to $4000-$FFFF
            std::memcpy(ram + 0x4000, result.program.data, 49152);
        }

        // Restore Z80 registers
        board_.z80.set(AF, hdr.af);
        board_.z80.set(BC, hdr.bc);
        board_.z80.set(DE, hdr.de);
        board_.z80.set(HL, hdr.hl);
        board_.z80.set(IX, hdr.ix);
        board_.z80.set(IY, hdr.iy);
        board_.z80.set(SP, hdr.sp);
        board_.z80.set(PC, hdr.pc);
        board_.z80.set(I, hdr.i_reg);
        board_.z80.set(R, hdr.r_reg);
        board_.z80.set_im(hdr.im_mode);
        board_.z80.set_iff1(hdr.iff1 != 0);
        board_.z80.set_iff2(hdr.iff2 != 0);
        board_.z80.set(AF_, hdr.af_prime);
        board_.z80.set(BC_, hdr.bc_prime);
        board_.z80.set(DE_, hdr.de_prime);
        board_.z80.set(HL_, hdr.hl_prime);

        // Restore border color
        board_.ula.set_border_color(hdr.border & 0x07);

        printf("%s: Z80 v%d loaded — PC=$%04X SP=$%04X\n", Traits::name,
               hdr.version, board_.z80.get(PC), board_.z80.get(SP));
        result.release();
        return true;
    }

    // ── Spectrum TAP ────────────────────────────────────────────────────
    if (result.format == &SPECTRUM_TAP_FORMAT_DESCRIPTOR) {
        // Metadata layout: [count:u8][spectrum_tap_header_t...]
        // One header per loadable block, in the same order as files[].
        const uint8_t* meta = result.metadata;
        int header_count = (result.metadata_size > 0) ? meta[0] : 0;
        const auto* headers = reinterpret_cast<const spectrum_tap_header_t*>(meta + 1);

        uint16_t code_addr = 0;
        bool has_code = false;
        uint16_t basic_len = 0;
        uint16_t basic_var_offset = 0;
        uint16_t autostart_line = 0xFFFF;  // >= 32768 means no autostart
        bool has_basic = false;

        auto load_block = [&](const program_data_t& prog, int block_idx) {
            if (!prog.data || prog.data_size == 0) return;
            uint16_t addr = prog.load_addr;
            size_t len = prog.data_size;
            if (addr + len > 0x10000) len = 0x10000 - addr;
            std::memcpy(ram + addr, prog.data, len);
            printf("%s: TAP loaded %zu bytes at $%04X\n", Traits::name, len, addr);

            // Use header type info to classify blocks
            if (block_idx < header_count) {
                const auto& hdr = headers[block_idx];
                if (hdr.type == SPECTRUM_TAP_CODE) {
                    code_addr = addr;
                    has_code = true;
                } else if (hdr.type == SPECTRUM_TAP_PROGRAM) {
                    has_basic = true;
                    basic_len = static_cast<uint16_t>(len);
                    basic_var_offset = hdr.param2;
                    if (hdr.param1 < 32768)
                        autostart_line = hdr.param1;
                }
            }
        };

        if (result.type == FORMAT_LOAD_PROGRAM) {
            load_block(result.program, 0);
        } else if (result.type == FORMAT_LOAD_ARCHIVE) {
            for (int i = 0; i < result.file_count; ++i)
                load_block(result.files[i], i);
        }

        // Set up BASIC system variables so the ROM can find the program.
        // Standard Spectrum memory layout after PROG ($5CCB):
        //   PROG ... PROG+var_offset-1  → BASIC program lines
        //   PROG+var_offset ... end     → Variables (already in the block)
        //   VARS+1                      → E_LINE (edit line: $0D $80)
        if (has_basic) {
            constexpr uint16_t PROG_ADDR = 0x5CCB;
            uint16_t vars_addr = PROG_ADDR + basic_var_offset;
            uint16_t eline_addr = PROG_ADDR + basic_len;

            // Write end-of-edit-line markers
            if (eline_addr + 1 < 0x10000) {
                ram[eline_addr]     = 0x0D;  // Newline
                ram[eline_addr + 1] = 0x80;  // End marker
            }

            // Helper to poke a 16-bit LE value into RAM
            auto poke16 = [&](uint16_t sysvar, uint16_t val) {
                ram[sysvar]     = val & 0xFF;
                ram[sysvar + 1] = val >> 8;
            };

            poke16(0x5C53, PROG_ADDR);        // PROG  — start of BASIC program
            poke16(0x5C4B, vars_addr);         // VARS  — start of variables
            poke16(0x5C59, eline_addr);        // E_LINE — command being edited
            uint16_t worksp = eline_addr + 2;
            poke16(0x5C61, worksp);            // WORKSP — temporary workspace
            poke16(0x5C63, worksp);            // STKBOT — calculator stack bottom
            poke16(0x5C65, worksp);            // STKEND — calculator stack end

            // Autostart: set NEWPPC/NSPPC so the ROM jumps to the line
            if (autostart_line < 32768) {
                poke16(0x5C42, autostart_line); // NEWPPC — line to jump to
                ram[0x5C44] = 0;                // NSPPC  — statement 0
            } else {
                poke16(0x5C42, 0xFFFE);         // NEWPPC — no auto-run
                ram[0x5C44] = 0xFF;             // NSPPC  — direct mode
            }

            if (autostart_line < 32768)
                printf("%s: BASIC program loaded (vars=$%04X, eline=$%04X, autostart=%u)\n",
                       Traits::name, vars_addr, eline_addr, autostart_line);
            else
                printf("%s: BASIC program loaded (vars=$%04X, eline=$%04X, no autostart)\n",
                       Traits::name, vars_addr, eline_addr);
        }

        // Jump target: prefer CODE blocks (machine code entry point),
        // otherwise enter the ROM's main execution loop for BASIC.
        if (has_code) {
            board_.z80.set(PC, code_addr);
            printf("%s: Jumping to CODE at $%04X\n", Traits::name, code_addr);
        } else if (has_basic) {
            // Enter the ROM main execution loop — it will honour NEWPPC/NSPPC
            board_.z80.set(PC, 0x12A2);  // MAIN-EXEC in the 48K ROM
            printf("%s: Entering BASIC via ROM MAIN-EXEC ($12A2)\n", Traits::name);
        }

        result.release();
        return true;
    }

    // ── SCL / TRD (TR-DOS containers) ────────────────────────────────
    if (result.format == &SCL_FORMAT_DESCRIPTOR ||
        result.format == &TRD_FORMAT_DESCRIPTOR) {
        if (result.type == FORMAT_LOAD_PROGRAM && result.program.data) {
            uint16_t addr = result.program.load_addr;
            size_t len = result.program.data_size;
            if (addr + len > 0x10000) len = 0x10000 - addr;
            std::memcpy(ram + addr, result.program.data, len);

            // Check metadata for file type
            const trdos::file_entry_t* entry = nullptr;
            if (result.metadata_size >= sizeof(trdos::file_entry_t))
                entry = reinterpret_cast<const trdos::file_entry_t*>(result.metadata);

            bool is_code = entry && entry->type == 'C';
            if (is_code) {
                board_.z80.set(PC, addr);
                printf("%s: %s Code loaded %zu bytes at $%04X — jumping\n",
                       Traits::name, result.format->name, len, addr);
            } else {
                printf("%s: %s loaded %zu bytes at $%04X\n",
                       Traits::name, result.format->name, len, addr);
            }
        }
        result.release();
        return true;
    }

    printf("%s: Unsupported format for file: %s\n", Traits::name, filepath);
    result.release();
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<SpectrumVariant V>
uint32_t SpectrumSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

template<SpectrumVariant V>
void SpectrumSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = spectrum_constants::CPU_FREQ_HZ / audio_sample_rate_;
}

// ============================================================================
// INPUT
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // ZX Spectrum keyboard matrix: 8 half-rows × 5 keys, active-low.
    //
    // Row 0 (port $FEFE): CAPS SHIFT, Z, X, C, V       (bits 0-4)
    // Row 1 (port $FDFE): A, S, D, F, G
    // Row 2 (port $FBFE): Q, W, E, R, T
    // Row 3 (port $F7FE): 1, 2, 3, 4, 5
    // Row 4 (port $EFFE): 0, 9, 8, 7, 6
    // Row 5 (port $DFFE): P, O, I, U, Y
    // Row 6 (port $BFFE): ENTER, L, K, J, H
    // Row 7 (port $7FFE): SPACE, SYMBOL SHIFT, M, N, B

    struct KeyMapping { SDL_Keycode sdl_key; int row; int bit; };
    static constexpr KeyMapping mappings[] = {
        // Row 0: CAPS SHIFT, Z, X, C, V
        { SDLK_LSHIFT,  0, 0 }, { SDLK_RSHIFT,  0, 0 },
        { SDLK_z,        0, 1 }, { SDLK_x,        0, 2 },
        { SDLK_c,        0, 3 }, { SDLK_v,        0, 4 },
        // Row 1: A, S, D, F, G
        { SDLK_a,        1, 0 }, { SDLK_s,        1, 1 },
        { SDLK_d,        1, 2 }, { SDLK_f,        1, 3 },
        { SDLK_g,        1, 4 },
        // Row 2: Q, W, E, R, T
        { SDLK_q,        2, 0 }, { SDLK_w,        2, 1 },
        { SDLK_e,        2, 2 }, { SDLK_r,        2, 3 },
        { SDLK_t,        2, 4 },
        // Row 3: 1, 2, 3, 4, 5
        { SDLK_1,        3, 0 }, { SDLK_2,        3, 1 },
        { SDLK_3,        3, 2 }, { SDLK_4,        3, 3 },
        { SDLK_5,        3, 4 },
        // Row 4: 0, 9, 8, 7, 6
        { SDLK_0,        4, 0 }, { SDLK_9,        4, 1 },
        { SDLK_8,        4, 2 }, { SDLK_7,        4, 3 },
        { SDLK_6,        4, 4 },
        // Row 5: P, O, I, U, Y
        { SDLK_p,        5, 0 }, { SDLK_o,        5, 1 },
        { SDLK_i,        5, 2 }, { SDLK_u,        5, 3 },
        { SDLK_y,        5, 4 },
        // Row 6: ENTER, L, K, J, H
        { SDLK_RETURN,  6, 0 }, { SDLK_l,        6, 1 },
        { SDLK_k,        6, 2 }, { SDLK_j,        6, 3 },
        { SDLK_h,        6, 4 },
        // Row 7: SPACE, SYMBOL SHIFT, M, N, B
        { SDLK_SPACE,   7, 0 }, { SDLK_LCTRL,   7, 1 }, { SDLK_RCTRL, 7, 1 },
        { SDLK_m,        7, 2 }, { SDLK_n,        7, 3 },
        { SDLK_b,        7, 4 },
        // Convenience: Backspace → CAPS SHIFT + 0 (DELETE)
        { SDLK_BACKSPACE, 0, 0 }, { SDLK_BACKSPACE, 4, 0 },
    };

    for (const auto& m : mappings) {
        if (m.sdl_key == key) {
            uint8_t row_state = keyboard_rows_[m.row];
            if (pressed)
                row_state &= ~(1 << m.bit);  // Active-low: clear bit
            else
                row_state |= (1 << m.bit);   // Release: set bit
            keyboard_rows_[m.row] = row_state;
            board_.ula.set_keyboard_row(m.row, row_state);
        }
    }
}

// ============================================================================
// GUI
// ============================================================================

template<SpectrumVariant V>
void SpectrumSystem<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    if (palette_selector::render(board_.ula, config_.custom_settings)) {
        set_configuration(config_);
        apply_configuration();
    }
#endif
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<SpectrumVariant V>
bool SpectrumSystem<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root(Traits::data_folder, rom_root, sizeof(rom_root))) {
        printf("Spectrum: Could not find ROM root folder\n");
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class SpectrumSystem<SpectrumVariant::ZX48K>;
template class SpectrumSystem<SpectrumVariant::ZX128K>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(spectrum48k_descriptor, [] {
    return std::make_unique<SpectrumSystem<SpectrumVariant::ZX48K>>();
});

REGISTER_SYSTEM(spectrum128k_descriptor, [] {
    return std::make_unique<SpectrumSystem<SpectrumVariant::ZX128K>>();
});
