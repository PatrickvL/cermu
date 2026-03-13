# Multi-System Emulator Architecture

> **⚠️ LARGELY SUPERSEDED** — This document was written during early multi-system
> prototyping (2025-07). The architecture has since evolved significantly:
>
> - `IEmulatedSystem` → `System` (base class in `src/core/system.hpp`)
> - `gui_state_t` / `imgui_interface.cpp` → `SessionGUI` / `EmulatorHost`
> - `code/cpp/` file paths → `src/`
> - `c64_t*` wrapper → chips are direct members on `C64System`
> - `SystemRegistry` + `REGISTER_SYSTEM` → still exist, modernized
> - All "Pending Work" items below have been completed (differently than described)
>
> See `CERMU_ARCHITECURE_COMPLETE_REFERENCE.md` for the current architecture.

## Overview
This document describes the plugin-style multi-system architecture for the emulator, allowing multiple emulated systems (CHIP-8, C64, NES, etc.) to coexist with automatic system detection.

## ✅ VERIFIED WORKING
The multi-system architecture has been successfully implemented and tested. Running `./multi_emu_console` shows:
- 2 registered systems (C64, CHIP-8)
- Automatic system detection working
- File extension mapping functional
- Both systems can be instantiated via the registry

## Completed Implementation

### 1. Core Interface (`code/cpp/src/core/emulated_system.h/cpp`)

**Key Components:**
- `SystemDescriptor` - Self-describing system metadata
- `IEmulatedSystem` - Pure virtual interface all systems implement
- `SystemRegistry` - Singleton factory for system instantiation
- `REGISTER_SYSTEM` macro - Automatic registration during static initialization

**Features:**
- File-based system detection via confidence scoring (0.0-1.0)
- Dynamic system creation without hardcoded enums
- Extensible registration system

### 2. CHIP-8 Implementation (`code/cpp/src/systems/chip8/`)

**Status:** ✅ Complete (~540 lines)

**Files:**
- `chip8_system.h` - System class declaration
- `chip8_system.cpp` - Full CHIP-8 emulator implementation with auto-registration

**Capabilities:**
- Complete CHIP-8 instruction set
- 64x32 display rendering
- Keyboard input handling
- File loading (.ch8 extensions)
- Self-registers on program startup

### 3. C64 Wrapper (`code/cpp/src/systems/c64/`)

**Status:** ✅ Complete

**Files:**
- `c64_system.h` - Adapter interface
- `c64_system.cpp` - Wrapper implementation (~200 lines)

**Capabilities:**
- Wraps existing C64 system
- File detection for .prg, .d64, .crt
- Integrates with system registry
- Self-registers alongside CHIP-8

### 4. Build System Updates

**Status:** ✅ Complete

`CMakeLists.txt` updated to include:
- `emulated_system.cpp` in core sources
- `chip8_system.cpp` in system sources  
- `c64_system.cpp` in system sources

## Pending Work

### 1. GUI Integration (High Priority)

**Current State:**
- `imgui_interface.cpp` is tightly coupled to `c64_t*`
- All functions take `c64_t*` directly
- Menu system is C64-specific

**Required Changes:**

#### A. Update Function Signatures
Replace `c64_t*` with `IEmulatedSystem*`:
```cpp
// Before:
void gui_render_frame(c64_t* c64, gui_state_t* gui_state, ...);

// After:
void gui_render_frame(IEmulatedSystem* system, gui_state_t* gui_state, ...);
```

#### B. Add System Context
Store active system in GUI state:
```cpp
typedef struct {
    // Add:
    IEmulatedSystem* active_system;
    
    // Existing fields...
    bool show_screen;
    int target_fps;
    // ...
} gui_state_t;
```

#### C. Dynamic Menu Rendering
```cpp
void gui_render_menu_bar(IEmulatedSystem* system, gui_state_t* gui_state, ...) {
    if (ImGui::BeginMainMenuBar()) {
        // Standard menus (File, View, Help)
        
        // System-specific menu items
        if (system) {
            system->render_system_menu_items();
        }
    }
}
```

#### D. Framebuffer Management
```cpp
void gui_update_screen_texture(IEmulatedSystem* system, gui_state_t* gui_state) {
    if (!system) return;
    
    // Get system's framebuffer
    uint32_t* fb = system->get_framebuffer();
    int width, height;
    system->get_display_dimensions(&width, &height);
    
    // Upload to OpenGL texture
    // ...
}
```

### 2. Multi-System Main (`code/cpp/src/main/multi_emu_main.cpp`)

**Create new main that:**
1. Parses command line for file path
2. Uses `SystemRegistry::create_system_for_file()` to auto-detect system
3. Initializes GUI with detected system
4. Runs main loop

**Example:**
```cpp
int main(int argc, char** argv) {
    const char* file_path = parse_args(argc, argv);
    
    // Auto-detect and create system
    auto system = SystemRegistry::get_instance()
        .create_system_for_file(file_path);
    
    if (!system) {
        printf("No system found for file: %s\n", file_path);
        return 1;
    }
    
    printf("Detected system: %s\n", 
           system->get_descriptor().name);
    
    // Initialize system
    if (!system->initialize()) {
        return 1;
    }
    
    // Load file
    if (!system->load_file(file_path)) {
        return 1;
    }
    
    // Set up GUI
    gui_init("Multi-System Emulator", 1200, 800);
    gui_state_t gui_state;
    gui_init_state(&gui_state);
    gui_state.active_system = system.get();
    
    // Main loop
    while (!gui_should_quit()) {
        gui_handle_events();
        system->run_frame();
        gui_render_frame(system.get(), &gui_state);
        gui_delay(16);
    }
    
    system->shutdown();
    gui_cleanup();
    return 0;
}
```

### 3. Remove Obsolete Code

As per user requirement: "do it in such a way that no unused code remains"

**Files to Update/Remove:**
- `c64_main_gui.cpp` - Replace with multi_emu_main.cpp
- `c64_main.cpp` - Keep for C64-only testing, or remove if not needed
- Any C64-specific GUI code that's been generalized

### 4. Additional Systems (Lower Priority)

**VIC-20 System:**
- Similar to C64 wrapper
- File extensions: .prg, .d64, .tap
- ~200 lines wrapper

**NES System:**
- Wrap existing NES code
- File extensions: .nes
- ~300 lines wrapper

**Apple 1 System:**
- Minimal wrapper needed
- File extensions: .mon, .bin
- ~150 lines wrapper

## Architecture Benefits

### Extensibility
- New systems added by creating class + REGISTER_SYSTEM call
- No modifications to core code required
- No global system type enums

### Maintainability
- Each system is self-contained
- Clear separation of concerns
- Systems can be developed independently

### Flexibility
- Systems selected automatically by file content
- Multiple systems can coexist in single binary
- Easy to add specialized systems (test runners, etc.)

## Testing Strategy

### Phase 1: Build Verification
```bash
cd code/cpp
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Phase 2: CHIP-8 Testing
```bash
./build/bin/multi_emu test.ch8
# Should auto-detect CHIP-8 and run
```

### Phase 3: C64 Testing
```bash
./build/bin/multi_emu test.prg
# Should auto-detect C64 and run
```

### Phase 4: File Detection
Test confidence scoring with:
- Ambiguous files
- Unknown file types
- Corrupted files

## Extension Guide

### Adding a New System

1. **Create System Class**
```cpp
// my_system.h
#include "emulated_system.h"

class MySystem : public IEmulatedSystem {
public:
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void shutdown() override;
    // ... implement all interface methods
};
```

2. **Implement File Detection**
```cpp
static float my_system_can_load(const char* path, 
                                const uint8_t* data, 
                                size_t size) {
    // Check file extension
    if (ends_with(path, ".mysys")) return 0.95f;
    
    // Check magic bytes
    if (size >= 4 && memcmp(data, "MSYS", 4) == 0) 
        return 1.0f;
    
    return 0.0f;
}
```

3. **Register System**
```cpp
// my_system.cpp
REGISTER_SYSTEM(my_descriptor, []() {
    return std::make_unique<MySystem>();
})
```

4. **Add to CMakeLists.txt**
```cmake
set(SYSTEM_SOURCES
    # ...
    src/systems/mysystem/my_system.cpp
)
```

That's it! The system is now available.

## Current Status Summary

| Component | Status | Lines | Notes |
|-----------|--------|-------|-------|
| Core Interface | ✅ Complete | ~200 | Tested, working |
| System Registry | ✅ Complete | ~150 | Tested, working |
| CHIP-8 System | ✅ Complete | ~540 | Full implementation |
| C64 Wrapper | ✅ Complete | ~200 | Wraps existing code |
| Build System | ✅ Complete | - | Compiles successfully |
| GUI Integration | ⏳ Pending | ~500 | Major refactor needed |
| Multi-System Main | ⏳ Pending | ~150 | New file needed |
| Code Cleanup | ⏳ Pending | - | Remove obsolete code |

## Next Steps

1. Start GUI refactoring (highest priority)
2. Create multi_emu_main.cpp
3. Test with both CHIP-8 and C64
4. Remove obsolete C64-specific code
5. Document system extension process
6. Add more systems as time permits

## Timeline Estimate

- GUI Integration: 2-3 hours
- Multi-System Main: 1 hour  
- Testing & Debugging: 2 hours
- Code Cleanup: 1 hour
- Documentation: 1 hour

**Total: ~7-8 hours of focused work**