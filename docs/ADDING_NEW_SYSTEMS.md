# Adding New Systems to the Multi-System Emulator

This guide shows how to add a new emulated system to the multi-system architecture.

## Quick Reference

To add a new system:
1. Create system class implementing `IEmulatedSystem`
2. Add system descriptor and file detection
3. Register with `REGISTER_SYSTEM` macro
4. Add source files to `CMakeLists.txt`
5. Build and test

**Estimated time:** 2-4 hours for a simple system

## Step-by-Step Example: Adding VIC-20

### Step 1: Create Header File

Create `code/cpp/src/systems/vic20/vic20_system.h`:

```cpp
#pragma once

#include "../../core/emulated_system.h"
#include <cstdint>
#include <memory>

class VIC20System : public IEmulatedSystem {
public:
    VIC20System();
    ~VIC20System() override;
    
    // System identification
    const SystemDescriptor& get_descriptor() const override;
    
    // System lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;
    
    // Execution
    void tick() override;
    void run_frame() override;
    
    // File loading
    bool load_file(const char* filepath) override;
    
    // Display
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;
    
    // Input
    void handle_keyboard_event(int key, bool pressed) override;
    void handle_controller_event(int controller, int button, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_debug_windows(void* gui_state) override;
    
    // State
    uint64_t get_total_cycles() const override;
    uint32_t get_target_fps() const override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;
    float get_speed_multiplier() const override;

private:
    // Your VIC-20 specific state here
    uint32_t* framebuffer_;
    int fb_width_;
    int fb_height_;
    uint64_t total_cycles_;
    float speed_multiplier_;
    uint32_t cycles_per_frame_;
};
```

### Step 2: Create Implementation File

Create `code/cpp/src/systems/vic20/vic20_system.cpp`:

```cpp
#include "vic20_system.h"
#include <cstring>
#include <cstdio>

// File detection callback
static float vic20_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    // Check file extension
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            // PRG files - but how to distinguish from C64?
            // Check size or header patterns
            if (size >= 2 && size <= 32768) {
                return 0.7f;  // Moderate confidence
            }
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            return 0.9f;  // High confidence for TAP files
        }
    }
    return 0.0f;
}

// Supported file extensions
static const char* vic20_extensions[] = {".prg", ".tap", ".d64", nullptr};

// System descriptor
static SystemDescriptor vic20_descriptor = {
    "VIC-20",                                              // name
    "VIC20",                                               // short_name
    "Commodore VIC-20 (1980) - 5KB RAM, 22-column display", // description
    vic20_extensions,                                      // supported_extensions
    vic20_can_load_file                                    // can_load_file callback
};

// Constructor
VIC20System::VIC20System()
    : framebuffer_(nullptr)
    , fb_width_(0)
    , fb_height_(0)
    , total_cycles_(0)
    , speed_multiplier_(1.0f)
    , cycles_per_frame_(14656)  // PAL: ~1.1MHz / 50Hz
{
}

VIC20System::~VIC20System() {
    shutdown();
}

const SystemDescriptor& VIC20System::get_descriptor() const {
    return vic20_descriptor;
}

bool VIC20System::initialize() {
    printf("VIC20: Initializing system\n");
    // Initialize your VIC-20 emulator here
    // Set up CPU, VIC chip, memory, etc.
    total_cycles_ = 0;
    return true;
}

void VIC20System::shutdown() {
    printf("VIC20: Shutting down system\n");
    // Clean up resources
}

void VIC20System::reset() {
    printf("VIC20: Resetting system\n");
    total_cycles_ = 0;
    // Reset CPU and hardware
}

void VIC20System::tick() {
    // Execute one cycle of VIC-20 hardware
    total_cycles_++;
}

void VIC20System::run_frame() {
    // Execute one frame worth of cycles
    for (uint32_t i = 0; i < cycles_per_frame_; i++) {
        tick();
    }
}

bool VIC20System::load_file(const char* filepath) {
    printf("VIC20: Loading file: %s\n", filepath);
    
    // Determine file type and load
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        return false;
    }
    
    if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
        // Load PRG file
        // Read file, parse load address, load into memory
        printf("VIC20: Loading PRG file\n");
        return true;  // Placeholder
    }
    
    return false;
}

uint32_t* VIC20System::get_framebuffer() {
    return framebuffer_;
}

void VIC20System::get_display_dimensions(int* width, int* height) const {
    // VIC-20 display: 176x184 (PAL)
    *width = 176;
    *height = 184;
}

void VIC20System::set_framebuffer(uint32_t* buffer, int width, int height) {
    framebuffer_ = buffer;
    fb_width_ = width;
    fb_height_ = height;
}

void VIC20System::handle_keyboard_event(int key, bool pressed) {
    // Map keyboard to VIC-20 keyboard matrix
    (void)key;
    (void)pressed;
}

void VIC20System::handle_controller_event(int controller, int button, bool pressed) {
    // Handle joystick if needed
    (void)controller;
    (void)button;
    (void)pressed;
}

void VIC20System::render_system_menu_items() {
    // Add VIC-20 specific menu items when GUI is available
}

void VIC20System::render_debug_windows(void* gui_state) {
    // Render debug windows when GUI is available
    (void)gui_state;
}

uint64_t VIC20System::get_total_cycles() const {
    return total_cycles_;
}

uint32_t VIC20System::get_target_fps() const {
    return 50;  // PAL
}

void VIC20System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    cycles_per_frame_ = static_cast<uint32_t>(14656 * multiplier);
}

float VIC20System::get_speed_multiplier() const {
    return speed_multiplier_;
}

// IMPORTANT: Register the system
REGISTER_SYSTEM(vic20_descriptor, []() {
    return std::make_unique<VIC20System>();
})
```

### Step 3: Add to Build System

Edit `code/cpp/CMakeLists.txt`, find the `SYSTEM_SOURCES` section and add:

```cmake
set(SYSTEM_SOURCES
    # ... existing systems ...
    
    # VIC-20 system
    src/systems/vic20/vic20_system.cpp
)
```

### Step 4: Create Directory Structure

```bash
mkdir -p code/cpp/src/systems/vic20
# Move your .h and .cpp files there
```

### Step 5: Build and Test

```bash
cd code/cpp/build
cmake ..
cmake --build . --target multi_emu_console
./bin/multi_emu_console
```

You should see:
```
Registered Systems: 3
  - Commodore 64 (C64)
  - CHIP-8 Interpreter (CHIP8)
  - VIC-20 (VIC20)  <-- Your new system!
```

## Testing Your System

```bash
# Test file detection
./bin/multi_emu_console test.prg

# Should output:
# Detected System: VIC-20
# ... loading and running ...
```

## Common Patterns

### Pattern 1: Wrapper for Existing Emulator

If you already have an emulator core (like C64):

```cpp
class MySystemWrapper : public IEmulatedSystem {
private:
    my_system_t* system_;  // Pointer to existing emulator
    
public:
    bool initialize() override {
        system_ = my_system_create();
        return system_ != nullptr;
    }
    
    void tick() override {
        my_system_tick(system_);
    }
    
    // Wrap other functions similarly
};
```

### Pattern 2: Self-Contained Emulator

If building from scratch (like CHIP-8):

```cpp
class MySystem : public IEmulatedSystem {
private:
    uint8_t memory_[65536];
    uint16_t pc_;
    uint8_t registers_[16];
    // All state owned by this class
    
public:
    void tick() override {
        // Fetch, decode, execute
        uint8_t opcode = memory_[pc_++];
        // ... emulation logic ...
    }
};
```

### Pattern 3: File Detection Strategies

```cpp
static float my_can_load_file(const char* path, const uint8_t* data, size_t size) {
    // Strategy 1: Extension only
    if (ends_with(path, ".mysys")) return 0.95f;
    
    // Strategy 2: Magic bytes
    if (size >= 4 && memcmp(data, "MYSY", 4) == 0) return 1.0f;
    
    // Strategy 3: Size heuristic
    if (size >= 1024 && size <= 65536) return 0.6f;
    
    // Strategy 4: Combined
    if (ends_with(path, ".bin") && size == 32768) return 0.8f;
    
    return 0.0f;
}
```

## Confidence Scoring Guidelines

| Confidence | Meaning | Example |
|------------|---------|---------|
| 1.0 | Perfect match | Magic bytes + correct extension |
| 0.9-0.95 | Very high | Correct extension + valid size |
| 0.7-0.8 | High | Extension match OR magic bytes |
| 0.5-0.6 | Moderate | Size heuristic match |
| 0.3-0.4 | Low | Weak indicator |
| 0.0 | No match | Doesn't match this system |

## Troubleshooting

### System Not Appearing

**Problem:** `multi_emu_console` doesn't list your system

**Solutions:**
1. Check `REGISTER_SYSTEM` macro is called
2. Verify source file is in CMakeLists.txt
3. Rebuild completely: `rm -rf build && mkdir build && cd build && cmake .. && make`
4. Check for compilation errors

### File Detection Not Working

**Problem:** System not detected for your file

**Solutions:**
1. Print debug info in `can_load_file`:
   ```cpp
   printf("Checking %s: size=%zu\n", filepath, size);
   ```
2. Check return value (0.0-1.0 range)
3. Test with known-good file
4. Check file extension matching

### Build Errors

**Problem:** Linker errors or undefined references

**Solutions:**
1. Include all necessary source files in CMakeLists.txt
2. Check header include paths
3. Verify `REGISTER_SYSTEM` syntax
4. Check for missing overrides

## Best Practices

### DO:
✅ Use descriptive system names
✅ Implement all interface methods (even if stubbed)
✅ Return meaningful confidence scores
✅ Handle file loading errors gracefully
✅ Clean up resources in shutdown()
✅ Test with real files

### DON'T:
❌ Hardcode paths or file locations
❌ Use global variables
❌ Assume GUI is always available
❌ Return confidence > 1.0 or < 0.0
❌ Forget to call REGISTER_SYSTEM
❌ Leave unimplemented methods

## Performance Tips

1. **Lazy Initialization:** Only allocate large buffers when needed
2. **Efficient Tick:** Keep tick() fast - it's called millions of times
3. **Frame Batching:** Process multiple cycles per call when possible
4. **Cache Lookups:** Pre-calculate lookup tables
5. **Avoid I/O:** Don't do file I/O during emulation

## Example: Minimal System

Here's a minimal working system (does nothing but registers):

```cpp
// minimal_system.cpp
#include "../../core/emulated_system.h"

class MinimalSystem : public IEmulatedSystem {
    static const SystemDescriptor& desc() {
        static const char* exts[] = {".min", nullptr};
        static SystemDescriptor d = {
            "Minimal", "MIN", "Test system", exts,
            [](const char* p, const uint8_t*, size_t) {
                return strstr(p, ".min") ? 0.9f : 0.0f;
            }
        };
        return d;
    }
public:
    const SystemDescriptor& get_descriptor() const override { return desc(); }
    bool initialize() override { return true; }
    void shutdown() override {}
    void reset() override {}
    void tick() override {}
    void run_frame() override {}
    bool load_file(const char*) override { return true; }
    uint32_t* get_framebuffer() override { return nullptr; }
    void get_display_dimensions(int* w, int* h) const override { *w = *h = 0; }
    void set_framebuffer(uint32_t*, int, int) override {}
    void handle_keyboard_event(int, bool) override {}
    void handle_controller_event(int, int, bool) override {}
    void render_system_menu_items() override {}
    void render_debug_windows(void*) override {}
    uint64_t get_total_cycles() const override { return 0; }
    uint32_t get_target_fps() const override { return 60; }
    void set_speed_multiplier(float) override {}
    float get_speed_multiplier() const override { return 1.0f; }
};

REGISTER_SYSTEM(MinimalSystem::desc(), []() {
    return std::make_unique<MinimalSystem>();
})
```

Add to CMakeLists.txt and it works!

## Next Steps

After adding your system:
1. Test with `multi_emu_console`
2. Verify file detection works
3. Test basic emulation (tick, run_frame)
4. Add GUI integration if needed
5. Write tests for your system
6. Document system-specific features

## Resources

- See [`chip8_system.cpp`](../code/cpp/src/systems/chip8/chip8_system.cpp) for a complete self-contained example
- See [`c64_system.cpp`](../code/cpp/src/systems/c64/c64_system.cpp) for a wrapper example
- Check [`emulated_system.h`](../code/cpp/src/core/emulated_system.h) for interface documentation