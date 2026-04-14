#define SDL_MAIN_HANDLED
#include "core/system.hpp"
#include "core/chip_registry.hpp"
#include "core/chip_manifest.hpp"
#include "core/device_registry.hpp"
#include "core/port_registry.hpp"
#include "core/formats/format_registry.hpp"
#include "core/archive_scanner.hpp"
#include "core/vfs/vfs.hpp"
#include "gui/session_gui.hpp"
#include "testing/vicii_test_harness.hpp"
#include "testing/vicii_pixel_tests.hpp"
#include "testing/sid_write_log.hpp"
#include "core/chip_layout_ascii.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifndef ATTACH_TO_PARENT_PROCESS
#define ATTACH_TO_PARENT_PROCESS ((DWORD)-1)
#endif

// When built as a WIN32 (GUI) app, stdout/stderr are not connected to any
// console. If we were launched from a terminal, reattach to the parent
// console so printf / fprintf output appears there as expected.
static bool s_attached_parent_console = false;

// On exit, send a synthetic Enter keypress to the parent console so the
// shell re-displays its prompt (it won't wait for a GUI-subsystem process).
static void win32_detach_console() {
    if (!s_attached_parent_console) return;

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        INPUT_RECORD ir = {};
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        ir.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(
            MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC));
        ir.Event.KeyEvent.uChar.AsciiChar = '\r';
        ir.Event.KeyEvent.dwControlKeyState = 0;
        DWORD written = 0;
        WriteConsoleInputA(hInput, &ir, 1, &written);
    }
    FreeConsole();
    s_attached_parent_console = false;
}

static void win32_attach_parent_console() {
    if (AttachConsole(ATTACH_TO_PARENT_PROCESS)) {
        s_attached_parent_console = true;
        atexit(win32_detach_console);

        // Redirect stdout
        FILE* fp = nullptr;
        if (_fileno(stdout) < 0 || _get_osfhandle(_fileno(stdout)) == -1) {
            freopen_s(&fp, "CONOUT$", "w", stdout);
            if (fp) setvbuf(fp, nullptr, _IONBF, 0);
        }
        // Redirect stderr
        if (_fileno(stderr) < 0 || _get_osfhandle(_fileno(stderr)) == -1) {
            freopen_s(&fp, "CONOUT$", "w", stderr);
            if (fp) setvbuf(fp, nullptr, _IONBF, 0);
        }
        // Redirect stdin
        if (_fileno(stdin) < 0 || _get_osfhandle(_fileno(stdin)) == -1) {
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
}
#endif

// Force linker to include system registrations
// Systems self-register during static initialization via REGISTER_SYSTEM macro
// We just need to ensure the system object files are linked
#include "systems/chip8/chip8_system.hpp"
#include "systems/commodore/c64/c64_system.hpp"

// ============================================================================
// PREFIX-MATCHING HELPERS
// ============================================================================

/// Case-insensitive prefix match.  Returns true if `prefix` is a
/// case-insensitive prefix of `full` (including exact match).
static bool iprefix(const char* full, const char* prefix) {
    for (; *prefix; ++full, ++prefix) {
        if (tolower(static_cast<unsigned char>(*full)) !=
            tolower(static_cast<unsigned char>(*prefix)))
            return false;
    }
    return true;
}

/// Find a system descriptor by case-insensitive prefix of short_name or any
/// alias.  Returns the matched descriptor, or nullptr on no match / ambiguity.
/// On ambiguity, prints an error listing the conflicting matches.
static const SystemDescriptor* prefix_match_system(const char* input) {
    const auto& systems = SystemRegistry::instance().get_systems();
    const SystemDescriptor* found = nullptr;

    // Exact match first (short_name + aliases)
    for (const auto& [desc, factory] : systems) {
        if (strcasecmp(input, desc.short_name) == 0) return &desc;
        for (const char* alias : desc.aliases)
            if (strcasecmp(input, alias) == 0) return &desc;
    }

    // Prefix match
    for (const auto& [desc, factory] : systems) {
        bool hit = iprefix(desc.short_name, input);
        if (!hit) {
            for (const char* alias : desc.aliases)
                if (iprefix(alias, input)) { hit = true; break; }
        }
        if (hit) {
            if (found) {
                printf("ERROR: '%s' is ambiguous.  Matches at least:\n", input);
                printf("  %s  (%s)\n", found->short_name, found->name);
                printf("  %s  (%s)\n", desc.short_name, desc.name);
                return nullptr;
            }
            found = &desc;
        }
    }
    return found;
}

// ============================================================================
// INFORMATIONAL DUMP HELPERS
// ============================================================================

static const char* video_standard_name(VideoStandard s) {
    switch (s) {
        case VideoStandard::NTSC:   return "NTSC";
        case VideoStandard::PAL:    return "PAL";
        case VideoStandard::PAL_M:  return "PAL-M";
        case VideoStandard::SECAM:  return "SECAM";
        case VideoStandard::CUSTOM: return "Custom";
    }
    return "Unknown";
}

static const char* system_type_name(SystemType t) {
    switch (t) {
        case SystemType::Home:     return "Home Computer";
        case SystemType::Console:  return "Console";
        case SystemType::Handheld: return "Handheld";
        case SystemType::Arcade:   return "Arcade";
        case SystemType::Other:    return "Other";
    }
    return "Unknown";
}

/// --list systems
static int dump_systems() {
    const auto& systems = SystemRegistry::instance().get_systems();
    std::vector<const SystemDescriptor*> sorted;
    sorted.reserve(systems.size());
    for (const auto& [desc, factory] : systems) sorted.push_back(&desc);
    // Sort by: category → maker → system ID
    auto type_order = [](SystemType t) -> int {
        switch (t) {
            case SystemType::Arcade:   return 0;
            case SystemType::Console:  return 1;
            case SystemType::Handheld: return 2;
            case SystemType::Home:     return 3;
            case SystemType::Other:    return 4;
        }
        return 5;
    };
    std::sort(sorted.begin(), sorted.end(), [&type_order](auto* a, auto* b) {
        int ta = type_order(a->type), tb = type_order(b->type);
        if (ta != tb) return ta < tb;
        int mk = (a->maker && b->maker) ? strcasecmp(a->maker, b->maker)
               : (a->maker ? -1 : (b->maker ? 1 : 0));
        if (mk != 0) return mk < 0;
        return strcasecmp(a->short_name, b->short_name) < 0;
    });
    printf("Registered systems (%zu):\n", sorted.size());
    for (const auto* dp : sorted) {
        const auto& desc = *dp;
        printf("  %-10s  %-24s  %-14s",
               desc.short_name, desc.name, system_type_name(desc.type));
        if (desc.maker && desc.year > 0)
            printf("  %s %d", desc.maker, desc.year);
        else if (desc.maker)
            printf("  %s", desc.maker);
        else if (desc.year > 0)
            printf("  %d", desc.year);
        if (desc.cpu_summary)
            printf("  [%s]", desc.cpu_summary);
        printf("\n");
    }
    return 0;
}

/// --list chips
static int dump_chips() {
    const auto& entries = ChipRegistry::instance().entries();

    // Collect chip info by instantiating each chip, then sort.
    struct ChipRow {
        std::string id;
        std::string category;
        std::string manufacturer;
        std::string display_name;
        std::string package;
    };
    std::vector<ChipRow> rows;
    rows.reserve(entries.size());

    ChipSlot dummy_slot{};
    for (const auto& entry : entries) {
        ChipRow row;
        row.id = std::string(entry.name);

        std::unique_ptr<ChipBase> chip;
        if (entry.factory)
            chip.reset(entry.factory(dummy_slot, nullptr, nullptr));
        if (chip) {
            row.category = chip->category();
            const auto& ci = chip->chip_info();
            if (!ci.manufacturer.empty()) row.manufacturer = std::string(ci.manufacturer);
            if (!ci.display_name.empty()) row.display_name = std::string(ci.display_name);
#ifdef CERMU_HAS_GUI
            if (auto* layout = chip->get_chip_layout())
                row.package = layout->get_package_name();
#endif
        }
        rows.push_back(std::move(row));
    }

    // Sort by type (category), then manufacturer, then id
    std::sort(rows.begin(), rows.end(), [](const ChipRow& a, const ChipRow& b) {
        int cmp = strcasecmp(a.category.c_str(), b.category.c_str());
        if (cmp != 0) return cmp < 0;
        cmp = strcasecmp(a.manufacturer.c_str(), b.manufacturer.c_str());
        if (cmp != 0) return cmp < 0;
        return strcasecmp(a.id.c_str(), b.id.c_str()) < 0;
    });

    printf("Registered chip types (%zu):\n", rows.size());
    for (const auto& row : rows) {
        printf("  %-20s  %-8s",
               row.id.c_str(), row.category.c_str());
        if (!row.display_name.empty()) printf("  %-24s", row.display_name.c_str());
        if (!row.manufacturer.empty()) printf("  [%s]", row.manufacturer.c_str());
        if (!row.package.empty())      printf("  %s", row.package.c_str());
        printf("\n");
    }
    return 0;
}

/// --list devices
static int dump_devices() {
    const auto& devices = DeviceRegistry::instance().get_all_devices();
    std::vector<const DeviceDescriptor*> sorted;
    sorted.reserve(devices.size());
    for (const auto& [desc, factory] : devices) sorted.push_back(&desc);
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) {
        return strcasecmp(a->id, b->id) < 0;
    });
    printf("Registered peripheral devices (%zu):\n", sorted.size());
    for (const auto* dp : sorted) {
        const auto& desc = *dp;
        printf("  %-16s  %-28s  [%s]%s\n",
               desc.id, desc.name,
               port_type_name(desc.port_type),
               desc.is_bus_device ? "  (bus)" : "");
        if (desc.description)
            printf("                    %s\n", desc.description);
    }
    return 0;
}

/// --list ports
static int dump_ports() {
    const auto& entries = PortRegistry::instance().entries();
    std::vector<const PortDefinition*> sorted;
    sorted.reserve(entries.size());
    for (const auto& def : entries) sorted.push_back(&def);
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) {
        return strcasecmp(a->name, b->name) < 0;
    });
    printf("Registered port definitions (%zu):\n", sorted.size());
    for (const auto* pp : sorted) {
        const auto& def = *pp;
        printf("  %-32s  %d signal(s)%s%s\n",
               def.name,
               def.signal_count,
               def.is_internal ? "  [internal]" : "",
               def.is_bus      ? "  [bus]"      : "");
        if (def.signals && def.signal_count > 0) {
            printf("    Signals:");
            for (uint8_t i = 0; i < def.signal_count; ++i) {
                const char* dir = "?";
                switch (def.signals[i].direction) {
                    case SignalDirection::INPUT:         dir = "in";    break;
                    case SignalDirection::OUTPUT:        dir = "out";   break;
                    case SignalDirection::BIDIRECTIONAL: dir = "bidir"; break;
                }
                printf(" %s(%s)", def.signals[i].name, dir);
            }
            printf("\n");
        }
    }
    return 0;
}

/// --list formats
static int dump_formats() {
    auto formats = FormatRegistry::instance().get_formats();
    std::sort(formats.begin(), formats.end(), [](auto* a, auto* b) {
        return strcasecmp(a->name, b->name) < 0;
    });
    printf("Registered file formats (%zu):\n", formats.size());
    for (const auto* fmt : formats) {
        // Collect extensions
        std::string exts;
        if (fmt->extensions) {
            for (const char** ext = fmt->extensions; *ext; ++ext) {
                if (!exts.empty()) exts += ", ";
                exts += *ext;
            }
        }
        // Collect capabilities
        std::string caps;
        if (fmt->capabilities & FORMAT_CAP_LOADABLE)  caps += "load ";
        if (fmt->capabilities & FORMAT_CAP_CONTAINER) caps += "container ";
        if (fmt->capabilities & FORMAT_CAP_STREAMABLE) caps += "stream ";
        if (fmt->capabilities & FORMAT_CAP_METADATA)  caps += "meta ";
        if (fmt->capabilities & FORMAT_CAP_VOLUME)    caps += "volume ";
        printf("  %-8s  %-28s  %-16s  [%s]\n",
               fmt->name,
               fmt->description ? fmt->description : "",
               exts.c_str(),
               caps.empty() ? "none" : caps.c_str());
    }
    return 0;
}

/// Dispatch --list <registry> (accepts unique prefix)
static int dump_registry(const char* which) {
    struct { const char* name; int (*fn)(); } regs[] = {
        {"systems", dump_systems}, {"chips", dump_chips},
        {"devices", dump_devices}, {"ports", dump_ports},
        {"formats", dump_formats},
    };
    // Exact match first
    for (const auto& r : regs)
        if (strcmp(which, r.name) == 0) return r.fn();
    // Prefix match
    decltype(&regs[0]) match = nullptr;
    for (auto& r : regs) {
        if (iprefix(r.name, which)) {
            if (match) {
                printf("ERROR: '%s' is ambiguous (matches '%s' and '%s')\n",
                       which, match->name, r.name);
                return 1;
            }
            match = &r;
        }
    }
    if (match) return match->fn();
    printf("ERROR: Unknown registry '%s'\n", which);
    printf("Valid registries: systems, chips, devices, ports, formats\n");
    return 1;
}

/// --pinout <name> [chip]
/// If <name> matches a registered chip type, dump its pinout directly.
/// If <name> matches a system, dump pinouts for all (or filtered) chips in
/// that system.  Chip filter matches case-insensitively against display
/// name, short name, part number, or as a substring of the display name.

/// Try to instantiate a chip from the registry and render its pinout.
/// Returns true if the name matched a registered chip type.
static bool try_dump_chip_pinout(const char* name) {
#ifdef CERMU_HAS_GUI
    auto factory = ChipRegistry::instance().lookup(name);
    if (!factory) {
        // Try case-insensitive exact match, then prefix match
        const auto& entries = ChipRegistry::instance().entries();
        ChipSlot::FactoryFn prefix_hit = nullptr;
        bool ambiguous = false;
        for (const auto& entry : entries) {
            if (entry.name.size() == strlen(name) &&
                strncasecmp(name, entry.name.data(), entry.name.size()) == 0) {
                factory = entry.factory;
                break;
            }
            if (iprefix(entry.name.data(), name)) {
                if (prefix_hit) ambiguous = true;
                prefix_hit = entry.factory;
            }
        }
        if (!factory && prefix_hit) {
            if (ambiguous) {
                printf("ERROR: '%s' is ambiguous.  Matches:\n", name);
                for (const auto& e : entries) {
                    if (iprefix(e.name.data(), name))
                        printf("  %s\n", std::string(e.name).c_str());
                }
                return true;  // Matched (ambiguously) — don't fall through to systems
            }
            factory = prefix_hit;
        }
    }
    if (!factory) return false;

    ChipSlot dummy_slot{};
    std::unique_ptr<ChipBase> chip(factory(dummy_slot, nullptr, nullptr));
    if (!chip) return false;

    ChipLayout* layout = chip->get_chip_layout();
    if (!layout) {
        printf("Chip '%s' has no pinout layout defined.\n", name);
        return true;  // Matched the name, just no layout
    }

    const char* label = chip->get_layout_chip_name();
    if (!label) label = chip->display_name();
    // Short name for the chip body (part number fits inside narrow DIP outline)
    const auto& ci = chip->chip_info();
    const char* body_name = !ci.part_number.empty() ? ci.part_number.data() : label;
    std::string pkg = layout->get_package_name();
    printf("\n=== %s ===", label);
    if (!pkg.empty()) printf("  (%s)", pkg.c_str());
    printf("\n\n");
    render_chip_pinout_ascii(*layout, body_name);
    return true;
#else
    (void)name;
    printf("ERROR: Pinout rendering requires a GUI-enabled build (CERMU_HAS_GUI)\n");
    return true;
#endif
}

/// Dump pinouts for all (or filtered) chips in a system.
static int dump_system_pinouts(const char* sys_name, const char* chip_filter) {
    LogLevelGuard guard(LogLevel::Silent);
    auto sys = SystemRegistry::instance().create_system(sys_name);
    if (!sys) {
        printf("ERROR: System not found: %s\n", sys_name);
        return 1;
    }
    if (!sys->initialize()) {
        printf("ERROR: Failed to initialize %s\n", sys_name);
        sys->shutdown();
        return 1;
    }

    const auto& chips = sys->get_registered_chips();
    bool found_any = false;

    for (const auto& sc : chips) {
        if (!sc.chip) continue;

#ifdef CERMU_HAS_GUI
        ChipLayout* layout = sc.chip->get_chip_layout();
        if (!layout) continue;

        // Apply chip filter if specified
        if (chip_filter) {
            bool match = false;
            if (sc.display_name && strcasecmp(chip_filter, sc.display_name) == 0)
                match = true;
            if (sc.short_name && strcasecmp(chip_filter, sc.short_name) == 0)
                match = true;
            const auto& ci = sc.chip->chip_info();
            if (!ci.part_number.empty() &&
                strcasecmp(chip_filter, std::string(ci.part_number).c_str()) == 0)
                match = true;
            // Substring match for convenience
            if (!match) {
                auto icontains = [](const char* haystack, const char* needle) {
                    if (!haystack || !needle) return false;
                    std::string h(haystack), n(needle);
                    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
                    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                    return h.find(n) != std::string::npos;
                };
                if (icontains(sc.display_name, chip_filter)) match = true;
                else if (icontains(sc.short_name, chip_filter)) match = true;
                else if (!ci.part_number.empty() &&
                         icontains(ci.part_number.data(), chip_filter)) match = true;
            }
            if (!match) continue;
        }

        found_any = true;
        const char* name = sc.chip->get_layout_chip_name();
        if (!name) name = sc.display_name ? sc.display_name : sc.chip->display_name();
        // Short name for the chip body
        const auto& ci = sc.chip->chip_info();
        const char* body_name = !ci.part_number.empty() ? ci.part_number.data() : name;
        std::string pkg = layout->get_package_name();
        printf("\n=== %s ===", name);
        if (sc.base_address > 0) printf("  ($%04X)", sc.base_address);
        if (!pkg.empty()) printf("  (%s)", pkg.c_str());
        printf("\n\n");
        render_chip_pinout_ascii(*layout, body_name);
#else
        (void)chip_filter;
        printf("ERROR: Pinout rendering requires a GUI-enabled build (CERMU_HAS_GUI)\n");
        sys->shutdown();
        return 1;
#endif
    }

    if (!found_any) {
        if (chip_filter)
            printf("No chip matching \"%s\" found in %s (or it has no layout defined).\n",
                   chip_filter, sys_name);
        else
            printf("No chips with pinout layouts found in %s.\n", sys_name);
    }

    sys->shutdown();
    return 0;
}

static int dump_pinout(const char* first_arg, const char* second_arg) {
    // First, try as a standalone chip name from the registry
    if (try_dump_chip_pinout(first_arg))
        return 0;

    // Next, try as a system name (prefix match, with optional chip filter)
    const SystemDescriptor* sd = prefix_match_system(first_arg);
    if (sd) return dump_system_pinouts(sd->short_name, second_arg);

    printf("ERROR: '%s' is not a registered chip or system name.\n", first_arg);
    printf("\nUse --list chips to see all registered chip types.\n");
    printf("Use --list systems to see all registered systems.\n");
    return 1;
}

/// --manifest <system> (prefix match on short_name / aliases)
static int dump_manifest(const char* name) {
    const SystemDescriptor* found = prefix_match_system(name);
    if (!found) {
        if (found == nullptr) {
            // prefix_match_system already printed ambiguity error, or no match
            const auto& systems = SystemRegistry::instance().get_systems();
            bool any_prefix = false;
            for (const auto& [desc, factory] : systems) {
                if (iprefix(desc.short_name, name)) { any_prefix = true; break; }
            }
            if (!any_prefix) {
                printf("ERROR: System not found: %s\n", name);
                printf("Available systems:\n");
                for (const auto& [desc, factory] : systems) {
                    printf("  %-10s  %s\n", desc.short_name, desc.name);
                }
            }
        }
        return 1;
    }

    const auto& d = *found;
    printf("=== System: %s ===\n", d.name);
    printf("  Short name:   %s\n", d.short_name);
    if (d.description) printf("  Description:  %s\n", d.description);
    if (d.maker)       printf("  Manufacturer: %s\n", d.maker);
    if (d.year > 0)    printf("  Year:         %d\n", d.year);
    if (d.cpu_summary) printf("  CPU:          %s\n", d.cpu_summary);
    printf("  Type:         %s\n", system_type_name(d.type));
    if (d.data_folder) printf("  Data folder:  %s\n", d.data_folder);

    // Aliases
    if (!d.aliases.empty()) {
        printf("  Aliases:      ");
        for (size_t i = 0; i < d.aliases.size(); ++i) {
            if (i > 0) printf(", ");
            printf("%s", d.aliases[i]);
        }
        printf("\n");
    }

    // Hardware traits — display
    const auto& disp = d.hardware_traits.display;
    printf("\n  Display:\n");
    printf("    Native:    %d x %d\n", disp.native_width, disp.native_height);
    printf("    Visible:   %d x %d\n", disp.visible_width, disp.visible_height);
    printf("    Palette:   %d colors\n", disp.palette_size);
    printf("    PAR:       %.3f\n", disp.pixel_aspect_ratio);
    printf("    Overscan:  %s\n", disp.has_overscan ? "yes" : "no");

    // Hardware traits — audio
    const auto& aud = d.hardware_traits.audio;
    if (aud.format != AudioFormat::NONE) {
        printf("\n  Audio:\n");
        printf("    Sample rate:  %d Hz\n", aud.sample_rate_hz);
        printf("    Channels:     %d\n", aud.channels);
        if (aud.chip_name) printf("    Chip:         %s\n", aud.chip_name);
    }

    // Hardware traits — timing
    const auto& t = d.hardware_traits.timing;
    if (t.cpu_frequency_hz > 0) {
        printf("\n  Timing:\n");
        printf("    CPU clock:     %u Hz\n", t.cpu_frequency_hz);
        if (t.video_frequency_hz) printf("    Video clock:   %u Hz\n", t.video_frequency_hz);
        printf("    Target FPS:    %u\n", t.target_fps);
        printf("    Cycles/frame:  %u\n", t.cycles_per_frame);
        printf("    Standard:      %s\n", video_standard_name(t.standard));
    }

    // Video standard configs
    const auto& vscs = d.hardware_traits.video_standard_configs;
    if (!vscs.empty()) {
        printf("\n  Video standards:\n");
        for (const auto& vs : vscs) {
            printf("    %s%s — %u Hz CPU, %u FPS\n",
                   vs.name,
                   vs.is_default ? " (default)" : "",
                   vs.timing.cpu_frequency_hz,
                   vs.timing.target_fps);
        }
    }

    // Memory options
    const auto& memos = d.hardware_traits.memory_options;
    if (!memos.empty()) {
        printf("\n  Memory options:\n");
        for (const auto& m : memos) {
            printf("    %s%s — RAM %u, ROM %u\n",
                   m.name,
                   m.is_default ? " (default)" : "",
                   m.ram_size, m.rom_size);
        }
    }

    // Peripheral options
    const auto& perifs = d.hardware_traits.peripheral_options;
    if (!perifs.empty()) {
        printf("\n  Peripheral options:\n");
        for (const auto& p : perifs) {
            printf("    %-20s  %s%s\n", p.name,
                   p.description ? p.description : "",
                   p.enabled_by_default ? "  [default]" : "");
        }
    }

    // Custom options
    const auto& customs = d.hardware_traits.custom_options;
    if (!customs.empty()) {
        printf("\n  Custom options:\n");
        for (const auto& c : customs) {
            printf("    %s:", c.name);
            for (size_t i = 0; i < c.choices.size(); ++i) {
                printf(" %s%s", c.choices[i],
                       (static_cast<int>(i) == c.default_index) ? "*" : "");
            }
            printf("\n");
        }
    }

    // Supported file formats
    if (d.supported_formats) {
        printf("\n  Supported formats:\n");
        for (const format_descriptor_t* const* fp = d.supported_formats; *fp; ++fp) {
            const auto* fmt = *fp;
            std::string exts;
            if (fmt->extensions) {
                for (const char** ext = fmt->extensions; *ext; ++ext) {
                    if (!exts.empty()) exts += ", ";
                    exts += *ext;
                }
            }
            printf("    %-8s  %-28s  %s\n", fmt->name,
                   fmt->description ? fmt->description : "",
                   exts.c_str());
        }
    }

    // Try to create and initialize the system for chip/port enumeration.
    // Suppress log output from system init/shutdown — only our printf matters.
    // Guard declared first so it outlives `sys` (reverse destruction order).
    LogLevelGuard guard(LogLevel::Silent);
    auto sys = SystemRegistry::instance().create_system(name);
    if (sys) {
        if (!sys->initialize()) { sys->shutdown(); sys.reset(); }
    }
    if (sys) {
        // Registered chips
        const auto& chips = sys->get_registered_chips();
        if (!chips.empty()) {
            printf("\n  Chips (%zu):\n", chips.size());
            for (const auto& sc : chips) {
                const char* cat = sc.category ? sc.category : "";
                const char* disp = sc.display_name ? sc.display_name : "";
                const char* part = "";
                const char* mfr  = "";
                if (sc.chip) {
                    const auto& ci = sc.chip->chip_info();
                    if (!ci.part_number.empty()) part = ci.part_number.data();
                    if (!ci.manufacturer.empty()) mfr = ci.manufacturer.data();
                }
                if (sc.base_address > 0) {
                    printf("    $%04X  %-8s  %-28s", sc.base_address, cat, disp);
                } else {
                    printf("           %-8s  %-28s", cat, disp);
                }
                if (*mfr)  printf("  [%s]", mfr);
                if (*part && strcmp(part, disp) != 0) printf("  (%s)", part);
                printf("\n");
            }
        }

        // Ports
        const auto& boards = sys->get_boards();
        for (const auto* board : boards) {
            const auto& ports = board->get_ports();
            if (!ports.empty()) {
                printf("\n  Ports (%zu):\n", ports.size());
                for (const auto& port : ports) {
                    const auto& def = port->get_definition();
                    printf("    %-28s  %d signal(s)", def.name, def.signal_count);
                    if (def.is_bus) printf("  [bus]");
                    printf("\n");
                }
            }
        }

        sys->shutdown();
    }

    return 0;
}

// ============================================================================
// MAIN FUNCTION - Multi-System Emulator with Automatic Detection
// ============================================================================
int main(int argc, char** argv) {
#ifdef _WIN32
    win32_attach_parent_console();
#endif
    const char* file_path = nullptr;
    const char* system_name = nullptr;
    bool vicii_test_mode = false;
    bool vicii_dump_mode = false;
    bool skip_memtest = false;
    const char* sid_log_path = nullptr;
    int sid_log_seconds = 60;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--system") == 0 || strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                system_name = argv[++i];
                log_info("System specified: %s\n", system_name);
            } else {
                printf("ERROR: --system requires an argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--vicii-test") == 0) {
            vicii_test_mode = true;
            system_name = "C64";  // Force C64 system
            log_info("VIC-II test mode enabled\n");
        } else if (strcmp(argv[i], "--vicii-dump") == 0) {
            vicii_dump_mode = true;
            system_name = "C64";
            log_info("VIC-II dump mode enabled\n");
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            log_level = LogLevel::Debug;
            g_verbose = true;  // backward compat for code still checking this
        } else if (strcmp(argv[i], "--skip-memtest") == 0) {
            skip_memtest = true;
            log_info("KERNAL memory test skip enabled\n");
        } else if (strcmp(argv[i], "--sid-log") == 0) {
            if (i + 1 < argc) {
                sid_log_path = argv[++i];
                system_name = "C64";
                log_info("SID log capture mode → %s\n", sid_log_path);
            } else {
                printf("ERROR: --sid-log requires an output file path\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--seconds") == 0) {
            if (i + 1 < argc) {
                sid_log_seconds = atoi(argv[++i]);
                if (sid_log_seconds <= 0) sid_log_seconds = 60;
            } else {
                printf("ERROR: --seconds requires a value\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                return dump_registry(argv[++i]);
            } else {
                return dump_systems(); // default: list systems
            }
        } else if (strcmp(argv[i], "--manifest") == 0 || strcmp(argv[i], "-m") == 0) {
            if (i + 1 < argc) {
                return dump_manifest(argv[++i]);
            } else {
                printf("ERROR: --manifest requires a system name\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--pinout") == 0 || strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                const char* first_arg = argv[++i];
                const char* second_arg = nullptr;
                // Optional second arg (chip filter for system mode)
                if (i + 1 < argc && argv[i + 1][0] != '-')
                    second_arg = argv[++i];
                return dump_pinout(first_arg, second_arg);
            } else {
                printf("ERROR: --pinout requires a chip or system name\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            // Check if --verbose / -v appears anywhere in argv
            bool verbose_help = g_verbose;
            for (int j = 1; j < argc && !verbose_help; j++)
                verbose_help = (strcmp(argv[j], "--verbose") == 0 || strcmp(argv[j], "-v") == 0);

            printf("Usage: %s [options] [file]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  --system, -s <name>   Select system by short name (e.g., C64, CHIP8)\n");
            printf("  --list, -l [registry] List registry contents and exit (default: systems)\n");
            printf("                        Registries: systems, chips, devices, ports, formats\n");
            printf("  --manifest, -m <sys>  Dump system manifest and hardware details\n");
            printf("  --pinout, -p <name> [chip] Dump ASCII chip pinout (chip or system name)\n");
            printf("  --verbose, -v         Enable verbose startup messages\n");
            printf("  --help, -h            Show this help message (use -h -v for more)\n");
            if (verbose_help) {
                printf("\nSystem-specific options:\n");
                printf("  --vicii-test          Run VIC-II register test suite (headless)\n");
                printf("  --sid-log <file>      Capture SID register writes to binary log (headless)\n");
                printf("  --seconds <N>         Duration for --sid-log capture (default: 60)\n");
                printf("  --skip-memtest        Patch C64 KERNAL to skip RAMTAS memory test\n");
            }
            return 0;
        } else if (file_path == nullptr) {
            file_path = argv[i];
            log_info("File specified: %s\n", file_path);
        }
    }

    // Resolve archive paths (ZIP, 7z, etc.) to inner loadable files
    // so that "cermu -s c64 game.zip" finds and loads the D64/PRG inside.
    std::string resolved_file;
    if (file_path) {
        std::string ext = vfs_extension(file_path);
        if (vfs_is_archive_extension(ext.c_str())) {
            auto scan = scan_archive(file_path);
            if (!scan.loadable_files.empty()) {
                resolved_file = scan.loadable_files[0].full_path;
                log_info("Archive resolved to: %s\n", resolved_file.c_str());
            }
        }
        if (resolved_file.empty()) resolved_file = file_path;
    }
    const char* load_path = resolved_file.empty() ? nullptr : resolved_file.c_str();

    std::unique_ptr<System> system;

    // If system name specified, create it directly
    if (system_name != nullptr) {
        // Resolve prefix / alias to canonical short_name
        const SystemDescriptor* sd = prefix_match_system(system_name);
        if (sd) system_name = sd->short_name;

        log_info("Creating system: %s\n", system_name);
        system = SystemRegistry::instance().create_system(system_name);
        
        if (!system) {
            log_error("ERROR: System not found: %s\n", system_name);
            log_error("Available systems:\n");
            for (const auto& desc : SystemRegistry::instance().get_all_descriptors()) {
                log_error("  %s (%s)\n", desc.short_name, desc.name);
            }
            return 1;
        }
        
        log_info("Created system: %s (%s)\n", 
               system->get_descriptor().name,
               system->get_descriptor().short_name);
        
        // If a file was specified, auto-detect optimal configuration
        // (e.g. memory expansion) before initializing
        if (load_path) {
            system->apply_file_configuration(load_path);
        }

        // Initialize the system
        if (!system->initialize()) {
            log_error("ERROR: Failed to initialize %s system\n", 
                   system->get_descriptor().name);
            return 1;
        }

        // Attach default peripheral devices declared by the system
        system->attach_default_peripherals();

        // Load file if specified
        if (load_path) {
            if (!system->load_file(load_path)) {
                log_error("ERROR: Failed to load file: %s\n", load_path);
                system->shutdown();
                return 1;
            }
            log_info("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    // If file was specified (but no system), try to auto-detect system
    else if (load_path) {
        log_info("Detecting system for file: %s\n", load_path);
        system = SystemRegistry::instance().create_system_for_file(load_path);

        if (!system) {
            log_warn("WARNING: Could not detect system for file: %s\n", load_path);
            log_warn("No emulator supports this file format.\n");
            log_warn("System selection dialog will be shown...\n\n");
            // Keep load_path / resolved_file so the GUI can load it after user picks a system
        } else {
            log_info("Detected system: %s (%s)\n", 
                   system->get_descriptor().name,
                   system->get_descriptor().short_name);
            log_info("Description: %s\n", system->get_descriptor().description);

            // Some create_system_for_file() paths can return an instance that
            // has not yet built its board graph. Ensure initialization/load is
            // complete before handing the system to the GUI.
            if (system->get_boards().empty() || !system->is_system_ready()) {
                // Initialize the system
                if (!system->initialize()) {
                    log_error("ERROR: Failed to initialize %s system\n", 
                           system->get_descriptor().name);
                    return 1;
                }

                // Attach default peripheral devices declared by the system
                system->attach_default_peripherals();

                // Load the file
                if (!system->load_file(load_path)) {
                    log_error("ERROR: Failed to load file: %s\n", load_path);
                    system->shutdown();
                    return 1;
                }
            }
            log_info("Successfully loaded file into %s\n", system->get_descriptor().name);
        }
    }
    
    // =========================================================================
    // Apply --skip-memtest patch for C64 systems
    // TODO: This should eventually be part of a configurable patch registry
    // where users can enable/disable patches per system.  For now, only
    // applies when explicitly requested via CLI.  Some whitelisted software
    // (SID files) already applies this automatically via ensure_compatible_for_sid.
    // =========================================================================
    if (skip_memtest && system) {
        auto* c64_sys = dynamic_cast<C64System*>(system.get());
        if (c64_sys) {
            c64_sys->patch_skip_memtest();
        } else {
            printf("WARNING: --skip-memtest is only supported for C64 systems\n");
        }
    }

    // =========================================================================
    // SID LOG CAPTURE MODE — headless register write recording
    // =========================================================================
    if (sid_log_path && system) {
        C64System* c64 = dynamic_cast<C64System*>(system.get());
        if (!c64) { printf("ERROR: --sid-log requires C64\n"); return 1; }
        if (!file_path) { printf("ERROR: --sid-log requires a PRG/SID file\n"); system->shutdown(); return 1; }

        // Skip KERNAL memory test for faster boot
        c64->patch_skip_memtest();

        // Allocate headless framebuffer (required for run_frame)
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        // Set up write-capture callback
        sid_log::write_log_t log;
        log.chip_model = (c64->board_.sid.revision == SID_REVISION_8580_R5) ? 1 : 0;
        log.cpu_clock  = system->get_current_timing().cpu_frequency_hz;
        log.entries.reserve(256 * 1024);  // Pre-allocate ~1.5 MB

        c64->board_.sid.write_capture_fn  = sid_log::capture_callback;
        c64->board_.sid.write_capture_ctx = &log;

        uint32_t target_fps = system->get_target_fps();
        uint32_t total_frames = static_cast<uint32_t>(sid_log_seconds) * target_fps;

        log_info("SID-LOG: Capturing %d seconds (%u frames) → %s\n",
               sid_log_seconds, total_frames, sid_log_path);
        log_info("SID-LOG: Model %s, clock %u Hz\n",
               log.chip_model ? "8580" : "6581", log.cpu_clock);

        float drain[4096];
        for (uint32_t frame = 0; frame < total_frames; frame++) {
            system->run_frame();
            system->get_audio_samples(drain, 4096);

            // Progress every 10 seconds
            if (frame > 0 && frame % (target_fps * 10) == 0) {
                log_info("SID-LOG: %u/%u frames, %zu writes so far\n",
                       frame, total_frames, log.entries.size());
            }
        }

        // Detach callback
        c64->board_.sid.write_capture_fn  = nullptr;
        c64->board_.sid.write_capture_ctx = nullptr;

        log_info("SID-LOG: Capture complete — %zu register writes\n", log.entries.size());

        // Write to file
        if (sid_log::write_file(sid_log_path, log)) {
            log_info("SID-LOG: Saved to %s (%zu bytes)\n", sid_log_path,
                   sizeof(sid_log::header_t) + log.entries.size() * sizeof(sid_log::entry_t));
        } else {
            log_error("ERROR: Failed to write %s\n", sid_log_path);
            system->shutdown();
            return 1;
        }

        system->shutdown();
        return 0;
    }

    // =========================================================================
    // VIC-II DUMP MODE — normal boot + framebuffer pixel dump
    // =========================================================================
    if (vicii_dump_mode && system) {
        C64System* c64 = dynamic_cast<C64System*>(system.get());
        if (!c64) { log_error("ERROR: --vicii-dump requires C64\n"); return 1; }

        // Allocate headless framebuffer (no GUI)
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        printf("VICII-DUMP: Normal boot, %dx%d framebuffer\n", fb_width, fb_height);

        // Palette lookup
        const uint32_t* PAL = vicii_base_t::get_default_palette();
        [[maybe_unused]] auto color_name = [&](uint32_t rgba) -> const char* {
            for (int i = 0; i < 16; i++) {
                if (PAL[i] == rgba) {
                    static const char* n[16] = {"BLK","WHT","RED","CYN","PUR","GRN","BLU","YEL","ORN","BRN","LRD","DG1","DG2","LGN","LBL","LG3"};
                    return n[i];
                }
            }
            return "???";
        };
        auto fb_px = [&](int x, int y) -> uint32_t {
            if (x < 0 || y < 0 || x >= fb_width || y >= fb_height) return 0xDEADBEEF;
            return fb.get()[y * fb_width + x];
        };

        // Run frames and dump at key points
        float drain[4096];
        int frame_targets[] = { 5, 500 };
        int frame_count = 0;
        auto& vicii = c64->board_.vicii;
        uint8_t* ram_data = c64->board_.ram.data();

        for (int t = 0; t < 2; t++) {
            while (frame_count < frame_targets[t]) {
                system->run_frame();
                system->get_audio_samples(drain, 4096);
                frame_count++;
            }
            printf("\n=== Frame %d ===\n", frame_count);
            uint8_t d011 = vicii.regs_[0x11];
            uint8_t d016 = vicii.regs_[0x16];
            uint8_t d018 = vicii.regs_[0x18];
            uint8_t d020 = vicii.regs_[0x20];
            uint8_t d021 = vicii.regs_[0x21];
            uint8_t yscroll = d011 & 0x07;
            uint8_t xscroll = d016 & 0x07;
            uint16_t bank_base = vicii.memory.bank_base;
            uint16_t vm_base = vicii.memory.vm_base;
            uint16_t cb_base = vicii.memory.cb_base;
            bool bmm = (d011 & 0x20) != 0;
            printf("$D011=$%02X $D016=$%02X $D018=$%02X $D020=$%02X $D021=$%02X YSCROLL=%d BMM=%d\n",
                   d011, d016, d018, d020, d021, yscroll, bmm?1:0);
            printf("VIC bank=%d base=$%04X  VM=$%04X (abs=$%04X)  CB=$%04X (abs=$%04X)\n",
                   bank_base / 0x4000, bank_base, vm_base, bank_base | vm_base, cb_base, bank_base | cb_base);
            
            // ===== SPRITE STATE =====
            uint8_t d015 = vicii.regs_[0x15]; // enable
            uint8_t d010 = vicii.regs_[0x10]; // X bit 8
            uint8_t d017 = vicii.regs_[0x17]; // Y expand
            uint8_t d01b = vicii.regs_[0x1B]; // priority
            uint8_t d01c = vicii.regs_[0x1C]; // multicolor
            uint8_t d01d = vicii.regs_[0x1D]; // X expand
            printf("\nSPRITE STATE:\n");
            printf("$D015=$%02X(enable) $D01C=$%02X(mc) $D01D=$%02X(xexp) $D017=$%02X(yexp) $D01B=$%02X(pri) $D010=$%02X(x8)\n",
                   d015, d01c, d01d, d017, d01b, d010);
            
            for (int s = 0; s < 8; s++) {
                uint16_t sx = vicii.regs_[0x00 + s*2] | ((d010 & (1<<s)) ? 256 : 0);
                uint8_t sy = vicii.regs_[0x01 + s*2];
                uint8_t sc = vicii.regs_[0x27 + s]; // color
                bool en = (d015 & (1<<s)) != 0;
                bool xexp = (d01d & (1<<s)) != 0;
                bool yexp = (d017 & (1<<s)) != 0;
                
                // Read sprite pointer from the CORRECT screen area
                uint16_t sp_ptr_addr = (bank_base | vm_base) + 0x3F8 + s;
                uint8_t sp_ptr = ram_data[sp_ptr_addr];
                uint16_t sp_data_addr = bank_base + (uint16_t)sp_ptr * 64;
                
                printf("  Spr%d: %s X=%3d Y=%3d col=%d ptr=$%02X (data@$%04X) %s%s",
                       s, en ? "ON " : "off", sx, sy, sc, sp_ptr, sp_data_addr,
                       xexp ? "Xexp " : "", yexp ? "Yexp " : "");
                
                if (en) {
                    // Show first 3 bytes of sprite data (first row)
                    printf(" data[0..2]=%02X %02X %02X",
                           ram_data[sp_data_addr],
                           ram_data[sp_data_addr+1],
                           ram_data[sp_data_addr+2]);
                }
                printf("\n");
            }
            
            // ===== EMULATOR SPRITE INTERNAL STATE =====
            printf("\nSprite internal state (emulator):\n");
            for (int s = 0; s < 8; s++) {
                auto& spr = vicii.sprites.sprites[s];
                bool enabled = (vicii.regs_[vicii_regs::MXE] & (1 << s)) != 0;
                printf("  Spr%d: enabled=%d dma=%d display=%d dp=$%02X mc=%d shift=$%06X\n",
                       s, enabled, spr.dma_enabled, spr.display_state,
                       spr.data_pointer, spr.mc, spr.shift_reg);
            }
            
            // ===== FRAMEBUFFER SCAN =====
            // Scan entire framebuffer to find non-black rows
            const auto& dt = c64->get_hardware_traits().display;
            int fb_w = dt.visible_width;
            int fb_h = dt.visible_height;
            printf("\nFramebuffer size: %dx%d\n", fb_w, fb_h);
            printf("Scanning for non-black rows (showing first non-bg pixel per row):\n");
            int shown_rows = 0;
            for (int y = 0; y < fb_h && shown_rows < 80; y++) {
                // Count non-black pixels in this row
                int non_black = 0;
                int first_non_black_x = -1;
                uint32_t first_color = 0;
                for (int x = 0; x < fb_w; x++) {
                    uint32_t c = fb_px(x, y);
                    if (c != PAL[0]) { // not black
                        non_black++;
                        if (first_non_black_x < 0) {
                            first_non_black_x = x;
                            first_color = c;
                        }
                    }
                }
                if (non_black > 0) {
                    int ci = -1;
                    for (int i = 0; i < 16; i++) {
                        if (PAL[i] == first_color) { ci = i; break; }
                    }
                    printf("  Y=%3d: %4d non-bg pixels, first at X=%d (color=%d)\n", 
                           y, non_black, first_non_black_x, ci);
                    shown_rows++;
                }
            }
            
            // Show a few representative rows in detail
            int check_rows[] = { 42, 50, 58, 66, 74, 82, 90, 100, 120, 140 };
            for (int r = 0; r < 10; r++) {
                int y = check_rows[r];
                if (y >= fb_h) continue;
                printf("Row Y=%d (X=0..%d): ", y, fb_w < 160 ? fb_w-1 : 159);
                for (int x = 0; x < fb_w && x < 160; x++) {
                    uint32_t c = fb_px(x, y);
                    int ci = -1;
                    for (int i = 0; i < 16; i++) {
                        if (PAL[i] == c) { ci = i; break; }
                    }
                    if (x > 0 && x % 8 == 0) printf("|");
                    printf("%X", ci >= 0 ? ci : 0);
                }
                printf("\n");
            }
            
            // ECM mode check
            bool ecm = (d011 & 0x40) != 0;
            printf("\nECM=%d BMM=%d → mode: %s\n", ecm?1:0, bmm?1:0,
                   ecm && !bmm ? "Extended Color Mode" : 
                   !ecm && bmm ? "Bitmap Mode" : 
                   !ecm && !bmm ? "Standard Text Mode" : "Invalid");

            // Check CIA2 DD00 for bank config
            printf("\nCIA2 $DD00 port A value: $%02X\n", ram_data[0xDD00]);
            // Actually read from CIA2 register directly
            printf("CIA2 PRA register: $%02X\n", uint8_t(c64->board_.cia2.regs_[0]) & 0x03);
            
            // ===== BITMAP MODE DATA =====
            if (bmm) {
                printf("\nBITMAP MODE DATA (bank=$%04X):\n", bank_base);
                uint16_t bitmap_base = bank_base + (cb_base & 0x2000); // CB13 selects $0000 or $2000
                printf("Bitmap base: $%04X (CB13=%d)\n", bitmap_base, (cb_base & 0x2000) ? 1 : 0);
                printf("Bitmap[0..7] at $%04X: ", bitmap_base);
                for (int i = 0; i < 8; i++) printf("$%02X ", ram_data[bitmap_base + i]);
                printf("\n");
                // Show bitmap data for cell(5,0) = offset 5*8 = 40
                printf("Bitmap cell(5,0) at $%04X: ", bitmap_base + 40);
                for (int i = 0; i < 8; i++) printf("$%02X ", ram_data[bitmap_base + 40 + i]);
                printf("\n");
            }
        }
        
        // Save final framebuffer as PNG for visual inspection
        printf("\nSaving framebuffer to /tmp/vicii_dump_f200.png...\n");
        c64->set_framebuffer(fb.get(), fb_width, fb_height);
        // Use base class save_screenshot method
        c64->save_screenshot("/tmp/vicii_dump_f200.png");
        printf("Done. Check /tmp/vicii_dump_f200.png\n");
        
        // Save full 64K RAM dump for offline analysis of decompressed demos
        {
            FILE* f = fopen("/tmp/c64_memdump.bin", "wb");
            if (f) {
                fwrite(ram_data, 1, 65536, f);
                fclose(f);
                printf("Saved 64K RAM dump to /tmp/c64_memdump.bin\n");
            }
            // Also dump IRQ vector and key zero-page/hardware state
            uint16_t irq_lo = ram_data[0xFFFE] | (ram_data[0xFFFF] << 8);
            uint16_t nmi_lo = ram_data[0xFFFA] | (ram_data[0xFFFB] << 8);
            // Hardware IRQ vector (from KERNAL RAM copy at $0314/$0315)
            uint16_t hw_irq = ram_data[0x0314] | (ram_data[0x0315] << 8);
            printf("IRQ vector: $%04X, NMI vector: $%04X, HW IRQ ($0314): $%04X\n", 
                   irq_lo, nmi_lo, hw_irq);
            printf("CIA1 ICR: $%02X, VIC $D01A: $%02X\n",
                   uint8_t(c64->board_.cia1.regs_[0x0D]),
                   uint8_t(vicii.regs_[0x1A]));
        }
        
        system->shutdown();
        return 0;
    }

    // =========================================================================
    // VIC-II TEST MODE — headless test suite
    // =========================================================================
    if (vicii_test_mode && system) {
        C64System* c64 = dynamic_cast<C64System*>(system.get());
        if (!c64) {
            printf("ERROR: --vicii-test requires C64 system\n");
            return 1;
        }

        // 1. Patch KERNAL to skip memory test + redirect BASIC→test program
        vicii_test::patch_kernal_for_test(c64);

        // 2. Inject the 6510 test program into RAM
        vicii_test::inject_test_program(c64);

        // 3. Allocate headless framebuffer
        int fb_width, fb_height;
        system->get_display_dimensions(&fb_width, &fb_height);
        std::unique_ptr<uint32_t[]> fb(new uint32_t[fb_width * fb_height]());
        system->set_framebuffer(fb.get(), fb_width, fb_height);

        // 4. Initialize harness
        vicii_test::vicii_test_state_t test_state;
        vicii_test::harness_init(&test_state);

        printf("VICII-TEST: Running headless test suite...\n");
        printf("VICII-TEST: Display: %dx%d, FPS target: %u\n",
               fb_width, fb_height, system->get_target_fps());

        // 5. Run frames until tests complete or timeout
        float drain_buf[4096];
        while (vicii_test::harness_poll(&test_state, c64)) {
            system->run_frame();
            // Drain audio to prevent overflow
            system->get_audio_samples(drain_buf, 4096);
        }

        // One final poll to capture last result
        vicii_test::harness_poll(&test_state, c64);

        // 6. Read results buffer and print details
        vicii_test::harness_read_results(&test_state, c64);

        // 7. Print summary
        vicii_test::harness_summary(&test_state);

        // =====================================================================
        // Phase 2: Pixel verification tests (C++ driven)
        // =====================================================================
        printf("\nVICII-TEST: Starting Phase 2 — Pixel Verification...\n");
        auto pixel_results = vicii_test::run_pixel_verification_tests(
            c64, system.get(), fb.get(), fb_width, fb_height);

        int total_fail = test_state.total_fail + pixel_results.total_fail;
        int total_pass = test_state.total_pass + pixel_results.total_pass;
        printf("\n═══ COMBINED RESULTS ═══\n");
        printf("Phase 1 (register):  %d pass / %d fail\n",
               test_state.total_pass, test_state.total_fail);
        printf("Phase 2 (pixel):     %d pass / %d fail\n",
               pixel_results.total_pass, pixel_results.total_fail);
        printf("TOTAL:               %d pass / %d fail\n", total_pass, total_fail);

        // Cleanup
        system->shutdown();
        return (total_fail == 0 && test_state.all_done) ? 0 : 1;
    }

    // Create GUI (with or without a system)
    // If no system, nullptr will cause GUI to show system selection dialog
    // Pass resolved file path so the GUI can load it after system selection
    SessionGUI gui(std::move(system), load_path);
    
    // Initialize GUI — window title is managed dynamically by update_window_title()
    if (!gui.init("cermu", 1200, 800)) {
        log_error("ERROR: Failed to initialize GUI\n");
        return 1;
    }
    
    printf("Starting GUI main loop\n");
    
    // Run the GUI main loop
    gui.run();
    
    printf("Shutting down\n");
    
    // Cleanup
    gui.cleanup();
    
    return 0;
}