#include "commodore_system.h"
#include "../../devices/storage/drive_1541.h"
#include "../../devices/storage/datasette_1530.h"
#include "../../core/formats/format_registry.h"
#include "../../core/vfs/vfs.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// CommodoreSystem — shared Commodore 8-bit base class implementation
// ============================================================================

bool CommodoreSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;

    // Update cached target FPS from region config
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        cached_target_fps_ = hardware_traits_.video_standard_configs[config_.region_option_index].timing.target_fps;
    } else {
        cached_target_fps_ = 50;  // Default PAL
    }

    return true;
}

void CommodoreSystem::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

void CommodoreSystem::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void CommodoreSystem::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

void CommodoreSystem::handle_keyboard_event_ex(
        SDL_Keycode key, SDL_Scancode scancode,
        uint16_t mod, bool pressed, bool repeat) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up(key, scancode, mod);
        }
    } else if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

// ============================================================================
// Guest keyboard scancodes — superset across C64, VIC-20, C16/Plus4
//
// Every SDL_Scancode listed here closes at least one keyboard matrix
// contact (via TEXTINPUT or direct-map) on at least one Commodore system.
// Controller keymap presets that overlap these keys will report collisions,
// guiding auto_assign_controller_keymaps() toward safer choices.
// ============================================================================

int CommodoreSystem::get_guest_keyboard_scancodes(const SDL_Scancode** out) const {
    static const SDL_Scancode scancodes[] = {
        // --- Letters (a–z, all close matrix contacts via TEXTINPUT) ---
        SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D,
        SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H,
        SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
        SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, SDL_SCANCODE_P,
        SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
        SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X,
        SDL_SCANCODE_Y, SDL_SCANCODE_Z,

        // --- Digits (0–9) ---
        SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7,
        SDL_SCANCODE_8, SDL_SCANCODE_9,

        // --- Symbols (close matrix contacts via TEXTINPUT char mapping) ---
        SDL_SCANCODE_SPACE,        SDL_SCANCODE_COMMA,
        SDL_SCANCODE_MINUS,        SDL_SCANCODE_PERIOD,
        SDL_SCANCODE_SLASH,        SDL_SCANCODE_SEMICOLON,
        SDL_SCANCODE_EQUALS,       SDL_SCANCODE_LEFTBRACKET,
        SDL_SCANCODE_BACKSLASH,    SDL_SCANCODE_RIGHTBRACKET,
        SDL_SCANCODE_APOSTROPHE,

        // --- Non-printable keys with matrix positions ---
        SDL_SCANCODE_RETURN,       SDL_SCANCODE_BACKSPACE,
        SDL_SCANCODE_TAB,
        SDL_SCANCODE_RIGHT,        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_DOWN,         SDL_SCANCODE_UP,
        SDL_SCANCODE_HOME,
        SDL_SCANCODE_F1,           SDL_SCANCODE_F3,
        SDL_SCANCODE_F5,           SDL_SCANCODE_F7,

        // --- Modifier keys with matrix positions ---
        SDL_SCANCODE_LSHIFT,       SDL_SCANCODE_RSHIFT,
        SDL_SCANCODE_LCTRL,        // CTRL
        SDL_SCANCODE_LGUI,         // C= (Commodore) key

        // --- C16/Plus4 extras (matrix positions unique to 264 series) ---
        SDL_SCANCODE_ESCAPE,       // ESC key on C16/Plus4
        SDL_SCANCODE_F2,           // F2 key on C16/Plus4
    };

    if (out) *out = scancodes;
    return static_cast<int>(sizeof(scancodes) / sizeof(scancodes[0]));
}

// ============================================================================
// Deferred Loading — shared load_file / apply / check infrastructure
// ============================================================================

bool CommodoreSystem::load_file(const char* filepath) {
    const char* name = get_descriptor().short_name;

    if (!is_system_initialized()) {
        printf("%s: System not initialized, initializing now...\n", name);
        if (!initialize()) {
            printf("%s: Failed to initialize system for file loading\n", name);
            return false;
        }
    }

    printf("%s: Loading file: %s\n", name, filepath);

    // Clear any previous pending load
    clear_pending_load();

    // Parse the file into a format result
    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("%s: Failed to load file: %s\n", name, result.error_msg);
        result.release();
        return false;
    }

    // Let the derived system inspect/modify the result (e.g. SID handling)
    if (!on_file_parsed(result, filepath)) {
        result.release();
        return false;
    }

    // Determine load mode based on format type
    LoadMode mode = LoadMode::DIRECT;
    if (result.format) {
        const char* fmt = result.format->name;
        if (fmt && strcmp(fmt, "D64") == 0) {
            mode = LoadMode::DISK_FAST;
        } else if (fmt && strcmp(fmt, "TAP") == 0) {
            mode = LoadMode::TAPE_INSERTED;
        }
    }

    // Defer loading until KERNAL/BASIC boot completes
    pending_load_.result = result;
    pending_load_.filepath = filepath;
    pending_load_.active = true;
    pending_load_.mode = mode;

    // Set program title to bare filename (on_file_parsed may have already
    // set a richer title from metadata — only overwrite if still empty)
    if (program_title_.empty()) {
        const char* bare = filepath;
        const char* sep = strrchr(filepath, '/');
        if (!sep) sep = strrchr(filepath, '\\');
        if (sep) bare = sep + 1;
        program_title_ = bare;
    }

    printf("%s: File parsed (mode=%s) — deferred until BASIC READY\n",
           name,
           mode == LoadMode::DISK_FAST ? "DISK_FAST" :
           mode == LoadMode::TAPE_INSERTED ? "TAPE_INSERTED" : "DIRECT");
    return true;
}

void CommodoreSystem::check_deferred_load() {
    if (pending_load_.active && is_basic_ready()) {
        apply_pending_load();
    }
}

void CommodoreSystem::clear_pending_load() {
    if (pending_load_.active) {
        pending_load_.result.release();
        pending_load_.active = false;
    }
}

void CommodoreSystem::apply_pending_load() {
    if (!pending_load_.active || !is_system_initialized()) return;

    const char* name = get_descriptor().short_name;

    // Let derived system handle special cases (e.g. SID player)
    if (pre_apply_pending_load()) {
        pending_load_.result.release();
        pending_load_.active = false;
        boot_completed_ = true;
        return;
    }

    // =========================================================================
    // DISK_FAST PATH — D64: Insert disk into 1541 + extract first PRG to RAM
    // =========================================================================
    if (pending_load_.mode == LoadMode::DISK_FAST) {
        printf("%s: BASIC READY — DISK_FAST load\n", name);

        int iec_port = get_iec_port_index();
        Drive1541Device* drive = nullptr;

        if (iec_port >= 0) {
            auto* port = get_connector_port(iec_port);
            if (port) {
                for (auto* dev : port->get_attached_devices()) {
                    drive = dynamic_cast<Drive1541Device*>(dev);
                    if (drive) break;
                }
            }

            if (!drive) {
                // Auto-attach a 1541 drive
                if (port && attach_device_to_port(iec_port, "1541")) {
                    for (auto* dev : port->get_attached_devices()) {
                        drive = dynamic_cast<Drive1541Device*>(dev);
                        if (drive) break;
                    }
                    if (drive) {
                        printf("%s: Auto-attached 1541 drive #8\n", name);
                    }
                }
            }

            if (drive) {
                drive->insert_disk(pending_load_.filepath.c_str());
                printf("%s: D64 inserted into drive #%d\n", name,
                       drive->get_device_number());
            } else {
                printf("%s: No IEC serial port available — D64 not mounted\n", name);
            }
        }

        // Hybrid: also write the first extracted PRG to RAM for fast load
        if (pending_load_.result.type == FORMAT_LOAD_PROGRAM &&
            pending_load_.result.program.data) {
            auto ctx = build_load_context();
            commodore_apply_load_result(&ctx, &pending_load_.result,
                                        pending_load_.filepath.c_str());
        } else if (drive) {
            inject_keys("LOAD\"*\",8,1\r");
            printf("%s: Injected LOAD\"*\",8,1 for disk loading\n", name);
        }
    }
    // =========================================================================
    // TAPE_INSERTED PATH — TAP: Insert tape + inject LOAD
    // =========================================================================
    else if (pending_load_.mode == LoadMode::TAPE_INSERTED) {
        printf("%s: BASIC READY — TAPE_INSERTED load\n", name);

        int cass_port = get_cassette_port_index();
        Datasette1530Device* datasette = nullptr;

        if (cass_port >= 0) {
            auto* port = get_connector_port(cass_port);
            if (port) {
                datasette = dynamic_cast<Datasette1530Device*>(
                    port->get_attached_device());
            }
        }

        if (datasette) {
            datasette->load_tap(pending_load_.filepath.c_str());
            datasette->press_play();
            printf("%s: TAP loaded into datasette, PLAY pressed\n", name);
            inject_keys("LOAD\r");
        } else {
            printf("%s: No datasette attached — TAP not loaded\n", name);
        }
    }
    // =========================================================================
    // STANDARD PATH — PRG / T64 / LNX / BIN: Write to RAM + auto-run
    // =========================================================================
    else {
        printf("%s: BASIC READY — applying deferred load\n", name);

        auto ctx = build_load_context();
        commodore_apply_load_result(&ctx, &pending_load_.result,
                                    pending_load_.filepath.c_str());
    }

    pending_load_.result.release();
    pending_load_.active = false;
    boot_completed_ = true;
}

// ============================================================================
// attach_media — swap-attach a container/streamable file to storage device
//
// Handles D64 → 1541 drive (IEC bus) and TAP → datasette (cassette port).
// Auto-attaches the required device if not yet connected — this is safe
// on a running system (just plugging in a peripheral).
// ============================================================================

bool CommodoreSystem::attach_media(const char* filepath) {
    if (!filepath) return false;

    const char* sysname = get_descriptor().short_name;

    // Determine format from file extension
    std::string ext = vfs_extension(filepath);
    const auto* fmt = FormatRegistry::instance().find_by_extension(ext.c_str());
    if (!fmt) return false;

    // D64 → IEC serial bus → 1541 drive
    if (fmt->capabilities & FORMAT_CAP_VOLUME) {
        int iec_port = get_iec_port_index();
        if (iec_port < 0) {
            printf("%s: No IEC serial port defined — cannot attach media\n", sysname);
            return false;
        }

        auto* port = get_connector_port(iec_port);
        if (!port) return false;

        Drive1541Device* drive = nullptr;
        for (auto* dev : port->get_attached_devices()) {
            drive = dynamic_cast<Drive1541Device*>(dev);
            if (drive) break;
        }

        // Auto-attach a 1541 if none is connected
        if (!drive) {
            if (attach_device_to_port(iec_port, "1541")) {
                for (auto* dev : port->get_attached_devices()) {
                    drive = dynamic_cast<Drive1541Device*>(dev);
                    if (drive) break;
                }
                if (drive) {
                    printf("%s: Auto-attached 1541 drive #%d for media insert\n",
                           sysname, drive->get_device_number());
                }
            }
        }

        if (drive) {
            if (drive->swap_disk(filepath)) {
                printf("%s: Disk swapped in drive #%d: %s\n",
                       sysname, drive->get_device_number(), filepath);
                return true;
            }
        }
        return false;
    }

    // TAP → Cassette port → Datasette
    if (fmt->capabilities & FORMAT_CAP_STREAMABLE) {
        int cass_port = get_cassette_port_index();
        if (cass_port < 0) {
            printf("%s: No cassette port defined — cannot attach media\n", sysname);
            return false;
        }

        auto* port = get_connector_port(cass_port);
        if (!port) return false;

        auto* datasette = dynamic_cast<Datasette1530Device*>(port->get_attached_device());

        // Auto-attach a datasette if none is connected
        if (!datasette) {
            if (attach_device_to_port(cass_port, "datasette")) {
                datasette = dynamic_cast<Datasette1530Device*>(port->get_attached_device());
                if (datasette) {
                    printf("%s: Auto-attached datasette for tape insert\n", sysname);
                }
            }
        }

        if (datasette) {
            if (datasette->load_tap(filepath)) {
                printf("%s: Tape inserted: %s\n", sysname, filepath);
                return true;
            }
        }
        return false;
    }

    return false;
}
