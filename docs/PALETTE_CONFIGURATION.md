# Palette Configuration System

## Overview

Systems using palette-indexed framebuffer formats (MONOCHROME_1, PALETTE_INDEXED_2/4/8) can have configurable color palettes set via the system configuration.

## Configuration Format

Palettes are configured through the `SystemConfiguration` custom settings:

```cpp
SystemConfiguration config;
config.custom_settings["display_palette"] = "palette_name";
system->set_configuration(config);
```

## CHIP-8 Palettes

CHIP-8 uses 1-bit monochrome format with 2 colors (off/on).

### Available Palettes

| Name | ID | Off Color (0) | On Color (1) | Description |
|------|-----|---------------|--------------|-------------|
| Green Phosphor | `green` | RGB(0,0,0) | RGB(0,255,0) | Original CHIP-8 terminal display (default) |
| Amber Monitor | `amber` | RGB(0,0,0) | RGB(255,176,0) | Classic amber CRT monitor |
| White on Black | `white` | RGB(0,0,0) | RGB(255,255,255) | High contrast monochrome |
| C64 Colors | `c64` | RGB(64,49,141) | RGB(123,112,252) | Commodore 64 blue colors |

### Example Usage

```cpp
// Set amber palette
SystemConfiguration config = chip8->get_configuration();
config.custom_settings["display_palette"] = "amber";
chip8->set_configuration(config);
```

### GUI Integration

The configuration UI renders a combo box for palette selection:

```cpp
void Chip8System::render_configuration_ui() {
    static const char* palette_names[] = {
        "Green Phosphor (Original)",
        "Amber Monitor", 
        "White on Black",
        "C64 Colors"
    };
    
    // Show combo box with color preview
    ImGui::Combo("Display Palette", &selected, palette_names, 4);
    ImGui::ColorButton("Off", color0);
    ImGui::ColorButton("On", color1);
}
```

## Adding New Palettes

To add a new palette to CHIP-8:

1. **Add palette ID** to the configuration handler in `set_configuration()`:

```cpp
else if (palette_name == "custom_name") {
    current_palette_[0] = PaletteColor(r0, g0, b0, 255);  // Off color
    current_palette_[1] = PaletteColor(r1, g1, b1, 255);  // On color
}
```

2. **Add to UI** in `render_configuration_ui()`:

```cpp
static const char* palette_names[] = {
    "Green Phosphor (Original)",
    "Amber Monitor",
    "White on Black", 
    "C64 Colors",
    "Custom Name"  // Add here
};

static const char* palette_ids[] = {
    "green", "amber", "white", "c64", "custom_name"  // And here
};
```

3. **Test** the new palette loads correctly

## Other Systems

### Game Boy (4 shades, 2-bit)

```cpp
config.custom_settings["display_palette"] = "gb_green";  // Original DMG green
config.custom_settings["display_palette"] = "gb_gray";   // Game Boy Pocket grayscale
config.custom_settings["display_palette"] = "gb_blue";   // Blue tint variant
```

4 colors required for Game Boy palettes.

### ZX Spectrum (16 colors, 1-bit + attributes)

```cpp
config.custom_settings["display_palette"] = "spectrum";  // Standard ZX Spectrum colors
```

16 colors required (8 normal + 8 bright).

### NES (64 colors, palette-indexed)

```cpp
config.custom_settings["display_palette"] = "composite";  // NTSC composite video
config.custom_settings["display_palette"] = "rgb";        // RGB mod
config.custom_settings["display_palette"] = "pvm";        // Professional video monitor
```

64-color master palette.

## Implementation Details

### Color Storage

Colors are stored as `PaletteColor` structs:

```cpp
struct PaletteColor {
    uint8_t r, g, b, a;  // RGBA components
    
    uint32_t to_rgba32() const {
        return (a << 24) | (b << 16) | (g << 8) | r;
    }
};
```

### Palette in Hardware Traits

Default palettes are defined in hardware traits:

```cpp
HardwareTraits traits;
traits.display.default_palette.push_back(PaletteColor(0, 0, 0, 255));     // Color 0
traits.display.default_palette.push_back(PaletteColor(0, 255, 0, 255));   // Color 1
```

### Runtime Palette Switching

Systems maintain a `current_palette_` that can differ from the default:

```cpp
class Chip8System {
private:
    std::vector<PaletteColor> current_palette_;  // Runtime palette
    
    // Initialize with default
    current_palette_ = hardware_traits_.display.default_palette;
};
```

### Conversion to RGBA

The generic renderer converts indexed pixels to RGBA using the current palette:

```cpp
FramebufferRenderer::convert_monochrome_1bit(
    native_display_,      // 1-bit source
    64, 32,               // dimensions
    current_palette_[0],  // Off color
    current_palette_[1],  // On color
    rgba_framebuffer_,    // RGBA destination
    rgba_width_, rgba_height_
);
```

## Save/Load Configuration

Palette settings persist with system configuration:

```cpp
// Save configuration
SystemConfiguration config = system->get_configuration();
save_to_file(config);

// Load configuration
SystemConfiguration config = load_from_file();
system->set_configuration(config);
system->apply_configuration();
```

The `custom_settings` map automatically preserves palette choices.

## Best Practices

1. **Provide sensible defaults** - Original hardware palette as default
2. **Show color preview** - Use `ImGui::ColorButton()` in UI
3. **Mark dirty on palette change** - Set `display_dirty_ = true`
4. **Support common variants** - Green, amber, white for monochrome systems
5. **Document color values** - Include RGB values in comments
6. **Test accessibility** - Ensure good contrast for readability

## Future Enhancements

- Custom palette editor in GUI
- Per-game palette overrides
- Palette presets file format
- Color correction filters
- Save palette screenshots