#include "c64_system_wrapper.h"
#include "../../chip/input/commodore_keyboard.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "../../chip/cpu/fam65xx/mos6510.h"
#include "../../gui/imgui_interface.h"
#include "../../core/formats/format_registry.h"
#include "../../core/formats/prg_format.h"
#include "../../core/formats/d64_format.h"
#include "../../core/formats/t64_format.h"
#include "../../core/formats/tap_format.h"
#include "../../core/formats/crt_format.h"
#include "../../core/formats/lnx_format.h"
#include "../../core/analysis/basic_parser.h"
#include <cstring>
#include <cstdio>
#include <cctype>

/**
 * C64 System Wrapper Implementation
 *
 * CURRENT ARCHITECTURE:
 * =====================
 * This file implements the wrapper layer that adapts the C-style c64_t system
 * to the C++ EmulatedSystem interface. It delegates most work to C functions
 * in c64.cpp.
 *
 * CYCLE COUNTING:
 * ===============
 * - c64_->total_cycles: Internal C64 cycle counter (updated by c64_system_tick)
 * - total_cycles_: Base class cycle counter (synced in tick() method)
 * - Both are kept in sync to maintain consistency
 *
 * FRAMEBUFFER MANAGEMENT:
 * =======================
 * - VIC-II owns the actual framebuffer: c64_->vicii->pixel.framebuffer
 * - get_framebuffer() returns pointer to VIC-II's buffer
 * - set_framebuffer() delegates to c64_set_framebuffer() which calls vicii_set_framebuffer()
 *
 * FUTURE MERGER NOTES:
 * ====================
 * When merging into unified C64System class, this file's logic should become
 * direct methods of C64System. The c64_t pointer would be eliminated and its
 * members would become direct members of C64System class.
 *
 * Key functions to convert:
 * - Constructor logic from c64_system_create()
 * - Destructor logic from c64_system_destroy()
 * - tick() from c64_system_tick()
 * - reset() from c64_system_reset()
 * - All chip management code
 */

/** Check if load address is a typical C64 address */
static bool is_c64_load_address(uint16_t addr) {
    return addr == 0x0801 || addr == 0xC000 || addr == 0x0800 ||
           addr == 0x4000 || addr == 0x8000 || addr == 0xE000;
}

// C64 file detection
static float c64_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    // Check extensions
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        // PRG files â€” check load address
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            if (size >= 2) {
                uint16_t load_addr = data[0] | (data[1] << 8);
                // C64 BASIC start address gives highest confidence
                if (load_addr == 0x0801) return 0.95f;
                // Common C64 ML addresses
                if (load_addr == 0xC000 || load_addr == 0x0800 || load_addr == 0x4000) return 0.85f;
                // VIC-20/C16 address â€” lower confidence
                if (load_addr == 0x1001) return 0.6f;
                return 0.7f;  // Generic PRG â€” C64 is the most common Commodore system
            }
        }
        // LNX files â€” Lynx archive; parse to inspect contained files' load addresses
        if (strcmp(ext, ".lnx") == 0 || strcmp(ext, ".LNX") == 0) {
            commodore_lynx_t lynx;
            if (commodore_lynx_open(filepath, &lynx)) {
                commodore_lynx_directory_t dir;
                if (commodore_lynx_read_directory(&lynx, &dir)) {
                    // Check ALL PRG entries' load addresses for C64 addresses
                    bool found_c64 = false;
                    bool found_any = false;
                    for (unsigned i = 0; i < dir.file_count; i++) {
                        if (dir.entries[i].file_type == 'P' && dir.entries[i].data_length >= 2) {
                            size_t off = dir.entries[i].data_offset;
                            if (off + 1 < lynx.data_size) {
                                uint16_t addr = lynx.data[off] | ((uint16_t)lynx.data[off+1] << 8);
                                found_any = true;
                                if (is_c64_load_address(addr)) {
                                    found_c64 = true;
                                    break;
                                }
                            }
                        }
                    }
                    commodore_lynx_close(&lynx);
                    if (found_c64) return 0.95f;
                    if (found_any) return 0.5f;
                }
                commodore_lynx_close(&lynx);
            }
            return 0.6f;  // Could not inspect â€” C64 is most common
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            // D64 disk images â€” inspect first PRG's load address to distinguish systems
            commodore_d64_t d64;
            if (commodore_d64_open(filepath, &d64)) {
                commodore_prg_t prg = {};
                if (commodore_d64_extract_first_prg(&d64, &prg)) {
                    float score = is_c64_load_address(prg.load_addr) ? 0.95f : 0.6f;
                    commodore_prg_free(&prg);
                    commodore_d64_close(&d64);
                    return score;
                }
                commodore_d64_close(&d64);
            }
            // Could not inspect â€” still likely C64 (most common system)
            if (size == D64_STANDARD_SIZE || size == D64_STANDARD_SIZE_ERR ||
                size == D64_EXTENDED_SIZE || size == D64_EXTENDED_SIZE_ERR) {
                return 0.7f;
            }
            return 0.6f;  // Non-standard size but .d64 extension
        }
        if (strcmp(ext, ".t64") == 0 || strcmp(ext, ".T64") == 0) {
            // T64 tape archives are C64-centric
            return 0.85f;
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            // Check TAP header to see if this is specifically a C64 tape
            int platform = commodore_tap_identify_platform(filepath);
            if (platform == 0) return 0.95f;  // C64 TAP
            if (platform == 1) return 0.3f;   // VIC-20 TAP
            return 0.6f;  // Unknown or error â€” C64 is most common
        }
        if (strcmp(ext, ".crt") == 0 || strcmp(ext, ".CRT") == 0) {
            // CRT cartridge files
            if (size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0) {
                return 1.0f;  // Perfect match
            }
        }
    }
    
    return 0.0f;
}

/** Formats the C64 can load — used by SystemDescriptor and file dialogs. */
static const format_descriptor_t* const c64_formats[] = {
    &PRG_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR, &CRT_FORMAT_DESCRIPTOR,
    &T64_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR,
    &BIN_FORMAT_DESCRIPTOR, nullptr
};

static HardwareTraits create_c64_hardware_traits() {
    HardwareTraits traits;
    
    // Display
    traits.display.native_width = 403;
    traits.display.native_height = 284;
    traits.display.visible_width = 403;
    traits.display.visible_height = 284;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 0;  // Direct RGB
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // Audio
    traits.audio.format = AudioFormat::STEREO_16BIT;
    traits.audio.sample_rate_hz = 44100;
    traits.audio.channels = 2;
    traits.audio.chip_name = "SID 6581";
    
    // Timing (PAL default)
    traits.timing.cpu_frequency_hz = 985248;
    traits.timing.video_frequency_hz = 985248;
    traits.timing.audio_sample_rate_hz = 44100;
    traits.timing.target_fps = 50;
    traits.timing.cycles_per_frame = 19705;
    traits.timing.region = VideoRegion::PAL;

    // Region options
    traits.region_options.push_back({
        "PAL",
        VideoRegion::PAL,
        traits.timing,
        true
    });

    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = 1022727;
    ntsc_timing.video_frequency_hz = 1022727;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 17045;   // 1022727 / 60
    ntsc_timing.region = VideoRegion::NTSC;

    traits.region_options.push_back({
        "NTSC",
        VideoRegion::NTSC,
        ntsc_timing,
        false
    });

    return traits;
}

static SystemDescriptor c64_descriptor = {
    "Commodore 64",
    "C64",
    "8-bit home computer with VIC-II graphics and SID sound chip (1982)",
    c64_formats,
    create_c64_hardware_traits(),
    c64_can_load_file
};

C64SystemWrapper::C64SystemWrapper()
    : EmulatedSystem()  // Call base class constructor
    , c64_(nullptr)
    , cycles_per_frame_(19705)  // PAL: 985248 Hz / 50 fps
    , gui_state_(nullptr)
{
    // Initialize C64-specific config with defaults
    c64_config_.vicii_standard = VIC_PAL;
    c64_config_.rom_config = nullptr;
    c64_config_.test_mode = C64_TEST_MODE_NORMAL;
    c64_config_.test_binary_config = nullptr;
    c64_config_.roml_present = false;
    c64_config_.romh_present = false;
    c64_config_.roml_filename = nullptr;
    c64_config_.romh_filename = nullptr;
    c64_config_.initial_exrom_state = true;
    c64_config_.initial_game_state = true;
    
    // Initialize base class members
    hardware_traits_ = c64_descriptor.hardware_traits;
    speed_multiplier_ = 1.0f;
    
    // Create persistent GUI state for menu handling
    gui_state_ = gui_create_state();
}

C64SystemWrapper::~C64SystemWrapper() {
    shutdown();
    
    // Destroy persistent GUI state
    if (gui_state_) {
        gui_destroy_state(static_cast<gui_state_t*>(gui_state_));
        gui_state_ = nullptr;
    }
}

const SystemDescriptor& C64SystemWrapper::get_descriptor() const {
    return c64_descriptor;
}

bool C64SystemWrapper::initialize() {
    if (c64_) {
        return true;  // Already initialized
    }
    
    c64_ = c64_system_create(&c64_config_);
    if (!c64_) {
        printf("C64: Failed to create system\n");
        return false;
    }
    
    // Create the layered keyboard mapper for character-based input
    if (c64_->keyboard) {
        keyboard_mapper_.reset(create_c64_keyboard_mapper(c64_->keyboard));
    }
    
    printf("C64: System initialized successfully\n");
    return true;
}

void C64SystemWrapper::shutdown() {
    if (c64_) {
        c64_system_destroy(c64_);
        c64_ = nullptr;
    }
}

void C64SystemWrapper::reset() {
    if (c64_) {
        c64_system_reset(c64_);
    }
}

void C64SystemWrapper::tick() {
    if (c64_) {
        c64_system_tick(c64_);
        
        // CRITICAL: Sync base class cycle counter with C64's internal counter
        // The C64 system maintains its own cycle count in c64_->total_cycles
        // which is updated by c64_system_tick(). We sync it here to keep the
        // base class counter accurate for the GUI and other systems that query it.
        //
        // FUTURE: When merged into unified C64System, total_cycles_ will be
        // the single authoritative counter, updated directly in tick().
        total_cycles_ = c64_->total_cycles;
    }
}

void C64SystemWrapper::run_frame() {
    if (!c64_) return;
    
    // Execute one frame worth of cycles
    for (uint32_t i = 0; i < cycles_per_frame_; i++) {
        c64_system_tick(c64_);
    }
    
    // Sync base class cycle counter with C64's internal counter
    total_cycles_ = c64_->total_cycles;
}

/** Memory read callback for BASIC SYS parsing â€” reads from C64 RAM */
static uint8_t c64_mem_read_for_basic(void* ctx, uint16_t addr) {
    ram_t* ram = static_cast<ram_t*>(ctx);
    return ram->memory[addr];
}

bool C64SystemWrapper::load_file(const char* filepath) {
    if (!c64_) {
        printf("C64: System not initialized\n");
        return false;
    }
    
    printf("C64: Loading file: %s\n", filepath);

    // Use shared Commodore file loader for format detection and parsing
    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("C64: Failed to load file: %s\n", result.error_msg);
        format_load_result_free(&result);
        return false;
    }

    bool success = false;

    switch (result.type) {
        case FORMAT_LOAD_PROGRAM: {
            // PRG/D64/T64 all produce a single program — copy into C64 RAM
            const program_data_t* prg = &result.program;
            
            printf("C64: Loading %s: $%04X-$%04X (%zu bytes)\n",
                   format_load_type_name(result.type),
                   prg->load_addr, prg->end_addr, prg->data_size);

            // Validate address range
            if (prg->data_size == 0 || (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
                printf("C64: Invalid PRG address range: $%04X-$%04X\n",
                       prg->load_addr, prg->end_addr);
                break;
            }

            // Write program data into C64 RAM
            memcpy(&c64_->ram->memory[prg->load_addr], prg->data, prg->data_size);

            // Try to find a SYS address in BASIC for auto-run
            uint16_t run_addr = 0;

            if (prg->load_addr == 0x0801) {
                // Standard C64 BASIC start â€” parse for SYS statement
                commodore_basic_sys_t sys_result = {};
                if (commodore_basic_parse_sys(c64_mem_read_for_basic, c64_->ram,
                                              prg->load_addr,
                                              &COMMODORE_BASIC_C64,
                                              10, &sys_result)) {
                    run_addr = sys_result.sys_address;
                    printf("C64: Found SYS %u on BASIC line %u\n",
                           run_addr, sys_result.line_number);
                }

                // Update BASIC pointers so LIST and RUN work correctly
                // TXTTAB ($2B/$2C) = start of BASIC text
                c64_->ram->memory[0x2B] = (uint8_t)(prg->load_addr & 0xFF);
                c64_->ram->memory[0x2C] = (uint8_t)(prg->load_addr >> 8);
                // VARTAB ($2D/$2E) = end of BASIC text
                c64_->ram->memory[0x2D] = (uint8_t)(prg->end_addr & 0xFF);
                c64_->ram->memory[0x2E] = (uint8_t)(prg->end_addr >> 8);
                // ARYTAB ($2F/$30) = start of arrays
                c64_->ram->memory[0x2F] = (uint8_t)(prg->end_addr & 0xFF);
                c64_->ram->memory[0x30] = (uint8_t)(prg->end_addr >> 8);
                // STREND ($31/$32) = end of arrays
                c64_->ram->memory[0x31] = (uint8_t)(prg->end_addr & 0xFF);
                c64_->ram->memory[0x32] = (uint8_t)(prg->end_addr >> 8);
            }

            if (run_addr != 0) {
                printf("C64: Auto-running from $%04X\n", run_addr);
                if (c64_->mos6510) {
                    mos6510_set_pc((mos6510_t*)c64_->mos6510, run_addr);
                }
            } else {
                printf("C64: No SYS found â€” program loaded, use RUN to start\n");
            }

            success = true;
            break;
        }

        case FORMAT_LOAD_METADATA: {
            // TAP or CRT — distinguished by format descriptor name
            if (result.format && strcmp(result.format->name, "TAP") == 0) {
                const commodore_tap_header_t* hdr = (const commodore_tap_header_t*)result.metadata;
                printf("C64: TAP file detected (platform=%u, version=%u)\n",
                       hdr->platform, hdr->version);
                printf("C64: TAP tape emulation not yet implemented (requires cycle-accurate datasette)\n");
            } else if (result.format && strcmp(result.format->name, "CRT") == 0) {
                const commodore_crt_header_t* hdr = (const commodore_crt_header_t*)result.metadata;
                printf("C64: CRT cartridge: \"%s\" (hw_type=%u, exrom=%u, game=%u)\n",
                       hdr->name, hdr->hardware_type, hdr->exrom, hdr->game);
                printf("C64: CRT cartridge loading not yet fully implemented\n");
                // TODO: Parse CHIP packets and map into address space
            } else {
                printf("C64: Unknown metadata format: %s\n",
                       result.format ? result.format->name : "(null)");
            }
            success = false;
            break;
        }

        case FORMAT_LOAD_RAW: {
            // Raw binary — load at $C000 (common ML area) by default
            const uint16_t default_addr = 0xC000;
            const program_data_t* prg = &result.program;
            
            printf("C64: Loading BIN at default $%04X (%zu bytes)\n",
                   default_addr, prg->data_size);

            if (default_addr + prg->data_size > 0x10000) {
                printf("C64: BIN too large for address space\n");
                break;
            }

            memcpy(&c64_->ram->memory[default_addr], prg->data, prg->data_size);
            success = true;
            break;
        }

        case FORMAT_LOAD_ARCHIVE: {
            // Archive (LNX etc.) — load ALL extracted PRG files into C64 RAM
            printf("C64: Loading archive with %d files\n", result.file_count);

            bool any_basic = false;
            uint16_t basic_end_addr = 0;

            for (int f = 0; f < result.file_count; f++) {
                const program_data_t* prg = &result.files[f];

                if (prg->data_size == 0 || (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
                    printf("C64: LNX file %d: Invalid address range $%04X-$%04X, skipping\n",
                           f, prg->load_addr, prg->end_addr);
                    continue;
                }

                printf("C64: LNX file %d: $%04X-$%04X (%zu bytes)\n",
                       f, prg->load_addr, prg->end_addr, prg->data_size);

                memcpy(&c64_->ram->memory[prg->load_addr], prg->data, prg->data_size);

                if (prg->load_addr == 0x0801) {
                    any_basic = true;
                    basic_end_addr = prg->end_addr;
                }
            }

            if (any_basic) {
                // Update BASIC end pointer and inject RUN
                c64_->ram->memory[0x2D] = (uint8_t)(basic_end_addr & 0xFF);
                c64_->ram->memory[0x2E] = (uint8_t)(basic_end_addr >> 8);

                const char* run_cmd = "RUN\r";
                int len = (int)strlen(run_cmd);
                for (int i = 0; i < len; i++) {
                    c64_->ram->memory[0x0277 + i] = (uint8_t)run_cmd[i];
                }
                c64_->ram->memory[0x00C6] = (uint8_t)len;
                printf("C64: LNX: Set BASIC end=$%04X and injected RUN\n", basic_end_addr);
            }

            success = result.file_count > 0;
            break;
        }

        default:
            printf("C64: Unsupported load result type: %d\n", result.type);
            break;
    }

    format_load_result_free(&result);
    return success;
}

uint32_t* C64SystemWrapper::get_framebuffer() {
    if (c64_ && c64_->vicii) {
        return c64_->vicii->pixel.framebuffer;
    }
    return nullptr;
}

void C64SystemWrapper::get_display_dimensions(int* width, int* height) const {
    // VIC-II visible area
    *width = 403;
    *height = 284;
}

void C64SystemWrapper::set_framebuffer(uint32_t* buffer, int width, int height) {
    if (c64_) {
        c64_set_framebuffer(c64_, buffer, width, height);
    }
}

void C64SystemWrapper::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Legacy path â€” still used when handle_keyboard_event_ex is not called
    // (e.g., from the old C64-only GUI, test harness, or non-SDL input)
    if (keyboard_mapper_) {
        // Route through the mapper with minimal info
        if (pressed) {
            keyboard_mapper_->process_key_down(
                key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(
                key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (c64_ && c64_->keyboard) {
        // No mapper â€” convert SDL keycode to EmuKey and pass through
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                commodore_keyboard_key_down(c64_->keyboard, ek, false);
            } else {
                commodore_keyboard_key_up(c64_->keyboard, ek, false);
            }
        }
    }
}

void C64SystemWrapper::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up(key, scancode, mod);
        }
    } else {
        // Fallback to legacy handler
        if (!repeat) {
            handle_keyboard_event(key, pressed);
        }
    }
}

void C64SystemWrapper::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void C64SystemWrapper::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

void C64SystemWrapper::handle_controller_event(int controller, int button, bool pressed) {
    // C64 joystick support would go here
    // TODO: Implement joystick handling
}

void C64SystemWrapper::render_system_menu_items() {
    // Forward menu creation to the old C64 GUI code
    // This allows C64-specific menus (Load Test Binary, Chip Debug Windows, etc.)
    // to appear in the multi_emu interface
    
    if (gui_state_ && c64_) {
        // Call the C64-specific menu rendering function from the old GUI
        // Using persistent gui_state_ so menu clicks persist
        gui_render_c64_system_menu_items(c64_, static_cast<gui_state_t*>(gui_state_));
    }
}

void C64SystemWrapper::render_debug_windows(void* gui_state) {
    if (!gui_state_ || !c64_) return;
    
    gui_state_t* state = static_cast<gui_state_t*>(gui_state_);
    
    // Render test binary dialog if needed
    if (state->show_test_binary_dialog) {
        gui_render_test_binary_dialog(c64_, state, nullptr);
    }
    
#ifdef IMGUI_VERSION
    // Iterate through all chips and render their debug windows
    system_8bit_t* sys = &c64_->system;
    for (uint8_t chip_id = 0; chip_id < sys->chip_count; chip_id++) {
        chip_entry_t* entry = &sys->chips[chip_id];
        
        if (state->show_chip_debug[chip_id] &&
            entry->desc &&
            entry->desc->render_debug_window) {
            
            entry->desc->render_debug_window(
                entry->chip,
                &state->show_chip_debug[chip_id]
            );
        }
    }
#endif
}

uint32_t C64SystemWrapper::get_target_fps() const {
    return 50;  // PAL
}

void C64SystemWrapper::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    cycles_per_frame_ = static_cast<uint32_t>(19705 * multiplier);
}

// Note: get_total_cycles() now provided by base class (returns total_cycles_)
// However, C64 has its own cycle counter, so we need to sync it
// For now, we'll update total_cycles_ in tick() method

// Note: get_speed_multiplier() now provided by base class (returns speed_multiplier_)

// Note: Hardware trait queries (get_hardware_traits, get_current_timing,
// get_display_traits, get_audio_traits) now provided by base class
// Note: get_configuration() now provided by base class (returns config_)

bool C64SystemWrapper::set_configuration(const SystemConfiguration& config) {
    config_ = config;  // Update base class SystemConfiguration
    // TODO: Map SystemConfiguration changes to c64_config_ when needed
    return true;
}

bool C64SystemWrapper::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        const RegionOption& region = hardware_traits_.region_options[config_.region_option_index];
        cycles_per_frame_ = region.timing.cycles_per_frame;

        // Map to legacy c64_config_t
        c64_config_.vicii_standard =
            (region.region == VideoRegion::NTSC) ? VIC_NTSC : VIC_PAL;
    }

    return true;
}

// ============================================================================
// Auto-detect optimal configuration from file contents
// ============================================================================
SystemConfiguration C64SystemWrapper::detect_optimal_configuration(
    const char* filepath, const uint8_t* data, size_t size) {

    SystemConfiguration config = EmulatedSystem::detect_optimal_configuration(filepath, data, size);

    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;

    // --- CRT cartridge: byte 8 of the header specifies hardware type ---
    // Byte 0x08-0x09 = hardware type.  Region cannot be derived directly,
    // but certain cartridges are NTSC-only.  For now, leave at PAL default.

    // --- PRG heuristic: filenames containing "ntsc" suggest NTSC ---
    if (filepath) {
        // Case-insensitive substring search in the filename
        const char* name = strrchr(filepath, '/');
        if (!name) name = strrchr(filepath, '\\');
        if (!name) name = filepath; else name++;

        // Simple case-insensitive search for "ntsc" in the filename
        std::string lower_name(name);
        for (auto& c : lower_name) c = static_cast<char>(tolower(c));

        if (lower_name.find("ntsc") != std::string::npos) {
            // Select NTSC region (index 1)
            if (hardware_traits_.region_options.size() > 1) {
                config.region_option_index = 1;
                printf("C64: Filename contains 'ntsc' â€” selecting NTSC region\n");
            }
        }
    }

    // --- TAP file: header byte 0x0C indicates platform/standard ---
    if (ext && (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0)) {
        // TAP v1 header: byte 0x0C = platform (0 = C64, 1 = VIC-20)
        // The TAP spec doesn't directly encode PAL/NTSC, so we keep default.
    }

    // --- D64 disk image: some SID tunes store region in metadata ---
    // Not enough reliable data in D64 to determine region automatically.

    return config;
}

void C64SystemWrapper::render_configuration_ui() {
    // TODO: Render C64-specific configuration UI
    // This will be implemented when we update the GUI
}

uint32_t C64SystemWrapper::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!c64_ || !c64_->sid || !buffer || max_samples == 0) return 0;
    mos6581_generate_samples(c64_->sid, buffer, max_samples);
    return max_samples;
}

// Register C64 system with the registry
REGISTER_SYSTEM(c64_descriptor, []() {
    return std::make_unique<C64SystemWrapper>();
})
