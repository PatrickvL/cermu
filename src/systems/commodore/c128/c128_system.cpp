/*
 * c128_system.cpp — Commodore 128 system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "systems/commodore/c128/c128_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

static HardwareTraits create_c128_hardware_traits() {
    HardwareTraits traits = {};

    // Display — VIC-IIe 40-column (default; VDC 80-col is secondary)
    traits.display.native_width    = c128_constants::VIC_DISPLAY_WIDTH_PAL;
    traits.display.native_height   = c128_constants::VIC_DISPLAY_HEIGHT_PAL;
    traits.display.visible_width   = c128_constants::VIC_DISPLAY_WIDTH_PAL;
    traits.display.visible_height  = c128_constants::VIC_DISPLAY_HEIGHT_PAL;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = true;

    // Audio — SID
    traits.audio.format            = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz    = c128_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "MOS 6581 SID";

    // Timing — PAL default (same VIC-II timing as C64)
    traits.timing.cpu_frequency_hz = c128_constants::CPU_FREQ_1MHZ_PAL;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = c128_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard         = VideoStandard::PAL;

    // Memory options
    traits.memory_options.push_back({"128KB RAM (Standard)", 131072, 0, true});

    // Region options
    traits.video_standard_configs.push_back({
        "PAL", VideoStandard::PAL, traits.timing, true
    });

    SystemTiming ntsc = traits.timing;
    ntsc.cpu_frequency_hz = c128_constants::CPU_FREQ_1MHZ_NTSC;
    ntsc.target_fps = 60;
    ntsc.cycles_per_frame = c128_constants::CYCLES_PER_FRAME_NTSC;
    ntsc.standard = VideoStandard::NTSC;
    traits.video_standard_configs.push_back({
        "NTSC", VideoStandard::NTSC, ntsc, false
    });

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor c128_descriptor = {
    "Commodore 128", "C128",
    "Commodore 128 (1985) — CSG 8502 + Z80, VIC-IIe, SID, 128KB RAM",
    "c128", {"C128", "Commodore128", "CBM128"},
    nullptr,
    create_c128_hardware_traits(),
    nullptr,
    "Commodore", 1985, "CSG 8502 + Z80", SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

C128System::C128System()
    : CommodoreSystem()
    , pins_(C128_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_c128_hardware_traits();
    cycles_per_frame_ = c128_constants::CYCLES_PER_FRAME_PAL;
}

C128System::~C128System() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

const SystemDescriptor& C128System::get_descriptor() const {
    return c128_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool C128System::apply_configuration() {
    // TODO: apply region, SID model, etc.
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool C128System::initialize() {
    printf("C128: Initializing system\n");
    register_board(&board_);

    // Bus pull-up defaults — same as C64 (BA, CNT, FLAG, data lines)
    default_state_ = CSG8502::default_bus_state()
                   | BUS_BIT(BUS_BA_BIT) | BUS_BIT(BUS_CNT_BIT)
                   | BUS_BIT(BUS_FLAG_BIT) | BUS_DATA_MASK;
    pins_ = default_state_;

    board_.bind_chipset();
    board_.create_chips(&pins_);
    board_.apply(bus_);

    basic_lo_rom_ = board_.find<ROMChip>();
    basic_hi_rom_ = board_.find<ROMChip>(1);
    editor_rom_   = board_.find<ROMChip>(2);
    kernal_rom_   = board_.find<ROMChip>(3);
    char_rom_     = board_.find<ROMChip>(4);
    vdc_vram_     = board_.find<RAMChip>(1);

    configure_bus_memory_map();
    if (!load_roms()) {
        printf("C128: Warning — ROMs not loaded\n");
    }

    // VIC-IIe — initialize with PAL traits (MOS8566)
    auto& vic_iie = board_.video();
    auto& sid     = board_.sound();
    auto& cia1    = board_.io();
    auto& cia2    = board_.chips().cia2;

    vic_iie.init(vicii_base_t::memory_bank_change);
    vic_iie.colorram = &board_.chips().colorram;

    // VIC-IIe memory read callback — routes through MemoryBus viewer 1
    vic_iie.bus.bus = nullptr;
    vic_iie.bus.bank_change = nullptr;
    vic_iie.bus.mem_read = [](void* ctx, bus_state_t bus, uint16_t addr) -> bus_state_t {
        auto* sys = static_cast<C128System*>(ctx);
        uint8_t data = sys->bus_.peek_byte(addr, kViewerVicII);
        BUS_SET_DATA(bus, data);
        return bus;
    };
    vic_iie.bus.mem_read_ctx = this;

    // CIA2 Port A → VIC-IIe bank selection
    cia2.port_a_change_callback = [](void* ctx, uint8_t value) {
        auto* sys = static_cast<C128System*>(ctx);
        vicii_base_t::memory_bank_change(&sys->board_.video(), value & 0x03);
    };
    cia2.port_a_callback_context = this;

    // CIA2 interrupt line → NMI
    cia2.configured_interrupt_bit = BUS_NMI_BIT;

    // SID — initialize
    sid.init();
    {
        float cpu_clock = static_cast<float>(c128_constants::CPU_FREQ_1MHZ_PAL);
        sid.set_cpu_clock(cpu_clock);
        sid.set_timing(true);  // PAL
    }

    register_bus_chips(board_);

    // Initialize CPU — reset vector will come from Kernal ROM
    board_.cpu().init();
    board_.cpu().init_io_port();
    board_.cpu().reset();

    // Video output — VIC-IIe drives composite video
    video_port_ = std::make_unique<CompositeVideoPort>();
    vic_iie.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    system_ready_ = true;
    printf("C128: System initialized\n");
    return true;
}

void C128System::shutdown() { system_ready_ = false; }

void C128System::reset() {
    pins_ = board_.cpu().reset(pins_);
    board_.reset_chips();
    cpu_mode_ = CPUMode::MODE_8502;
    c64_mode_ = false;
    std::memset(mmu_pcr_, 0, sizeof(mmu_pcr_));
    mmu_cr_ = 0; mmu_mcr_ = 0; mmu_rcr_ = 0;
    mmu_p0_[0] = 0; mmu_p0_[1] = 0;
    mmu_p1_[0] = 0; mmu_p1_[1] = 1;
    reset_load_state();
}

// ============================================================================
// EXECUTION (stub — to be filled in)
// ============================================================================

void C128System::tick() {
    total_cycles_++;

    auto& cpu     = board_.cpu();
    auto& vic_iie = board_.video();
    auto& sid     = board_.sound();
    auto& cia1    = board_.io();
    auto& cia2    = board_.chips().cia2;

    // Start each cycle with pull-up defaults
    bus_state_t s = default_state_;
    BUS_SET_ADDR(s, BUS_GET_ADDR(pins_));
    BUS_SET_DATA(s, BUS_GET_DATA(pins_));

    // PHASE 1: VIC-IIe PHI1 — g-access read, pixel sequencing
    s = vic_iie.tick_phi1(s);

    // PHASE 1.5: CIA PHI2 — apply pending interrupt lines before CPU
    s = cia2.tick_phi2(s);
    s = cia1.tick_phi2(s);

    // BA→RDY wiring (direct bit test + set/clear)
    if (BUS_GET_BIT(s, BUS_BA_BIT))
        BUS_SET_BIT(s, BUS_RDY_BIT);
    else
        BUS_CLR_BIT(s, BUS_RDY_BIT);

    // PHASE 2: CPU PHI2 — instruction execution
    s = cpu.tick<CSG8502::Phase::PHI2>(s);

    // PHASE 3: Memory service — AEC determines CPU vs VIC-IIe bus ownership
    {
        const bool is_write = !BUS_GET_BIT(s, BUS_RW_BIT);
        const size_t viewer = (is_write || BUS_GET_BIT(s, BUS_AEC_BIT))
                                  ? kViewerCpu : kViewerVicII;
        s = bus_.tick(s, viewer);
    }

    // NMI edge detection
    cpu.sample_nmi_pin(s);

    // PHASE 3.1: VIC-IIe PHI2 — c/p/s-access data delivery
    vic_iie.tick_phi2(s);

    // PHASE 3.5: CIA PHI1 — timer counting, TOD, interrupt generation
    s = cia2.tick_phi1(s);
    s = cia1.tick_phi1(s);

    // PHASE 4: CPU PHI1 — prepare next fetch
    s = cpu.tick<CSG8502::Phase::PHI1>(s);

    // Restore R/W line to read mode
    BUS_SET_BIT(s, BUS_RW_BIT);

    // PHASE 5: SID — sound generation
    s = sid.tick(s);

    pins_ = s;
}

void C128System::run_frame() {
    if (!system_ready_ || !video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
    check_deferred_load();
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

uint32_t C128System::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    // TODO: SID audio output
    return 0;
}

void C128System::set_audio_sample_rate(int rate) {
    audio_sample_rate_ = rate;
}

// ============================================================================
// INPUT
// ============================================================================

void C128System::handle_keyboard_event(SDL_Keycode /*key*/, bool /*pressed*/) {
    // TODO: C128 keyboard matrix (11 columns × 8 rows)
}

// ============================================================================
// COMMODORE SYSTEM HOOKS
// ============================================================================

bool C128System::is_basic_ready() const {
    // TODO: check C128 BASIC 7.0 READY state
    return false;
}

commodore_load_context_t C128System::build_load_context() {
    // TODO: build load context with C128 memory callbacks
    return {};
}

void C128System::inject_keys(const char* /*str*/) {
    // TODO: inject into C128 keyboard buffer
}

// ============================================================================
// INTERNAL HELPERS (stubs)
// ============================================================================

void C128System::configure_bus_memory_map() {
    // C128 default boot config (MMU CR = 0x00):
    //   $0000-$3FFF : RAM bank 0
    //   $4000-$7FFF : BASIC lo ROM
    //   $8000-$BFFF : BASIC hi ROM
    //   $C000-$CFFF : Editor ROM
    //   $D000-$DFFF : I/O (TODO: register MMIO handlers for VIC, SID, CIA, MMU, VDC)
    //   $E000-$FFFF : Kernal ROM
    //
    // Chip IDs (4 KB pages, bank_size = chip size for ROMs):
    //   Slot 0 (RAM 128KB, bank_size=65536): 2 banks → IDs 0-1
    //   Slot 1 (BASIC lo 16KB):  1 bank → ID 2
    //   Slot 2 (BASIC hi 16KB): 1 bank → ID 3
    //   Slot 3 (Editor 4KB):    1 bank → ID 4
    //   Slot 4 (Kernal 8KB):    1 bank → ID 5
    //   Slot 5 (Char ROM 8KB):  1 bank → ID 6
    //   Slot 6 (VDC VRAM 16KB): 1 bank → ID 7

    using ChipId    = Bus::ChipId;
    using WriteId   = Bus::WriteChipId;

    constexpr ChipId  kRamBank0  = ChipId(0);
    constexpr WriteId kRamBank0W = WriteId(0);
    constexpr ChipId  kBasicLo   = ChipId(2);
    constexpr ChipId  kBasicHi   = ChipId(3);
    constexpr ChipId  kEditor    = ChipId(4);
    constexpr ChipId  kKernal    = ChipId(5);
    constexpr ChipId  kCharRom   = ChipId(6);

    // ── CPU viewer (viewer 0) ────────────────────────────────────────
    // apply() already mapped RAM bank 0 at $0000-$FFFF.
    // Overlay ROM chips at their proper addresses for reads;
    // writes always go to underlying RAM.

    // $4000-$7FFF → BASIC lo ROM (read), RAM (write)
    bus_.set_page(kViewerCpu, 0x4, kBasicLo, kRamBank0W);
    bus_.set_page(kViewerCpu, 0x5, kBasicLo, kRamBank0W);
    bus_.set_page(kViewerCpu, 0x6, kBasicLo, kRamBank0W);
    bus_.set_page(kViewerCpu, 0x7, kBasicLo, kRamBank0W);

    // $8000-$BFFF → BASIC hi ROM (read), RAM (write)
    bus_.set_page(kViewerCpu, 0x8, kBasicHi, kRamBank0W);
    bus_.set_page(kViewerCpu, 0x9, kBasicHi, kRamBank0W);
    bus_.set_page(kViewerCpu, 0xA, kBasicHi, kRamBank0W);
    bus_.set_page(kViewerCpu, 0xB, kBasicHi, kRamBank0W);

    // $C000-$CFFF → Editor ROM (read), RAM (write)
    bus_.set_page(kViewerCpu, 0xC, kEditor, kRamBank0W);

    // $D000-$DFFF → RAM for now (I/O dispatch TODO)
    // (apply() already mapped RAM here)

    // $E000-$FFFF → Kernal ROM (read), RAM (write)
    bus_.set_page(kViewerCpu, 0xE, kKernal, kRamBank0W);
    bus_.set_page(kViewerCpu, 0xF, kKernal, kRamBank0W);

    // ── VIC-IIe viewer (viewer 1) ────────────────────────────────────
    // VIC-IIe sees 64KB of RAM bank 0 by default, with character ROM
    // ghosted at $1000-$1FFF and $9000-$9FFF (bank 0 and 2)
    for (size_t page = 0; page < 16; ++page)
        bus_.set_read_page(kViewerVicII, page, kRamBank0);

    // Character ROM ghost at $1000 and $9000 (VIC bank 0 and bank 2)
    bus_.set_read_page(kViewerVicII, 0x1, kCharRom);
    bus_.set_read_page(kViewerVicII, 0x9, kCharRom);
}

bool C128System::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("c128", rom_root, sizeof(rom_root))) {
        printf("C128: ROM root not found — cannot load ROMs\n");
        return false;
    }
    return board_.load_roms(rom_root, "C128");
}

void C128System::mmu_write(uint16_t /*addr*/, uint8_t /*data*/) {
    // TODO: 8722 MMU register writes → update bank configuration
}

uint8_t C128System::mmu_read(uint16_t /*addr*/) {
    // TODO: 8722 MMU register reads
    return 0;
}

void C128System::update_bank_config() {
    // TODO: apply MMU configuration register to page table mapping
}

void C128System::switch_cpu_mode(CPUMode /*mode*/) {
    // TODO: switch between 8502 and Z80
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(c128_descriptor, [] {
    return std::make_unique<C128System>();
});
