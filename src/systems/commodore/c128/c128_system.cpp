/*
 * c128_system.cpp — Commodore 128 system implementation
 *
 * Stub implementation — system skeleton with descriptor, hardware traits,
 * and registration.  Emulation logic to be filled in.
 */

#include "core/cermu.hpp"
#include "systems/commodore/c128/c128_system.hpp"
#include "systems/commodore/pla_banking.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include "devices/keyboard/commodore_keyboard_device.hpp"
#include "devices/storage/drive_1541.hpp"
#include <cstring>
#include <cstdio>

// Forward declarations
static KeyboardMapper* create_c128_keyboard_mapper(commodore_keyboard_t* keyboard);
// C64 mapper factory — defined in c64_system.cpp, used for C64 compatibility mode
extern KeyboardMapper* create_c64_keyboard_mapper(commodore_keyboard_t* keyboard);
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// ============================================================================
// KERNAL SERIAL TRAPS — IEC bus trap handlers
// ============================================================================
// Intercept KERNAL serial bus routines to provide instant drive I/O.
// The C128 has two sets: C128-mode KERNAL and C64-mode KERNAL.
// Addresses from VICE c128.c c128_serial_traps[].
// ============================================================================

// C128-mode KERNAL serial routine addresses
static constexpr uint16_t C128_TRAP_SERIAL_LISTEN      = 0xE355;
static constexpr uint16_t C128_TRAP_SERIAL_SA_LISTEN    = 0xE37C;
static constexpr uint16_t C128_TRAP_SERIAL_SEND_BYTE    = 0xE38C;
static constexpr uint16_t C128_TRAP_SERIAL_RECEIVE_BYTE = 0xE43E;
static constexpr uint16_t C128_TRAP_SERIAL_READY        = 0xE569;
static constexpr uint16_t C128_TRAP_SERIAL_READY_ALT    = 0xE4F5;
static constexpr uint16_t C128_TRAP_RESUME_ADDRESS      = 0xE5BA;
static constexpr uint16_t C128_TRAP_READY_RESUME        = 0xE572;

// C64-mode KERNAL serial routine addresses (same as standalone C64)
static constexpr uint16_t C64_TRAP_SERIAL_LISTEN        = 0xED24;
static constexpr uint16_t C64_TRAP_SERIAL_SA_LISTEN     = 0xED37;
static constexpr uint16_t C64_TRAP_SERIAL_SEND_BYTE     = 0xED41;
static constexpr uint16_t C64_TRAP_SERIAL_RECEIVE_BYTE  = 0xEE14;
static constexpr uint16_t C64_TRAP_SERIAL_READY         = 0xEEA9;
static constexpr uint16_t C64_TRAP_RESUME_ADDRESS       = 0xEDAB;

// KERNAL zero-page addresses for serial I/O (same for C128 and C64 mode)
static constexpr uint16_t ZP_BSOUR  = 0x95;
static constexpr uint16_t ZP_TMP_IN = 0xA4;
static constexpr uint16_t ZP_STATUS = 0x90;

// IEC command byte masks
static constexpr uint8_t IEC_LISTEN_MASK   = 0x20;
static constexpr uint8_t IEC_TALK_MASK     = 0x40;
static constexpr uint8_t IEC_SECOND_MASK   = 0x60;
static constexpr uint8_t IEC_CLOSE_MASK    = 0xE0;
static constexpr uint8_t IEC_OPEN_MASK     = 0xF0;
static constexpr uint8_t IEC_UNLISTEN      = 0x3F;
static constexpr uint8_t IEC_UNTALK        = 0x5F;

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

    bind_all(board_, board_.components_, kC128Manifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = kC128Manifest.port_slots;
    port_manifest_count_ = kC128Manifest.port_count;
    board_.apply(bus_);

    basic_lo_rom_    = &board_.basic_lo;
    basic_hi_rom_    = &board_.basic_hi;
    editor_rom_      = &board_.editor_rom;
    kernal_rom_      = &board_.kernal_rom;
    char_rom_        = &board_.char_rom;
    c64_basic_rom_   = &board_.c64_basic;
    c64_kernal_rom_  = &board_.c64_kernal;
    z80_bios_rom_    = &board_.z80_bios;
    vdc_vram_        = &board_.vdc_vram;

    configure_bus_memory_map();
    init_io_dispatch();
    generate_bank_snapshots();
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
    cia1.cycles_tod[0] = 1000000 / 60;  // NTSC TOD frequency
    cia1.cycles_tod[1] = 1000000 / 50;  // PAL TOD frequency

    // CIA2 interrupt line → NMI
    cia2.configured_interrupt_bit = BUS_NMI_BIT;
    cia2.cycles_tod[0] = 1000000 / 60;
    cia2.cycles_tod[1] = 1000000 / 50;

    // SID — initialize
    sid.init();
    {
        float cpu_clock = static_cast<float>(c128_constants::CPU_FREQ_1MHZ_PAL);
        sid.set_cpu_clock(cpu_clock);
        sid.set_timing(true);  // PAL
    }

    register_bus_chips(board_);

    // Initialize 8502 CPU — held in reset while Z80 runs bootstrap
    board_.csg8502.init();
    board_.csg8502.bank_change_fn = cpu_banking_callback;
    board_.csg8502.bank_change_ctx = this;
    board_.csg8502.reset();
    // CSG 8502 port bit 6 is the CAPS LOCK key sense line (active-low,
    // directly wired to the physical key).  Bit 6 must default to 1
    // (key not pressed), otherwise the KERNAL scan routine at $C55D
    // thinks CAPS LOCK is engaged and uses the shifted decode table,
    // producing graphics characters ($C1) instead of letters ($41).
    // Must be AFTER reset() — reset_conditional_features() clobbers
    // init_io_port() with default init_pins (0x17, bit 6 = 0).
    board_.csg8502.init_io_port(0x2F, 0x17, 0x57);
    sync_caps_lock_from_host();

    // Sync PLA-style banking with freshly-reset I/O port (same as C64)
    {
        uint8_t banking_bits = board_.csg8502.io_port_regs.data
                             & board_.csg8502.io_port_regs.ddr
                             & 0x07;
        cpu_banking_callback(this, banking_bits);
    }

    // Initialize Z80 CPU — starts first after reset (real hardware behavior)
    // Z80 PC = $0000, which maps to the Z80 BIOS ROM.  The BIOS checks for
    // CP/M boot conditions and hands off to the 8502 by writing MCR bit 0.
    z80_pins_ = board_.z80.init();
    cpu_mode_ = CPUMode::MODE_Z80;
    active_cpu_ = &board_.z80;

    // Set initial VIC-IIe bank from CIA2 PA (defaults to bank 0 = $0000-$3FFF)
    vicii_base_t::memory_bank_change(&vic_iie, cia2.port_a_value & 0x03);

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

    // VDC RGBI video output — create port and connect to VDC's video_out_.
    // The VDC drives RGBI samples into this port at its 1 MHz character
    // clock rate (8 or 16 pixels per character cell).  The port collects
    // samples and classifies sync edges; swap_frame() retrieves completed
    // frames for the display pipeline.
    vdc_video_port_ = std::make_unique<RGBIVideoPort>();
    vdc.set_video_out(&vdc_video_port_->output());
    vdc_video_port_->bind_display(nullptr, nullptr,
                                  c128_constants::VDC_DISPLAY_WIDTH, 0);
    log_info("C128: VDC RGBI video output connected (%dx%d)\n",
             c128_constants::VDC_DISPLAY_WIDTH, c128_constants::VDC_DISPLAY_HEIGHT);

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

    // Register virtual key menu items for C128 keys that have no common
    // host keyboard equivalent (HELP, LINE FEED, 40/80 DISPLAY, NO SCROLL).
    // Always registered: SDL may report rare scancodes as "available" even
    // when no physical key produces them, leaving the menu empty.
    {
        struct { const char* label; emu_key_t key; bool toggle; bool initial; } c128_extra_keys[] = {
            {"HELP",           EMUKEY_CBM_HELP,          false, false},
            {"LINE FEED",      EMUKEY_CBM_LINE_FEED,     false, false},
            {"40/80 DISPLAY",  EMUKEY_CBM_40_80_DISPLAY, true,  false},
            {"NO SCROLL",      EMUKEY_CBM_NO_SCROLL,     true,  false},
        };
        for (auto& k : c128_extra_keys) {
            register_unmapped_input(k.label, k.key, k.toggle, k.initial);
        }
    }

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
    board_.reset_chips();
    // Z80 starts first after reset (real hardware behavior)
    z80_pins_ = board_.z80.init();
    pins_ = board_.csg8502.reset(pins_);
    // Restore CAPS LOCK sense line (bit 6 = 1 = not pressed) after CPU reset
    // clobbers it with the default init_pins (0x17).
    board_.csg8502.init_io_port(0x2F, 0x17, 0x57);
    sync_caps_lock_from_host();
    cpu_mode_ = CPUMode::MODE_Z80;
    active_cpu_ = &board_.z80;
    c64_mode_ = false;
    cpu_port_bits_ = 0x07;
    if (keyboard_) keyboard_->reset();

    // Sync CPU I/O port banking with freshly-reset state
    {
        uint8_t banking_bits = board_.csg8502.io_port_regs.data
                             & board_.csg8502.io_port_regs.ddr
                             & 0x07;
        cpu_banking_callback(this, banking_bits);
    }
    update_bank_config();

    // Re-sync VIC-IIe bank from CIA2 PA
    vicii_base_t::memory_bank_change(&board_.vic_iie, board_.cia2.port_a_value & 0x03);

    reset_load_state();
    total_cycles_ = 0;
}

// ============================================================================
// KERNAL SERIAL TRAPS — IEC bus trap handlers
// ============================================================================

Drive1541Device* C128System::find_iec_drive(int device_number) {
    if (device_number < 4) return nullptr;
    auto* port = get_port(PORT_IEC_SERIAL);
    if (!port) return nullptr;
    for (auto* dev : port->get_attached_devices()) {
        auto* drive = dynamic_cast<Drive1541Device*>(dev);
        if (drive && drive->get_device_number() == device_number) return drive;
    }
    return nullptr;
}

bool C128System::check_serial_traps(uint16_t pc) {
    if (c64_mode_) {
        // C64-mode KERNAL addresses
        switch (pc) {
            case C64_TRAP_SERIAL_LISTEN:
            case C64_TRAP_SERIAL_SA_LISTEN:
                return serial_trap_attention(C64_TRAP_RESUME_ADDRESS);
            case C64_TRAP_SERIAL_SEND_BYTE:
                return serial_trap_send(C64_TRAP_RESUME_ADDRESS);
            case C64_TRAP_SERIAL_RECEIVE_BYTE:
                return serial_trap_receive(C64_TRAP_RESUME_ADDRESS);
            case C64_TRAP_SERIAL_READY:
                return serial_trap_ready(C64_TRAP_RESUME_ADDRESS);
            default:
                return false;
        }
    } else {
        // C128-mode KERNAL addresses
        switch (pc) {
            case C128_TRAP_SERIAL_LISTEN:
            case C128_TRAP_SERIAL_SA_LISTEN:
                return serial_trap_attention(C128_TRAP_RESUME_ADDRESS);
            case C128_TRAP_SERIAL_SEND_BYTE:
                return serial_trap_send(C128_TRAP_RESUME_ADDRESS);
            case C128_TRAP_SERIAL_RECEIVE_BYTE:
                return serial_trap_receive(C128_TRAP_RESUME_ADDRESS);
            case C128_TRAP_SERIAL_READY:
            case C128_TRAP_SERIAL_READY_ALT:
                return serial_trap_ready(C128_TRAP_READY_RESUME);
            default:
                return false;
        }
    }
}

bool C128System::serial_trap_attention(uint16_t resume) {
    auto& cpu = board_.csg8502;
    uint8_t iecdata = board_.main_ram.data()[ZP_BSOUR];

    if (iecdata == IEC_UNLISTEN) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_unlisten();
        serial_trap_.active_device = -1;
    } else if (iecdata == IEC_UNTALK) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_untalk();
        serial_trap_.active_device = -1;
    } else if ((iecdata & 0xF0) == IEC_LISTEN_MASK || (iecdata & 0xF0) == IEC_TALK_MASK) {
        serial_trap_.active_device = iecdata & 0x0F;
        serial_trap_.trap_device = iecdata;
        serial_trap_.trap_secondary = 0;
    } else if ((iecdata & 0xF0) == IEC_SECOND_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_second(iecdata & 0x0F);
    } else if ((iecdata & 0xF0) == IEC_OPEN_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_open(iecdata & 0x0F);
    } else if ((iecdata & 0xF0) == IEC_CLOSE_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_close(iecdata & 0x0F);
    }

    if (serial_trap_.active_device >= 4) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (!drive) board_.main_ram.data()[ZP_STATUS] |= 0x80;
    }

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;  // Clear carry
    p &= ~0x04;  // Clear interrupt disable
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume);
    cpu.transition_to_fetch();
    return true;
}

bool C128System::serial_trap_send(uint16_t resume) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    auto& cpu = board_.csg8502;
    uint8_t iecdata = board_.main_ram.data()[ZP_BSOUR];

    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC_SECOND_MASK;
        drive->trap_second(0);
    }
    drive->trap_send(iecdata);

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;
    p &= ~0x04;
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume);
    cpu.transition_to_fetch();
    return true;
}

bool C128System::serial_trap_receive(uint16_t resume) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    auto& cpu = board_.csg8502;

    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC_SECOND_MASK;
        drive->trap_second(0);
    }

    uint8_t data = 0;
    int status = drive->trap_receive(data);

    board_.main_ram.data()[ZP_TMP_IN] = data;
    cpu.set(reg::A, data);

    if (status) board_.main_ram.data()[ZP_STATUS] |= static_cast<uint8_t>(status);

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;
    p &= ~0x04;
    if (data & 0x80) p |= 0x80; else p &= ~0x80;
    if (data == 0)   p |= 0x02; else p &= ~0x02;
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume);
    cpu.transition_to_fetch();
    return true;
}

bool C128System::serial_trap_ready(uint16_t resume) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    auto& cpu = board_.csg8502;
    cpu.set(reg::A, 1);

    uint8_t p = cpu.get(reg::P);
    p &= ~0x80;
    p &= ~0x02;
    p &= ~0x04;
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume);
    cpu.transition_to_fetch();
    return true;
}

// ============================================================================
// EXECUTION
// ============================================================================

void C128System::tick() {
    total_cycles_++;

    auto& vic_iie = board_.vic_iie;
    auto& sid     = board_.sid;
    auto& cia1    = board_.cia1;
    auto& cia2    = board_.cia2;
    auto& cpu     = board_.csg8502;

    // In Z80 mode the Z80 runs its own T-state with inline bus servicing
    // (MREQ/IORQ dispatch).  Peripherals still tick but receive a dummy bus
    // state — only the Z80 drives real addresses this cycle.
    if (cpu_mode_ == CPUMode::MODE_Z80)
        tick_z80();

    // ── Bus state setup ──────────────────────────────────────────────
    // In 8502 mode the bus state flows through every phase; in Z80 mode
    // peripherals see a neutral default (no address, data = pull-ups).
    bus_state_t s = default_state_;
    if (cpu_mode_ == CPUMode::MODE_8502) {
        BUS_SET_ADDR(s, BUS_GET_ADDR(pins_));
        BUS_SET_DATA(s, BUS_GET_DATA(pins_));
    }

    // ── PHASE 1: VIC-IIe PHI1 — g-access read, pixel sequencing ─────
    s = vic_iie.tick_phi1(s);

    // ── PHASE 1.5: CIA PHI2 — apply pending interrupt lines before CPU
    s = cia2.tick_phi2(s);
    s = cia1.tick_phi2(s);

    // ── 8502-only: BA→RDY wiring + CPU PHI2 + address decode ────────
    if (cpu_mode_ == CPUMode::MODE_8502) {
        // BA→RDY wiring (direct bit test + set/clear)
        if (BUS_GET_BIT(s, BUS_BA_BIT))
            BUS_SET_BIT(s, BUS_RDY_BIT);
        else
            BUS_CLR_BIT(s, BUS_RDY_BIT);

        // CPU PHI2 — instruction execution
        s = cpu.tick<CSG8502::Phase::PHI2>(s);

        // Address decode + buffer service + MMIO self-dispatch
        const uint16_t addr = BUS_GET_ADDR(s);
        const bool is_write = !BUS_GET_BIT(s, BUS_RW_BIT);

        // $FF00-$FF04: MMU configuration registers (always visible)
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

            s = bus_.resolve(s, viewer);
            s = bus_.service(s);

            s = vic_iie.tick_mmio(s);
            s = sid.tick_mmio(s);
            s = board_.colorram.tick_mmio(s);
            s = board_.mmu.tick_mmio(s);
            s = cia1.tick_mmio(s);
            s = cia2.tick_mmio(s);
            s = board_.vdc.tick_mmio(s);
        }
    }

    // ── MMU bank config + CPU/mode switch checks (both modes) ────────
    if (unlikely(board_.mmu.bank_config_dirty())) {
        update_bank_config();
    }

    if (unlikely(board_.mmu.cpu_switch_requested())) {
        board_.mmu.acknowledge_cpu_switch();
        bool want_8502 = board_.mmu.cpu_is_8502();
        if (want_8502 && cpu_mode_ != CPUMode::MODE_8502)
            switch_cpu_mode(CPUMode::MODE_8502);
        else if (!want_8502 && cpu_mode_ != CPUMode::MODE_Z80)
            switch_cpu_mode(CPUMode::MODE_Z80);
    }

    if (unlikely(!c64_mode_ && board_.mmu.c64_mode_requested())) {
        enter_c64_mode();
    }

    // ── 8502-only: NMI edge detection ────────────────────────────────
    if (cpu_mode_ == CPUMode::MODE_8502)
        cpu.sample_nmi_pin(s);

    // ── PHASE 3.1: VIC-IIe PHI2 — c/p/s-access data delivery ────────
    vic_iie.tick_phi2(s);

    // ── PHASE 3.5: CIA PHI1 — timer counting, TOD, interrupt gen ─────
    s = cia2.tick_phi1(s);
    s = cia1.tick_phi1(s);

    // ── 8502-only: CPU PHI1 + R/W restore ────────────────────────────
    if (cpu_mode_ == CPUMode::MODE_8502) {
        s = cpu.tick<CSG8502::Phase::PHI1>(s);
        BUS_SET_BIT(s, BUS_RW_BIT);

        // KERNAL serial trap check — intercept IEC bus routines at instruction boundaries
        if (serial_traps_enabled_ && cpu.opdone()) {
            uint16_t pc = cpu.get(reg::PC);
            if (c64_mode_) {
                if (pc >= 0xED00 && pc < 0xEF00)
                    check_serial_traps(pc);
            } else {
                if (pc >= 0xE300 && pc < 0xE600)
                    check_serial_traps(pc);
            }
        }
    }

    // ── PHASE 5: SID — sound generation ──────────────────────────────
    s = sid.tick_audio(s);

    // ── VDC character clock (~1 MHz, same rate as slow-mode CPU) ─────
    // Disabled in C64 mode (VDC is not accessible).
    if (!c64_mode_)
        board_.vdc.tick();

    if (cpu_mode_ == CPUMode::MODE_8502)
        pins_ = s;
}

void C128System::run_frame() {
    if (!system_ready_ || !video_port_) return;

    // Service GUI-requested actions (safe: we're on the emu thread)
    if (reset_requested_.exchange(false)) {
        reset();
        // After reset, restore C128 keyboard mapper (reset clears C64 mode)
        keyboard_mapper_.reset(create_c128_keyboard_mapper(keyboard_));
    }
    if (c64_mode_requested_.exchange(false) && !c64_mode_) {
        enter_c64_mode();
    }

    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();

    // VDC runs on its own frame timing (driven by `tick()` above).
    // Swap its frame independently — the VDC FrameEnd signal may or
    // may not have fired this iteration depending on timing alignment.
    if (vdc_video_port_) {
        auto& vdc_out = vdc_video_port_->output();
        if (vdc_out.frame_ended())
            vdc_video_port_->swap_frame();
    }
    check_deferred_load();
    tick_peripherals();
    tick_unmapped_inputs();
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

void C128System::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode,
                                          uint16_t mod, bool pressed, bool repeat) {
    // CAPS LOCK → processor port $01 bit 6 (active-low, directly wired).
    // The C128 CAPS LOCK is a physical toggle; mirror the host lock state.
    // SDL updates KMOD_CAPS in the event's mod field on key-down.
    if (key == SDLK_CAPSLOCK && pressed) {
        if (mod & KMOD_CAPS)
            board_.csg8502.port.pull_low(0x40);   // bit 6 → 0 (engaged)
        else
            board_.csg8502.port.release(0x40);     // bit 6 → 1 (released)
    }

    // Delegate to CommodoreSystem for keyboard mapper dispatch
    CommodoreSystem::handle_keyboard_event_ex(key, scancode, mod, pressed, repeat);
}

// ============================================================================
// COMMODORE SYSTEM HOOKS
// ============================================================================

void C128System::sync_caps_lock_from_host() {
    // Mirror host Caps Lock state to processor port $01 bit 6 (active-low).
    if (SDL_GetModState() & KMOD_CAPS)
        board_.csg8502.port.pull_low(0x40);    // engaged
    else
        board_.csg8502.port.release(0x40);      // released
}

bool C128System::is_basic_ready() const {
    // C128 BASIC 7.0 stores the cursor blink phase in $0A27.  When it
    // reaches a non-zero value after boot, BASIC has printed "READY." and
    // the main loop is running.  This is the same approach used for the C64
    // ($D3 cursor column check) adapted to C128's different memory layout.
    // Alternatively, check the KERNAL warm-start vector at $0300-$0301.
    const uint8_t* ram = board_.main_ram.data();
    // $0300/$0301 = BASIC warm-start vector.  BASIC 7.0 sets this to $4DC6.
    uint16_t warm = static_cast<uint16_t>(ram[0x0300]) | (static_cast<uint16_t>(ram[0x0301]) << 8);
    return warm == 0x4DC6;
}

commodore_load_context_t C128System::build_load_context() {
    commodore_load_context_t ctx = {};
    ctx.system_name     = "C128";
    ctx.write_byte      = [](void* c, uint16_t a, uint8_t v) {
        static_cast<RAMChip*>(c)->data()[a] = v;
    };
    ctx.write_block     = [](void* c, uint16_t a, const uint8_t* d, size_t n) {
        memcpy(&static_cast<RAMChip*>(c)->data()[a], d, n);
    };
    ctx.mem_read        = [](void* c, uint16_t a) -> uint8_t {
        return static_cast<RAMChip*>(c)->data()[a];
    };
    ctx.mem_ctx         = &board_.main_ram;
    ctx.basic_params    = &COMMODORE_BASIC_C64;  // BASIC 7.0 shares V2 tokens for SYS
    ctx.basic_start_addrs[0] = 0x1C01;           // C128 BASIC start (TXTTAB)
    ctx.default_raw_addr = 0xC000;
    ctx.set_pc          = nullptr;
    ctx.pc_ctx          = nullptr;
    return ctx;
}

void C128System::inject_keys(const char* str) {
    uint8_t* ram = board_.main_ram.data();
    int len = static_cast<int>(strlen(str));
    if (len > 10) len = 10;

    // C128 mode: buffer at $034A, count at $D0
    // C64 mode:  buffer at $0277, count at $C6
    const uint16_t buf_addr   = c64_mode_ ? 0x0277 : 0x034A;
    const uint16_t count_addr = c64_mode_ ? 0x00C6 : 0x00D0;

    for (int i = 0; i < len; i++)
        ram[buf_addr + i] = static_cast<uint8_t>(str[i]);
    ram[count_addr] = static_cast<uint8_t>(len);
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
    //   z80_bios    (  4KB):                 1 bank  → ID 7
    //   c64_basic   (  8KB):                 1 bank  → ID 8
    //   c64_kernal  (  8KB):                 1 bank  → ID 9
    //   vdc_vram    ( 16KB):                 1 bank  → ID 10

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

// ============================================================================
// BANK MAP SNAPSHOT GENERATION
// ============================================================================
//
// Pre-computes all possible memory mappings for both operating modes:
//
//   C64 mode:    32 PLA modes (LORAM/HIRAM/CHAREN/EXROM/GAME) × 2 viewers.
//                Uses the shared PLA906114 simulation from pla_banking.hpp.
//
//   C128 native: CR[6:0] × RCR[3:0] = 2048 CPU modes + 4 VIC-IIe modes.
//                Encoding: cpu_mode_index = (CR & 0x7F) | ((RCR & 0x0F) << 7).
//
// At runtime, update_bank_config() is a single snapshot load instead of
// a per-page reprogramming loop.
//

void C128System::generate_bank_snapshots() {
    using ChipId  = Bus::ChipId;
    using WriteId = Bus::WriteChipId;
    using PT = PackingTraits<C128BusSpec>;

    constexpr ChipId  kRamBank0    = ChipId(0);
    constexpr ChipId  kRamBank1    = ChipId(1);
    constexpr WriteId kRamBank0W   = WriteId(0);
    constexpr WriteId kRamBank1W   = WriteId(1);
    constexpr ChipId  kBasicLo     = ChipId(2);
    constexpr ChipId  kBasicHi     = ChipId(3);
    constexpr ChipId  kEditor      = ChipId(4);
    constexpr ChipId  kCharRom     = ChipId(5);
    constexpr ChipId  kKernal      = ChipId(6);
    constexpr ChipId  kC64Basic    = ChipId(8);
    constexpr ChipId  kC64Kernal   = ChipId(9);
    const auto no_chip_rd = ChipId(PT::kNoChipSelected);
    const auto no_chip_wr = WriteId(PT::kNoChipSelectedWrite);

    // ── C64-mode PLA snapshots ──────────────────────────────────────────
    {
        PlaChipMapping<C128BusSpec> mapping;
        // RAM bank 0 — single chip ID covers entire 64 KB (bank_size=64K)
        mapping.ram           = {kRamBank0, 0};
        mapping.basic         = {kC64Basic, 0};
        mapping.kernal        = {kC64Kernal, 0};
        mapping.charrom       = {kCharRom, 0};
        mapping.roml          = {no_chip_rd, 0};   // No C64 cartridge ROM in manifest
        mapping.romh          = {no_chip_rd, 0};
        mapping.io_sub_read   = Bus::indexed_sub_chip(0);
        mapping.io_sub_write  = Bus::indexed_sub_write_chip(0);
        mapping.no_chip_read  = no_chip_rd;
        mapping.no_chip_write = no_chip_wr;
        mapping.ram_write_base = kRamBank0W;
        mapping.ram_write_mask = 0;

        generate_pla_mode_snapshots<C128BusSpec>(
            bus_, mapping, kViewerCpu, kViewerVicII,
            c64_cpu_snapshots_, c64_vic_snapshots_);

        log_info("C128: C64-mode PLA snapshots generated (%zu modes × 2 viewers)\n",
                kNumPlaModes);
    }

    // ── C128-native CPU snapshots ───────────────────────────────────────
    // Enumerate all CR[6:0] × RCR[3:0] = 2048 combinations.
    {
        c128_cpu_snapshots_.resize(kC128NumCpuModes);
        const auto io_sub_rd = Bus::indexed_sub_chip(0);

        for (uint16_t mode = 0; mode < kC128NumCpuModes; ++mode) {
            const uint8_t cr  = mode & 0x7F;
            const uint8_t rcr = (mode >> 7) & 0x0F;

            // Decode CR bits
            const uint8_t ram_bank    = (cr & 0x40) ? 1 : 0;
            const uint8_t high_sel    = (cr >> 4) & 0x03;
            const uint8_t mid_hi_sel  = (cr >> 2) & 0x03;
            const bool    basic_lo_en = (cr & 0x02) == 0;   // Bit 1: 0=ROM, 1=RAM
            const bool    io_visible  = (cr & 0x01) == 0;   // Bit 0: 0=I/O, 1=char ROM

            // Decode RCR common RAM bits
            const bool    top_en        = (rcr & 0x08) != 0;
            const bool    bot_en        = (rcr & 0x04) != 0;
            const uint8_t common_sz_sel = rcr & 0x03;
            const uint32_t common_size  = (common_sz_sel == 0) ? 1024 : (2048U << common_sz_sel);
            const uint8_t bot_pages     = bot_en ? (uint8_t)((common_size + 0xFFF) >> 12) : 0;
            const uint8_t top_start     = top_en ? (uint8_t)((0x10000 - common_size) >> 12) : 16;

            const ChipId  ramRd = ram_bank ? kRamBank1 : kRamBank0;
            const WriteId ramWr = ram_bank ? kRamBank1W : kRamBank0W;

            // Per-page RAM chip, accounting for common areas (always bank 0)
            auto ram_for_page = [&](size_t page) -> std::pair<ChipId, WriteId> {
                if (page < bot_pages || page >= top_start)
                    return {kRamBank0, kRamBank0W};
                return {ramRd, ramWr};
            };

            bus_.reset_viewer(kViewerCpu);

            // $0000-$3FFF: RAM (common pages stay bank 0)
            for (size_t p = 0; p < 4; ++p) {
                auto [rd, wr] = ram_for_page(p);
                bus_.set_page(kViewerCpu, p, rd, wr);
            }

            // $4000-$7FFF: BASIC LO ROM or RAM (CR bit 1)
            {
                ChipId rd = basic_lo_en ? kBasicLo : ramRd;
                for (size_t p = 4; p < 8; ++p)
                    bus_.set_page(kViewerCpu, p, rd, ramWr);
            }

            // $8000-$BFFF: mid-hi ROM select (CR bits 3-2)
            {
                ChipId rd = ramRd;
                if (mid_hi_sel == mos8722::cr::ROM_DEFAULT) rd = kBasicHi;
                // sel 1/2 = internal/external function ROM (not yet implemented → RAM)
                for (size_t p = 8; p < 12; ++p)
                    bus_.set_page(kViewerCpu, p, rd, ramWr);
            }

            // $C000-$CFFF: Editor ROM when default, else RAM
            {
                auto [rd, wr] = ram_for_page(0xC);
                bus_.set_page(kViewerCpu, 0xC,
                    (high_sel == mos8722::cr::ROM_DEFAULT) ? kEditor : rd, wr);
            }

            // $D000-$DFFF: I/O / char ROM / RAM (CR bits 5-4 + bit 0)
            if (high_sel == mos8722::cr::ROM_RAM) {
                auto [rd, wr] = ram_for_page(0xD);
                bus_.set_page(kViewerCpu, 0xD, rd, wr);
            } else if (io_visible) {
                bus_.map_to_indexed_sub(kViewerCpu, 0xD, 0);
            } else {
                auto [_, wr] = ram_for_page(0xD);
                bus_.set_page(kViewerCpu, 0xD, kCharRom, wr);
            }

            // $E000-$FFFF: Kernal ROM when default, else RAM
            for (size_t p = 0xE; p <= 0xF; ++p) {
                auto [rd, wr] = ram_for_page(p);
                bus_.set_page(kViewerCpu, p,
                    (high_sel == mos8722::cr::ROM_DEFAULT) ? kKernal : rd, wr);
            }

            bus_.save_snapshot(kViewerCpu, c128_cpu_snapshots_[mode]);
        }

        log_info("C128: Native-mode CPU snapshots generated (%zu modes)\n",
                kC128NumCpuModes);
    }

    // ── C128-native VIC-IIe snapshots ───────────────────────────────────
    // VIC-IIe always sees 64 KB of RAM (bank 0 or 1 from RCR[7:6]) with
    // character ROM ghosts at $1000 and $9000.
    {
        for (uint8_t vic_mode = 0; vic_mode < kC128NumVicModes; ++vic_mode) {
            bus_.reset_viewer(kViewerVicII);
            // RCR bits 7-6: 00/10/11 = bank 0, 01 = bank 1
            ChipId ram_chip = (vic_mode == 1) ? kRamBank1 : kRamBank0;
            for (size_t p = 0; p < 16; ++p)
                bus_.set_read_page(kViewerVicII, p, ram_chip);
            // Character ROM ghosts at $1000 and $9000 (VIC bank 0 and 2)
            bus_.set_read_page(kViewerVicII, 0x1, kCharRom);
            bus_.set_read_page(kViewerVicII, 0x9, kCharRom);
            bus_.save_snapshot(kViewerVicII, c128_vic_snapshots_[vic_mode]);
        }

        log_info("C128: Native-mode VIC-IIe snapshots generated (%zu modes)\n",
                (size_t)kC128NumVicModes);
    }

    // Load initial configuration (C128 native boot: CR=0x00, RCR=0x00)
    c128_cpu_mode_ = 0;
    c128_vic_mode_ = 0;
    bus_.load_snapshot(kViewerCpu, c128_cpu_snapshots_[0]);
    bus_.load_snapshot(kViewerVicII, c128_vic_snapshots_[0]);
}

void C128System::update_bank_config() {
    // Load the correct pre-computed snapshot for the current MMU state.
    // All 2048 C128-native + 32 C64-mode memory maps were generated by
    // generate_bank_snapshots() during initialize().

    auto& mmu = board_.mmu;
    mmu.acknowledge_bank_config();

    if (c64_mode_) {
        // C64 mode: PLA-style banking from processor port bits 0-2.
        // EXROM/GAME default to 1/1 (no cartridge) → bits 3-4 of PLA mode.
        // When C128 cartridge support is added, these would come from
        // the expansion port device signals.
        c64_pla_mode_ = cpu_port_bits_ & 0x07;
        c64_pla_mode_ |= 0x18;    // EXROM=1, GAME=1 (no cartridge)
        bus_.load_snapshot(kViewerCpu, c64_cpu_snapshots_[c64_pla_mode_]);
        bus_.load_snapshot(kViewerVicII, c64_vic_snapshots_[c64_pla_mode_]);
        return;
    }

    // C128 native mode: indexed by CR[6:0] | (RCR[3:0] << 7).
    const uint8_t cr  = mmu.cr();
    const uint8_t rcr_lo = mmu.rcr_banking_bits();  // RCR[3:0]
    const uint16_t cpu_mode = (cr & 0x7F) | (uint16_t(rcr_lo) << 7);

    if (cpu_mode != c128_cpu_mode_) {
        c128_cpu_mode_ = cpu_mode;
        bus_.load_snapshot(kViewerCpu, c128_cpu_snapshots_[cpu_mode]);
    }

    const uint8_t vic_mode = mmu.vic_ram_bank() & 0x03;
    if (vic_mode != c128_vic_mode_) {
        c128_vic_mode_ = vic_mode;
        bus_.load_snapshot(kViewerVicII, c128_vic_snapshots_[vic_mode]);
    }
}

void C128System::enter_c64_mode() {
    c64_mode_ = true;
    board_.mmu.clear_c64_mode_request();

    // Must be in 8502 mode for C64 compatibility
    cpu_mode_ = CPUMode::MODE_8502;
    active_cpu_ = &board_.csg8502;

    // Default processor port: LORAM=1, HIRAM=1, CHAREN=1
    cpu_port_bits_ = 0x07;

    // Reconfigure memory map to use C64 ROMs
    update_bank_config();

    // Reset CPU — it must fetch the C64 KERNAL reset vector from $FFFC/$FFFD
    // to boot with the classic "**** COMMODORE 64 BASIC V2 ****" screen.
    pins_ = active_cpu_->reset(pins_);

    // Switch keyboard mapper to C64 layout (8×8 matrix, no numpad).
    // The C128 keyboard matrix columns 0-7 are identical to the C64 matrix,
    // but the C128 mapper includes numpad keys in columns 8-10 that duplicate
    // standard characters.  The C64 KERNAL only scans columns 0-7, so numpad
    // mappings would produce "dead" keys.  Using the C64 mapper ensures
    // character→matrix-position assignments stay within columns 0-7.
    if (keyboard_)
        keyboard_mapper_.reset(create_c64_keyboard_mapper(keyboard_));

    log_info("C128: Entered C64 compatibility mode\n");
}

void C128System::switch_cpu_mode(CPUMode mode) {
    if (mode == cpu_mode_) return;

    if (mode == CPUMode::MODE_8502) {
        // Z80 → 8502: the 8502 starts from its reset vector.
        // On real hardware the 8502 was held in reset while Z80 was active.
        cpu_mode_ = CPUMode::MODE_8502;
        active_cpu_ = &board_.csg8502;
        pins_ = active_cpu_->reset(pins_);
        // Restore CAPS LOCK sense line after CPU reset clobbers init_pins.
        board_.csg8502.init_io_port(0x2F, 0x17, 0x57);
        sync_caps_lock_from_host();
        log_info("C128: CPU switch → 8502\n");
    } else {
        // 8502 → Z80: re-enter Z80 mode (e.g. for CP/M)
        cpu_mode_ = CPUMode::MODE_Z80;
        active_cpu_ = &board_.z80;
        z80_pins_ = active_cpu_->init();
        log_info("C128: CPU switch → Z80\n");
    }
}

// ============================================================================
// Z80 TICK — one T-state per call
// ============================================================================
//
// The Z80 shares the same address bus as the 8502 through the MMU.
// Memory map while Z80 is active:
//   $0000-$0FFF : Z80 BIOS ROM (4KB, overlay from KERNAL chip)
//   $1000-$FFFF : Same as 8502 view (controlled by MMU CR)
//   $D000-$DFFF : I/O when MMU CR bit 0 = 0 (same as 8502)
//   $FF00-$FF04 : MMU registers (always visible)
//
// The Z80 BIOS ROM is only visible to the Z80, not to the 8502.
// Writes to $0000-$0FFF pass through to RAM underneath.

void C128System::tick_z80() {
    z80_pins_ = board_.z80.tick(z80_pins_);

    const bool mreq = !BUS_GET_BIT(z80_pins_, Z80_MREQ_BIT);
    const bool iorq = !BUS_GET_BIT(z80_pins_, Z80_IORQ_BIT);
    // IORQ + M1 = interrupt acknowledge (not a port I/O operation)
    const bool m1   = !BUS_GET_BIT(z80_pins_, Z80_M1_BIT);

    if (mreq) {
        const uint16_t addr = BUS_GET_ADDR(z80_pins_);
        const bool is_write = !BUS_GET_BIT(z80_pins_, BUS_RW_BIT);

        // $FF00-$FF04: MMU registers (always visible, same as 8502)
        if (addr >= 0xFF00 && addr <= 0xFF04) {
            if (is_write) {
                const uint8_t data = BUS_GET_DATA(z80_pins_);
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
                BUS_SET_DATA(z80_pins_, data);
            }
        }
        // $0000-$0FFF: Z80 BIOS ROM overlay (read) / RAM (write)
        else if (addr < 0x1000 && !is_write) {
            if (z80_bios_rom_ && z80_bios_rom_->data()) {
                BUS_SET_DATA(z80_pins_, z80_bios_rom_->data()[addr & 0x0FFF]);
            } else {
                BUS_SET_DATA(z80_pins_, 0xFF);
            }
        }
        // Everything else: use the normal memory bus (same as 8502 viewer)
        else {
            z80_pins_ = bus_.resolve(z80_pins_, kViewerCpu);
            z80_pins_ = bus_.service(z80_pins_);

            // CS-tick MMIO dispatch (for I/O page $D000-$DFFF)
            z80_pins_ = board_.vic_iie.tick_mmio(z80_pins_);
            z80_pins_ = board_.sid.tick_mmio(z80_pins_);
            z80_pins_ = board_.colorram.tick_mmio(z80_pins_);
            z80_pins_ = board_.mmu.tick_mmio(z80_pins_);
            z80_pins_ = board_.cia1.tick_mmio(z80_pins_);
            z80_pins_ = board_.cia2.tick_mmio(z80_pins_);
            z80_pins_ = board_.vdc.tick_mmio(z80_pins_);
        }

        if (unlikely(board_.mmu.bank_config_dirty())) {
            update_bank_config();
        }
    } else if (iorq && !m1) {
        // IORQ without M1 = port I/O (IN/OUT)
        z80_pins_ = z80_io_tick(z80_pins_);
    }
}

bus_state_t C128System::z80_io_tick(bus_state_t pins) {
    // On real C128 hardware the Z80 IORQ signal is decoded the same way as
    // 8502 memory-mapped I/O.  OUT (C),A with BC=$D505 reaches the MMU at
    // $D505, etc.  The Z80 BIOS relies on this — it programs VIC-IIe, MMU,
    // and CIA registers exclusively via OUT (C),A / IN A,(C).
    //
    // Direct dispatch by address range — bypasses CS-tick because the bus
    // resolve / page-table path only works for MREQ (the CS field isn't
    // populated during IORQ cycles).
    const uint16_t addr = BUS_GET_ADDR(pins);
    const bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);  // HIGH = read
    const uint8_t data = BUS_GET_DATA(pins);

    // $FF00-$FF04: MMU configuration registers
    if (addr >= 0xFF00 && addr <= 0xFF04) {
        if (!is_read) {
            if (addr == 0xFF00)
                board_.mmu.write_ff00(data);
            else
                board_.mmu.write_ff01_ff04(static_cast<uint8_t>(addr - 0xFF01));
        } else {
            BUS_SET_DATA(pins, (addr == 0xFF00)
                ? board_.mmu.read_ff00()
                : board_.mmu.read_ff01_ff04(static_cast<uint8_t>(addr - 0xFF01)));
        }
        if (unlikely(board_.mmu.bank_config_dirty()))
            update_bank_config();
        return pins;
    }

    // $D500-$D50B: MMU registers
    if (addr >= 0xD500 && addr <= 0xD50B) {
        const uint8_t reg = static_cast<uint8_t>(addr - 0xD500);
        if (!is_read) {
            board_.mmu.write_register(reg, data);
        } else {
            BUS_SET_DATA(pins, board_.mmu.read_register(reg));
        }
        if (unlikely(board_.mmu.bank_config_dirty()))
            update_bank_config();
        return pins;
    }

    // Helper: populate the CS field so that tick_mmio() recognises the chip.
    auto set_cs = [](bus_state_t& b, uint16_t id) {
        b = (b & ~uint64_t(BUS_CS_MASK)) | (bus_state_t(id) << BUS_CS_SHIFT);
    };

    // $DC00-$DC0F: CIA1
    if ((addr & 0xFF00) == 0xDC00) {
        set_cs(pins, board_.cia1.bus_chip_id());
        pins = board_.cia1.tick_mmio(pins);
        return pins;
    }

    // $DD00-$DD0F: CIA2
    if ((addr & 0xFF00) == 0xDD00) {
        set_cs(pins, board_.cia2.bus_chip_id());
        pins = board_.cia2.tick_mmio(pins);
        return pins;
    }

    // $D000-$D3FF: VIC-IIe
    if (addr >= 0xD000 && addr < 0xD400) {
        set_cs(pins, board_.vic_iie.bus_chip_id());
        pins = board_.vic_iie.tick_mmio(pins);
        return pins;
    }

    // $D400-$D4FF: SID
    if (addr >= 0xD400 && addr < 0xD500) {
        set_cs(pins, board_.sid.bus_chip_id());
        pins = board_.sid.tick_mmio(pins);
        return pins;
    }

    // $D600-$D601: VDC
    if (addr >= 0xD600 && addr < 0xD700) {
        set_cs(pins, board_.vdc.bus_chip_id());
        pins = board_.vdc.tick_mmio(pins);
        return pins;
    }

    // $D800-$DBFF: Color RAM
    if (addr >= 0xD800 && addr < 0xDC00) {
        set_cs(pins, board_.colorram.bus_chip_id());
        pins = board_.colorram.tick_mmio(pins);
        return pins;
    }

    // All other ports: open bus
    if (is_read) {
        BUS_SET_DATA(pins, 0xFF);
    }
    return pins;
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
//

void C128System::on_port_device_changed(int port_index) {
    if (port_index == PORT_IEC_SERIAL) {
        serial_traps_enabled_ = false;
        auto* port = get_port(PORT_IEC_SERIAL);
        if (port) {
            for (auto* dev : port->get_attached_devices()) {
                if (dynamic_cast<Drive1541Device*>(dev)) {
                    serial_traps_enabled_ = true;
                    break;
                }
            }
        }
    }
}

// TODO: "Reset C128" duplicates the generic "System > Reset" menu item.
// If they stay identical, remove the system-specific one.  Alternatively,
// give them distinct roles — e.g. "Power Cycle" (cold boot: full chip
// re-init, RAM cleared) vs "Reset" (warm reset: reset vector fetch, RAM
// preserved).  Use era-appropriate terms and expose the distinction for
// all systems that support it, not just C128.

void C128System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Enter C64 Mode", nullptr, false, !c64_mode_)) {
        c64_mode_requested_.store(true, std::memory_order_relaxed);
    }
    ImGui::Separator();
    render_unmapped_inputs_menu();
#endif
}

void C128System::on_unmapped_toggle_changed(emu_key_t key, bool pressed) {
    if (key == EMUKEY_CBM_40_80_DISPLAY) {
        // 40/80 DISPLAY is a hardware switch wired to MMU MCR bit 5,
        // not a keyboard matrix key.  Update the MMU sense line directly.
        board_.mmu.key_40_80_pressed = pressed;
    }
}

const char* C128System::get_mode_label() const {
    return c64_mode_ ? "C64 Mode" : nullptr;
}

int C128System::get_active_video_port_index() const {
    // The 40/80 DISPLAY key is a hardware toggle wired to MMU MCR bit 5.
    // When pressed (40-col selected), return the VIC-IIe composite port;
    // when not pressed (80-col selected), return the VDC RGBI port.
    return board_.mmu.key_40_80_pressed ? PORT_VIDEO_40 : PORT_VIDEO_80;
}

void* C128System::get_video_port_ptr() {
    // Return whichever video port is currently active.
    // The display pipeline (SessionGUI) uses this to connect the signal
    // decoder.  In 40-col mode → VIC-IIe composite; 80-col → VDC RGBI.
    if (board_.mmu.key_40_80_pressed)
        return video_port_.get();
    return vdc_video_port_.get();
}

VideoSignalType C128System::get_video_signal_type() const {
    if (board_.mmu.key_40_80_pressed)
        return VideoSignalType::Composite;
    return VideoSignalType::RGBI;
}

void C128System::rebind_active_video_output() {
    // Switch which port writes to last_frame_data_ so the emu thread's
    // snapshot code picks up the correct signal data.
    // Unbind the inactive port first (set its frame_output_ to nullptr),
    // then bind the active one.
    if (board_.mmu.key_40_80_pressed) {
        // 40-col: VIC-IIe composite
        if (vdc_video_port_)
            vdc_video_port_->bind_frame_output(nullptr);
        if (video_port_)
            video_port_->bind_frame_output(&last_frame_data_);
    } else {
        // 80-col: VDC RGBI
        if (video_port_)
            video_port_->bind_frame_output(nullptr);
        if (vdc_video_port_)
            vdc_video_port_->bind_frame_output(&last_frame_data_);
    }
}

// ============================================================================
// Keyboard mapper factory
// ============================================================================

static KeyboardMapper* create_c128_keyboard_mapper(commodore_keyboard_t* keyboard) {
    KeyboardMapper* mapper = new KeyboardMapper();
    mapper->set_guest_keyboard(keyboard);
    mapper->build_character_map_from_matrix(&c128_keyboard_config);

    mapper->register_default_synthetic_mappings();

    // Register emu-specific key candidates for C128.
    // Shared Commodore keys first, then C128-specific extras.
    auto& sdl_map = EmuKeySDLMap::instance();
    sdl_map.clear_system_mappings();
    sdl_map.register_candidates(EMUKEY_CBM_RESTORE,       {SDL_SCANCODE_SYSREQ, SDL_SCANCODE_GRAVE});
    sdl_map.register_candidates(EMUKEY_CBM_POUND,         {SDL_SCANCODE_NONUSHASH});

    // C128-specific keys:
    // Host LAlt → CBM key (Commodore key, more accessible than Super/LGUI
    // which Linux WMs intercept).  Host RAlt → C128 ALT key.
    sdl_map.register_candidates(EMUKEY_CBM_COMMODORE,     {SDL_SCANCODE_LALT});
    sdl_map.register_candidates(EMUKEY_CBM_ALT,           {SDL_SCANCODE_RALT});
    // HELP: prefer the rare HELP scancode (117), no common fallback.
    sdl_map.register_candidates(EMUKEY_CBM_HELP,          {SDL_SCANCODE_HELP});
    // LINE FEED: prefer RETURN2 (second Return on ISO/terminal keyboards).
    sdl_map.register_candidates(EMUKEY_CBM_LINE_FEED,     {SDL_SCANCODE_RETURN2});
    // 40/80 DISPLAY: MODE key (rare international keyboards).
    sdl_map.register_candidates(EMUKEY_CBM_40_80_DISPLAY, {SDL_SCANCODE_MODE});
    // NO SCROLL: Scroll Lock (present on most full-size keyboards).
    sdl_map.register_candidates(EMUKEY_CBM_NO_SCROLL,     {SDL_SCANCODE_SCROLLLOCK});

    return mapper;
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(c128_descriptor, [] {
    return std::make_unique<C128System>();
});
