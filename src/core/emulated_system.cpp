#include "emulated_system.h"
#include "chip.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../gui/connector_icons.h"
#endif

// ============================================================================
// EmulatedSystem Base Class Implementation
// ============================================================================

EmulatedSystem::EmulatedSystem()
    : rgba_framebuffer_(nullptr)
    , rgba_width_(0)
    , rgba_height_(0)
    , total_cycles_(0)
    , speed_multiplier_(1.0f)
    , quit_requested_(false)
{
}

// Final implementations (identical for all systems)
const SystemConfiguration& EmulatedSystem::get_configuration() const {
    return config_;
}

const HardwareTraits& EmulatedSystem::get_hardware_traits() const {
    return hardware_traits_;
}

const SystemTiming& EmulatedSystem::get_current_timing() const {
    int idx = config_.region_option_index;
    if (idx >= 0 && idx < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[idx].timing;
    }
    return hardware_traits_.timing;
}

const DisplayTraits& EmulatedSystem::get_display_traits() const {
    return hardware_traits_.display;
}

const AudioTraits& EmulatedSystem::get_audio_traits() const {
    return hardware_traits_.audio;
}

uint64_t EmulatedSystem::get_total_cycles() const {
    return total_cycles_;
}

float EmulatedSystem::get_speed_multiplier() const {
    return speed_multiplier_;
}

// Default implementations (can be overridden)
void EmulatedSystem::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

bool EmulatedSystem::initialize() {
    reset();
    return true;
}

void EmulatedSystem::shutdown() {
    // Default: nothing to clean up
}

void EmulatedSystem::handle_controller_event(int controller, int button, bool pressed) {
    // Default: no controller support
    (void)controller;
    (void)button;
    (void)pressed;
}

void EmulatedSystem::render_debug_windows(void* gui_state) {
    // Render detached combined windows for chips that were "detached" from
    // the Hardware menu preview.  Each detached window shows layout + debug
    // + settings content in a standalone ImGui window.
    (void)gui_state;
#ifdef IMGUI_VERSION
    for (auto& sc : registered_chips_) {
        if (!sc.show_detached || !sc.chip) continue;
        bool show = true;
        ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(sc.display_name, &show)) {
            if (sc.chip->has_debug_content()) {
                sc.chip->render_debug_content();
            } else if (sc.chip->has_layout_content()) {
                sc.chip->render_layout_content();
            }
            if (sc.chip->has_settings_content()) {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Settings")) {
                    sc.chip->render_settings_content();
                }
            }
        }
        ImGui::End();
        sc.show_detached = show;
    }
#endif
}

void EmulatedSystem::register_chip(std::unique_ptr<ChipBase> chip,
                                   const char* display_name, const char* short_name,
                                   const char* category, uint16_t base_address) {
    SystemChip sc;
    sc.chip = std::move(chip);
    sc.display_name = display_name;
    sc.short_name = short_name;
    sc.category = category;
    sc.base_address = base_address;
    sc.show_detached = 0;
    registered_chips_.push_back(std::move(sc));
}

void EmulatedSystem::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    // Default: fall back to the simple handle_keyboard_event (ignoring extra info)
    (void)scancode;
    (void)mod;
    (void)repeat;
    if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

void EmulatedSystem::handle_text_input(const char* text) {
    // Default: no text input handling (systems using KeyboardMapper override this)
    (void)text;
}

void EmulatedSystem::release_all_keys() {
    // Default: nothing to release
}

SystemConfiguration EmulatedSystem::detect_optimal_configuration(
    const char* /*filepath*/, const uint8_t* /*data*/, size_t /*size*/) {
    // Default: walk hardware traits and select the default option for each axis
    SystemConfiguration config;

    // Memory: find the default option
    for (size_t i = 0; i < hardware_traits_.memory_options.size(); i++) {
        if (hardware_traits_.memory_options[i].is_default) {
            config.memory_option_index = static_cast<int>(i);
            break;
        }
    }

    // Region: find the default option
    for (size_t i = 0; i < hardware_traits_.region_options.size(); i++) {
        if (hardware_traits_.region_options[i].is_default) {
            config.region_option_index = static_cast<int>(i);
            break;
        }
    }

    // Peripherals: apply defaults
    for (const auto& p : hardware_traits_.peripheral_options) {
        config.enabled_peripherals[p.id] = p.enabled_by_default;
    }

    return config;
}

void EmulatedSystem::apply_file_configuration(const char* filepath) {
    if (!filepath) return;

    FILE* f = fopen(filepath, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t read_size = fsize < 65536 ? (size_t)fsize : 65536;
    std::vector<uint8_t> buf(read_size);
    fread(buf.data(), 1, read_size, f);
    fclose(f);

    SystemConfiguration detected =
        detect_optimal_configuration(filepath, buf.data(), (size_t)fsize);

    // Merge: never downgrade memory, keep detected region
    SystemConfiguration merged = config_;
    if (detected.memory_option_index > merged.memory_option_index) {
        merged.memory_option_index = detected.memory_option_index;
    }
    if (detected.region_option_index >= 0) {
        merged.region_option_index = detected.region_option_index;
    }
    // Propagate file-detected custom settings (e.g. SID revision from header)
    for (const auto& [key, value] : detected.custom_settings) {
        merged.custom_settings[key] = value;
    }

    set_configuration(merged);
    apply_configuration();
}

uint32_t EmulatedSystem::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    return 0; // No audio by default
}

void EmulatedSystem::set_audio_sample_rate(int /*sample_rate_hz*/) {
    // No-op by default; systems with audio override this.
}

// ============================================================================
// CONNECTOR PORT & PERIPHERAL DEVICE MANAGEMENT (generic)
// ============================================================================

int EmulatedSystem::add_connector_port(const ConnectorDefinition& def, int port_number) {
    int index = static_cast<int>(connector_ports_.size());
    connector_ports_.push_back(std::make_unique<ConnectorPort>(def, port_number));
    return index;
}

void EmulatedSystem::tick_peripherals() {
    for (auto& device : owned_devices_) {
        device->tick();
    }
}

bool EmulatedSystem::attach_device_to_port(int port_index, const char* device_id) {
    if (port_index < 0 || port_index >= static_cast<int>(connector_ports_.size())) {
        printf("System: Invalid port index %d\n", port_index);
        return false;
    }

    auto& port = connector_ports_[port_index];

    // Create device from registry
    auto device = DeviceRegistry::instance().create_device(device_id);
    if (!device) {
        printf("System: Unknown device '%s'\n", device_id);
        return false;
    }

    // For point-to-point ports: detach existing device first
    if (!port->is_bus()) {
        detach_device_from_port(port_index);
    }

    // Attach and take ownership
    auto* raw_ptr = device.get();
    if (!port->attach_device(raw_ptr)) {
        return false;
    }

    raw_ptr->reset();
    owned_devices_.push_back(std::move(device));
    printf("System: Attached '%s' to %s\n", raw_ptr->get_name(), port->get_name());
    on_port_device_changed(port_index);

    // Auto-bind host input to newly attached device (gamepad if available, etc.)
    auto_bind_host_inputs();
    return true;
}

void EmulatedSystem::detach_device_from_port(int port_index) {
    if (port_index < 0 || port_index >= static_cast<int>(connector_ports_.size())) return;

    auto& port = connector_ports_[port_index];
    auto devices_copy = port->get_attached_devices();  // Copy — detach modifies the vector
    if (devices_copy.empty()) return;

    for (auto* device : devices_copy) {
        printf("System: Detached '%s' from %s\n", device->get_name(), port->get_name());
    }
    port->detach_device(nullptr);  // Detach all

    // Remove all detached devices from owned_devices_
    for (auto* device : devices_copy) {
        owned_devices_.erase(
            std::remove_if(owned_devices_.begin(), owned_devices_.end(),
                            [device](const std::unique_ptr<PeripheralDevice>& p) {
                                return p.get() == device;
                            }),
            owned_devices_.end()
        );
    }
    on_port_device_changed(port_index);
}

void EmulatedSystem::detach_device_from_port(int port_index, PeripheralDevice* device) {
    if (port_index < 0 || port_index >= static_cast<int>(connector_ports_.size())) return;
    if (!device) return;

    auto& port = connector_ports_[port_index];
    printf("System: Detached '%s' from %s\n", device->get_name(), port->get_name());
    port->detach_device(device);

    // Remove from owned_devices_
    owned_devices_.erase(
        std::remove_if(owned_devices_.begin(), owned_devices_.end(),
                        [device](const std::unique_ptr<PeripheralDevice>& p) {
                            return p.get() == device;
                        }),
        owned_devices_.end()
    );
    on_port_device_changed(port_index);
}

bool EmulatedSystem::process_sdl_event_for_devices(const SDL_Event& event) {
    bool consumed = false;
    for (auto& device : owned_devices_) {
        if (device->accepts_host_input()) {
            if (device->process_sdl_event(event)) {
                consumed = true;
            }
        }
    }
    return consumed;
}

void EmulatedSystem::render_peripheral_connector_ui() {
#ifdef IMGUI_VERSION
    if (connector_ports_.empty()) return;

    auto& registry = DeviceRegistry::instance();

    // Check if there's anything worth showing (external ports with devices,
    // or internal ports with attached devices that have UI)
    bool any_visible = false;
    for (auto& port : connector_ports_) {
        const auto& def = port->get_definition();
        if (def.is_internal) {
            if (port->get_attached_device()) any_visible = true;
        } else {
            if (!registry.get_compatible_devices(port->get_type()).empty())
                any_visible = true;
        }
    }
    if (!any_visible) return;

    ImGui::Separator();
    ImGui::Text("Peripheral Connectors");
    ImGui::Spacing();

    for (int i = 0; i < static_cast<int>(connector_ports_.size()); i++) {
        auto& port = connector_ports_[i];
        const auto& def = port->get_definition();
        auto* attached = port->get_attached_device();

        ImGui::PushID(i);

        if (def.is_internal) {
            // Internal connector: show as fixed label, no attach/detach combo
            if (attached) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                                   "%s: %s", def.name, attached->get_name());
                ImGui::Indent();
                attached->render_device_ui();
                ImGui::Unindent();
            }
        } else if (def.is_bus) {
            // =================================================================
            // BUS PORT — Multiple devices can be attached simultaneously
            // =================================================================
            auto compatible = registry.get_compatible_devices(port->get_type());
            if (compatible.empty()) {
                ImGui::PopID();
                continue;
            }

            const auto& devices = port->get_attached_devices();
            int dev_count = static_cast<int>(devices.size());

            // Header: "IEC Serial Bus (2 devices)"
            bool open = ImGui::TreeNodeEx(def.name, ImGuiTreeNodeFlags_DefaultOpen,
                                          "%s (%d device%s)", def.name,
                                          dev_count, dev_count == 1 ? "" : "s");
            if (open) {
                // List each attached device with a [x] remove button
                for (int d = 0; d < dev_count; d++) {
                    auto* dev = devices[d];
                    ImGui::PushID(d);

                    // Remove button
                    if (ImGui::SmallButton("x")) {
                        detach_device_from_port(i, dev);
                        ImGui::PopID();
                        break;  // Vector invalidated — exit loop, will redraw next frame
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", dev->get_name());

                    // Device-specific UI
                    ImGui::Indent();
                    dev->render_device_ui();
                    if (dev->accepts_host_input()) {
                        render_host_input_binding_ui(dev);
                    }
                    ImGui::Unindent();

                    ImGui::PopID();
                }

                // "Add device" combo
                if (ImGui::BeginCombo("Add device...", nullptr, ImGuiComboFlags_NoPreview)) {
                    for (const auto* desc : compatible) {
                        if (ImGui::Selectable(desc->name)) {
                            attach_device_to_port(i, desc->id);
                        }
                        if (ImGui::IsItemHovered() && desc->description) {
                            ImGui::SetTooltip("%s", desc->description);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TreePop();
            }
        } else {
            // =================================================================
            // POINT-TO-POINT PORT — Single device, combo selector
            // =================================================================
            auto compatible = registry.get_compatible_devices(port->get_type());
            if (compatible.empty()) {
                ImGui::PopID();
                continue;
            }

            const char* current_name = attached ? attached->get_name() : "<none>";

            if (ImGui::BeginCombo(def.name, current_name)) {
                // "<none>" option — detach
                if (ImGui::Selectable("<none>", attached == nullptr)) {
                    detach_device_from_port(i);
                    attached = nullptr;  // Device destroyed — clear dangling pointer
                }

                for (const auto* desc : compatible) {
                    bool is_selected = (attached && strcmp(attached->get_id(), desc->id) == 0);
                    if (ImGui::Selectable(desc->name, is_selected)) {
                        if (!is_selected) {
                            attach_device_to_port(i, desc->id);
                            attached = port->get_attached_device();  // Refresh pointer
                        }
                    }
                    if (ImGui::IsItemHovered() && desc->description) {
                        ImGui::SetTooltip("%s", desc->description);
                    }
                }
                ImGui::EndCombo();
            }

            // Show device-specific UI if attached
            if (attached) {
                ImGui::Indent();
                attached->render_device_ui();

                // Host input binding selector for input-accepting devices
                if (attached->accepts_host_input()) {
                    render_host_input_binding_ui(attached);
                }

                ImGui::Unindent();
            }
        }

        ImGui::PopID();
    }
#endif
}

void EmulatedSystem::render_host_input_binding_ui(PeripheralDevice* device) {
#ifdef IMGUI_VERSION
    if (!device || !device->accepts_host_input()) return;

    const auto& binding = device->get_host_input_binding();
    int type_count = device->get_supported_input_type_count();
    if (type_count <= 0) return;

    // Build combo label from current binding
    ImGui::PushID("host_input");

    if (ImGui::BeginCombo("Input Source", binding.label.c_str())) {
        // "None" option
        bool is_none = (binding.type == HostInputType::NONE);
        if (ImGui::Selectable("None", is_none)) {
            HostInputBinding none;
            none.type = HostInputType::NONE;
            none.label = "None";
            device->set_host_input_binding(none);
        }

        for (int t = 0; t < type_count; t++) {
            HostInputType supported = device->get_supported_input_type(t);
            if (supported == HostInputType::NONE) continue;

            if (supported == HostInputType::SDL_GAMEPAD) {
                // List available SDL game controllers
                int num_joysticks = SDL_NumJoysticks();
                for (int j = 0; j < num_joysticks; j++) {
                    if (SDL_IsGameController(j)) {
                        const char* name = SDL_GameControllerNameForIndex(j);
                        if (!name) name = "Unknown Controller";

                        char label[128];
                        snprintf(label, sizeof(label), "Gamepad #%d: %s", j, name);

                        SDL_JoystickID jid = -1;
                        SDL_GameController* gc = SDL_GameControllerOpen(j);
                        if (gc) {
                            SDL_Joystick* js = SDL_GameControllerGetJoystick(gc);
                            if (js) jid = SDL_JoystickInstanceID(js);
                        }

                        bool is_sel = (binding.type == HostInputType::SDL_GAMEPAD &&
                                       binding.gamepad_instance_id == jid);
                        if (ImGui::Selectable(label, is_sel)) {
                            HostInputBinding b;
                            b.type = HostInputType::SDL_GAMEPAD;
                            b.gamepad_instance_id = jid;
                            b.label = label;
                            device->set_host_input_binding(b);
                        }
                    }
                }
            } else {
                // Keyboard, Host Mouse — single selectable entry
                const char* name = host_input_type_name(supported);
                bool is_sel = (binding.type == supported);
                if (ImGui::Selectable(name, is_sel)) {
                    HostInputBinding b;
                    b.type = supported;
                    b.label = name;
                    device->set_host_input_binding(b);
                }
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PopID();
#else
    (void)device;
#endif
}

// ============================================================================
// CONNECTOR MENU BAR ICONS — Right-aligned icon buttons with popup menus
// ============================================================================

float EmulatedSystem::render_connector_menu_bar_icons() {
#ifdef IMGUI_VERSION
    if (connector_ports_.empty()) return 0.0f;

    auto& registry = DeviceRegistry::instance();

    // --- 1. Collect visible (external) ports --------------------------------
    struct VisiblePort {
        int            index;
        ConnectorPort* port;
    };
    std::vector<VisiblePort> visible;
    for (int i = 0; i < static_cast<int>(connector_ports_.size()); i++) {
        const auto& def = connector_ports_[i]->get_definition();
        if (def.is_internal) continue;  // Skip keyboard etc.
        visible.push_back({ i, connector_ports_[i].get() });
    }
    if (visible.empty()) return 0.0f;

    // --- 2. Calculate total width -------------------------------------------
    const float icon_sz  = static_cast<float>(ConnectorIcons::ICON_SIZE);
    const float btn_pad  = 4.0f;   // padding inside ImageButton
    const float spacing  = 2.0f;   // gap between buttons
    const float btn_w    = icon_sz + btn_pad * 2.0f;
    const float total_w  = visible.size() * btn_w +
                           (visible.size() - 1) * spacing + 8.0f;

    // --- 3. Right-align: position cursor so icons hug the right edge -------
    float window_w = ImGui::GetWindowWidth();
    float start_x  = window_w - total_w;
    ImGui::SetCursorPosX(start_x);

    // --- 4. Render buttons --------------------------------------------------
    for (size_t vi = 0; vi < visible.size(); vi++) {
        auto& vp = visible[vi];
        auto* port = vp.port;
        const auto& def  = port->get_definition();
        GLuint tex = ConnectorIcons::get_icon(def.type);
        if (!tex) tex = ConnectorIcons::get_icon(ConnectorType::CUSTOM);
        if (!tex) continue;

        ImGui::PushID(vp.index);

        // Tint: full brightness if device attached, dim gray if empty
        bool has_device = (port->get_device_count() > 0);

        // Check if any attached device is actively transferring data
        bool any_activity = false;
        if (has_device) {
            for (auto* dev : port->get_attached_devices()) {
                if (dev->has_activity()) { any_activity = true; break; }
            }
        }

        ImVec4 tint;
        if (any_activity) {
            // Blink: pulse between bright green and dim green
            float t = static_cast<float>(ImGui::GetTime());
            float pulse = 0.55f + 0.45f * sinf(t * 8.0f);  // ~1.3 Hz blink
            tint = ImVec4(pulse * 0.3f, pulse, pulse * 0.3f, 1.0f);
        } else if (has_device) {
            tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            tint = ImVec4(0.5f, 0.5f, 0.5f, 0.7f);
        }

        // Use plain Image + hover detection so it blends with menu bar
        ImGui::SameLine(0, spacing);
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        // Draw the icon image (ImageWithBg supports tint)
        ImGui::ImageWithBg(ImTextureRef((ImTextureID)(intptr_t)tex),
                     ImVec2(icon_sz, icon_sz), ImVec2(0, 0), ImVec2(1, 1),
                     ImVec4(0, 0, 0, 0), tint);

        // Hover highlight
        if (ImGui::IsItemHovered()) {
            ImGui::GetWindowDrawList()->AddRect(
                ImVec2(cursor.x - 1, cursor.y - 1),
                ImVec2(cursor.x + icon_sz + 1, cursor.y + icon_sz + 1),
                IM_COL32(255, 255, 255, 120), 2.0f);
        }

        // Tooltip showing port name and attached device(s)
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", def.name);
            if (def.is_bus) {
                int n = port->get_device_count();
                if (n == 0) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty)");
                } else {
                    for (auto* d : port->get_attached_devices()) {
                        ImGui::BulletText("%s", d->get_name());
                    }
                }
            } else {
                auto* dev = port->get_attached_device();
                if (dev)
                    ImGui::Text("  %s", dev->get_name());
                else
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty)");
            }
            ImGui::EndTooltip();
        }

        // Click opens popup
        char popup_id[64];
        snprintf(popup_id, sizeof(popup_id), "##ConnPopup_%d", vp.index);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            ImGui::OpenPopup(popup_id);
        }

        // --- 4. Popup menu --------------------------------------------------
        if (ImGui::BeginPopup(popup_id)) {
            // Header
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.6f, 1.0f), "%s", def.name);
            ImGui::Separator();

            auto compatible = registry.get_compatible_devices(port->get_type());

            if (def.is_bus) {
                // ============ BUS PORT (IEC) ============
                const auto& devices = port->get_attached_devices();
                if (devices.empty()) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                       "No devices attached");
                } else {
                    for (int d = 0; d < static_cast<int>(devices.size()); d++) {
                        auto* dev = devices[d];
                        ImGui::PushID(d);

                        // Device name as a collapsible header
                        bool dev_open = ImGui::TreeNodeEx(
                            dev->get_name(), ImGuiTreeNodeFlags_DefaultOpen);

                        if (dev_open) {
                            // Detach button
                            if (ImGui::MenuItem("Detach")) {
                                detach_device_from_port(vp.index, dev);
                                ImGui::TreePop();
                                ImGui::PopID();
                                ImGui::EndPopup();
                                ImGui::PopID();
                                return total_w;  // vector invalidated
                            }

                            // Device-specific UI
                            dev->render_device_ui();

                            // Host input binding
                            if (dev->accepts_host_input()) {
                                render_host_input_binding_ui(dev);
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::Separator();

                // "Add device" submenu
                if (!compatible.empty() && ImGui::BeginMenu("Add Device...")) {
                    for (const auto* desc : compatible) {
                        if (ImGui::MenuItem(desc->name)) {
                            attach_device_to_port(vp.index, desc->id);
                        }
                        if (ImGui::IsItemHovered() && desc->description) {
                            ImGui::SetTooltip("%s", desc->description);
                        }
                    }
                    ImGui::EndMenu();
                }
            } else {
                // ============ POINT-TO-POINT PORT ============
                auto* attached = port->get_attached_device();

                if (attached) {
                    ImGui::Text("Current: %s", attached->get_name());
                    ImGui::Separator();

                    // Detach
                    if (ImGui::MenuItem("Detach")) {
                        detach_device_from_port(vp.index);
                        attached = nullptr;
                    }

                    // Device UI (inline in popup)
                    if (attached) {
                        ImGui::Separator();
                        attached->render_device_ui();

                        if (attached->accepts_host_input()) {
                            ImGui::Separator();
                            render_host_input_binding_ui(attached);
                        }
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                       "No device attached");
                }

                // Switch device submenu
                if (!compatible.empty()) {
                    ImGui::Separator();
                    if (ImGui::BeginMenu("Attach Device...")) {
                        for (const auto* desc : compatible) {
                            bool is_current = (attached &&
                                               strcmp(attached->get_id(), desc->id) == 0);
                            if (ImGui::MenuItem(desc->name, nullptr, is_current)) {
                                if (!is_current) {
                                    attach_device_to_port(vp.index, desc->id);
                                    // Old 'attached' pointer is now dangling (device destroyed).
                                    // Bail out immediately to avoid use-after-free.
                                    ImGui::EndMenu();
                                    ImGui::EndPopup();
                                    ImGui::PopID();
                                    return total_w;
                                }
                            }
                            if (ImGui::IsItemHovered() && desc->description) {
                                ImGui::SetTooltip("%s", desc->description);
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    return total_w;
#else
    return 0.0f;
#endif
}

// ============================================================================
// AUTO-BIND HOST INPUTS — assign available controllers to peripherals
// ============================================================================

void EmulatedSystem::auto_bind_host_inputs() {
    // Collect owned devices that accept host input
    std::vector<PeripheralDevice*> gamepad_devices;   // devices that support SDL_GAMEPAD
    std::vector<PeripheralDevice*> keyboard_devices;  // devices that support KEYBOARD (but not gamepad-assigned)
    std::vector<PeripheralDevice*> mouse_devices;     // devices that support HOST_MOUSE

    for (auto& dev : owned_devices_) {
        if (!dev || !dev->accepts_host_input()) continue;

        bool supports_gamepad  = false;
        bool supports_keyboard = false;
        bool supports_mouse    = false;

        int type_count = dev->get_supported_input_type_count();
        for (int t = 0; t < type_count; t++) {
            HostInputType ht = dev->get_supported_input_type(t);
            if (ht == HostInputType::SDL_GAMEPAD) supports_gamepad = true;
            if (ht == HostInputType::KEYBOARD)    supports_keyboard = true;
            if (ht == HostInputType::HOST_MOUSE)  supports_mouse = true;
        }

        if (supports_gamepad)       gamepad_devices.push_back(dev.get());
        else if (supports_keyboard) keyboard_devices.push_back(dev.get());

        if (supports_mouse)         mouse_devices.push_back(dev.get());
    }

    // --- Enumerate available SDL gamepads ---
    struct GamepadInfo {
        int             device_index;   // SDL device index
        SDL_JoystickID  instance_id;    // SDL joystick instance ID
        const char*     name;
    };
    std::vector<GamepadInfo> gamepads;

    int num_joysticks = SDL_NumJoysticks();
    for (int j = 0; j < num_joysticks; j++) {
        if (!SDL_IsGameController(j)) continue;
        SDL_GameController* gc = SDL_GameControllerOpen(j);
        if (!gc) continue;
        SDL_Joystick* js = SDL_GameControllerGetJoystick(gc);
        if (!js) continue;
        SDL_JoystickID jid = SDL_JoystickInstanceID(js);
        const char* name = SDL_GameControllerNameForIndex(j);
        if (!name) name = "Game Controller";
        gamepads.push_back({ j, jid, name });
    }

    // --- Assign gamepads to gamepad-compatible devices (round-robin) ---
    int gp_idx = 0;
    for (auto* dev : gamepad_devices) {
        if (gp_idx < static_cast<int>(gamepads.size())) {
            // Assign a specific gamepad
            auto& gp = gamepads[gp_idx];
            HostInputBinding b;
            b.type = HostInputType::SDL_GAMEPAD;
            b.gamepad_instance_id = gp.instance_id;
            char label[128];
            snprintf(label, sizeof(label), "Gamepad #%d: %s", gp.device_index, gp.name);
            b.label = label;
            dev->set_host_input_binding(b);
            printf("Auto-bind: %s -> %s\n", dev->get_name(), label);
            gp_idx++;
        } else {
            // No more gamepads available; fall back to keyboard if supported
            bool supports_keyboard = false;
            int type_count = dev->get_supported_input_type_count();
            for (int t = 0; t < type_count; t++) {
                if (dev->get_supported_input_type(t) == HostInputType::KEYBOARD) {
                    supports_keyboard = true;
                    break;
                }
            }
            if (supports_keyboard) {
                HostInputBinding b;
                b.type = HostInputType::KEYBOARD;
                b.label = "Keyboard";
                dev->set_host_input_binding(b);
                printf("Auto-bind: %s -> Keyboard (no gamepad available)\n", dev->get_name());
            }
        }
    }

    // --- Keyboard-only devices (no gamepad support, e.g. paddles on KEYBOARD) ---
    for (auto* dev : keyboard_devices) {
        HostInputBinding b;
        b.type = HostInputType::KEYBOARD;
        b.label = "Keyboard";
        dev->set_host_input_binding(b);
        printf("Auto-bind: %s -> Keyboard\n", dev->get_name());
    }

    // --- Mouse devices ---
    for (auto* dev : mouse_devices) {
        // Only bind mouse if the device isn't already bound to a gamepad
        const auto& current = dev->get_host_input_binding();
        if (current.type == HostInputType::SDL_GAMEPAD) continue;

        HostInputBinding b;
        b.type = HostInputType::HOST_MOUSE;
        b.label = "Host Mouse";
        dev->set_host_input_binding(b);
        printf("Auto-bind: %s -> Host Mouse\n", dev->get_name());
    }

    if (gamepads.empty() && gamepad_devices.empty() && mouse_devices.empty()
        && keyboard_devices.empty()) {
        printf("Auto-bind: no input-accepting devices on this system\n");
    }
}