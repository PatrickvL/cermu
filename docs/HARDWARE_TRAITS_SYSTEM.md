# Hardware Traits and Configuration System

## Overview

The emulator now supports a comprehensive hardware traits system that allows each emulated system to describe its fixed hardware characteristics and provide user-configurable options.

## Architecture

### Three-Level System

1. **Hardware Traits** (Fixed) - Immutable characteristics of the hardware
2. **Configuration Options** (Choices) - Available configuration choices for the system
3. **System Configuration** (Selected) - User's current configuration selection

## Hardware Traits

### Display Traits

Describes the video output characteristics:

```cpp
struct DisplayTraits {
    int native_width;           // Native display width (e.g., 403 for C64)
    int native_height;          // Native display height (e.g., 284 for C64)
    int visible_width;          // Visible area (e.g., 320 for C64)
    int visible_height;         // Visible area (e.g., 200 for C64)
    FramebufferFormat format;   // Color format (RGBA8888, palette-indexed, etc.)
    int palette_size;           // Number of palette colors (0 for RGB)
    float pixel_aspect_ratio;   // Pixel aspect ratio (1.0 = square)
    bool has_overscan;          // Whether system has border area
};
```

**Example - Commodore 64:**
```cpp
traits.display.native_width = 403;          // Full VIC-II output
traits.display.native_height = 284;
traits.display.visible_width = 320;         // Active area
traits.display.visible_height = 200;
traits.display.format = FramebufferFormat::PALETTE_INDEXED_4;
traits.display.palette_size = 16;
traits.display.pixel_aspect_ratio = 0.936f; // PAL pixels
traits.display.has_overscan = true;
```

**Example - CHIP-8:**
```cpp
traits.display.native_width = 64;
traits.display.native_height = 32;
traits.display.visible_width = 64;
traits.display.visible_height = 32;
traits.display.format = FramebufferFormat::RGBA8888;
traits.display.palette_size = 2;            // Monochrome
traits.display.pixel_aspect_ratio = 1.0f;
traits.display.has_overscan = false;
```

### Audio Traits

Describes the audio output characteristics:

```cpp
struct AudioTraits {
    AudioFormat format;         // MONO_16BIT, STEREO_16BIT, etc.
    int sample_rate_hz;         // Audio sample rate
    int channels;               // 1 = mono, 2 = stereo
    const char* chip_name;      // Name of audio chip
};
```

**Example - Commodore 64:**
```cpp
traits.audio.format = AudioFormat::MONO_16BIT;
traits.audio.sample_rate_hz = 44100;
traits.audio.channels = 1;
traits.audio.chip_name = "MOS 6581 SID";
```

**Example - NES:**
```cpp
traits.audio.format = AudioFormat::STEREO_16BIT;
traits.audio.sample_rate_hz = 48000;
traits.audio.channels = 2;  // APU can be mixed to stereo
traits.audio.chip_name = "RP2A03 APU";
```

### System Timing

Describes clock frequencies and timing:

```cpp
struct SystemTiming {
    uint32_t cpu_frequency_hz;      // CPU clock (e.g., 985248 for C64 PAL)
    uint32_t video_frequency_hz;    // Video chip clock
    uint32_t audio_sample_rate_hz;  // Audio sampling rate
    uint32_t target_fps;            // Target frames per second
    uint32_t cycles_per_frame;      // CPU cycles per frame
    VideoStandard region;             // PAL, NTSC, etc.
};
```

**Example - C64 PAL:**
```cpp
timing.cpu_frequency_hz = 985248;       // ~1 MHz
timing.video_frequency_hz = 985248;     // VIC-II runs at CPU speed
timing.audio_sample_rate_hz = 44100;
timing.target_fps = 50;
timing.cycles_per_frame = 19705;        // 985248 / 50
timing.standard = VideoStandard::PAL;
```

**Example - C64 NTSC:**
```cpp
timing.cpu_frequency_hz = 1022730;      // ~1.02 MHz
timing.video_frequency_hz = 1022730;
timing.audio_sample_rate_hz = 44100;
timing.target_fps = 60;
timing.cycles_per_frame = 17045;        // 1022730 / 60
timing.standard = VideoStandard::NTSC;
```

## Configuration Options

### Memory Options

Allows users to select different memory configurations:

```cpp
struct MemoryOption {
    const char* name;           // Display name
    uint32_t ram_size;          // RAM size in bytes
    uint32_t rom_size;          // ROM size in bytes
    bool is_default;            // Whether this is default
};
```

**Example - C64:**
```cpp
// Standard C64
traits.memory_options.push_back({
    "64KB Standard",
    65536,      // 64KB RAM
    24576,      // BASIC + KERNAL + CHARGEN
    true        // default
});

// C64 with RAM Expansion Unit
traits.memory_options.push_back({
    "64KB + REU 512KB",
    65536 + 524288,  // 64KB + 512KB expansion
    24576,
    false
});
```

**Example - VIC-20:**
```cpp
// Unexpanded VIC-20
traits.memory_options.push_back({
    "5KB Unexpanded",
    5120,       // 5KB RAM
    16384,      // ROMs
    true
});

// 32KB Expanded
traits.memory_options.push_back({
    "32KB Expanded",
    32768,
    16384,
    false
});
```

### Region Options

Allows users to select PAL/NTSC and associated timing:

```cpp
struct VideoStandardConfig {
    const char* name;           // Display name
    VideoStandard region;         // Region enum
    SystemTiming timing;        // Timing for this region
    bool is_default;            // Whether this is default
};
```

**Example - C64:**
```cpp
// PAL (European)
traits.video_standard_configs.push_back({
    "PAL (European)",
    VideoStandard::PAL,
    pal_timing,     // See timing examples above
    true            // default
});

// NTSC (American)
traits.video_standard_configs.push_back({
    "NTSC (American)",
    VideoStandard::NTSC,
    ntsc_timing,
    false
});
```

### Peripheral Options

Allows users to enable/disable peripherals and extensions:

```cpp
struct PeripheralOption {
    const char* id;             // Unique identifier
    const char* name;           // Display name
    const char* description;    // Tooltip/description
    bool enabled_by_default;    // Default state
};
```

**Example - C64:**
```cpp
// Disk drive
traits.peripheral_options.push_back({
    "disk_1541",                // ID for lookup
    "1541 Disk Drive",          // Display name
    "Commodore 1541 floppy disk drive",
    true                        // enabled by default
});

// Printer
traits.peripheral_options.push_back({
    "printer_mps803",
    "MPS-803 Printer",
    "Commodore MPS-803 dot matrix printer",
    false                       // not enabled by default
});

// Cartridge
traits.peripheral_options.push_back({
    "cartridge",
    "Cartridge Port",
    "Expansion cartridge support",
    true
});
```

**Example - CHIP-8:**
```cpp
// SUPER-CHIP extension
traits.peripheral_options.push_back({
    "superchip",
    "SUPER-CHIP 1.1",
    "Enables 128x64 hi-res mode and extended instructions",
    false
});

// XO-CHIP extension
traits.peripheral_options.push_back({
    "xochip",
    "XO-CHIP",
    "Enables XO-CHIP extensions (4-color, audio)",
    false
});
```

## System Configuration

User's current selected configuration:

```cpp
struct SystemConfiguration {
    int memory_option_index;        // Index into memory_options
    int region_option_index;        // Index into video_standard_configs
    std::map<std::string, bool> enabled_peripherals;  // Peripheral states
    std::map<std::string, std::string> custom_settings;  // Custom key-value pairs
};
```

### Configuration Workflow

1. **System provides default configuration** via hardware traits
2. **User modifies configuration** via GUI or config file
3. **System validates configuration** in `set_configuration()`
4. **User applies configuration** via `apply_configuration()`
5. **System may reset** to apply new configuration

### Interface Methods

```cpp
class IEmulatedSystem {
    // Get current configuration
    virtual const SystemConfiguration& get_configuration() const = 0;
    
    // Set new configuration (validates but doesn't apply)
    virtual bool set_configuration(const SystemConfiguration& config) = 0;
    
    // Apply current configuration (may require reset)
    virtual bool apply_configuration() = 0;
    
    // Get hardware traits
    virtual const HardwareTraits& get_hardware_traits() const = 0;
    
    // Get current timing (based on selected region)
    virtual const SystemTiming& get_current_timing() const = 0;
    
    // Render configuration UI
    virtual void render_configuration_ui() = 0;
};
```

## Implementation Example

### Step 1: Define Hardware Traits

```cpp
// In your system's .cpp file
static HardwareTraits create_c64_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display
    traits.display.native_width = 403;
    traits.display.native_height = 284;
    // ... (see examples above)
    
    // Audio
    traits.audio.format = AudioFormat::MONO_16BIT;
    // ... (see examples above)
    
    // Default timing (PAL)
    traits.timing.cpu_frequency_hz = 985248;
    // ... (see examples above)
    
    // Memory options
    traits.memory_options.push_back({
        "64KB Standard", 65536, 24576, true
    });
    // ... add more options
    
    // Region options
    traits.video_standard_configs.push_back({
        "PAL (European)", VideoStandard::PAL, pal_timing, true
    });
    traits.video_standard_configs.push_back({
        "NTSC (American)", VideoStandard::NTSC, ntsc_timing, false
    });
    
    // Peripherals
    traits.peripheral_options.push_back({
        "disk_1541", "1541 Disk Drive", "Floppy disk drive", true
    });
    
    return traits;
}
```

### Step 2: Update System Descriptor

```cpp
static SystemDescriptor c64_descriptor = {
    "Commodore 64",
    "C64",
    "Commodore 64 (1982) - 64KB RAM, VIC-II, SID",
    c64_extensions,
    create_c64_hardware_traits(),  // Add hardware traits
    c64_can_load_file
};
```

### Step 3: Implement Configuration Methods

```cpp
class C64System : public IEmulatedSystem {
private:
    SystemConfiguration config_;
    
public:
    const SystemConfiguration& get_configuration() const override {
        return config_;
    }
    
    bool set_configuration(const SystemConfiguration& config) override {
        // Validate configuration
        const HardwareTraits& traits = get_hardware_traits();
        
        if (config.memory_option_index < 0 || 
            config.memory_option_index >= traits.memory_options.size()) {
            return false;  // Invalid memory option
        }
        
        if (config.region_option_index < 0 ||
            config.region_option_index >= traits.video_standard_configs.size()) {
            return false;  // Invalid region option
        }
        
        // Configuration is valid
        config_ = config;
        return true;
    }
    
    bool apply_configuration() override {
        // Apply the current configuration
        const HardwareTraits& traits = get_hardware_traits();
        
        // Get selected memory config
        const MemoryOption& mem_opt = traits.memory_options[config_.memory_option_index];
        
        // Reallocate memory if needed
        if (mem_opt.ram_size != current_ram_size_) {
            reallocate_ram(mem_opt.ram_size);
        }
        
        // Get selected region config
        const VideoStandardConfig& region_opt = traits.video_standard_configs[config_.region_option_index];
        
        // Update timing
        update_timing(region_opt.timing);
        
        // Enable/disable peripherals
        for (const auto& periph : traits.peripheral_options) {
            bool enabled = config_.enabled_peripherals.count(periph.id) > 0
                         ? config_.enabled_peripherals.at(periph.id)
                         : periph.enabled_by_default;
            
            enable_peripheral(periph.id, enabled);
        }
        
        // Reset system to apply changes
        reset();
        
        return true;
    }
    
    const SystemTiming& get_current_timing() const override {
        const HardwareTraits& traits = get_hardware_traits();
        return traits.video_standard_configs[config_.region_option_index].timing;
    }
};
```

### Step 4: Implement Configuration UI

```cpp
void C64System::render_configuration_ui() {
    const HardwareTraits& traits = get_hardware_traits();
    
    ImGui::Text("Commodore 64 Configuration");
    ImGui::Separator();
    
    // Memory configuration
    ImGui::Text("Memory:");
    for (size_t i = 0; i < traits.memory_options.size(); i++) {
        bool selected = (config_.memory_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(traits.memory_options[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.memory_option_index = static_cast<int>(i);
            set_configuration(new_config);
        }
    }
    ImGui::Separator();
    
    // Region configuration
    ImGui::Text("Region/Timing:");
    for (size_t i = 0; i < traits.video_standard_configs.size(); i++) {
        bool selected = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(traits.video_standard_configs[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.region_option_index = static_cast<int>(i);
            set_configuration(new_config);
        }
    }
    ImGui::Separator();
    
    // Peripherals
    ImGui::Text("Peripherals:");
    for (const auto& periph : traits.peripheral_options) {
        bool enabled = config_.enabled_peripherals.count(periph.id) > 0
                     ? config_.enabled_peripherals.at(periph.id)
                     : periph.enabled_by_default;
        
        if (ImGui::Checkbox(periph.name, &enabled)) {
            SystemConfiguration new_config = config_;
            new_config.enabled_peripherals[periph.id] = enabled;
            set_configuration(new_config);
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", periph.description);
        }
    }
    ImGui::Separator();
    
    // Apply button
    if (ImGui::Button("Apply Configuration")) {
        if (apply_configuration()) {
            // Success
        }
    }
}
```

## GUI Integration

### Settings Window

The GUI can automatically generate a settings panel for any system:

```cpp
// In settings window
if (active_system) {
    ImGui::Text("System: %s", active_system->get_descriptor().name);
    ImGui::Separator();
    
    // Let system render its configuration UI
    active_system->render_configuration_ui();
}
```

### Display Adaptation

The GUI can adapt to any system's display requirements:

```cpp
// Get display traits
const DisplayTraits& display = system->get_display_traits();

// Use native dimensions
int width = display.native_width;
int height = display.native_height;

// Handle pixel aspect ratio
float aspect = display.pixel_aspect_ratio;

// Show/hide overscan based on trait
if (display.has_overscan) {
    // Render with overscan options
}
```

### Timing Adaptation

The GUI can adapt to system timing:

```cpp
// Get current timing
const SystemTiming& timing = system->get_current_timing();

// Use correct FPS target
uint32_t target_fps = timing.target_fps;

// Calculate frame time
float frame_time_ms = 1000.0f / target_fps;
```

## Configuration File Format

Configurations can be saved/loaded from JSON:

```json
{
    "system": "C64",
    "configuration": {
        "memory_option": 0,
        "region_option": 0,
        "peripherals": {
            "disk_1541": true,
            "printer_mps803": false,
            "cartridge": true
        },
        "custom": {
            "true_drive_emulation": "true",
            "warp_mode": "false"
        }
    }
}
```

## Benefits

1. **Unified Interface** - All systems use the same configuration system
2. **Self-Describing** - Systems declare their own capabilities
3. **GUI-Friendly** - Easy to generate UI automatically
4. **Flexible** - Supports system-specific options via custom settings
5. **User-Friendly** - Clear names and descriptions for all options
6. **Persistent** - Configurations can be saved and restored
7. **Validated** - Systems validate configurations before applying

## Best Practices

1. **Provide sensible defaults** - Mark most common configuration as default
2. **Group related options** - Use clear naming conventions
3. **Add descriptions** - Help users understand what options do
4. **Validate carefully** - Ensure configurations are valid before applying
5. **Document timing** - Clearly specify which region is default
6. **Handle errors gracefully** - Return false and log errors if configuration fails