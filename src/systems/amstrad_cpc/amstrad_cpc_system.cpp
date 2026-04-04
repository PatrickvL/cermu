/*
 * amstrad_cpc_system.cpp — Amstrad CPC 464/664/6128 system implementation
 *
 * Tick loop:
 *   Z80A @ 4 MHz.  Gate Array generates interrupts every 52 HSYNCs.
 *   MC6845 drives display timing; Gate Array translates CRTC addresses to
 *   screen memory + mode/color decoding.  AY-3-8912 driven via PPI port.
 */

#include "core/cermu.hpp"
#include "systems/amstrad_cpc/amstrad_cpc_system.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

using namespace fam6845::reg;

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<CPCModel M>
static HardwareTraits create_cpc_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = amstrad_cpc_constants::FB_WIDTH;
    traits.display.native_height   = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.visible_width   = amstrad_cpc_constants::FB_WIDTH;
    traits.display.visible_height  = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = amstrad_cpc_constants::GA_COLOR_COUNT;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8912";

    traits.timing.cpu_frequency_hz   = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = amstrad_cpc_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor cpc464_descriptor = {
    "Amstrad CPC 464", "CPC464",
    "Amstrad CPC 464 — Z80A @ 4MHz, 64KB RAM, integrated tape (1984)",
    "amstrad_cpc", {"CPC464", "CPC", "AmstradCPC"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC464>(), nullptr,
    "Amstrad", 1984, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor cpc664_descriptor = {
    "Amstrad CPC 664", "CPC664",
    "Amstrad CPC 664 — Z80A @ 4MHz, 64KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC664"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC664>(), nullptr,
    "Amstrad", 1985, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor cpc6128_descriptor = {
    "Amstrad CPC 6128", "CPC6128",
    "Amstrad CPC 6128 — Z80A @ 4MHz, 128KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC6128"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC6128>(), nullptr,
    "Amstrad", 1985, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

template<CPCModel M>
AmstradCPCSystem<M>::AmstradCPCSystem()
    : System()
    , pins_(CPC_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_cpc_hardware_traits<M>();
}

template<CPCModel M>
AmstradCPCSystem<M>::~AmstradCPCSystem() {}

template<CPCModel M>
const SystemDescriptor& AmstradCPCSystem<M>::get_descriptor() const {
    if constexpr (M == CPCModel::CPC464) return cpc464_descriptor;
    else if constexpr (M == CPCModel::CPC664) return cpc664_descriptor;
    else return cpc6128_descriptor;
}

template<CPCModel M>
bool AmstradCPCSystem<M>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<CPCModel M>
bool AmstradCPCSystem<M>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<CPCModel M>
bool AmstradCPCSystem<M>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Create chips from manifest and bind chipset ───────────────────
    bind_all(board_, board_.components_, BT::kManifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = BT::kManifest.port_slots;
    port_manifest_count_ = BT::kManifest.port_count;
    // ── Configure page tables for this variant ──────────────────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.crtc.init();
    // Gate array drives interrupts from CRTC HSYNC (every 52 HSYNCs)
    board_.crtc.on_hsync = [this]() {
        board_.gate_array.interrupt_counter++;
        if (board_.gate_array.interrupt_counter >= 52) {
            board_.gate_array.interrupt_counter = 0;
            board_.gate_array.interrupt_pending = true;
        }
    };
    board_.ppi.init();
    board_.psg.init();
    // AY clock = CPU / 4 = 1 MHz
    board_.psg.set_clock_frequency(amstrad_cpc_constants::CPU_FREQ_HZ / 4);
    board_.psg.set_audio_sample_rate(amstrad_cpc_constants::DEFAULT_SAMPLE_RATE);

    // Wire AY to audio thread — cpu_cycles_per_tick=4 (AY = CPU / 4)
    ay_adapter_ = std::make_unique<WriteOnlySynthAdapter<AY_3_8912, true>>(&board_.psg, 4);
    audio_thread_.register_engine(ay_adapter_.get());
    audio_thread_.start();

    // Wire AY to audio signal port
    audio_port_ = std::make_unique<AudioPort>();
    board_.psg.set_audio_port(audio_port_.get());

    load_roms();
    // ── Cache RAM chip pointer for rendering ───────────────────────
    ram_chip_ = &board_.ram;

    // Video output — composite video from Gate Array
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.gate_array.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);
    // ── Register all manifest chips for Hardware menu ────────────────
    register_bus_chips(board_);

    log_info("%s: System initialized (%dKB RAM)\n", Traits::name, Traits::ram_size_kb);
    system_ready_ = true;
    return true;
}

template<CPCModel M>
void AmstradCPCSystem<M>::shutdown() {
    audio_thread_.stop();
    system_ready_ = false;
}

template<CPCModel M>
void AmstradCPCSystem<M>::reset() {
    if (!system_ready_) return;
    audio_thread_.stop();
    // Reset all manifest chips (CRTC, PPI, AY, Gate Array; RAM/ROM are no-op)
    board_.reset_chips();
    if (ay_adapter_) ay_adapter_->reset();
    pins_ = board_.z80.reset(pins_);
    configure_bus_memory_map();
    audio_thread_.start();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<CPCModel M>
void AmstradCPCSystem<M>::tick() {
    if (!system_ready_) return;

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // Gate array interrupt: IRQ is level-sensitive, keep asserted while pending
    if (board_.gate_array.interrupt_pending) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    }

    // CRTC ticks at 1 MHz (CPU clock / 4).
    // AY synthesis is driven by the audio thread — no direct tick here.
    if ((total_cycles_ & 3) == 0) {
        board_.crtc.tick();
    }

    total_cycles_++;

    // Render at frame boundary — snapshot current VRAM into video output
    if (total_cycles_ % amstrad_cpc_constants::TSTATES_PER_FRAME == 0) {
        render_frame();
    }
}

template<CPCModel M>
void AmstradCPCSystem<M>::run_frame() {
    if (!video_port_) return;

    // Stream-driven: Gate Array drives FrameEnd at frame boundary
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    audio_thread_.signal_progress(total_cycles_);
    video_port_->swap_frame();
}

template<CPCModel M> uint32_t AmstradCPCSystem<M>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }
    return board_.psg.audio_read(buffer, max_samples);
}
template<CPCModel M> void AmstradCPCSystem<M>::set_audio_sample_rate(int hz) {
    audio_sample_rate_ = hz;
    board_.psg.set_audio_sample_rate(hz);
}

// ============================================================================
// VIDEO RENDERING — delegate to Gate Array chip
// ============================================================================

template<CPCModel M>
void AmstradCPCSystem<M>::render_frame() {
    if (!ram_chip_) return;

    // CRTC display start address (R12:R13)
    uint16_t crtc_start = (board_.crtc.regs_[R12_START_ADDR_HI] << 8)
                        | board_.crtc.regs_[R13_START_ADDR_LO];

    board_.gate_array.render_frame(ram_chip_->data(), crtc_start);
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<CPCModel M>
void AmstradCPCSystem<M>::configure_bus_memory_map() {
    // apply() maps base-layer chips (RAM) and skips overlay_group > 0 (ROMs).
    board_.apply(bus_);

    if constexpr (Traits::ram_size_kb == 128) {
        // 6128: remap RAM banks per current board_.gate_array.ram_config.
        // Default config 0 = {0,1,2,3} — identity, matches apply() output.
        update_banking();
    }

    // Build overlay snapshots from the manifest's overlay_group tags.
    // 4 modes: {none, lower, upper, both} — derived from groups 1+2.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);

    // Apply current ROM overlay state.
    apply_rom_overlay();
}

// ── RAM banking (CPC 6128 only) + rebuild overlay snapshots ──────────────
// Called when board_.gate_array.ram_config changes (Gate Array opcode 11xxxxxx).
// Remaps RAM bank assignments, rebuilds overlay snapshots for the new base
// state, then re-applies the current ROM overlay.
template<CPCModel M>
void AmstradCPCSystem<M>::update_banking() {
    if constexpr (Traits::ram_size_kb == 128) {
        // CPC 6128: 8 banking configurations mapping 4 logical pages to 8 physical banks
        static constexpr uint8_t bank_table[8][4] = {
            {0, 1, 2, 3}, {0, 1, 2, 7}, {4, 5, 6, 7}, {0, 3, 2, 7},
            {0, 4, 2, 3}, {0, 5, 2, 3}, {0, 6, 2, 3}, {0, 7, 2, 3}
        };
        uint8_t config = board_.gate_array.ram_config & 7;
        constexpr size_t kPagesPerBank = 64;  // 16384 / 256
        for (int pg = 0; pg < 4; ++pg) {
            board_.select_bank_at(bus_, 0, BT::kManifest.template find<RAMChip>(),
                                   bank_table[config][pg],
                                   pg * kPagesPerBank);
        }
    }

    // Rebuild overlay snapshots for the new RAM base state, then apply.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);
    apply_rom_overlay();
}

// ── ROM overlay snapshot selection ───────────────────────────────────────
// Loads the pre-computed snapshot for the current ROM enable state.
// Mode bits: 0 = lower ROM (group 1), 1 = upper ROM (group 2).
template<CPCModel M>
void AmstradCPCSystem<M>::apply_rom_overlay() {
    size_t mode = (board_.gate_array.lower_rom_enabled ? 1 : 0)
               |  (board_.gate_array.upper_rom_enabled ? 2 : 0);
    bus_.load_snapshot(0, snapshots_[0][mode]);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<CPCModel M>
bus_state_t AmstradCPCSystem<M>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1 asserted simultaneously
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        board_.gate_array.interrupt_pending = false;
        BUS_SET_BIT(pins, BUS_IRQ_BIT);  // Deassert INT
        BUS_SET_DATA(pins, 0xFF);
        return pins;
    }

    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // CPC uses partial address-line decoding for I/O

    // Gate Array (active when A15=0) -- write-only
    if (!is_read && !(addr & 0x8000)) {
        switch (data >> 6) {
            case 0:  // Pen select
                board_.gate_array.pen_select = data & 0x1F;
                break;
            case 1:  // Set color for current pen
                if (board_.gate_array.pen_select < amstrad_cpc_constants::GA_PEN_COUNT)
                    board_.gate_array.ink[board_.gate_array.pen_select] = data & 0x1F;
                break;
            case 2:  // Screen mode + ROM control + interrupt reset
                board_.gate_array.screen_mode = data & 0x03;
                board_.gate_array.lower_rom_enabled = !(data & 0x04);
                board_.gate_array.upper_rom_enabled = !(data & 0x08);
                if (data & 0x10) {
                    board_.gate_array.interrupt_counter = 0;
                    board_.gate_array.interrupt_pending = false;
                    BUS_SET_BIT(pins, BUS_IRQ_BIT);
                }
                apply_rom_overlay();  // ROM visibility changed
                break;
            case 3:  // RAM banking (CPC6128 only)
                if constexpr (Traits::ram_size_kb == 128) {
                    board_.gate_array.ram_config = data & 0x3F;
                    update_banking();  // RAM bank configuration changed → rebuild + apply
                }
                break;
        }
    }

    // CRTC 6845 (active when A14=0)
    if (!(addr & 0x4000)) {
        // A9:A8 selects function: 00=reg select, 01=data write, 10=status read, 11=data read
        uint8_t crtc_func = (addr >> 8) & 0x03;
        if (is_read) {
            if (crtc_func >= 2) {
                BUS_SET_DATA(pins, board_.crtc.read(crtc_func & 1));
            }
        } else {
            if (crtc_func < 2) {
                board_.crtc.write(crtc_func & 1, data);
            }
        }
    }

    // PPI 8255 (active when A11=0)
    if (!(addr & 0x0800)) {
        uint8_t ppi_reg = addr & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, board_.ppi.read(ppi_reg));
        } else {
            board_.ppi.write(ppi_reg, data);
            // AY-3-8912 is controlled via PPI Port C bits 7:6 (BDIR/BC1)
            // and Port A carries the data bus
            uint8_t port_c = board_.ppi.get_port_c_output();
            bool bdir = (port_c >> 7) & 1;
            bool bc1  = (port_c >> 6) & 1;
            if (bdir && bc1) {
                ay_latch_ = board_.ppi.get_port_a_output() & 0x0F;
                board_.psg.latch_address(board_.ppi.get_port_a_output());
            } else if (bdir && !bc1) {
                // Shadow write for immediate readback, enqueue for audio thread
                uint8_t val = board_.ppi.get_port_a_output();
                board_.psg.write_register_shadow(val);
                ay_adapter_->cmd_queue().push_write(total_cycles_, ay_latch_, val);
            } else if (!bdir && bc1) {
                board_.ppi.set_port_a_input(board_.psg.read_register());
            }
        }
    }

    return pins;
}

template<CPCModel M>
bool AmstradCPCSystem<M>::load_roms() { return false; }

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class AmstradCPCSystem<CPCModel::CPC464>;
template class AmstradCPCSystem<CPCModel::CPC664>;
template class AmstradCPCSystem<CPCModel::CPC6128>;

REGISTER_SYSTEM(cpc464_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC464>>(); });
REGISTER_SYSTEM(cpc664_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC664>>(); });
REGISTER_SYSTEM(cpc6128_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC6128>>(); });
