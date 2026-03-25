/*
 * acorn_atom_system.cpp — Acorn Atom system implementation
 */

#include "systems/acorn_atom/acorn_atom_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor atom_descriptor = {
    "Acorn Atom", "Atom",
    "Acorn Atom — MOS 6502 @ 1MHz, MC6847 VDG, 2KB–12KB RAM (1980)",
    "acorn_atom", {"Atom", "AcornAtom"},
    nullptr, {}, nullptr,
    "Acorn", 1980, "MOS 6502", SystemType::Home
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

AcornAtomSystem::AcornAtomSystem() : System(), pins_(ATOM_BUS_DEFAULT_STATE) {
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

AcornAtomSystem::~AcornAtomSystem() = default;

const SystemDescriptor& AcornAtomSystem::get_descriptor() const { return atom_descriptor; }
bool AcornAtomSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool AcornAtomSystem::apply_configuration() { return true; }

bool AcornAtomSystem::initialize() {
    printf("Acorn Atom: Initializing system\n");
    register_board(&board_);

    // ── Create chips from manifest and bind chipset ───────────────────
    board_.bind_chipset();
    board_.create_chips(&pins_);
    basic_rom_ = board_.find<ROMChip>();
    fp_rom_    = board_.find<ROMChip>(1);
    os_rom_    = board_.find<ROMChip>(2);

    // Direct pointer for MC6847 rendering
    video_ram_ptr_ = board_.find<RAMChip>(1)->data();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.cpu().init();
    board_.video().init();
    board_.io().init();
    board_.io().set_port_b_read_callback(ppi_keyboard_scan, this);
    board_.chips().via.reset();
    board_.chips().via.interrupt_bit = BUS_IRQ_BIT;

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_)); // all keys released (active-low)

    // ── Trim page tables to current RAM config ──────────────────────────
    configure_bus_memory_map();

    // ── Load ROMs into flat mem ───────────────────────────────────
    if (!load_roms()) {
        printf("Acorn Atom: Warning — ROMs not loaded, system will not boot correctly\n");
    }

    // ── Register all manifest-created chips for Hardware menu ────────
    register_bus_chips(board_);

    // Video output — composite video from MC6847 VDG
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.video().set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    printf("Acorn Atom: System initialized (RAM: %dKB)\n", ram_size_kb_);
    system_ready_ = true;
    return true;
}

void AcornAtomSystem::shutdown() { system_ready_ = false; }

void AcornAtomSystem::reset() {
    if (!system_ready_) return;
    pins_ = board_.cpu().reset(pins_);
    // Reset all manifest chips (VDG, PPI, VIA; RAM/ROM are no-op)
    board_.reset_chips();
    board_.io().set_port_b_read_callback(ppi_keyboard_scan, this);
    board_.chips().via.interrupt_bit = BUS_IRQ_BIT;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

void AcornAtomSystem::tick() {
    if (!system_ready_) return;

    // ---- VIA timer tick — may assert IRQ ----
    {
        bus_state_t vbus = ATOM_BUS_DEFAULT_STATE;
        BUS_SET_BIT(vbus, BUS_RW_BIT);
        vbus = board_.chips().via.tick(vbus);
        if (!BUS_GET_BIT(vbus, BUS_IRQ_BIT)) {
            BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
        }
    }

    // ---- CPU PHI2 — address/R#W valid on bus ----
    pins_ = board_.cpu().tick<MOS6502::Phase::PHI2>(pins_);

    // ---- Memory dispatch via MemoryBus ----
    pins_ = bus_.tick(pins_);

    // ---- CPU PHI1 ----
    pins_ = board_.cpu().tick<MOS6502::Phase::PHI1>(pins_);

    // Re-assert deasserted control lines for next cycle
    BUS_SET_BIT(pins_, BUS_RW_BIT);

    // ---- VDG timing: one pixel clock per CPU cycle ----
    board_.video().tick();
    if (board_.video().check_fs()) {
        render_frame();
    }

    total_cycles_++;
}

void AcornAtomSystem::run_frame() {
    if (!video_port_) return;

    // Stream-driven: MC6847 VDG drives FrameEnd on Field Sync
    auto& output = video_port_->output();
    const int frames = (speed_multiplier_ > 1.0) ? static_cast<int>(speed_multiplier_) : 1;
    for (int f = 0; f < frames; f++) {
        while (!output.frame_ended()) {
            tick();
        }
        video_port_->swap_frame();
    }
}


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
// BUS CONFIGURATION
// ============================================================================

void AcornAtomSystem::configure_bus_memory_map() {
    // Trim RAM to configured size BEFORE apply() — effective_size limits
    // Phase 1 page mapping.  MMIO sub-tables on pages beyond the effective
    // range automatically capture kNoChipSelected as their base chip.
    board_.set_effective_size(board_.find_index<RAMChip>(),
                              ram_size_kb_ * 1024);
    board_.apply(bus_);
}

// ============================================================================
// KEYBOARD MATRIX CALLBACK — called by i8255_t before Port B reads
// ============================================================================

uint8_t AcornAtomSystem::ppi_keyboard_scan(void* context, uint8_t port_a_output) {
    auto* sys = static_cast<AcornAtomSystem*>(context);
    uint8_t cols = 0xFF;
    for (int r = 0; r < 8; r++) {
        if (!(port_a_output & (1u << r))) {
            cols &= sys->keyboard_matrix_[r];
        }
    }
    return cols;
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
    // Delegate rendering to the MC6847 chip — it owns the font ROM,
    // palette, and frame index buffer.
    board_.video().render_frame(video_ram_ptr_);
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
    return board_.load_roms(rom_root, "Acorn Atom");
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(atom_descriptor, [] { return std::make_unique<AcornAtomSystem>(); });
