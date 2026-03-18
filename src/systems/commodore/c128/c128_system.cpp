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
    nullptr
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

C128System::C128System()
    : CommodoreSystem()
    , pins_(C128_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_c128_hardware_traits();
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
    board_.create_chips(&pins_);

    cpu_8502_     = board_.cpu<CSG8502>();
    cpu_z80_      = board_.find<ZilogZ80A>();
    vic_          = board_.find<vicii_t>();
    sid_          = board_.find<mos6581_t>();
    cia1_         = board_.find<mos6526_t>();
    cia2_         = board_.find<mos6526_t>(1);
    basic_lo_rom_ = board_.find<ROMChip>();
    basic_hi_rom_ = board_.find<ROMChip>(1);
    editor_rom_   = board_.find<ROMChip>(2);
    kernal_rom_   = board_.find<ROMChip>(3);
    char_rom_     = board_.find<ROMChip>(4);
    vdc_vram_     = board_.find<RAMChip>(1);

    pins_ = board_.cpu_chip()->init();

    configure_bus_memory_map();
    if (!load_roms()) {
        printf("C128: Warning — ROMs not loaded\n");
    }

    register_bus_chips(board_);

    display_.init(c128_constants::VIC_DISPLAY_WIDTH_PAL,
                  c128_constants::VIC_DISPLAY_HEIGHT_PAL);
    register_display(&display_);

    system_ready_ = true;
    printf("C128: System initialized\n");
    return true;
}

void C128System::shutdown() { system_ready_ = false; }

void C128System::reset() {
    if (!cpu_8502_) return;
    pins_ = board_.cpu_chip()->reset(pins_);
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
    // TODO: implement tick — dual-CPU dispatch + VIC-IIe + SID + CIA + MMU
}

void C128System::run_frame() {
    if (!system_ready_) return;
    for (uint32_t i = 0; i < cycles_per_frame_; ++i) {
        tick();
        check_deferred_load();
    }
}

// ============================================================================
// DISPLAY
// ============================================================================

void C128System::get_display_dimensions(int* width, int* height) const {
    // Default to VIC-IIe 40-column display
    if (width) *width = c128_constants::VIC_DISPLAY_WIDTH_PAL;
    if (height) *height = c128_constants::VIC_DISPLAY_HEIGHT_PAL;
}

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
// GUI (stubs)
// ============================================================================

void C128System::render_system_menu_items() {}
void C128System::render_configuration_ui() {}
void C128System::set_speed_multiplier(float /*multiplier*/) {}

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
    // TODO: setup page tables based on MMU configuration
}

bool C128System::load_roms() {
    return board_.load_roms("c128");
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
