#include "systems/chip8/chip8_system.hpp"
#include "systems/chip8/chip8_constants.hpp"
#include "core/chip.hpp"
#include "core/vfs/vfs.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>

// SDL is only needed for keyboard mapping in GUI builds
#ifdef CERMU_HAS_GUI
#include <SDL.h>
#include <imgui.h>
#endif

// ============================================================================
// Font Data
// ============================================================================

// Standard CHIP-8 font — 5 bytes per glyph, 16 glyphs (0–F)
static const uint8_t chip8_font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// SCHIP hi-res font — 10 bytes per glyph, 16 glyphs (0–F)
// Stored at offset 80 in memory (right after the standard font)
static const uint8_t schip_font[160] = {
    0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
    0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
    0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
    0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
};

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_chip8_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits — always 128×64 (lo-res is pixel-doubled)
    traits.display.native_width = chip8_constants::HIRES_WIDTH;
    traits.display.native_height = chip8_constants::HIRES_HEIGHT;
    traits.display.visible_width = chip8_constants::HIRES_WIDTH;
    traits.display.visible_height = chip8_constants::HIRES_HEIGHT;
    traits.display.format = FramebufferFormat::PALETTE_INDEXED_2;  // 2-bit for XO-CHIP dual-plane
    traits.display.palette_size = 4;  // 4 colors (XO-CHIP dual-plane)
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = false;
    
    // Default 4-color palette (supports all modes)
    traits.display.default_palette.push_back(PaletteColor(0, 0, 0, 255));       // 0: Background
    traits.display.default_palette.push_back(PaletteColor(0, 255, 0, 255));     // 1: Plane 1
    traits.display.default_palette.push_back(PaletteColor(0, 0, 255, 255));     // 2: Plane 2
    traits.display.default_palette.push_back(PaletteColor(255, 255, 255, 255)); // 3: Both planes
    
    // Audio traits
    traits.audio.format = AudioFormat::MONO_8BIT;
    traits.audio.sample_rate_hz = chip8_constants::AUDIO_SAMPLE_RATE;
    traits.audio.channels = 1;
    traits.audio.chip_name = "Beeper / XO-CHIP Audio";
    
    // Timing
    traits.timing.cpu_frequency_hz = 600;
    traits.timing.video_frequency_hz = chip8_constants::TIMER_HZ;
    traits.timing.audio_sample_rate_hz = chip8_constants::AUDIO_SAMPLE_RATE;
    traits.timing.target_fps = chip8_constants::TIMER_HZ;
    traits.timing.cycles_per_frame = 10;
    traits.timing.standard = VideoStandard::NTSC;
    
    // Memory options
    traits.memory_options.push_back({"4KB Standard", 4096, 0, true});
    traits.memory_options.push_back({"64KB (XO-CHIP)", 65536, 0, false});
    
    // Speed presets
    traits.video_standard_configs.push_back({
        "Standard (600 Hz)", VideoStandard::NTSC, traits.timing, true
    });
    
    SystemTiming fast = traits.timing;
    fast.cpu_frequency_hz = 1200;
    fast.cycles_per_frame = 20;
    traits.video_standard_configs.push_back({
        "Fast (1200 Hz)", VideoStandard::CUSTOM, fast, false
    });
    
    SystemTiming xo = traits.timing;
    xo.cpu_frequency_hz = 1000;
    xo.cycles_per_frame = 17;
    traits.video_standard_configs.push_back({
        "XO-CHIP (1000 Hz)", VideoStandard::CUSTOM, xo, false
    });
    
    // Custom options — interpreter mode
    CustomOption mode_opt;
    mode_opt.id = "chip8_mode";
    mode_opt.name = "Interpreter Mode";
    mode_opt.description = "Select CHIP-8 variant (Auto detects from ROM)";
    mode_opt.choices = {"Auto", "CHIP-8", "SCHIP 1.1", "XO-CHIP"};
    mode_opt.default_index = 0;
    traits.custom_options.push_back(mode_opt);
    
    return traits;
}

// ============================================================================
// ROM analysis — scan for extended instructions to determine mode
// ============================================================================
static Chip8Mode detect_mode_from_rom(const uint8_t* data, size_t size) {
    bool uses_schip = false;
    bool uses_xochip = false;

    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t op = (data[i] << 8) | data[i + 1];

        uint8_t hi = (op >> 12) & 0xF;
        uint8_t lo = op & 0xFF;

        // SCHIP instructions
        if ((op & 0xFFF0) == 0x00C0) uses_schip = true;   // 00Cn: scroll down
        if (op == 0x00FB) uses_schip = true;                // scroll right
        if (op == 0x00FC) uses_schip = true;                // scroll left
        if (op == 0x00FD) uses_schip = true;                // EXIT
        if (op == 0x00FE) uses_schip = true;                // lo-res
        if (op == 0x00FF) uses_schip = true;                // hi-res
        if (hi == 0xD && (op & 0xF) == 0) uses_schip = true; // DXY0: 16x16 sprite
        if (hi == 0xF && lo == 0x30) uses_schip = true;      // FX30: hi-res font
        if (hi == 0xF && (lo == 0x75 || lo == 0x85)) uses_schip = true; // RPL flags

        // XO-CHIP instructions
        if ((op & 0xFFF0) == 0x00D0) uses_xochip = true;   // 00Dn: scroll up
        if (op == 0xF000) uses_xochip = true;                // F000 NNNN: long I
        if ((op & 0xFF00) == 0xF000 && lo == 0x02) uses_xochip = true; // F002: audio
        if (hi == 0xF && lo == 0x3A) uses_xochip = true;     // F03A: pitch
        if (hi == 0x5 && (op & 0xF) == 2) uses_xochip = true; // 5XY2: save range
        if (hi == 0x5 && (op & 0xF) == 3) uses_xochip = true; // 5XY3: load range
        if (hi == 0xF && lo == 0x01) uses_xochip = true;      // FN01: planes
    }

    if (uses_xochip) return Chip8Mode::XOCHIP;
    if (uses_schip)  return Chip8Mode::SCHIP;
    return Chip8Mode::CHIP8;
}

// System file detection + configuration probe
static SystemProbeResult chip8_probe_file(
    const format_descriptor_t* /*matched_format*/,
    const char* filepath, const uint8_t* data, size_t size) {

    SystemProbeResult result = { 0.0f, {} };

    // Extension-based confidence
    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
    if (ext) {
        if (strcmp(ext, ".ch8") == 0 || strcmp(ext, ".c8") == 0)  result.confidence = 0.9f;
        else if (strcmp(ext, ".sc8") == 0)  result.confidence = 0.95f;
        else if (strcmp(ext, ".xo8") == 0)  result.confidence = 0.95f;
    }

    // Heuristic fallback for unknown extensions — require CHIP-8 opcode
    // validation to avoid claiming every small binary (especially PRG files
    // extracted from Commodore containers where the extension may be lost).
    if (result.confidence == 0.0f && data && size >= 10 && size <= 65024) {
        // A valid CHIP-8 ROM starts at $200.  Scan the first N words for
        // valid CHIP-8 opcodes; if the ratio is high, claim it.
        size_t check = std::min(size, (size_t)256);
        int valid = 0, total = 0;
        for (size_t i = 0; i + 1 < check; i += 2) {
            uint16_t op = (data[i] << 8) | data[i + 1];
            uint8_t hi = (op >> 12) & 0xF;
            uint8_t lo = op & 0xFF;
            bool ok = false;
            switch (hi) {
                case 0x0: ok = (op == 0x00E0 || op == 0x00EE ||         // CLS, RET
                                (op & 0xFFF0) == 0x00C0 ||              // SCD (SCHIP)
                                op == 0x00FB || op == 0x00FC ||         // SCR/SCL
                                op == 0x00FD || op == 0x00FE ||         // EXIT/LORES
                                op == 0x00FF ||                          // HIRES
                                (op & 0xFFF0) == 0x00D0);               // SCU (XO-CHIP)
                          break;
                case 0x1: ok = true; break;  // JP addr
                case 0x2: ok = true; break;  // CALL addr
                case 0x3: ok = true; break;  // SE Vx, byte
                case 0x4: ok = true; break;  // SNE Vx, byte
                case 0x5: ok = (lo & 0x0F) <= 3; break;  // SE Vx, Vy + XO-CHIP
                case 0x6: ok = true; break;  // LD Vx, byte
                case 0x7: ok = true; break;  // ADD Vx, byte
                case 0x8: ok = (lo & 0x0F) <= 7 || (lo & 0x0F) == 0xE; break;  // ALU
                case 0x9: ok = (lo & 0x0F) == 0; break;  // SNE Vx, Vy
                case 0xA: ok = true; break;  // LD I, addr
                case 0xB: ok = true; break;  // JP V0, addr
                case 0xC: ok = true; break;  // RND Vx, byte
                case 0xD: ok = true; break;  // DRW Vx, Vy, n
                case 0xE: ok = (lo == 0x9E || lo == 0xA1); break;  // SKP/SKNP
                case 0xF: ok = (lo == 0x07 || lo == 0x0A || lo == 0x15 ||
                                lo == 0x18 || lo == 0x1E || lo == 0x29 ||
                                lo == 0x30 || lo == 0x33 || lo == 0x55 ||
                                lo == 0x65 || lo == 0x75 || lo == 0x85 ||
                                lo == 0x00 || lo == 0x01 || lo == 0x02 ||
                                lo == 0x3A); break;
                default: break;
            }
            total++;
            if (ok) valid++;
        }
        float ratio = total > 0 ? (float)valid / (float)total : 0.0f;

        // Require the first instruction to look like CHIP-8 entry point
        // (JP, CALL, or CLS).  This filters out 6502 ML blobs.
        bool valid_entry = false;
        if (size >= 2) {
            uint8_t hi = (data[0] >> 4) & 0xF;
            valid_entry = (hi == 0x1 || hi == 0x2 ||     // JP addr, CALL addr
                           hi == 0x6 || hi == 0xA ||     // LD Vx,byte  LD I,addr
                           (data[0] == 0x00 && data[1] == 0xE0));  // CLS
        }

        if (ratio >= 0.80f && valid_entry)
            result.confidence = 0.45f;
        else if (ratio >= 0.60f && valid_entry)
            result.confidence = 0.30f;
        // else: not convincing enough for CHIP-8
    }

    if (result.confidence == 0.0f) return result;

    // --- Detect optimal configuration ---

    // Mode from extension first
    Chip8Mode detected = Chip8Mode::CHIP8;
    if (ext) {
        if (strcmp(ext, ".sc8") == 0) detected = Chip8Mode::SCHIP;
        else if (strcmp(ext, ".xo8") == 0) detected = Chip8Mode::XOCHIP;
    }

    // If no extension hint, scan ROM for extended instructions
    if (detected == Chip8Mode::CHIP8 && data && size > 0) {
        detected = detect_mode_from_rom(data, size);
    }

    // Auto-extend memory for large ROMs
    if (size > chip8_constants::MAX_ROM_STANDARD) {
        detected = Chip8Mode::XOCHIP;
        result.configuration.memory_option_index = 1;  // 64KB
        printf("CHIP8: ROM size %zu > %u, selecting XO-CHIP mode with 64KB\n", size, chip8_constants::MAX_ROM_STANDARD);
    }

    switch (detected) {
        case Chip8Mode::SCHIP:
            result.configuration.custom_settings["chip8_mode"] = "SCHIP 1.1";
            result.configuration.region_option_index = 1;  // Fast (1200 Hz)
            printf("CHIP8: Detected SCHIP mode\n");
            break;
        case Chip8Mode::XOCHIP:
            result.configuration.custom_settings["chip8_mode"] = "XO-CHIP";
            result.configuration.memory_option_index = 1;  // 64KB
            result.configuration.region_option_index = 2;   // XO-CHIP (1000 Hz)
            printf("CHIP8: Detected XO-CHIP mode\n");
            break;
        default:
            result.configuration.custom_settings["chip8_mode"] = "CHIP-8";
            if (size > 2048) result.configuration.region_option_index = 1;
            break;
    }

    return result;
}

static SystemDescriptor chip8_descriptor = {
    "CHIP-8 / SCHIP / XO-CHIP",
    "CHIP8",
    "CHIP-8 interpreter with Super-CHIP and XO-CHIP extensions",
    nullptr,  // no data folder
    {"CHIP8", "CHIP-8", "SCHIP", "XO-CHIP"},
    nullptr,
    create_chip8_hardware_traits(),
    chip8_probe_file,
    nullptr, 1977, "CHIP-8 VM", SystemType::Other
};

// ============================================================================
// Constructor
// ============================================================================
Chip8System::Chip8System()
    : System()
    , mode_(Chip8Mode::CHIP8)
    , memory_(4096, 0)
    , hires_(false)
    , active_plane_mask_(1)
    , cycles_per_frame_(10)
    , display_dirty_(false)
    , wait_for_key_(false)
    , wait_key_reg_(0)
    , shift_quirk_(false)
    , load_store_quirk_(false)
    , jump_quirk_(false)
    , clip_quirk_(true)
    , vf_reset_quirk_(true)
    , beeper_phase_(0)
    , pitch_register_(64)
    , has_audio_pattern_(false)
{
    // Register main board (no ports, but required by System)
    register_board(&board_);

    hardware_traits_ = create_chip8_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    memset(rpl_flags_, 0, sizeof(rpl_flags_));
    memset(audio_pattern_, 0, sizeof(audio_pattern_));
    reset();

    // Register logical chips for the Hardware menu (no debug windows)
    register_chip8_chips();

    // GPU indexed palette rendering
    memset(pixel_buffer_, 0, sizeof(pixel_buffer_));
    {
        uint32_t pal[4] = {};
        for (int i = 0; i < 4 && i < static_cast<int>(current_palette_.size()); i++)
            pal[i] = current_palette_[i].to_rgba32();
        palette_.set(pal, 4);
    }

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(), chip8_constants::HIRES_WIDTH, 1);
    video_port_->set_palette(palette_.data(), 4);
    video_port_->bind_frame_output(&last_frame_data_);
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& Chip8System::get_descriptor() const {
    return chip8_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

bool Chip8System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    
    // Apply palette selection
    auto palette_it = config.custom_settings.find("display_palette");
    if (palette_it != config.custom_settings.end()) {
        const std::string& pal = palette_it->second;
        
        // Colors: [bg, plane1, plane2, both]
        if (pal == "green") {
            current_palette_ = {
                PaletteColor(0, 0, 0, 255),
                PaletteColor(0, 255, 0, 255),
                PaletteColor(0, 128, 0, 255),
                PaletteColor(128, 255, 128, 255)
            };
        } else if (pal == "amber") {
            current_palette_ = {
                PaletteColor(0, 0, 0, 255),
                PaletteColor(255, 176, 0, 255),
                PaletteColor(128, 88, 0, 255),
                PaletteColor(255, 220, 128, 255)
            };
        } else if (pal == "white") {
            current_palette_ = {
                PaletteColor(0, 0, 0, 255),
                PaletteColor(255, 255, 255, 255),
                PaletteColor(128, 128, 128, 255),
                PaletteColor(200, 200, 255, 255)
            };
        } else if (pal == "c64") {
            current_palette_ = {
                PaletteColor(0x40, 0x31, 0x8D, 255),
                PaletteColor(0x7B, 0x70, 0xFC, 255),
                PaletteColor(0x58, 0x4F, 0xC4, 255),
                PaletteColor(0xA0, 0x98, 0xFF, 255)
            };
        }
        // Sync GPU palette
        for (int i = 0; i < 4 && i < static_cast<int>(current_palette_.size()); i++)
            palette_.set_entry(i, current_palette_[i].to_rgba32());
    }
    
    // Apply mode selection
    auto mode_it = config.custom_settings.find("chip8_mode");
    if (mode_it != config.custom_settings.end()) {
        const std::string& m = mode_it->second;
        if (m == "CHIP-8")      mode_ = Chip8Mode::CHIP8;
        else if (m == "SCHIP 1.1")  mode_ = Chip8Mode::SCHIP;
        else if (m == "XO-CHIP")    mode_ = Chip8Mode::XOCHIP;
        // "Auto" is handled by probe_file during identification
    }
    
    display_dirty_ = true;
    return true;
}

bool Chip8System::apply_configuration() {
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        cycles_per_frame_ = hardware_traits_.video_standard_configs[config_.region_option_index]
                                .timing.cycles_per_frame;
    }
    
    // Resize memory for XO-CHIP
    size_t needed = (mode_ == Chip8Mode::XOCHIP) ? 65536 : 4096;
    if (memory_.size() < needed) {
        memory_.resize(needed, 0);
    }
    
    // Set default quirks per mode
    switch (mode_) {
        case Chip8Mode::CHIP8:
            shift_quirk_ = false;
            load_store_quirk_ = false;
            jump_quirk_ = false;
            clip_quirk_ = true;
            vf_reset_quirk_ = true;
            break;
        case Chip8Mode::SCHIP:
            shift_quirk_ = true;   // SCHIP uses Vx for shifts
            load_store_quirk_ = false;
            jump_quirk_ = true;    // BXNN jumps to XNN + Vx
            clip_quirk_ = true;
            vf_reset_quirk_ = false;
            break;
        case Chip8Mode::XOCHIP:
            shift_quirk_ = false;
            load_store_quirk_ = false;
            jump_quirk_ = false;
            clip_quirk_ = false;   // XO-CHIP wraps sprites
            vf_reset_quirk_ = false;
            break;
    }
    
    return true;
}

// ============================================================================
// Audio
// ============================================================================

uint32_t Chip8System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    if (mode_ == Chip8Mode::XOCHIP && has_audio_pattern_) {
        // XO-CHIP: play 16-byte (128-bit) pattern as a waveform
        // Pitch formula: rate = 4000 * 2^((pitch - 64) / 48)
        double rate = 4000.0 * pow(2.0, (pitch_register_ - 64.0) / 48.0);
        double step = rate / 4000.0;  // samples per output sample
        
        for (uint32_t i = 0; i < max_samples; i++) {
            if (sound_timer_ > 0) {
                // 128-bit pattern: index into 16 bytes × 8 bits
                int bit_pos = static_cast<int>(beeper_phase_) % 128;
                int byte_idx = bit_pos / 8;
                int bit_idx = 7 - (bit_pos % 8);
                bool on = (audio_pattern_[byte_idx] >> bit_idx) & 1;
                buffer[i] = on ? 0.3f : -0.3f;
                beeper_phase_ += static_cast<uint32_t>(step);
            } else {
                buffer[i] = 0.0f;
                beeper_phase_ = 0;
            }
        }
    } else {
        // Standard CHIP-8 / SCHIP: 440 Hz square wave
        const uint32_t half_period = 5;  // 4000 / (440 * 2) ≈ 4.5
        const float amplitude = 0.3f;

        for (uint32_t i = 0; i < max_samples; i++) {
            if (sound_timer_ > 0) {
                buffer[i] = (beeper_phase_ < half_period) ? amplitude : -amplitude;
                beeper_phase_ = (beeper_phase_ + 1) % (half_period * 2);
            } else {
                buffer[i] = 0.0f;
                beeper_phase_ = 0;
            }
        }
    }
    return max_samples;
}

// ============================================================================
// Reset
// ============================================================================

void Chip8System::reset() {
    std::fill(memory_.begin(), memory_.end(), 0);
    memset(V_, 0, sizeof(V_));
    memset(stack_, 0, sizeof(stack_));
    memset(planes_, 0, sizeof(planes_));
    memset(keys_, 0, sizeof(keys_));
    memset(rpl_flags_, 0, sizeof(rpl_flags_));
    memset(audio_pattern_, 0, sizeof(audio_pattern_));
    
    // Load fonts into lower memory
    memcpy(memory_.data(), chip8_font, sizeof(chip8_font));
    memcpy(memory_.data() + 80, schip_font, sizeof(schip_font));
    
    I_ = 0;
    PC_ = chip8_constants::PROGRAM_START;
    SP_ = 0;
    delay_timer_ = 0;
    sound_timer_ = 0;
    hires_ = false;
    active_plane_mask_ = 1;
    wait_for_key_ = false;
    wait_key_reg_ = 0;
    pitch_register_ = 64;
    has_audio_pattern_ = false;
    
    total_cycles_ = 0;
    display_dirty_ = true;
}

// ============================================================================
// Execution
// ============================================================================

void Chip8System::tick() {
    if (wait_for_key_) {
        // Blocked on FX0A — check if any key is pressed
        for (int i = 0; i < 16; i++) {
            if (keys_[i]) {
                V_[wait_key_reg_] = i;
                wait_for_key_ = false;
                break;
            }
        }
        if (wait_for_key_) return;  // Still waiting
    }
    
    uint16_t opcode = (memory_[PC_] << 8) | memory_[PC_ + 1];
    execute_instruction(opcode);
    total_cycles_++;
}

void Chip8System::run_frame() {
    for (uint32_t i = 0; i < cycles_per_frame_; i++) {
        tick();
    }
    update_timers();

    // Convert planes_ to palette indices and flush
    static constexpr int w = chip8_constants::HIRES_WIDTH;
    static constexpr int h = chip8_constants::HIRES_HEIGHT;
    uint8_t* indices = pixel_buffer_;

    if (hires_) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int byte_idx = y * (w / 8) + (x / 8);
                int bit_idx = 7 - (x % 8);
                uint8_t idx = 0;
                if ((planes_[0][byte_idx] >> bit_idx) & 1) idx |= 1;
                if ((planes_[1][byte_idx] >> bit_idx) & 1) idx |= 2;
                indices[y * w + x] = idx;
            }
        }
    } else {
        // Lo-res: 64×32 doubled to 128×64
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 64; x++) {
                int byte_idx = y * 8 + (x / 8);
                int bit_idx = 7 - (x % 8);
                uint8_t idx = 0;
                if ((planes_[0][byte_idx] >> bit_idx) & 1) idx |= 1;
                if ((planes_[1][byte_idx] >> bit_idx) & 1) idx |= 2;
                int dx = x * 2, dy = y * 2;
                indices[dy * w + dx]         = idx;
                indices[dy * w + dx + 1]     = idx;
                indices[(dy + 1) * w + dx]   = idx;
                indices[(dy + 1) * w + dx + 1] = idx;
            }
        }
    }

    // Drive video output with per-line pixel data
    if (video_port_) {
        auto& output = video_port_->output();
        for (int y = 0; y < h; y++) {
            const uint8_t* line = pixel_buffer_ + y * w;
            output.drive({0, SyncFlag::HSync});
            for (int x = 0; x < w; x++) {
                output.drive({line[x], SyncFlag::BeamOn});
            }
        }
        output.drive({0, SyncFlag::FrameEnd});
        video_port_->swap_frame();
    }
}

// ============================================================================
// File Loading
// ============================================================================

bool Chip8System::load_file(const char* filepath) {
    // Use VFS to read — supports archive paths like "roms.zip!/game.ch8"
    size_t size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &size);
    if (!file_data) {
        printf("CHIP-8: Failed to open file: %s\n", filepath);
        return false;
    }
    
    // Auto-extend memory if ROM > 3584 bytes
    size_t max_rom = memory_.size() - 512;
    if (size > max_rom) {
        if (size <= 65024) {
            memory_.resize(65536, 0);
            mode_ = Chip8Mode::XOCHIP;
            printf("CHIP-8: ROM %zu bytes > 4KB, auto-extending to 64KB (XO-CHIP)\n", size);
        } else {
            printf("CHIP-8: File too large: %zu bytes (max 65024)\n", size);
            free(file_data);
            return false;
        }
    }
    
    reset();
    
    memcpy(memory_.data() + chip8_constants::PROGRAM_START, file_data, size);
    free(file_data);

    // Set program title to bare filename (VFS-aware)
    std::string name_str = vfs_filename(filepath);
    program_title_ = name_str.empty() ? filepath : name_str;

    printf("CHIP-8: Loaded %zu bytes from %s (mode: %s)\n", size, filepath,
           mode_ == Chip8Mode::XOCHIP ? "XO-CHIP" :
           mode_ == Chip8Mode::SCHIP ? "SCHIP" : "CHIP-8");
    
    return true;
}

// ============================================================================
// Display
// ============================================================================


void Chip8System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    int chip8_key = map_sdl_key_to_chip8(key);
    if (chip8_key >= 0 && chip8_key < 16) {
        keys_[chip8_key] = pressed ? 1 : 0;
    }
}

// ============================================================================
// Chip Visualization
// ============================================================================

// ============================================================================
// Chip Registration
// ============================================================================

void Chip8System::register_chip8_chips() {
    // CHIP-8 is a virtual machine — no discrete physical chips.
    // List logical functional blocks so the Hardware menu remains useful.
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"CHIP-8", "COSMAC"}, "CHIP-8 Interpreter", "CPU", "CPU", chip8_constants::PROGRAM_START));
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"SRAM", "Various"}, "RAM (4KB)", "RAM", "Memory"));
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Display", "COSMAC"}, "Display (64x32)", "Display", "Video"));
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Keypad", "COSMAC"}, "Hex Keypad (16 keys)", "Keypad", "I/O"));
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Timer", "COSMAC"}, "Delay Timer (60 Hz)", "DT", "I/O"));
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Beeper", "COSMAC"}, "Sound Timer / Beeper", "ST", "Audio"));
}

// ============================================================================
// GUI Integration
// ============================================================================

void Chip8System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset CHIP-8")) {
        reset();
    }
#endif
}

void Chip8System::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("CHIP-8 Configuration");
    ImGui::Separator();
    
    // Mode display
    const char* mode_str = (mode_ == Chip8Mode::XOCHIP) ? "XO-CHIP" :
                           (mode_ == Chip8Mode::SCHIP)  ? "SCHIP 1.1" : "CHIP-8";
    ImGui::Text("Active Mode: %s", mode_str);
    ImGui::Text("Resolution: %s", hires_ ? "128x64 (hi-res)" : "64x32 (lo-res)");
    ImGui::Text("Memory: %zu bytes", memory_.size());
    ImGui::Separator();
    
    // Palette selection
    static const char* palette_names[] = {
        "Green Phosphor", "Amber Monitor", "White on Black", "C64 Colors"
    };
    static const char* palette_ids[] = { "green", "amber", "white", "c64" };
    
    std::string current = config_.custom_settings.count("display_palette") > 0
                        ? config_.custom_settings.at("display_palette") : "green";
    
    int selected = 0;
    for (int i = 0; i < 4; i++) {
        if (current == palette_ids[i]) { selected = i; break; }
    }
    
    if (ImGui::Combo("Display Palette", &selected, palette_names, 4)) {
        SystemConfiguration new_config = config_;
        new_config.custom_settings["display_palette"] = palette_ids[selected];
        set_configuration(new_config);
    }
    
    // Show palette swatches
    ImGui::Text("Palette:");
    for (size_t i = 0; i < current_palette_.size() && i < 4; i++) {
        ImVec4 c(current_palette_[i].r / 255.0f,
                 current_palette_[i].g / 255.0f,
                 current_palette_[i].b / 255.0f, 1.0f);
        char label[32];
        snprintf(label, sizeof(label), "Color %zu##c%zu", i, i);
        ImGui::ColorButton(label, c, 0, ImVec2(30, 30));
        if (i < 3) ImGui::SameLine();
    }
    
    ImGui::Separator();
    
    // Speed configuration
    ImGui::Text("Execution Speed:");
    for (size_t i = 0; i < hardware_traits_.video_standard_configs.size(); i++) {
        bool sel = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.video_standard_configs[i].name, sel)) {
            SystemConfiguration new_config = config_;
            new_config.region_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
    
    ImGui::Separator();
    
    // Quirk toggles (for advanced users)
    ImGui::Text("Quirks:");
    ImGui::Checkbox("Shift uses Vy (COSMAC)", &shift_quirk_);
    ImGui::Checkbox("FX55/65 increment I", &load_store_quirk_);
    ImGui::Checkbox("BNNN uses Vx (SCHIP)", &jump_quirk_);
    ImGui::Checkbox("Clip sprites at edge", &clip_quirk_);
    ImGui::Checkbox("VF reset on 8XY1/2/3", &vf_reset_quirk_);
#endif
}

void Chip8System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    cycles_per_frame_ = static_cast<uint32_t>(10 * multiplier);
}

// ============================================================================
// Scroll Helpers
// ============================================================================

void Chip8System::scroll_down(int n) {
    int w = display_width();
    int bpr = w / 8;
    int h = display_height();
    
    for (int p = 0; p < 2; p++) {
        if (!((active_plane_mask_ >> p) & 1)) continue;
        // Move rows down by n
        for (int y = h - 1; y >= n; y--) {
            memcpy(&planes_[p][y * bpr], &planes_[p][(y - n) * bpr], bpr);
        }
        // Clear top n rows
        for (int y = 0; y < n; y++) {
            memset(&planes_[p][y * bpr], 0, bpr);
        }
    }
    display_dirty_ = true;
}

void Chip8System::scroll_up(int n) {
    int w = display_width();
    int bpr = w / 8;
    int h = display_height();
    
    for (int p = 0; p < 2; p++) {
        if (!((active_plane_mask_ >> p) & 1)) continue;
        for (int y = 0; y < h - n; y++) {
            memcpy(&planes_[p][y * bpr], &planes_[p][(y + n) * bpr], bpr);
        }
        for (int y = h - n; y < h; y++) {
            memset(&planes_[p][y * bpr], 0, bpr);
        }
    }
    display_dirty_ = true;
}

void Chip8System::scroll_right() {
    int w = display_width();
    int h = display_height();
    int shift = hires_ ? 4 : 4;  // Always 4 pixels
    
    for (int p = 0; p < 2; p++) {
        if (!((active_plane_mask_ >> p) & 1)) continue;
        for (int y = 0; y < h; y++) {
            // Shift row right by `shift` pixels
            // Working with packed bits, MSB first
            int bpr = w / 8;
            // Start from rightmost byte
            for (int bx = bpr - 1; bx >= 0; bx--) {
                uint8_t curr = planes_[p][y * bpr + bx];
                uint8_t prev = (bx > 0) ? planes_[p][y * bpr + bx - 1] : 0;
                planes_[p][y * bpr + bx] = (curr >> shift) | (prev << (8 - shift));
            }
        }
    }
    display_dirty_ = true;
}

void Chip8System::scroll_left() {
    int w = display_width();
    int h = display_height();
    int shift = hires_ ? 4 : 4;  // Always 4 pixels
    
    for (int p = 0; p < 2; p++) {
        if (!((active_plane_mask_ >> p) & 1)) continue;
        for (int y = 0; y < h; y++) {
            int bpr = w / 8;
            for (int bx = 0; bx < bpr; bx++) {
                uint8_t curr = planes_[p][y * bpr + bx];
                uint8_t next = (bx < bpr - 1) ? planes_[p][y * bpr + bx + 1] : 0;
                planes_[p][y * bpr + bx] = (curr << shift) | (next >> (8 - shift));
            }
        }
    }
    display_dirty_ = true;
}

// ============================================================================
// Sprite Drawing
// ============================================================================

void Chip8System::draw_sprite(uint8_t vx, uint8_t vy, uint8_t n) {
    int w = display_width();
    int h = display_height();
    int bpr = w / 8;
    
    int xpos = V_[vx] % w;
    int ypos = V_[vy] % h;
    V_[0xF] = 0;
    
    // Determine sprite dimensions
    int sprite_width = 8;
    int sprite_height = n;
    int bytes_per_sprite_row = 1;
    
    if (n == 0 && (mode_ == Chip8Mode::SCHIP || mode_ == Chip8Mode::XOCHIP)) {
        // SCHIP/XO-CHIP: DXY0 = 16×16 sprite
        sprite_width = 16;
        sprite_height = 16;
        bytes_per_sprite_row = 2;
    }
    
    for (int p = 0; p < 2; p++) {
        if (!((active_plane_mask_ >> p) & 1)) continue;
        
        for (int row = 0; row < sprite_height; row++) {
            int py = ypos + row;
            if (clip_quirk_ && py >= h) break;
            py %= h;
            
            // Read sprite data (1 or 2 bytes per row)
            uint16_t sprite_data = 0;
            for (int b = 0; b < bytes_per_sprite_row; b++) {
                sprite_data |= static_cast<uint16_t>(
                    memory_[I_ + row * bytes_per_sprite_row + b]
                ) << (8 * (bytes_per_sprite_row - 1 - b));
            }
            
            for (int col = 0; col < sprite_width; col++) {
                int px = xpos + col;
                if (clip_quirk_ && px >= w) break;
                px %= w;
                
                bool sprite_bit = (sprite_data >> (sprite_width - 1 - col)) & 1;
                if (!sprite_bit) continue;
                
                int byte_index = py * bpr + (px / 8);
                int bit_index = 7 - (px % 8);
                
                if ((planes_[p][byte_index] >> bit_index) & 1) {
                    V_[0xF] = 1;  // Collision
                }
                planes_[p][byte_index] ^= (1 << bit_index);
            }
        }
        
        // Advance I past sprite data for second plane (XO-CHIP)
        // For multi-plane drawing, the second plane's data follows the first
        if (active_plane_mask_ == 3 && p == 0) {
            // I already points to start; plane 1 data is at I + sprite_height * bytes_per_sprite_row
            // We handle this by offsetting I temporarily
            I_ += sprite_height * bytes_per_sprite_row;
        }
    }
    
    // Restore I if we advanced it for dual-plane
    if (active_plane_mask_ == 3) {
        I_ -= sprite_height * (n == 0 ? 2 : 1);  // Undo the advance
    }
    
    display_dirty_ = true;
}

// ============================================================================
// Instruction Execution
// ============================================================================

void Chip8System::execute_instruction(uint16_t opcode) {
    uint16_t nnn = opcode & 0x0FFF;
    uint8_t n = opcode & 0x000F;
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    
    PC_ += 2;
    
    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) {
                // CLS — clear active planes
                for (int p = 0; p < 2; p++) {
                    if ((active_plane_mask_ >> p) & 1)
                        memset(planes_[p], 0, sizeof(planes_[p]));
                }
                display_dirty_ = true;
            } else if (opcode == 0x00EE) {
                // RET
                if (SP_ > 0) { SP_--; PC_ = stack_[SP_]; }
            } else if ((opcode & 0xFFF0) == 0x00C0) {
                // 00Cn — Scroll down n pixels (SCHIP)
                scroll_down(n);
            } else if ((opcode & 0xFFF0) == 0x00D0) {
                // 00Dn — Scroll up n pixels (XO-CHIP)
                scroll_up(n);
            } else if (opcode == 0x00FB) {
                // Scroll right 4 pixels (SCHIP)
                scroll_right();
            } else if (opcode == 0x00FC) {
                // Scroll left 4 pixels (SCHIP)
                scroll_left();
            } else if (opcode == 0x00FD) {
                // EXIT (SCHIP)
                quit_requested_ = true;
            } else if (opcode == 0x00FE) {
                // Lo-res mode (SCHIP)
                hires_ = false;
            } else if (opcode == 0x00FF) {
                // Hi-res mode (SCHIP)
                hires_ = true;
            }
            break;
            
        case 0x1000: PC_ = nnn; break;  // JP addr
        
        case 0x2000:  // CALL addr
            if (SP_ < 16) { stack_[SP_] = PC_; SP_++; }
            PC_ = nnn;
            break;
        
        case 0x3000: if (V_[x] == kk) PC_ += 2; break;  // SE Vx, byte
        case 0x4000: if (V_[x] != kk) PC_ += 2; break;  // SNE Vx, byte
        
        case 0x5000:
            if (n == 0) {
                // 5XY0 — SE Vx, Vy
                if (V_[x] == V_[y]) PC_ += 2;
            } else if (n == 2 && mode_ == Chip8Mode::XOCHIP) {
                // 5XY2 — Save Vx..Vy to memory[I..] (XO-CHIP)
                if (x <= y) {
                    for (int i = x; i <= y; i++)
                        memory_[(I_ + i - x) & 0xFFFF] = V_[i];
                } else {
                    for (int i = x; i >= y; i--)
                        memory_[(I_ + x - i) & 0xFFFF] = V_[i];
                }
            } else if (n == 3 && mode_ == Chip8Mode::XOCHIP) {
                // 5XY3 — Load Vx..Vy from memory[I..] (XO-CHIP)
                if (x <= y) {
                    for (int i = x; i <= y; i++)
                        V_[i] = memory_[(I_ + i - x) & 0xFFFF];
                } else {
                    for (int i = x; i >= y; i--)
                        V_[i] = memory_[(I_ + x - i) & 0xFFFF];
                }
            }
            break;
        
        case 0x6000: V_[x] = kk; break;  // LD Vx, byte
        case 0x7000: V_[x] += kk; break;  // ADD Vx, byte
            
        case 0x8000:
            switch (n) {
                case 0x0: V_[x] = V_[y]; break;
                case 0x1:
                    V_[x] |= V_[y];
                    if (vf_reset_quirk_) V_[0xF] = 0;
                    break;
                case 0x2:
                    V_[x] &= V_[y];
                    if (vf_reset_quirk_) V_[0xF] = 0;
                    break;
                case 0x3:
                    V_[x] ^= V_[y];
                    if (vf_reset_quirk_) V_[0xF] = 0;
                    break;
                case 0x4: {
                    uint16_t sum = V_[x] + V_[y];
                    V_[x] = sum & 0xFF;
                    V_[0xF] = (sum > 255) ? 1 : 0;
                    break;
                }
                case 0x5: {
                    bool no_borrow = V_[x] >= V_[y];
                    V_[x] -= V_[y];
                    V_[0xF] = no_borrow ? 1 : 0;
                    break;
                }
                case 0x6:
                    if (!shift_quirk_) {
                        // COSMAC: shift Vy into Vx
                        V_[0xF] = V_[y] & 0x1;
                        V_[x] = V_[y] >> 1;
                    } else {
                        // SCHIP: shift Vx in-place
                        V_[0xF] = V_[x] & 0x1;
                        V_[x] >>= 1;
                    }
                    break;
                case 0x7: {
                    bool no_borrow = V_[y] >= V_[x];
                    V_[x] = V_[y] - V_[x];
                    V_[0xF] = no_borrow ? 1 : 0;
                    break;
                }
                case 0xE:
                    if (!shift_quirk_) {
                        V_[0xF] = (V_[y] >> 7) & 1;
                        V_[x] = V_[y] << 1;
                    } else {
                        V_[0xF] = (V_[x] >> 7) & 1;
                        V_[x] <<= 1;
                    }
                    break;
            }
            break;
            
        case 0x9000:
            if (n == 0) {
                if (V_[x] != V_[y]) PC_ += 2;  // SNE Vx, Vy
            }
            break;
        
        case 0xA000: I_ = nnn; break;  // LD I, addr
        
        case 0xB000:
            if (jump_quirk_) {
                PC_ = nnn + V_[x];  // SCHIP: BXNN jumps to XNN + Vx
            } else {
                PC_ = nnn + V_[0];  // CHIP-8: BNNN jumps to NNN + V0
            }
            break;
        
        case 0xC000:
            V_[x] = (rand() & 0xFF) & kk;  // RND Vx, byte
            break;
            
        case 0xD000:
            // DRW Vx, Vy, n
            draw_sprite(x, y, n);
            break;
            
        case 0xE000:
            if (kk == 0x9E) {
                if (keys_[V_[x] & 0xF]) PC_ += 2;  // SKP Vx
            } else if (kk == 0xA1) {
                if (!keys_[V_[x] & 0xF]) PC_ += 2;  // SKNP Vx
            }
            break;
            
        case 0xF000:
            switch (kk) {
                case 0x00:
                    if (mode_ == Chip8Mode::XOCHIP) {
                        if (x == 0) {
                            // F000 NNNN — Long I (XO-CHIP)
                            I_ = (memory_[PC_] << 8) | memory_[PC_ + 1];
                            PC_ += 2;
                        } else {
                            // FN01, FN02, FN03 — Plane selection (XO-CHIP)
                            // Actually these are Fx01 where x is the plane mask
                            // But the encoding is FX01 where X is the plane mask
                        }
                    }
                    break;
                case 0x01:
                    if (mode_ == Chip8Mode::XOCHIP) {
                        // FX01 — Select drawing plane(s) (XO-CHIP)
                        active_plane_mask_ = x & 0x3;
                        if (active_plane_mask_ == 0) active_plane_mask_ = 1; // fallback
                    }
                    break;
                case 0x02:
                    if (mode_ == Chip8Mode::XOCHIP) {
                        // F002 — Load audio pattern from memory[I..I+15] (XO-CHIP)
                        for (int i = 0; i < 16; i++) {
                            audio_pattern_[i] = memory_[(I_ + i) & 0xFFFF];
                        }
                        has_audio_pattern_ = true;
                    }
                    break;
                case 0x07: V_[x] = delay_timer_; break;  // LD Vx, DT
                case 0x0A:
                    // LD Vx, K — Wait for key press
                    wait_for_key_ = true;
                    wait_key_reg_ = x;
                    break;
                case 0x15: delay_timer_ = V_[x]; break;  // LD DT, Vx
                case 0x18: sound_timer_ = V_[x]; break;  // LD ST, Vx
                case 0x1E: I_ += V_[x]; break;            // ADD I, Vx
                case 0x29:
                    // LD F, Vx — Point I to lo-res font sprite
                    I_ = (V_[x] & 0xF) * 5;
                    break;
                case 0x30:
                    // LD HF, Vx — Point I to hi-res font sprite (SCHIP)
                    I_ = 80 + (V_[x] & 0xF) * 10;
                    break;
                case 0x33:
                    // LD B, Vx — BCD
                    memory_[I_] = V_[x] / 100;
                    memory_[I_ + 1] = (V_[x] / 10) % 10;
                    memory_[I_ + 2] = V_[x] % 10;
                    break;
                case 0x3A:
                    if (mode_ == Chip8Mode::XOCHIP) {
                        // FX3A — Set pitch register (XO-CHIP)
                        pitch_register_ = V_[x];
                    }
                    break;
                case 0x55:
                    // LD [I], Vx — Store V0..Vx
                    for (int i = 0; i <= x; i++) {
                        memory_[(I_ + i) & 0xFFFF] = V_[i];
                    }
                    if (load_store_quirk_) I_ += x + 1;
                    break;
                case 0x65:
                    // LD Vx, [I] — Load V0..Vx
                    for (int i = 0; i <= x; i++) {
                        V_[i] = memory_[(I_ + i) & 0xFFFF];
                    }
                    if (load_store_quirk_) I_ += x + 1;
                    break;
                case 0x75:
                    // LD R, Vx — Store V0..Vx in RPL flags (SCHIP, max x=7)
                    for (int i = 0; i <= std::min((int)x, 15); i++) {
                        rpl_flags_[i] = V_[i];
                    }
                    break;
                case 0x85:
                    // LD Vx, R — Load V0..Vx from RPL flags (SCHIP)
                    for (int i = 0; i <= std::min((int)x, 15); i++) {
                        V_[i] = rpl_flags_[i];
                    }
                    break;
            }
            break;
    }
}

void Chip8System::update_timers() {
    if (delay_timer_ > 0) delay_timer_--;
    if (sound_timer_ > 0) sound_timer_--;
}

int Chip8System::map_sdl_key_to_chip8(int sdl_key) {
#ifdef CERMU_HAS_GUI
    switch (sdl_key) {
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_4: return 0xC;
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_r: return 0xD;
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_f: return 0xE;
        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;
        default: return -1;
    }
#else
    (void)sdl_key;
    return -1;
#endif
}

// Register CHIP-8 system
REGISTER_SYSTEM(chip8_descriptor, []() {
    return std::make_unique<Chip8System>();
})