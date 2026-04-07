#include "core/cermu.hpp"
#include "systems/commodore/commodore_system.hpp"
#include "devices/storage/drive_1541.hpp"
#include "devices/storage/datasette_1530.hpp"
#include "core/formats/format_registry.hpp"
#include "core/config/path_discovery.hpp"
#include "core/vfs/vfs.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include <cstring>
#include <cstdio>
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

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

    // Apply drive mode from custom settings
    apply_drive_mode_config();

    return true;
}

// ============================================================================
// Drive Subsystem — shared cycle-accurate drive infrastructure
// ============================================================================

void CommodoreSystem::apply_drive_mode_config() {
    auto it = config_.custom_settings.find("drive_mode");
    if (it != config_.custom_settings.end()) {
        if (it->second == "Cycle-accurate")
            drive_mode_ = DriveMode::ACCURATE;
        else if (it->second == "Hooked I/O")
            drive_mode_ = DriveMode::HOOKED;
        else
            drive_mode_ = DriveMode::WARP;  // Default
    }
    // Update warp controller based on mode
    drive_subsystem_.warp().set_auto_warp(drive_mode_ == DriveMode::WARP);
}

void CommodoreSystem::init_drive_subsystem() {
    apply_drive_mode_config();

    if (!is_drive_cycle_accurate()) return;

    // Attach a default 1541 as device #8
    drive_subsystem_.attach_drive<CBM1541Traits>(8);

    // Load drive ROM from the standard data directory
    auto* drive = drive_subsystem_.get_drive(8);
    if (drive) {
        auto* adapter = dynamic_cast<IECDriveAdapter<CBM1541Traits>*>(drive);
        if (adapter) {
            char rom_root[1024];
            if (system_config_discover_rom_root("1541", rom_root, sizeof(rom_root))) {
                adapter->drive().load_roms(rom_root);
            } else {
                log_info("1541: ROM directory not found — drive may not boot\n");
            }
        }
    }
}

void CommodoreSystem::reset_drive_subsystem() {
    drive_subsystem_.reset();
    serial_trap_ = {};
}

// static
void CommodoreSystem::add_drive_mode_option(HardwareTraits& traits) {
    traits.custom_options.push_back({
        "drive_mode",
        "Drive Mode",
        "Warp: cycle-accurate drive CPU with auto-warp when motor spins. "
        "Cycle-accurate: real-time drive CPU (slow but accurate). "
        "Hooked I/O: instant KERNAL serial traps (fast, less compatible).",
        { "Warp (recommended)", "Cycle-accurate", "Hooked I/O" },
        0  // Warp default
    });
}

Drive1541Device* CommodoreSystem::find_iec_drive(int device_number) {
    if (device_number < 4) return nullptr;
    int iec_port = get_iec_port_index();
    if (iec_port < 0) return nullptr;
    auto* port = get_port(iec_port);
    if (!port) return nullptr;
    for (auto* dev : port->get_attached_devices()) {
        auto* drive = dynamic_cast<Drive1541Device*>(dev);
        if (drive && drive->get_device_number() == device_number) return drive;
    }
    return nullptr;
}

void CommodoreSystem::update_serial_traps_enabled() {
    serial_traps_enabled_ = false;
    int iec_port = get_iec_port_index();
    if (iec_port < 0) return;
    auto* port = get_port(iec_port);
    if (!port) return;
    for (auto* dev : port->get_attached_devices()) {
        if (dynamic_cast<Drive1541Device*>(dev)) {
            serial_traps_enabled_ = true;
            break;
        }
    }
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

void CommodoreSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
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
        log_info("%s: System not initialized, initializing now...\n", name);
        if (!initialize()) {
            log_info("%s: Failed to initialize system for file loading\n", name);
            return false;
        }
    }

    log_info("%s: Loading file: %s\n", name, filepath);

    // Clear any previous pending load
    clear_pending_load();

    // Parse the file into a format result
    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        log_info("%s: Failed to load file: %s\n", name, result.error_msg);
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
        } else if (fmt && strcmp(fmt, "CRT") == 0) {
            mode = LoadMode::CARTRIDGE;
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

    log_info("%s: File parsed (mode=%s) — deferred until BASIC READY\n",
           name,
           mode == LoadMode::DISK_FAST ? "DISK_FAST" :
           mode == LoadMode::TAPE_INSERTED ? "TAPE_INSERTED" :
           mode == LoadMode::CARTRIDGE ? "CARTRIDGE" : "DIRECT");
    return true;
}

void CommodoreSystem::check_deferred_load() {
    if (pending_load_.active) {
        // Cartridges are installed immediately — they provide the startup ROM,
        // so there's no BASIC READY to wait for.
        if (pending_load_.mode == LoadMode::CARTRIDGE) {
            apply_pending_load();
            return;
        }
        if (is_basic_ready()) {
            apply_pending_load();
            return;
        }
    }

    // Post-load injection: after an IEC LOAD completes and BASIC prints
    // READY again, inject the deferred command (typically "RUN\r").
    // Three-phase handshake prevents premature injection:
    //   Phase 0: queue draining (LOAD"*",8,1\r still being typed)
    //   Phase 1: drive active (LOAD in progress — 1541 motor spinning)
    //   Phase 2: drive idle + BASIC ready (LOAD complete → inject RUN)
    if (!post_load_inject_.empty()) {
        auto* drive = drive_subsystem_.get_drive(8);
        switch (post_load_phase_) {
        case 0:
            // Wait for the LOAD command to finish typing
            if (key_inject_queue_.empty() &&
                key_inject_state_ == KeyInjectState::IDLE)
                post_load_phase_ = 1;
            break;
        case 1:
            // Wait for drive to become active (motor on = LOAD started)
            if (drive && drive->is_active())
                post_load_phase_ = 2;
            break;
        case 2:
            // Wait for drive to become idle (motor off = LOAD finished)
            // then confirm BASIC is back at its READY prompt.
            if (drive && !drive->is_active() && is_basic_ready()) {
                inject_keys(post_load_inject_.c_str());
                log_info("%s: Post-load: injecting %s\n",
                         get_descriptor().short_name,
                         post_load_inject_.c_str());
                post_load_inject_.clear();
                post_load_phase_ = 0;
            }
            break;
        }
    }
}

void CommodoreSystem::clear_pending_load() {
    if (pending_load_.active) {
        pending_load_.result.release();
        pending_load_.active = false;
    }
}

void CommodoreSystem::reset_load_state() {
    boot_completed_ = false;
    clear_pending_load();
    // Clear any in-progress keyboard injection
    key_inject_queue_.clear();
    post_load_inject_.clear();
    post_load_phase_ = 0;
    if (key_inject_state_ == KeyInjectState::PRESSED)
        release_petscii_action(key_inject_current_);
    key_inject_state_ = KeyInjectState::IDLE;
    key_inject_current_ = {};
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
    // DISK_FAST PATH — D64: behaviour depends on drive emulation mode
    //
    //   WARP / ACCURATE  — insert D64 into cycle-accurate 1541, inject
    //                       LOAD"*",8,1 so the KERNAL loads via IEC bus.
    //   HOOKED           — extract PRG from D64 directly to RAM (instant),
    //                       also insert into hooked drive for directory access.
    // =========================================================================
    if (pending_load_.mode == LoadMode::DISK_FAST) {
        log_info("%s: BASIC READY — DISK_FAST load (drive mode: %s)\n", name,
                 is_drive_cycle_accurate() ? "cycle-accurate" : "hooked");

        if (is_drive_cycle_accurate()) {
            // ── WARP / ACCURATE: let the real 1541 handle everything ────
            auto* iec_drive = drive_subsystem_.get_drive(8);
            if (iec_drive) {
                if (iec_drive->insert_disk(pending_load_.filepath.c_str())) {
                    log_info("%s: D64 inserted into cycle-accurate drive #8\n", name);
                } else {
                    log_info("%s: Failed to insert D64 into cycle-accurate drive #8\n", name);
                }
            }
            inject_keys("LOAD\"*\",8,1\r");
            post_load_inject_ = "RUN\r";
            log_info("%s: Injected LOAD\"*\",8,1 (RUN deferred until LOAD completes)\n", name);
        } else {
            // ── HOOKED: fast-extract PRG to RAM, also mount for browsing ──
            int iec_port = get_iec_port_index();
            if (iec_port >= 0) {
                auto* port = get_port(iec_port);
                Drive1541Device* drive = nullptr;
                if (port) {
                    for (auto* dev : port->get_attached_devices()) {
                        drive = dynamic_cast<Drive1541Device*>(dev);
                        if (drive) break;
                    }
                }
                if (!drive && port && attach_device_to_port(iec_port, "1541")) {
                    for (auto* dev : port->get_attached_devices()) {
                        drive = dynamic_cast<Drive1541Device*>(dev);
                        if (drive) break;
                    }
                    if (drive) log_info("%s: Auto-attached 1541 drive #8\n", name);
                }
                if (drive) {
                    drive->insert_disk(pending_load_.filepath.c_str());
                    log_info("%s: D64 inserted into hooked drive #%d\n", name,
                             drive->get_device_number());
                }
            }

            if (pending_load_.result.type == FORMAT_LOAD_PROGRAM &&
                pending_load_.result.program.data) {
                auto ctx = build_load_context();
                commodore_apply_load_result(&ctx, &pending_load_.result,
                                            pending_load_.filepath.c_str());
            } else {
                log_info("%s: No PRG data in D64 — cannot fast-load in hooked mode\n", name);
            }
        }
    }
    // =========================================================================
    // TAPE_INSERTED PATH — TAP: Insert tape + inject LOAD
    // =========================================================================
    else if (pending_load_.mode == LoadMode::TAPE_INSERTED) {
        log_info("%s: BASIC READY — TAPE_INSERTED load\n", name);

        int cass_port = get_cassette_port_index();
        Datasette1530Device* datasette = nullptr;

        if (cass_port >= 0) {
            auto* port = get_port(cass_port);
            if (port) {
                datasette = dynamic_cast<Datasette1530Device*>(
                    port->get_attached_device());
            }
        }

        if (datasette) {
            datasette->load_tap(pending_load_.filepath.c_str());
            datasette->press_play();
            log_info("%s: TAP loaded into datasette, PLAY pressed\n", name);
            inject_keys("LOAD\r");
        } else {
            log_info("%s: No datasette attached — TAP not loaded\n", name);
        }
    }
    // =========================================================================
    // CARTRIDGE PATH — CRT: inject ROM + EXROM/GAME + reset
    // =========================================================================
    else if (pending_load_.mode == LoadMode::CARTRIDGE) {
        log_info("%s: Installing CRT cartridge from %s\n", name,
                 pending_load_.filepath.c_str());

        if (!install_cartridge(pending_load_.filepath.c_str())) {
            log_info("%s: CRT cartridge installation failed\n", name);
        }
    }
    // =========================================================================
    // STANDARD PATH — PRG / T64 / LNX / BIN: Write to RAM + auto-run
    // =========================================================================
    else {
        log_info("%s: BASIC READY — applying deferred load\n", name);

        auto ctx = build_load_context();
        commodore_apply_load_result(&ctx, &pending_load_.result,
                                    pending_load_.filepath.c_str());
    }

    pending_load_.result.release();
    pending_load_.active = false;
    boot_completed_ = true;
}

// ============================================================================
// Per-frame keyboard injection
//
// Types characters at the keyboard matrix level, one key per frame.
// Works identically across all Commodore systems (C64, VIC-20, C16, C128)
// because it operates on the shared commodore_keyboard_t matrix contacts.
// ============================================================================

void CommodoreSystem::inject_keys(const char* str) {
    if (!str) return;
    key_inject_queue_.append(str, strlen(str));
}

void CommodoreSystem::build_petscii_map() {
    // Clear the map
    for (int i = 0; i < 256; i++)
        petscii_map_[i] = {0, 0, 0, false};

    if (!keyboard_ || !keyboard_->decode_tables) return;

    uint8_t rows = keyboard_->matrix_rows;
    uint8_t cols = keyboard_->matrix_cols;

    // Reverse-map the decode tables: for each PETSCII code, record the
    // first (row, col, modifier) that produces it.  KEYMOD_NONE first,
    // then KEYMOD_SHIFT, so unshifted mappings win for duplicates.
    for (int t = 0; t < keyboard_->num_decode_tables; t++) {
        const keyboard_decode_table_t& table = keyboard_->decode_tables[t];
        if (!table.petscii) continue;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                petscii_t p = table.petscii[row * cols + col];
                if (p == 0) continue;

                // First-write-wins: don't overwrite existing mappings
                if (!petscii_map_[p].valid) {
                    petscii_map_[p] = {
                        static_cast<uint8_t>(row),
                        static_cast<uint8_t>(col),
                        table.modifiers, true
                    };
                }
            }
        }
    }

    // Map RETURN ($0D) — not in decode tables (listed as 0).
    // Find EMUKEY_RETURN in the key identity table.
    if (!petscii_map_[0x0D].valid && keyboard_->active_keys) {
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                if (keyboard_->active_keys[row * cols + col] == EMUKEY_RETURN) {
                    petscii_map_[0x0D] = {
                        static_cast<uint8_t>(row),
                        static_cast<uint8_t>(col),
                        KEYMOD_NONE, true
                    };
                    goto found_return;
                }
            }
        }
        found_return:;
    }

    int count = 0;
    for (int i = 0; i < 256; i++)
        if (petscii_map_[i].valid) count++;
    log_info("CommodoreSystem: Built PETSCII→matrix map (%d entries)\n", count);
}

void CommodoreSystem::press_petscii_action(const PetsciiKeyAction& action) {
    if (!keyboard_ || !action.valid) return;

    uint8_t rows = keyboard_->matrix_rows;
    uint8_t cols = keyboard_->matrix_cols;
    if (action.row >= rows || action.col >= cols) return;

    // Convert array indices to hardware port bit numbers (same as KeyboardMapper)
    auto close_contact = [&](uint8_t row, uint8_t col) {
        uint8_t row_bit = (rows - 1) - row;
        uint8_t col_bit = (col < 8) ? (7 - col) : col;
        keyboard_->row_open_contacts[row_bit] &= ~(1 << col_bit);
        keyboard_->col_open_contacts[col_bit] &= ~(1 << row_bit);
    };

    // Press modifier(s) if required
    if (action.modifiers & KEYMOD_SHIFT) {
        // Find left shift position
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (keyboard_->active_keys[r * cols + c] == EMUKEY_LSHIFT) {
                    close_contact(r, c);
                    goto shift_done;
                }
            }
        }
        shift_done:;
    }

    // Press the main key
    close_contact(action.row, action.col);
}

void CommodoreSystem::release_petscii_action(const PetsciiKeyAction& action) {
    if (!keyboard_ || !action.valid) return;

    uint8_t rows = keyboard_->matrix_rows;
    uint8_t cols = keyboard_->matrix_cols;
    if (action.row >= rows || action.col >= cols) return;

    auto open_contact = [&](uint8_t row, uint8_t col) {
        uint8_t row_bit = (rows - 1) - row;
        uint8_t col_bit = (col < 8) ? (7 - col) : col;
        keyboard_->row_open_contacts[row_bit] |= (1 << col_bit);
        keyboard_->col_open_contacts[col_bit] |= (1 << row_bit);
    };

    // Release the main key
    open_contact(action.row, action.col);

    // Release modifier(s)
    if (action.modifiers & KEYMOD_SHIFT) {
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (keyboard_->active_keys[r * cols + c] == EMUKEY_LSHIFT) {
                    open_contact(r, c);
                    goto shift_released;
                }
            }
        }
        shift_released:;
    }
}

void CommodoreSystem::tick_key_injection() {
    if (key_inject_queue_.empty() && key_inject_state_ == KeyInjectState::IDLE)
        return;

    switch (key_inject_state_) {
    case KeyInjectState::IDLE: {
        // Pop next character and press it
        uint8_t ch = static_cast<uint8_t>(key_inject_queue_.front());
        key_inject_queue_.erase(key_inject_queue_.begin());

        const auto& action = petscii_map_[ch];
        if (action.valid) {
            key_inject_current_ = action;
            press_petscii_action(action);
            key_inject_state_ = KeyInjectState::PRESSED;
        }
        // If no mapping for this char, skip it silently
        break;
    }
    case KeyInjectState::PRESSED:
        // Release the previously pressed key
        release_petscii_action(key_inject_current_);
        key_inject_current_ = {};
        key_inject_state_ = KeyInjectState::IDLE;
        break;
    }
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
            log_info("%s: No IEC serial port defined — cannot attach media\n", sysname);
            return false;
        }

        auto* port = get_port(iec_port);
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
                    log_info("%s: Auto-attached 1541 drive #%d for media insert\n",
                           sysname, drive->get_device_number());
                }
            }
        }

        if (drive) {
            if (drive->swap_disk(filepath)) {
                log_info("%s: Disk swapped in drive #%d: %s\n",
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
            log_info("%s: No cassette port defined — cannot attach media\n", sysname);
            return false;
        }

        auto* port = get_port(cass_port);
        if (!port) return false;

        auto* datasette = dynamic_cast<Datasette1530Device*>(port->get_attached_device());

        // Auto-attach a datasette if none is connected
        if (!datasette) {
            if (attach_device_to_port(cass_port, "datasette")) {
                datasette = dynamic_cast<Datasette1530Device*>(port->get_attached_device());
                if (datasette) {
                    log_info("%s: Auto-attached datasette for tape insert\n", sysname);
                }
            }
        }

        if (datasette) {
            if (datasette->load_tap(filepath)) {
                log_info("%s: Tape inserted: %s\n", sysname, filepath);
                return true;
            }
        }
        return false;
    }

    return false;
}

// ============================================================================
// Unmapped input — virtual key menu for keys without host mapping
// ============================================================================

void CommodoreSystem::register_unmapped_input(const char* label, emu_key_t key, bool toggle, bool initial_state) {
    unmapped_inputs_.emplace_back(label, key, toggle, initial_state);
}

void CommodoreSystem::render_unmapped_inputs_menu() {
#ifdef CERMU_HAS_GUI
    if (unmapped_inputs_.empty()) return;
    if (!keyboard_) return;

    if (ImGui::BeginMenu("Virtual Keys")) {
        // Momentary keys first (press-and-release on click)
        for (int i = 0; i < static_cast<int>(unmapped_inputs_.size()); i++) {
            auto& input = unmapped_inputs_[i];
            if (input.toggle) continue;
            if (ImGui::MenuItem(input.label)) {
                keyboard_->key_down(input.key, false);
                unmapped_release_index_ = i;
                unmapped_release_countdown_ = 3;  // Release after 3 frames
            }
        }
        // Separator between momentary and toggle groups
        bool has_toggles = false;
        for (auto& input : unmapped_inputs_) {
            if (input.toggle) { has_toggles = true; break; }
        }
        if (has_toggles) ImGui::Separator();
        // Toggle keys (stay pressed until clicked again)
        for (int i = 0; i < static_cast<int>(unmapped_inputs_.size()); i++) {
            auto& input = unmapped_inputs_[i];
            if (!input.toggle) continue;
            if (ImGui::MenuItem(input.label, nullptr, input.pressed)) {
                input.pressed = !input.pressed;
                if (input.pressed)
                    keyboard_->key_down(input.key, false);
                else
                    keyboard_->key_up(input.key, false);
                on_unmapped_toggle_changed(input.key, input.pressed);
            }
        }
        ImGui::EndMenu();
    }
#endif
}

void CommodoreSystem::tick_unmapped_inputs() {
    if (unmapped_release_countdown_ > 0) {
        if (--unmapped_release_countdown_ == 0 && keyboard_) {
            if (unmapped_release_index_ >= 0 &&
                unmapped_release_index_ < static_cast<int>(unmapped_inputs_.size())) {
                keyboard_->key_up(unmapped_inputs_[unmapped_release_index_].key, false);
            }
            unmapped_release_index_ = -1;
        }
    }
}

void CommodoreSystem::set_unmapped_toggle_state(emu_key_t key, bool pressed) {
    for (auto& input : unmapped_inputs_) {
        if (input.toggle && input.key == key) {
            input.pressed = pressed;
            return;
        }
    }
}
