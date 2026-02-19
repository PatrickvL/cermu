#include "emulated_system.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

#ifdef IMGUI_VERSION
#include <imgui.h>
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
    // Default: no debug windows
    (void)gui_state;
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

    // Detach any existing device first
    detach_device_from_port(port_index);

    // Attach and take ownership
    auto* raw_ptr = device.get();
    if (!port->attach_device(raw_ptr)) {
        return false;
    }

    raw_ptr->reset();
    owned_devices_.push_back(std::move(device));
    printf("System: Attached '%s' to %s\n", raw_ptr->get_name(), port->get_name());
    return true;
}

void EmulatedSystem::detach_device_from_port(int port_index) {
    if (port_index < 0 || port_index >= static_cast<int>(connector_ports_.size())) return;

    auto& port = connector_ports_[port_index];
    auto* attached = port->get_attached_device();
    if (!attached) return;

    printf("System: Detached '%s' from %s\n", attached->get_name(), port->get_name());
    port->detach_device();

    // Remove from owned_devices_ list
    owned_devices_.erase(
        std::remove_if(owned_devices_.begin(), owned_devices_.end(),
                        [attached](const std::unique_ptr<PeripheralDevice>& p) {
                            return p.get() == attached;
                        }),
        owned_devices_.end()
    );
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
        } else {
            // External connector: show attach/detach combo
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