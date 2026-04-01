/*
 * bbc_master_system.cpp — BBC Micro B+ / Master system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 *
 * The B+ and Master share the Model B's chipset (MC6845, SN76489, 2×VIA,
 * Video ULA) but use the WDC 65C02 CPU and have enhanced memory banking.
 * All new behavior is system-level glue — no new chips required.
 */

#include "systems/bbc/bbc_master_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

using namespace fam6845::reg;

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<BBCMasterVariant V>
static HardwareTraits create_bbc_master_hardware_traits() {
    using Traits = BBCMasterVariantTraits<V>;
    HardwareTraits traits = {};

    // Display — same as Model B (same video hardware)
    traits.display.native_width    = bbc_constants::DISPLAY_WIDTH;
    traits.display.native_height   = bbc_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = bbc_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = bbc_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 8;
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan    = false;

    // Same 8-color physical palette
    traits.display.default_palette = {
        PaletteColor(  0,   0,   0, 255),  // 0: Black
        PaletteColor(255,   0,   0, 255),  // 1: Red
        PaletteColor(  0, 255,   0, 255),  // 2: Green
        PaletteColor(255, 255,   0, 255),  // 3: Yellow
        PaletteColor(  0,   0, 255, 255),  // 4: Blue
        PaletteColor(255,   0, 255, 255),  // 5: Magenta
        PaletteColor(  0, 255, 255, 255),  // 6: Cyan
        PaletteColor(255, 255, 255, 255),  // 7: White
    };

    // Audio — SN76489 (same as Model B)
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = bbc_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "SN76489";

    // Timing — same 2 MHz, PAL 50 Hz
    traits.timing.cpu_frequency_hz = bbc_constants::CPU_FREQ;
    traits.timing.video_frequency_hz = bbc_constants::MASTER_CLOCK;
    traits.timing.audio_sample_rate_hz = bbc_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps       = bbc_constants::TARGET_FPS;
    traits.timing.cycles_per_frame = bbc_constants::CYCLES_PER_FRAME;
    traits.timing.standard         = VideoStandard::PAL;

    // Memory options
    traits.memory_options.push_back({
        V == BBCMasterVariant::MODEL_B_PLUS ? "64KB RAM (B+)" : "128KB RAM (Master)",
        Traits::ram_size, 0, true
    });

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor bbc_bplus_descriptor = {
    "BBC Micro Model B+", "BBCB+",
    "Acorn BBC Micro Model B+ (1985) — 65C02, 64KB RAM, OS 2.0",
    "bbc", {"BBCB+", "BBC B+", "BBCBPlus"},
    nullptr,
    create_bbc_master_hardware_traits<BBCMasterVariant::MODEL_B_PLUS>(),
    nullptr,
    "Acorn", 1985, fam65xx::WDC_65C02_EARLYTraits.display_name, SystemType::Home
};

static SystemDescriptor bbc_master_descriptor = {
    "BBC Master 128", "BBCMaster",
    "Acorn BBC Master 128 (1986) — 65C02, 128KB RAM, MOS 3.20",
    "bbc", {"BBCMaster", "BBC Master", "Master128"},
    nullptr,
    create_bbc_master_hardware_traits<BBCMasterVariant::MASTER_128>(),
    nullptr,
    "Acorn", 1986, fam65xx::WDC_65C02_EARLYTraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<BBCMasterVariant V>
BBCMasterSystem<V>::BBCMasterSystem()
    : System()
    , pins_(BBC_MASTER_BUS_DEFAULT_STATE)
    , cycles_per_frame_(bbc_constants::CYCLES_PER_FRAME)
{
    hardware_traits_ = create_bbc_master_hardware_traits<V>();
}

template<BBCMasterVariant V>
BBCMasterSystem<V>::~BBCMasterSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<BBCMasterVariant V>
const SystemDescriptor& BBCMasterSystem<V>::get_descriptor() const {
    if constexpr (V == BBCMasterVariant::MODEL_B_PLUS) return bbc_bplus_descriptor;
    else return bbc_master_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<BBCMasterVariant V>
bool BBCMasterSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<BBCMasterVariant V>
bool BBCMasterSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<BBCMasterVariant V>
bool BBCMasterSystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // Pre-bind all value-typed chips
    { size_t slot_idx_ = 0;
      BBC_BPLUS_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);

    ram_chip_       = &board_.ram;
    paged_rom_chip_ = &board_.paged_rom;
    os_rom_chip_    = &board_.os_rom;
    memory_         = ram_chip_->data();

    pins_ = board_.w65c02.init();
    board_.sys_via.reset();
    board_.user_via.reset();
    board_.sys_via.interrupt_bit = BUS_IRQ_BIT;
    board_.user_via.interrupt_bit   = BUS_IRQ_BIT;

    configure_bus_memory_map();
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    // ── CRTC (MC6845) ───────────────────────────────────────────────────
    board_.crtc.init();

    // Program CRTC with Mode 7 register values (the MOS does this too, but
    // we prime them so the display works even before the ROM runs)
    for (int i = 0; i < 14; i++) {
        board_.crtc.regs_[i] = bbc_constants::MODE7_CRTC_REGS[i];
    }

    // Wire CRTC callbacks
    board_.crtc.on_display_char = [this](uint16_t ma, uint8_t ra, bool cursor) {
        this->crtc_display_char(ma, ra, cursor);
    };
    board_.crtc.on_vsync = [this]() { this->crtc_vsync(); };
    board_.crtc.on_hsync = [this]() { this->crtc_hsync(); };

    // Set memory for video rendering
    board_.vidproc.set_memory(memory_);

    // ── Video output output ─────────────────────────────────────────────
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vidproc.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // ── Video ULA defaults ──────────────────────────────────────────────
    // Default palette: identity mapping (logical N → physical N)
    for (int i = 0; i < 16; i++) {
        board_.vidproc.write_palette((i << 4) | (((i & 0x07) ^ 0x07) << 1));
    }

    register_bus_chips(board_);

    system_ready_ = true;
    printf("%s: System initialized (%dKB RAM)\n", Traits::name,
           Traits::ram_size / 1024);
    return true;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::shutdown() { system_ready_ = false; }

template<BBCMasterVariant V>
void BBCMasterSystem<V>::reset() {
    if (!system_ready_) return;
    pins_ = board_.w65c02.reset(pins_);
    board_.reset_chips();
    board_.sys_via.reset();
    board_.user_via.reset();
    board_.sys_via.interrupt_bit = BUS_IRQ_BIT;
    board_.user_via.interrupt_bit   = BUS_IRQ_BIT;
    rom_select_ = 0;
    acccon_ = 0;
    shadow_active_ = false;
    std::memset(key_matrix_, 0, sizeof(key_matrix_));
    any_key_pressed_ = false;
    addressable_latch_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

// CS-tick architecture (same as Model B):
//   resolve() → service() handles RAM/ROM via page table.
//   CRTC, VIDPROC, System VIA, User VIA self-select via CS field.
//   ROM select ($FE30) handled as system glue after CS dispatch.

template<BBCMasterVariant V>
void BBCMasterSystem<V>::tick() {
    bus_state_t s = pins_;

    // ---- CRTC character clock (1 MHz = every other CPU cycle) ----
    crtc_divider_++;
    if (crtc_divider_ >= 2) {
        crtc_divider_ = 0;
        board_.crtc.tick();  // character clock only (void)
    }

    // ---- Propagate VIA interrupt state from previous cycle ----
    if (board_.sys_via.ifr & board_.sys_via.ier & 0x7F) {
        if (board_.sys_via.interrupt_bit != 0)
            BUS_CLR_BIT(s, board_.sys_via.interrupt_bit);
    }
    if (board_.user_via.ifr & board_.user_via.ier & 0x7F) {
        if (board_.user_via.interrupt_bit != 0)
            BUS_CLR_BIT(s, board_.user_via.interrupt_bit);
    }

    // ---- CPU PHI2 — address/R#W valid on bus ----
    s = board_.w65c02.template tick<WDC_65C02::Phase::PHI2>(s);

    // ---- Address decode + flat-mem service + MMIO self-dispatch ----
    s = bus_.resolve(s);
    s = bus_.service(s);

    // VIA ticks: advance timers (every cycle) + CS-gated register access
    s = board_.sys_via.tick(s);
    s = board_.user_via.tick(s);

    // CRTC register access (CS-gated, no character clock — already ticked above)
    if (board_.crtc.is_cs_selected(s)) {
        s = BUS_GET_BIT(s, BUS_RW_BIT)
            ? board_.crtc.on_bus_read(s) : board_.crtc.on_bus_write(s);
        board_.crtc.mark_cs_serviced(s);
    }

    // Video ULA register access (CS-gated, write-only)
    s = board_.vidproc.tick(s);

    // ROM select register ($FE30) — system glue, not a chip.
    if (unlikely(!BUS_GET_BIT(s, BUS_RW_BIT))) {
        uint16_t addr = BUS_GET_ADDR(s);
        if (addr == bbc_constants::ROM_SELECT_REG) {
            rom_select_ = BUS_GET_DATA(s) & 0x0F;
            update_paged_rom();
        }
    }

    // ---- NMI edge detection ----
    board_.w65c02.sample_nmi_pin(s);

    // ---- CPU PHI1 ----
    s = board_.w65c02.template tick<WDC_65C02::Phase::PHI1>(s);

    BUS_SET_BIT(s, BUS_RW_BIT);
    pins_ = s;
    total_cycles_++;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::run_frame() {
    if (!system_ready_ || !video_port_) return;

    // Stream-driven: VIDPROC drives FrameEnd via CRTC timing
    auto& output = video_port_->output();
    const int frames = (speed_multiplier_ > 1.0) ? static_cast<int>(speed_multiplier_) : 1;
    for (int f = 0; f < frames; f++) {
        while (!output.frame_ended()) {
            tick();
        }
        video_port_->swap_frame();
    }
    audio_thread_.signal_progress(total_cycles_);
    tick_peripherals();
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<BBCMasterVariant V>
bool BBCMasterSystem<V>::load_file(const char* /*filepath*/) {
    // TODO: support SSD, DSD, UEF formats
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<BBCMasterVariant V>
uint32_t BBCMasterSystem<V>::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    // TODO: SN76489 audio output
    return 0;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::set_audio_sample_rate(int /*sample_rate_hz*/) {
    // TODO: configure audio sample rate
}

// ============================================================================
// INPUT
// ============================================================================

template<BBCMasterVariant V>
void BBCMasterSystem<V>::handle_keyboard_event(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: BBC keyboard matrix mapping (same layout as Model B)
}


// ============================================================================
// INTERNAL HELPERS (stubs)
// ============================================================================

template<BBCMasterVariant V>
void BBCMasterSystem<V>::tick_cpu() {
    // Unused — tick() does inline PHI2/PHI1 dispatch
}

template<BBCMasterVariant V>
bus_state_t BBCMasterSystem<V>::sheila_tick(bus_state_t s) {
    // Unused — CS dispatch handles SHEILA chips inline in tick()
    return s;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::configure_bus_memory_map() {
    // apply() establishes the default linear map from the manifest:
    //   $00-$7F: RAM (read+write)
    //   $80-$BF: Paged ROM bank 0 (read) — clipped from 256 KB pool
    //   $C0-$FF: OS ROM (read)
    //
    // With EnableCs=true, Phase 3 creates MaskedSubTable entries for
    // SHEILA ($FE) with regions for CRTC, VIDPROC, two VIAs.
    board_.apply(bus_);

    // Unmap FRED ($FC) and JIM ($FD) — no hardware on these pages yet
    bus_.map_no_chip_selected(0, 0xFC, 2);

    // Map currently selected paged ROM bank to $80-$BF
    update_paged_rom();
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::update_paged_rom() {
    board_.select_bank_at(bus_, 0, BTraits::kManifest.template find<ROMChip>(),
                           rom_select_ & 0x0F, 0x80);
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::update_shadow_mapping() {
    // TODO: apply ACCCON register to shadow screen RAM mapping
}

template<BBCMasterVariant V>
bool BBCMasterSystem<V>::load_roms() {
    return board_.load_roms("bbc");
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::update_key_matrix(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: same keyboard matrix as Model B
}

template<BBCMasterVariant V>
uint8_t BBCMasterSystem<V>::scan_keyboard(uint8_t /*column*/) const {
    return 0xFF;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::crtc_display_char(uint16_t ma, uint8_t ra, bool cursor) {
    board_.vidproc.display_char(ma, ra, cursor,
                                        board_.crtc.regs_[R9_MAX_SCANLINE]);
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::crtc_vsync() {
    board_.vidproc.vsync();
    board_.sys_via.ifr |= MOS6522_IFR_CA1;
}

template<BBCMasterVariant V>
void BBCMasterSystem<V>::crtc_hsync() {
    // HSYNC — no action needed for basic rendering
}

template<BBCMasterVariant V>
uint8_t BBCMasterSystem<V>::sys_via_port_a_read(void* /*ctx*/, uint8_t /*output*/) {
    return 0xFF;
}

template<BBCMasterVariant V>
uint8_t BBCMasterSystem<V>::sys_via_port_b_read(void* /*ctx*/, uint8_t /*output*/) {
    return 0xFF;
}

// ============================================================================
// System VIA Port B Write — Addressable Latch + SN76489 Trigger
// ============================================================================
//
// Same mechanism as Model B: 74LS259 addressable latch driven by Port B.
//   PB0-PB2: latch address (0-7)
//   PB3:     latch data (1 = set, 0 = clear)
//
// Latch bit 0 = SN76489 /WE.  SN76489 data comes from VIA Port A.
// We detect the falling edge and enqueue a timestamped write.

template<BBCMasterVariant V>
void BBCMasterSystem<V>::sys_via_port_b_write(void* context, uint8_t data) {
    auto* sys = static_cast<BBCMasterSystem*>(context);

    uint8_t latch_addr = data & 0x07;
    bool latch_data = (data >> 3) & 0x01;
    uint8_t old_latch = sys->addressable_latch_;

    if (latch_data)
        sys->addressable_latch_ |= (1 << latch_addr);
    else
        sys->addressable_latch_ &= ~(1 << latch_addr);

    // SN76489 /WE falling edge: old bit 0 was HIGH, now LOW
    if (latch_addr == 0 && (old_latch & 0x01) && !latch_data) {
        if (sys->psg_adapter_) {
            uint8_t psg_data = sys->board_.sys_via.regs_[PORTA];
            sys->psg_adapter_->cmd_queue().push_write(sys->total_cycles_, 0, psg_data);
        }
    }
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class BBCMasterSystem<BBCMasterVariant::MODEL_B_PLUS>;
template class BBCMasterSystem<BBCMasterVariant::MASTER_128>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bbc_bplus_descriptor, [] {
    return std::make_unique<BBCMasterSystem<BBCMasterVariant::MODEL_B_PLUS>>();
});

REGISTER_SYSTEM(bbc_master_descriptor, [] {
    return std::make_unique<BBCMasterSystem<BBCMasterVariant::MASTER_128>>();
});
