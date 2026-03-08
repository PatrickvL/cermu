/*
 * acorn_atom_system.cpp — Acorn Atom system implementation
 */

#include "acorn_atom_system.h"
#include "../../core/system_registry.h"
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// MC6847 Internal Character ROM (64 chars, 8×8 pixel cells)
//
// Character set layout:
//   Index  0–31: @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_
//   Index 32–63:  !"#$%&'()*+,-./0123456789:;<=>?
//
// Each entry is 8 bytes, one per pixel row, bit 7 = leftmost pixel.
// ============================================================================

static constexpr uint8_t kMC6847Font[64][8] = {
    // 0: @
    {0x3C,0x42,0x9A,0xA2,0xA2,0x9E,0x40,0x3C},
    // 1: A
    {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00},
    // 2: B
    {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},
    // 3: C
    {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00},
    // 4: D
    {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00},
    // 5: E
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00},
    // 6: F
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00},
    // 7: G
    {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00},
    // 8: H
    {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},
    // 9: I
    {0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00},
    // 10: J
    {0x02,0x02,0x02,0x02,0x02,0x42,0x3C,0x00},
    // 11: K
    {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},
    // 12: L
    {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00},
    // 13: M
    {0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00},
    // 14: N
    {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00},
    // 15: O
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    // 16: P
    {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00},
    // 17: Q
    {0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00},
    // 18: R
    {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00},
    // 19: S
    {0x3E,0x40,0x40,0x3C,0x02,0x02,0x7C,0x00},
    // 20: T
    {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x00},
    // 21: U
    {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    // 22: V
    {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00},
    // 23: W
    {0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00},
    // 24: X
    {0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00},
    // 25: Y
    {0x41,0x41,0x22,0x14,0x08,0x08,0x08,0x00},
    // 26: Z
    {0x7E,0x02,0x04,0x18,0x20,0x40,0x7E,0x00},
    // 27: [
    {0x3E,0x20,0x20,0x20,0x20,0x20,0x3E,0x00},
    // 28: backslash
    {0x40,0x20,0x10,0x08,0x04,0x02,0x01,0x00},
    // 29: ]
    {0x7C,0x04,0x04,0x04,0x04,0x04,0x7C,0x00},
    // 30: ^
    {0x18,0x24,0x42,0x00,0x00,0x00,0x00,0x00},
    // 31: _
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},
    // 32: space
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 33: !
    {0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00},
    // 34: "
    {0x28,0x28,0x00,0x00,0x00,0x00,0x00,0x00},
    // 35: #
    {0x24,0x24,0x7E,0x24,0x7E,0x24,0x24,0x00},
    // 36: $
    {0x08,0x3E,0x48,0x38,0x0A,0x7C,0x08,0x00},
    // 37: %
    {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00},
    // 38: &
    {0x30,0x48,0x50,0x20,0x52,0x4C,0x32,0x00},
    // 39: '
    {0x10,0x20,0x00,0x00,0x00,0x00,0x00,0x00},
    // 40: (
    {0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00},
    // 41: )
    {0x10,0x08,0x04,0x04,0x04,0x08,0x10,0x00},
    // 42: *
    {0x00,0x22,0x14,0x7F,0x14,0x22,0x00,0x00},
    // 43: +
    {0x00,0x08,0x08,0x3E,0x08,0x08,0x00,0x00},
    // 44: ,
    {0x00,0x00,0x00,0x00,0x00,0x18,0x08,0x10},
    // 45: -
    {0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00},
    // 46: .
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    // 47: /
    {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00},
    // 48: 0
    {0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C,0x00},
    // 49: 1
    {0x08,0x18,0x08,0x08,0x08,0x08,0x1C,0x00},
    // 50: 2
    {0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00},
    // 51: 3
    {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00},
    // 52: 4
    {0x04,0x0C,0x14,0x24,0x7E,0x04,0x04,0x00},
    // 53: 5
    {0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00},
    // 54: 6
    {0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00},
    // 55: 7
    {0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00},
    // 56: 8
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00},
    // 57: 9
    {0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00},
    // 58: :
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    // 59: ;
    {0x00,0x18,0x18,0x00,0x18,0x08,0x10,0x00},
    // 60: <
    {0x06,0x08,0x10,0x20,0x10,0x08,0x06,0x00},
    // 61: =
    {0x00,0x00,0x3E,0x00,0x3E,0x00,0x00,0x00},
    // 62: >
    {0x60,0x10,0x08,0x04,0x08,0x10,0x60,0x00},
    // 63: ?
    {0x3C,0x42,0x02,0x0C,0x08,0x00,0x08,0x00},
};

// MC6847 color palette (green-on-black for alpha-semigraphics mode)
// CSS=0: green/yellow/blue/red; CSS=1: buff/cyan/magenta/orange
static constexpr uint32_t kMC6847Green  = 0xFF00CC00;
static constexpr uint32_t kMC6847Black  = 0xFF000000;

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor atom_descriptor = {
    "Acorn Atom", "Atom",
    "Acorn Atom — MOS 6502 @ 1MHz, MC6847 VDG, 2KB–12KB RAM (1980)",
    "acorn_atom", {"Atom", "AcornAtom"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

AcornAtomSystem::AcornAtomSystem() : EmulatedSystem(), pins_(ATOM_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = acorn_atom_constants::FB_WIDTH;
    traits.display.native_height   = acorn_atom_constants::FB_HEIGHT;
    traits.display.visible_width   = acorn_atom_constants::FB_WIDTH;
    traits.display.visible_height  = acorn_atom_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = acorn_atom_constants::COLOR_COUNT;
    traits.timing.cpu_frequency_hz = acorn_atom_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = acorn_atom_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
    atom_descriptor.hardware_traits = traits;
}

AcornAtomSystem::~AcornAtomSystem() { delete cpu_; }

const SystemDescriptor& AcornAtomSystem::get_descriptor() const { return atom_descriptor; }
bool AcornAtomSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool AcornAtomSystem::apply_configuration() { return true; }

bool AcornAtomSystem::initialize() {
    printf("Acorn Atom: Initializing system\n");
    cpu_ = new MOS6502();
    pins_ = cpu_->init();
    vdg_.init();
    ppi_.init();
    via_.reset();
    via_.interrupt_bit = BUS_IRQ_BIT;
    ram_.resize(ram_size_kb_ * 1024, 0x00);
    video_ram_.resize(acorn_atom_constants::VIDEO_RAM_SIZE, 0x00);
    basic_rom_.resize(acorn_atom_constants::BASIC_ROM_SIZE, 0xFF);
    fp_rom_.resize(acorn_atom_constants::FP_ROM_SIZE, 0xFF);
    os_rom_.resize(acorn_atom_constants::OS_ROM_SIZE, 0xFF);
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_)); // all keys released (active-low)
    if (!load_roms()) {
        printf("Acorn Atom: Warning — ROMs not loaded, system will not boot correctly\n");
    }
    system_ready_ = true;
    return true;
}

void AcornAtomSystem::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }

void AcornAtomSystem::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    vdg_.init();
    ppi_.init();
    via_.reset();
    via_.interrupt_bit = BUS_IRQ_BIT;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

void AcornAtomSystem::tick() {
    if (!cpu_) return;

    // ---- VIA timer tick — may assert IRQ ----
    {
        bus_state_t vbus = ATOM_BUS_DEFAULT_STATE;
        BUS_SET_BIT(vbus, BUS_RW_BIT);
        vbus = via_.tick(vbus);
        if (!BUS_GET_BIT(vbus, BUS_IRQ_BIT)) {
            BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
        }
    }

    // ---- CPU PHI2 — address/R#W valid on bus ----
    pins_ = cpu_->tick<MOS6502::Phase::PHI2>(pins_);

    // ---- Memory dispatch ----
    pins_ = mem_tick(pins_);

    // ---- CPU PHI1 ----
    pins_ = cpu_->tick<MOS6502::Phase::PHI1>(pins_);

    // Re-assert deasserted control lines for next cycle
    BUS_SET_BIT(pins_, BUS_RW_BIT);

    // ---- VDG timing: one pixel clock per CPU cycle ----
    vdg_.tick();
    if (vdg_.check_fs()) {
        render_frame();
    }

    total_cycles_++;
}

void AcornAtomSystem::run_frame() {
    const uint32_t cycles = static_cast<uint32_t>(
        acorn_atom_constants::CYCLES_PER_FRAME_PAL * speed_multiplier_);
    for (uint32_t i = 0; i < cycles; ++i) tick();
}

bool AcornAtomSystem::load_file(const char*) { return false; }

uint32_t* AcornAtomSystem::get_framebuffer() { return framebuffer_; }

void AcornAtomSystem::get_display_dimensions(int* w, int* h) const {
    *w = acorn_atom_constants::FB_WIDTH; *h = acorn_atom_constants::FB_HEIGHT;
}
void AcornAtomSystem::set_framebuffer(uint32_t*, int, int) {}

uint32_t AcornAtomSystem::get_audio_samples(float*, uint32_t) { return 0; }
void AcornAtomSystem::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }

void AcornAtomSystem::render_system_menu_items() {}
void AcornAtomSystem::render_configuration_ui() {}
void AcornAtomSystem::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// KEYBOARD HANDLING
// ============================================================================
//
// Acorn Atom keyboard matrix (10 rows × 8 columns), scanned through the 8255 PPI:
//   Port A (output): row select — one bit low strobes the corresponding row
//   Port B (input):  column data — bit is low when a key in that column is pressed
//
// Matrix layout (approximate UK Atom keyboard):
//   Row/Col: 7     6     5     4     3     2     1     0
//   0: REPT  COPY  DEL   RET   ]     [     ;     :
//   1: UP    DOWN  RIGHT LEFT  SPACE ?     /     ?
//   2: Z     X     C     V     B     N     M
//   3: A     S     D     F     G     H     J     K
//   4: L     ;     :     @     .     ,
//   5: P     O     I     U     Y     T     R
//   6: Q     W     E
//   7: 0     9     8     7     6     5     4     3     (Row 7: digits)
//   8: 2     1     BS    (via PC0)
//   9: ESC   CTRL  SHIFT (via PC1)
//
// Note: rows 8–9 use PC0–PC1 for row selection (not Port A).
// For this implementation we cover the main 8 rows (rows 0–7) via Port A.
// ============================================================================

void AcornAtomSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Helper: set or clear a bit in keyboard_matrix_ (active-low)
    auto set_key = [&](int row, int col) {
        if (row < 0 || row >= acorn_atom_constants::KEYBOARD_ROWS) return;
        if (pressed)
            keyboard_matrix_[row] &= ~(uint8_t)(1 << col); // press: pull low
        else
            keyboard_matrix_[row] |=  (uint8_t)(1 << col); // release: pull high
    };

    // Map SDL key to (row, col) in the Atom keyboard matrix
    switch (key) {
    // Row 3: A S D F G H J K
    case SDLK_a: set_key(3,7); break;
    case SDLK_s: set_key(3,6); break;
    case SDLK_d: set_key(3,5); break;
    case SDLK_f: set_key(3,4); break;
    case SDLK_g: set_key(3,3); break;
    case SDLK_h: set_key(3,2); break;
    case SDLK_j: set_key(3,1); break;
    case SDLK_k: set_key(3,0); break;
    // Row 4: L ;/: @/^ . ,
    case SDLK_l:         set_key(4,7); break;
    case SDLK_SEMICOLON: set_key(4,6); break;
    case SDLK_AT:        set_key(4,4); break;
    case SDLK_PERIOD:    set_key(4,2); break;
    case SDLK_COMMA:     set_key(4,1); break;
    // Row 5: P O I U Y T R
    case SDLK_p: set_key(5,7); break;
    case SDLK_o: set_key(5,6); break;
    case SDLK_i: set_key(5,5); break;
    case SDLK_u: set_key(5,4); break;
    case SDLK_y: set_key(5,3); break;
    case SDLK_t: set_key(5,2); break;
    case SDLK_r: set_key(5,1); break;
    // Row 6: Q W E
    case SDLK_q: set_key(6,7); break;
    case SDLK_w: set_key(6,6); break;
    case SDLK_e: set_key(6,5); break;
    // Row 7: 0-9 (digits on main keyboard row)
    case SDLK_0: set_key(7,7); break;
    case SDLK_9: set_key(7,6); break;
    case SDLK_8: set_key(7,5); break;
    case SDLK_7: set_key(7,4); break;
    case SDLK_6: set_key(7,3); break;
    case SDLK_5: set_key(7,2); break;
    case SDLK_4: set_key(7,1); break;
    case SDLK_3: set_key(7,0); break;
    // Row 1: SPACE
    case SDLK_SPACE: set_key(1,3); break;
    // Row 0: RETURN
    case SDLK_RETURN: set_key(0,3); break;
    // Row 0: DEL / BACKSPACE
    case SDLK_BACKSPACE: set_key(0,5); break;
    case SDLK_DELETE:    set_key(0,5); break;
    // Row 0: ESC
    case SDLK_ESCAPE: set_key(0,7); break;
    default: break;
    }
}

// ============================================================================
// MEMORY BUS DISPATCH
// ============================================================================

bus_state_t AcornAtomSystem::mem_tick(bus_state_t pins) {
    uint16_t addr  = BUS_GET_ADDR(pins);
    bool     is_rd = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (is_rd) {
        uint8_t data = 0xFF;

        if (addr < (uint16_t)ram_.size()) {
            // $0000–$2BFF: RAM
            data = ram_[addr];
        } else if (addr >= acorn_atom_constants::VIDEO_RAM_BASE &&
                   addr <  acorn_atom_constants::VIDEO_RAM_BASE + (uint16_t)video_ram_.size()) {
            // $8000–$97FF: Video RAM
            data = video_ram_[addr - acorn_atom_constants::VIDEO_RAM_BASE];
        } else if (addr >= acorn_atom_constants::PPI_BASE &&
                   addr <  acorn_atom_constants::PPI_BASE + acorn_atom_constants::PPI_SIZE) {
            // $B000–$B003: Intel 8255 PPI
            // Before reading Port B, update column data from keyboard matrix.
            // Port A (output) selects which rows to scan; active-low strobe.
            uint8_t row_sel = ppi_.get_port_a_output();
            uint8_t cols = 0xFF;
            for (int r = 0; r < 8; r++) {
                if (!(row_sel & (1u << r))) {
                    cols &= keyboard_matrix_[r];
                }
            }
            ppi_.set_port_b_input(cols);
            data = ppi_.read(static_cast<uint8_t>(addr - acorn_atom_constants::PPI_BASE));
        } else if (addr >= acorn_atom_constants::VIA_BASE &&
                   addr <  acorn_atom_constants::VIA_BASE + acorn_atom_constants::VIA_SIZE) {
            // $B800–$B80F: MOS 6522 VIA
            bus_state_t v = ATOM_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(v, addr - acorn_atom_constants::VIA_BASE);
            BUS_SET_BIT(v, BUS_RW_BIT);
            v    = via_.registers_read(v);
            data = BUS_GET_DATA(v);
        } else if (addr >= acorn_atom_constants::BASIC_ROM_BASE &&
                   addr <  acorn_atom_constants::BASIC_ROM_BASE + (uint16_t)basic_rom_.size()) {
            // $C000–$CFFF: BASIC ROM
            data = basic_rom_[addr - acorn_atom_constants::BASIC_ROM_BASE];
        } else if (addr >= acorn_atom_constants::FP_ROM_BASE &&
                   addr <  acorn_atom_constants::FP_ROM_BASE + (uint16_t)fp_rom_.size()) {
            // $D000–$D7FF: Floating-point ROM
            data = fp_rom_[addr - acorn_atom_constants::FP_ROM_BASE];
        } else if (addr >= acorn_atom_constants::OS_ROM_BASE &&
                   addr <  acorn_atom_constants::OS_ROM_BASE + (uint16_t)os_rom_.size()) {
            // $F000–$FFFF: OS ROM
            data = os_rom_[addr - acorn_atom_constants::OS_ROM_BASE];
        }

        BUS_SET_DATA(pins, data);
    } else {
        uint8_t data = BUS_GET_DATA(pins);

        if (addr < (uint16_t)ram_.size()) {
            ram_[addr] = data;
        } else if (addr >= acorn_atom_constants::VIDEO_RAM_BASE &&
                   addr <  acorn_atom_constants::VIDEO_RAM_BASE + (uint16_t)video_ram_.size()) {
            video_ram_[addr - acorn_atom_constants::VIDEO_RAM_BASE] = data;
        } else if (addr >= acorn_atom_constants::PPI_BASE &&
                   addr <  acorn_atom_constants::PPI_BASE + acorn_atom_constants::PPI_SIZE) {
            ppi_.write(static_cast<uint8_t>(addr - acorn_atom_constants::PPI_BASE), data);
            // Port A write: row strobe changed — column data will be refreshed on next read
        } else if (addr >= acorn_atom_constants::VIA_BASE &&
                   addr <  acorn_atom_constants::VIA_BASE + acorn_atom_constants::VIA_SIZE) {
            bus_state_t v = ATOM_BUS_DEFAULT_STATE;
            BUS_SET_ADDR(v, addr - acorn_atom_constants::VIA_BASE);
            BUS_SET_DATA(v, data);
            BUS_CLR_BIT(v, BUS_RW_BIT);
            via_.registers_write(v);
        }
        // Writes to ROM regions are silently ignored
    }

    return pins;
}

bus_state_t AcornAtomSystem::io_tick(bus_state_t pins) {
    // The Acorn Atom uses memory-mapped I/O (no separate I/O address space)
    return pins;
}

// ============================================================================
// DISPLAY RENDERING — MC6847 VDG
// ============================================================================
//
// In alpha (text) mode (AG=0, AS=0):
//   32 columns × 16 rows; each cell 8×12 pixels = 256×192 display.
//   Video RAM byte: bit7=INV, bit6=AS, bits5–0 = character index (0–63).
//
// In semigraphics-4 mode (AG=0, AS=1):
//   32 columns × 16 rows; each cell splits into four 4×6-pixel quadrants.
//   Byte: bits 5–4 = color, bits 3–0 = quadrant enable mask.
//
// Graphics modes (AG=1) are not yet rendered; framebuffer is left black.
// ============================================================================

void AcornAtomSystem::render_frame() {
    // Each cell is 8 pixels wide × 12 pixels tall → 32×8 = 256, 16×12 = 192
    static constexpr int CELL_W  = 8;
    static constexpr int CELL_H  = 12;
    static constexpr int VCOLS   = acorn_atom_constants::TEXT_COLS;
    static constexpr int VROWS   = acorn_atom_constants::TEXT_ROWS;

    if (vdg_.is_graphics_mode()) {
        // Full-graphics modes: clear to black (placeholder)
        std::memset(framebuffer_, 0, sizeof(framebuffer_));
        return;
    }

    for (int row = 0; row < VROWS; row++) {
        for (int col = 0; col < VCOLS; col++) {
            uint8_t byte = video_ram_[row * VCOLS + col];
            bool     inv = (byte & 0x80) != 0;
            bool     sem = (byte & 0x40) != 0;
            uint8_t  chr = byte & 0x3F;

            int fb_x = col * CELL_W;
            int fb_y = row * CELL_H;

            if (sem) {
                // Semigraphics-4: 4 quadrants, 2 colours
                uint8_t color_idx = (byte >> 4) & 0x03;
                (void)color_idx;  // colour set expansion reserved for later
                uint32_t fg = kMC6847Green;
                uint32_t bg = kMC6847Black;
                // Quadrant bits: bit3=TL, bit2=TR, bit1=BL, bit0=BR
                for (int qr = 0; qr < 2; qr++) {       // top/bottom half
                    for (int qc = 0; qc < 2; qc++) {   // left/right half
                        int bit = (1 - qr) * 2 + (1 - qc);
                        uint32_t col_pix = (chr & (1 << bit)) ? fg : bg;
                        int px0 = fb_x + qc * (CELL_W / 2);
                        int py0 = fb_y + qr * (CELL_H / 2);
                        for (int dy = 0; dy < CELL_H / 2; dy++) {
                            for (int dx = 0; dx < CELL_W / 2; dx++) {
                                int px = px0 + dx;
                                int py = py0 + dy;
                                if (px < acorn_atom_constants::FB_WIDTH &&
                                    py < acorn_atom_constants::FB_HEIGHT)
                                    framebuffer_[py * acorn_atom_constants::FB_WIDTH + px] = col_pix;
                            }
                        }
                    }
                }
            } else {
                // Alpha mode: render character from internal MC6847 font ROM
                const uint8_t* glyph = kMC6847Font[chr];
                uint32_t fg = inv ? kMC6847Black : kMC6847Green;
                uint32_t bg = inv ? kMC6847Green : kMC6847Black;

                for (int gy = 0; gy < CELL_H; gy++) {
                    // Font rows 0–7 mapped to cell rows 2–9, blank above/below
                    uint8_t bits = (gy >= 2 && gy < 10) ? glyph[gy - 2] : 0x00;
                    for (int gx = 0; gx < CELL_W; gx++) {
                        int    px  = fb_x + gx;
                        int    py  = fb_y + gy;
                        bool   set = (bits & (0x80u >> gx)) != 0;
                        framebuffer_[py * acorn_atom_constants::FB_WIDTH + px] = set ? fg : bg;
                    }
                }
            }
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

bool AcornAtomSystem::load_roms() {
    char rom_root[512];
    const char* names[] = {"acorn_atom", "atom", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        printf("Acorn Atom: ROM path not found\n");
        return false;
    }

    bool ok = true;

    // Atom BASIC ROM (~4 KB at $C000)
    const char* basic_names[] = {"atom_basic.rom", "BASIC.ROM", "basic.rom", nullptr};
    if (!rom_loader_load_from_root(rom_root, basic_names,
                                   acorn_atom_constants::BASIC_ROM_SIZE,
                                   basic_rom_.data(), basic_rom_.size())) {
        printf("Acorn Atom: BASIC ROM not loaded\n");
        ok = false;
    }

    // Floating-point ROM (~2 KB at $D000, optional)
    const char* fp_names[] = {"atom_fp.rom", "FP.ROM", "fp.rom", nullptr};
    rom_loader_load_from_root(rom_root, fp_names,
                              acorn_atom_constants::FP_ROM_SIZE,
                              fp_rom_.data(), fp_rom_.size());  // optional

    // OS / Monitor ROM (~4 KB at $F000)
    const char* os_names[] = {"atom_os.rom", "ABASIC.ROM", "os.rom", nullptr};
    if (!rom_loader_load_from_root(rom_root, os_names,
                                   acorn_atom_constants::OS_ROM_SIZE,
                                   os_rom_.data(), os_rom_.size())) {
        printf("Acorn Atom: OS ROM not loaded\n");
        ok = false;
    }

    return ok;
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(atom_descriptor, [] { return std::make_unique<AcornAtomSystem>(); });
