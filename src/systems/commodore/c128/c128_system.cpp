/*
 * c128_system.cpp — Commodore 128 system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "core/cermu.hpp"
#include "systems/commodore/c128/c128_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include "devices/keyboard/commodore_keyboard_device.hpp"
#include <cstring>
#include <cstdio>
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

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
// CIA1 KEYBOARD MATRIX CALLBACKS
// ============================================================================
//
// The C128 scans an 11×8 keyboard matrix through CIA1:
//   Columns 0-7:  selected by CIA1 Port A output (active-low, same as C64)
//   Columns 8-10: selected by VIC-IIe register $D02F bits 0-2 (active-low)
//
// Forward scan: KERNAL writes to Port A → reads Port B (row contacts)
// Reverse scan: some routines write to Port B → read Port A (column contacts)
//
// Reverse scan only returns columns 0-7 (8-bit Port A); extended columns
// are not observable in reverse direction (matching VICE behavior).

/// CIA1 Port A read — reverse scan: given row select from Port B, return column contacts.
uint8_t C128System::c128_cia1_port_a_read(void* context, uint8_t /*port_a_output*/) {
    auto* sys = static_cast<C128System*>(context);
    if (!sys->keyboard_) return 0xFF;

    // Port B output selects rows (active-low); we narrow to columns 0-7
    uint8_t port_b_output = sys->board_.cia1.port_b_value;
    uint8_t row_select = ~port_b_output;
    uint8_t col_state = 0xFF;
    for (int row = 0; row < 8; row++) {
        if (row_select & (1 << row)) {
            // row_open_contacts[row] holds column bitmask; truncate to 8 bits
            // (reverse scan only returns columns 0-7)
            col_state &= static_cast<uint8_t>(sys->keyboard_->row_open_contacts[row]);
        }
    }
    return col_state;
}

/// CIA1 Port B read — forward scan: given column select from Port A + extended mask,
/// return row contacts.
uint8_t C128System::c128_cia1_port_b_read(void* context, uint8_t /*port_b_output*/) {
    auto* sys = static_cast<C128System*>(context);
    if (!sys->keyboard_) return 0xFF;

    uint8_t port_a_value = sys->board_.cia1.port_a_value;
    uint8_t column_select = ~port_a_value;
    uint8_t row_state = 0xFF;

    // Standard columns 0-7 via CIA Port A
    for (int col = 0; col < 8; col++) {
        if (column_select & (1 << col)) {
            row_state &= static_cast<uint8_t>(sys->keyboard_->col_open_contacts[col]);
        }
    }

    // Extended columns 8-10 via VIC-IIe register $D02F (active-low bits 0-2).
    // On C128 hardware, the VIC-IIe drives these signals directly to the
    // keyboard matrix; we read the register value each scan.
    uint8_t ext_mask = sys->board_.vic_iie.regs_[0x2F];
    for (int i = 0; i < 3; i++) {
        if (!(ext_mask & (1 << i))) {
            row_state &= static_cast<uint8_t>(sys->keyboard_->col_open_contacts[8 + i]);
        }
    }

    return row_state;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool C128System::initialize() {
    log_info("C128: Initializing system\n");
    register_board(&board_);

    // Bus pull-up defaults — same as C64 (BA, CNT, FLAG, data lines)
    default_state_ = CSG8502::default_bus_state()
                   | BUS_BIT(BUS_BA_BIT) | BUS_BIT(BUS_CNT_BIT)
                   | BUS_BIT(BUS_FLAG_BIT) | BUS_DATA_MASK;
    pins_ = default_state_;

    {   size_t slot_idx_ = 0;
        C128_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_)
    }
    board_.create_chips(&pins_);
    board_.apply(bus_);

    basic_lo_rom_    = &board_.basic_lo;
    basic_hi_rom_    = &board_.basic_hi;
    editor_rom_      = &board_.editor_rom;
    kernal_rom_      = &board_.kernal_rom;
    char_rom_        = &board_.char_rom;
    c64_basic_rom_   = &board_.c64_basic;
    c64_kernal_rom_  = &board_.c64_kernal;
    vdc_vram_        = &board_.vdc_vram;

    configure_bus_memory_map();
    init_io_dispatch();
    if (!load_roms()) {
        log_info("C128: Warning — ROMs not loaded\n");
    }

    // VIC-IIe — initialize with PAL traits (MOS8566)
    auto& vic_iie = board_.vic_iie;
    auto& sid     = board_.sid;
    auto& cia1    = board_.cia1;
    auto& cia2    = board_.cia2;

    vic_iie.init(vicii_base_t::memory_bank_change);
    vic_iie.colorram = &board_.colorram;

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
        vicii_base_t::memory_bank_change(&sys->board_.vic_iie, value & 0x03);
    };
    cia2.port_a_callback_context = this;

    // CIA1 interrupt line → IRQ (keyboard scan, cursor blink, timer events)
    cia1.configured_interrupt_bit = BUS_IRQ_BIT;

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
    board_.csg8502.init();
    board_.csg8502.init_io_port();
    board_.csg8502.bank_change_fn = cpu_banking_callback;
    board_.csg8502.bank_change_ctx = this;
    board_.csg8502.reset();

    // Video output — VIC-IIe drives composite video (primary, 40-col)
    video_port_ = std::make_unique<CompositeVideoPort>();
    vic_iie.set_video_out(&video_port_->output());

    // Compute back porch for signal→framebuffer reconstruction.
    // Back porch = distance from HSync falling edge to first visible pixel.
    const uint16_t ppl = MOS8566_traits.cycles_per_line * 8;
    const int back_porch = (int(MOS8566_traits.first_visible_x_coord) - int(MOS8566_traits.hsync_end) + ppl) % ppl;
    video_port_->bind_display(nullptr, nullptr,
                              c128_constants::VIC_DISPLAY_WIDTH_PAL, back_porch);
    video_port_->bind_frame_output(&last_frame_data_);

    // VDC (8563) — 80-column RGBI output (secondary display)
    auto& vdc = board_.vdc;
    vdc.init();
    vdc.bind_vram(vdc_vram_->data(), static_cast<uint32_t>(vdc_vram_->size_bytes()));

    // Program VDC with power-on defaults matching the C128 KERNAL's
    // initialization sequence.  Without these the VDC's zero-initialized
    // registers produce degenerate timing (R0=0 → instant line wraps)
    // that overflows the signal buffer before a FrameEnd is ever emitted.
    {
        using namespace fam6845::reg;
        auto& r = vdc.regs_;
        r[R0_HTOTAL]       = 126;  //  127 character clocks per line
        r[R1_HDISPLAYED]   =  80;  //   80 visible characters
        r[R2_HSYNC_POS]    = 102;  //  HSYNC at char 102
        r[R3_SYNC_WIDTHS]  = 0x49; //  HSYNC width 9, VSYNC width 4
        r[R4_VTOTAL]       =  32;  //   33 character rows total
        r[R5_VADJUST]      =   0;
        r[R6_VDISPLAYED]   =  25;  //   25 visible rows
        r[R7_VSYNC_POS]    =  29;  //  VSYNC at row 29
        r[R8_MODE_CTRL]    = 0x00; //  Non-interlaced
        r[R9_MAX_SCANLINE] =   7;  //    8 scan lines per character row
        r[R10_CURSOR_START]= 0x20; //  Cursor: no blink, start line 0
        r[R11_CURSOR_END]  =   7;  //  Cursor end at line 7
    }

    // VDC video output: not wired yet.  On the real C128 the VDC
    // drives a separate RGBI connector; a dual-display pipeline is
    // needed before pixel output can be enabled.  Register I/O and
    // DRAM operations work without it.

    // ── Keyboard matrix ──────────────────────────────────────────────
    keyboard_ = new commodore_keyboard_t();
    if (!keyboard_->init(&c128_keyboard_config)) {
        log_info("C128: ERROR — Failed to create keyboard\n");
        delete keyboard_;
        keyboard_ = nullptr;
        return false;
    }

    // Create keyboard mapper (host layout → C128 matrix)
    keyboard_mapper_.reset(create_c128_keyboard_mapper(keyboard_));

    // Wire CIA1 port read callbacks for keyboard matrix scanning.
    // Port A read = reverse scan (Port B output selects rows → return column contacts)
    // Port B read = forward scan (Port A output selects columns → return row contacts)
    cia1.port_a_read_callback = c128_cia1_port_a_read;
    cia1.port_a_read_context  = this;
    cia1.port_b_read_callback = c128_cia1_port_b_read;
    cia1.port_b_read_context  = this;
    log_info("C128: Keyboard matrix wired to CIA1\n");

    system_ready_ = true;
    log_info("C128: System initialized\n");
    return true;
}

void C128System::shutdown() {
    keyboard_mapper_.reset();
    delete keyboard_;
    keyboard_ = nullptr;
    system_ready_ = false;
}

void C128System::reset() {
    pins_ = board_.csg8502.reset(pins_);
    board_.reset_chips();
    cpu_mode_ = CPUMode::MODE_8502;
    c64_mode_ = false;
    cpu_port_bits_ = 0x07;
    if (keyboard_) keyboard_->reset();
    update_bank_config();
    reset_load_state();
}

// ============================================================================
// EXECUTION (stub — to be filled in)
// ============================================================================

void C128System::tick() {
    total_cycles_++;

    auto& cpu     = board_.csg8502;
    auto& vic_iie = board_.vic_iie;
    auto& sid     = board_.sid;
    auto& cia1    = board_.cia1;
    auto& cia2    = board_.cia2;

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

    // PHASE 3: Address decode + buffer service + MMIO self-dispatch
    {
        const uint16_t addr = BUS_GET_ADDR(s);
        const bool is_write = !BUS_GET_BIT(s, BUS_RW_BIT);

        // $FF00-$FF04: MMU configuration registers (always visible)
        // Intercepted before bus resolve because they exist outside the
        // normal I/O page and must be accessible regardless of bank config.
        if (addr >= 0xFF00 && addr <= 0xFF04) {
            if (is_write) {
                const uint8_t data = BUS_GET_DATA(s);
                if (addr == 0xFF00) {
                    board_.mmu.write_ff00(data);
                } else {
                    board_.mmu.write_ff01_ff04(static_cast<uint8_t>(addr - 0xFF01));
                }
            } else {
                uint8_t data;
                if (addr == 0xFF00) {
                    data = board_.mmu.read_ff00();
                } else {
                    data = board_.mmu.read_ff01_ff04(static_cast<uint8_t>(addr - 0xFF01));
                }
                BUS_SET_DATA(s, data);
            }
        } else {
            const size_t viewer = (is_write || BUS_GET_BIT(s, BUS_AEC_BIT))
                                      ? kViewerCpu : kViewerVicII;

            // 1. resolve() — address decode, embed CS field
            s = bus_.resolve(s, viewer);

            // 2. service() — buffer (RAM/ROM) access, MMIO IDs skipped
            s = bus_.service(s);

            // 3. Self-dispatch — each chip checks is_cs_selected() and handles own I/O
            s = vic_iie.tick_mmio(s);
            s = sid.tick_mmio(s);
            s = board_.colorram.tick_mmio(s);
            s = board_.mmu.tick_mmio(s);
            s = cia1.tick_mmio(s);
            s = cia2.tick_mmio(s);
            s = board_.vdc.tick_mmio(s);
        }

        // If the MMU's bank config was changed, apply it now
        if (unlikely(board_.mmu.bank_config_dirty())) {
            update_bank_config();
        }

        // Check for C64 mode transition (MCR bit 6 latched)
        if (unlikely(!c64_mode_ && board_.mmu.c64_mode_requested())) {
            enter_c64_mode();
        }
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

    // VDC character clock — runs at ~1 MHz (same rate as slow-mode CPU)
    // Disabled in C64 mode (VDC is not accessible).
    if (!c64_mode_)
        board_.vdc.tick();

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

void C128System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (keyboard_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                keyboard_->key_down(ek, false);
            } else {
                keyboard_->key_up(ek, false);
            }
        }
    }
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
    // Chip IDs (4 KB pages, declaration order, bank_size = chip size for ROMs):
    //   main_ram    (128KB, bank_size=64KB): 2 banks → IDs 0-1
    //   basic_lo    ( 16KB):                 1 bank  → ID 2
    //   basic_hi    ( 16KB):                 1 bank  → ID 3
    //   editor_rom  (  4KB):                 1 bank  → ID 4
    //   char_rom    (  8KB):                 1 bank  → ID 5
    //   kernal_rom  (  8KB):                 1 bank  → ID 6
    //   c64_basic   (  8KB):                 1 bank  → ID 7
    //   c64_kernal  (  8KB):                 1 bank  → ID 8
    //   vdc_vram    ( 16KB):                 1 bank  → ID 9

    using ChipId    = Bus::ChipId;
    using WriteId   = Bus::WriteChipId;

    constexpr ChipId  kRamBank0  = ChipId(0);
    constexpr WriteId kRamBank0W = WriteId(0);
    constexpr ChipId  kBasicLo   = ChipId(2);
    constexpr ChipId  kBasicHi   = ChipId(3);
    constexpr ChipId  kEditor    = ChipId(4);
    constexpr ChipId  kCharRom   = ChipId(5);
    constexpr ChipId  kKernal    = ChipId(6);

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

    // $D000-$DFFF → I/O sub-table (default boot config has I/O visible)
    bus_.map_to_indexed_sub(kViewerCpu, 0xD, 0);

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
        log_info("C128: ROM root not found — cannot load ROMs\n");
        return false;
    }
    return board_.load_roms(rom_root, "C128");
}

void C128System::init_io_dispatch() {
    using PT = PackingTraits<C128BusSpec>;

    // Register legacy MMIO handlers (for debug peek/poke)
    (void)bus_.register_handler({&board_.vic_iie,  vicii_base_t::registers_read,  vicii_base_t::registers_write});
    (void)bus_.register_handler({&board_.sid,      mos6581_t::registers_read,     mos6581_t::registers_write});
    (void)bus_.register_handler({&board_.colorram, MOS2114::bus_read,             MOS2114::bus_write});
    (void)bus_.register_handler({&board_.cia1,     mos6526_t::registers_read,     mos6526_t::registers_write});
    (void)bus_.register_handler({&board_.cia2,     mos6526_t::registers_read,     mos6526_t::registers_write});

    // Create indexed sub-table for I/O page ($D000-$DFFF)
    // 4 bits → 16 entries, extracted from address bits 11-8
    const int io_sub = bus_.add_indexed_sub_table(kViewerCpu, 4, 8);
    (void)bus_.add_indexed_sub_table(kViewerVicII, 4, 8);

    // Helper to wrap bus_chip_id into typed ChipId/WriteChipId
    using ChipId  = Bus::ChipId;
    using WriteId = Bus::WriteChipId;
    auto cs_rd = [](uint16_t id) { return ChipId(id); };
    auto cs_wr = [](uint16_t id) { return WriteId(id); };

    const uint16_t idVicIIe = board_.vic_iie.bus_chip_id();
    const uint16_t idSid    = board_.sid.bus_chip_id();
    const uint16_t idColRam = board_.colorram.bus_chip_id();
    const uint16_t idCia1   = board_.cia1.bus_chip_id();
    const uint16_t idCia2   = board_.cia2.bus_chip_id();

    const auto no_chip_rd = ChipId(PT::kNoChipSelected);
    const auto no_chip_wr = WriteId(PT::kNoChipSelectedWrite);

    // VIC-IIe: $D000-$D3FF (sub-entries 0-3)
    for (int i = 0; i < 4; ++i)
        bus_.set_indexed_entry(kViewerCpu, io_sub, i, cs_rd(idVicIIe), cs_wr(idVicIIe));

    // SID: $D400-$D4FF (sub-entry 4)
    bus_.set_indexed_entry(kViewerCpu, io_sub, 4, cs_rd(idSid), cs_wr(idSid));

    // MMU (8722): $D500-$D5FF (sub-entry 5)
    const uint16_t idMmu = board_.mmu.bus_chip_id();
    bus_.set_indexed_entry(kViewerCpu, io_sub, 5, cs_rd(idMmu), cs_wr(idMmu));

    // VDC (8563): $D600-$D6FF (sub-entry 6)
    const uint16_t idVdc = board_.vdc.bus_chip_id();
    bus_.set_indexed_entry(kViewerCpu, io_sub, 6, cs_rd(idVdc), cs_wr(idVdc));

    // $D700: unused
    bus_.set_indexed_entry(kViewerCpu, io_sub, 7, no_chip_rd, no_chip_wr);

    // Color RAM: $D800-$DBFF (sub-entries 8-11)
    for (int i = 8; i < 12; ++i)
        bus_.set_indexed_entry(kViewerCpu, io_sub, i, cs_rd(idColRam), cs_wr(idColRam));

    // CIA1: $DC00-$DCFF (sub-entry 12)
    bus_.set_indexed_entry(kViewerCpu, io_sub, 12, cs_rd(idCia1), cs_wr(idCia1));

    // CIA2: $DD00-$DDFF (sub-entry 13)
    bus_.set_indexed_entry(kViewerCpu, io_sub, 13, cs_rd(idCia2), cs_wr(idCia2));

    // I/O 1: $DE00-$DEFF (sub-entry 14) — expansion port
    bus_.set_indexed_entry(kViewerCpu, io_sub, 14, no_chip_rd, no_chip_wr);

    // I/O 2: $DF00-$DFFF (sub-entry 15) — expansion port
    bus_.set_indexed_entry(kViewerCpu, io_sub, 15, no_chip_rd, no_chip_wr);

    log_info("C128: I/O dispatch initialized (CS-tick, VIC-IIe=%u SID=%u ColRAM=%u CIA1=%u CIA2=%u VDC=%u MMU=%u)\n",
           idVicIIe, idSid, idColRam, idCia1, idCia2, idVdc, idMmu);
}

void C128System::update_bank_config() {
    // Apply MMU configuration to CPU viewer page tables.
    // In C128 mode: only the MMU CR register controls banking.
    // In C64 mode:  processor port bits 0-2 control banking (like C64 PLA).

    auto& mmu = board_.mmu;
    mmu.acknowledge_bank_config();

    using ChipId  = Bus::ChipId;
    using WriteId = Bus::WriteChipId;

    // Chip IDs (from manifest declaration order, bank_size splits):
    //   main_ram    (128KB, bank_size=64KB): bank 0 → ID 0, bank 1 → ID 1
    //   basic_lo    ( 16KB):                 ID 2
    //   basic_hi    ( 16KB):                 ID 3
    //   editor_rom  (  4KB):                 ID 4
    //   char_rom    (  8KB):                 ID 5
    //   kernal_rom  (  8KB):                 ID 6
    //   c64_basic   (  8KB):                 ID 7
    //   c64_kernal  (  8KB):                 ID 8
    constexpr ChipId  kRamBank0    = ChipId(0);
    constexpr ChipId  kRamBank1    = ChipId(1);
    constexpr WriteId kRamBank0W   = WriteId(0);
    constexpr WriteId kRamBank1W   = WriteId(1);
    constexpr ChipId  kBasicLo     = ChipId(2);
    constexpr ChipId  kBasicHi     = ChipId(3);
    constexpr ChipId  kEditor      = ChipId(4);
    constexpr ChipId  kCharRom     = ChipId(5);
    constexpr ChipId  kKernal      = ChipId(6);
    constexpr ChipId  kC64Basic    = ChipId(7);
    constexpr ChipId  kC64Kernal   = ChipId(8);

    if (c64_mode_) {
        // ── C64 compatibility mode ──────────────────────────────────
        // Processor port bits 0-2 control banking (same as C64 PLA).
        // Writes always go to RAM bank 0.
        const uint8_t port = cpu_port_bits_;
        const bool loram  = (port & 0x01) != 0;
        const bool hiram  = (port & 0x02) != 0;
        const bool charen = (port & 0x04) != 0;

        // $0000-$3FFF: always RAM bank 0
        for (size_t p = 0; p < 4; ++p)
            bus_.set_page(kViewerCpu, p, kRamBank0, kRamBank0W);

        // $4000-$7FFF: always RAM in C64 mode
        for (size_t p = 4; p < 8; ++p)
            bus_.set_page(kViewerCpu, p, kRamBank0, kRamBank0W);

        // $8000-$9FFF: always RAM in C64 mode
        bus_.set_page(kViewerCpu, 0x8, kRamBank0, kRamBank0W);
        bus_.set_page(kViewerCpu, 0x9, kRamBank0, kRamBank0W);

        // $A000-$BFFF: C64 BASIC ROM when LORAM=1 && HIRAM=1, else RAM
        {
            ChipId rd = (loram && hiram) ? kC64Basic : kRamBank0;
            bus_.set_page(kViewerCpu, 0xA, rd, kRamBank0W);
            bus_.set_page(kViewerCpu, 0xB, rd, kRamBank0W);
        }

        // $C000-$CFFF: always RAM in C64 mode
        bus_.set_page(kViewerCpu, 0xC, kRamBank0, kRamBank0W);

        // $D000-$DFFF: I/O, char ROM, or RAM depending on port bits
        if (hiram || loram) {
            if (charen) {
                // I/O visible
                bus_.map_to_indexed_sub(kViewerCpu, 0xD, 0);
            } else {
                // Character ROM visible
                bus_.set_page(kViewerCpu, 0xD, kCharRom, kRamBank0W);
            }
        } else {
            // All RAM
            bus_.set_page(kViewerCpu, 0xD, kRamBank0, kRamBank0W);
        }

        // $E000-$FFFF: C64 Kernal ROM when HIRAM=1, else RAM
        {
            ChipId rd = hiram ? kC64Kernal : kRamBank0;
            bus_.set_page(kViewerCpu, 0xE, rd, kRamBank0W);
            bus_.set_page(kViewerCpu, 0xF, rd, kRamBank0W);
        }
        return;
    }

    // ── C128 native mode ────────────────────────────────────────────
    // Only the MMU CR register controls ROM/RAM/I/O banking.
    // Processor port bits are NOT consulted.

    // RAM bank from CR bit 6
    const uint8_t bank = mmu.ram_bank();
    const ChipId  ramRd = bank ? kRamBank1 : kRamBank0;
    const WriteId ramWr = bank ? kRamBank1W : kRamBank0W;

    // $0000-$3FFF: always RAM (selected bank)
    for (size_t p = 0; p < 4; ++p)
        bus_.set_page(kViewerCpu, p, ramRd, ramWr);

    // $4000-$7FFF: BASIC LO ROM (bit 1 = 0) or RAM (bit 1 = 1)
    {
        ChipId rd = mmu.basic_lo_rom_enabled() ? kBasicLo : ramRd;
        for (size_t p = 4; p < 8; ++p)
            bus_.set_page(kViewerCpu, p, rd, ramWr);
    }

    // $8000-$BFFF: mid-hi ROM select (bits 3-2)
    {
        const uint8_t sel = mmu.mid_hi_select();
        ChipId rd = ramRd;
        if (sel == mos8722::cr::ROM_DEFAULT) rd = kBasicHi;
        // sel == 1 or 2: function ROM (not implemented, fall back to RAM)
        for (size_t p = 8; p < 12; ++p)
            bus_.set_page(kViewerCpu, p, rd, ramWr);
    }

    // $C000-$FFFF: high ROM select (bits 5-4)
    {
        const uint8_t high_sel = mmu.high_rom_select();

        // $C000-$CFFF: Editor ROM when default, else RAM
        if (high_sel == mos8722::cr::ROM_DEFAULT) {
            bus_.set_page(kViewerCpu, 0xC, kEditor, ramWr);
        } else {
            bus_.set_page(kViewerCpu, 0xC, ramRd, ramWr);
        }

        // $D000-$DFFF: controlled by high_sel AND CR bit 0 (I/O select)
        if (high_sel == mos8722::cr::ROM_RAM) {
            // All RAM — bits 5-4 = 11 overrides everything
            bus_.set_page(kViewerCpu, 0xD, ramRd, ramWr);
        } else if (mmu.io_visible()) {
            // CR bit 0 = 0: I/O devices visible
            bus_.map_to_indexed_sub(kViewerCpu, 0xD, 0);
        } else {
            // CR bit 0 = 1: character ROM visible (or function ROM for sel 1/2)
            bus_.set_page(kViewerCpu, 0xD, kCharRom, ramWr);
        }

        // $E000-$FFFF: Kernal ROM when default, else RAM
        if (high_sel == mos8722::cr::ROM_DEFAULT) {
            bus_.set_page(kViewerCpu, 0xE, kKernal, ramWr);
            bus_.set_page(kViewerCpu, 0xF, kKernal, ramWr);
        } else {
            bus_.set_page(kViewerCpu, 0xE, ramRd, ramWr);
            bus_.set_page(kViewerCpu, 0xF, ramRd, ramWr);
        }
    }

    // ── VIC-IIe viewer — always sees bank 0 by default ──────────────
    // The VIC bank from RCR bits 7-6 is separate from the CIA2 bank select
    // (which selects 16KB windows).  For now, keep bank 0 as default.
    // Character ROM ghosts remain at $1000 and $9000.
}

void C128System::enter_c64_mode() {
    c64_mode_ = true;
    board_.mmu.clear_c64_mode_request();

    // Default processor port: LORAM=1, HIRAM=1, CHAREN=1
    cpu_port_bits_ = 0x07;

    // Reconfigure memory map to use C64 ROMs
    update_bank_config();

    // Reset CPU — it must fetch the C64 KERNAL reset vector from $FFFC/$FFFD
    // to boot with the classic "**** COMMODORE 64 BASIC V2 ****" screen.
    pins_ = board_.csg8502.reset(pins_);

    log_info("C128: Entered C64 compatibility mode\n");
}

void C128System::switch_cpu_mode(CPUMode /*mode*/) {
    // TODO: switch between 8502 and Z80
}

void C128System::cpu_banking_callback(void* ctx, uint8_t banking_state) {
    auto* sys = static_cast<C128System*>(ctx);
    sys->cpu_port_bits_ = banking_state & 0x07;
    // In C128 native mode, processor port does NOT affect memory banking —
    // only the MMU CR does.  In C64 mode, port bits drive the PLA.
    if (sys->c64_mode_) {
        sys->update_bank_config();
    }
}

// ============================================================================
// SYSTEM MENU
// ============================================================================

void C128System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset C128")) {
        reset();
    }
    if (ImGui::MenuItem("Enter C64 Mode", nullptr, false, !c64_mode_)) {
        // Emulate BASIC's GO64 command: write MCR bit 6 to trigger C64 mode.
        enter_c64_mode();
    }
#endif
}

const char* C128System::get_mode_label() const {
    return c64_mode_ ? "C64 Mode" : nullptr;
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(c128_descriptor, [] {
    return std::make_unique<C128System>();
});
