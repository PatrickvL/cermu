# Hardware Visualization & Board Layout Architecture

**Date:** 2026-01-18  
**Purpose:** Complete architecture for data-driven hardware visualization, board layouts, and multi-board systems  
**Scope:** Applies to all emulated systems (C64, VIC-20, Apple1, arcade systems, peripherals)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Principles](#architecture-principles)
3. [Data-Driven Visualization](#data-driven-visualization)
4. [Board Layout System](#board-layout-system)
5. [Multi-Board Architecture](#multi-board-architecture)
6. [System Integration](#system-integration)
7. [Implementation Roadmap](#implementation-roadmap)

---

## Executive Summary

This document defines a comprehensive architecture for externalizing all visual and physical hardware characteristics into TOML data files, enabling:

- ✅ **Data-Driven Visualization** - Separate emulation logic from visual presentation
- ✅ **Board Layout Awareness** - Physical PCB layouts with chip socket positions
- ✅ **Multi-Board Support** - Handle complex peripherals and arcade systems
- ✅ **User Customization** - Edit appearance without recompiling
- ✅ **Multiple Themes** - Different visual styles for same hardware
- ✅ **Accurate Rendering** - Correct chip orientations and positioning
- ✅ **Educational Value** - Understand real hardware construction

### Key Concepts

1. **Chip Layout** - Physical package (DIP-40, PLCC-44) with pin definitions
2. **Board Layout** - Physical PCB with socket locations and chip placements  
3. **Chip Orientation** - Correct pin 1 direction for accurate rendering
4. **Multi-Board System** - Multiple interconnected PCBs (motherboard + peripherals)
5. **Cabinet/Case** - Physical enclosure with connectors and mounting points
6. **Visual Metadata** - Colors, dimensions, materials in TOML files

---

## Architecture Principles

### 1. Separation of Concerns

```
┌─────────────────────────────────────────┐
│   EMULATION LAYER (C++ Code)            │
│   - Chip logic, timing, signals          │
│   - Bus operations, memory access        │
│   - CPU execution, interrupts            │
│   - NO visual/physical attributes        │
└─────────────────────────────────────────┘
            ↓ References ↑ Updates
┌─────────────────────────────────────────┐
│   METADATA LAYER (TOML Files)           │
│   - Physical dimensions, colors          │
│   - Socket positions, orientations       │
│   - PCB layouts, chip packages           │
│   - Cable types, connector specs         │
│   - 3D models, textures                  │
└─────────────────────────────────────────┘
            ↓ Loaded by ↑
┌─────────────────────────────────────────┐
│   VISUALIZATION LAYER (Rendering)        │
│   - Reads metadata for current entity    │
│   - Renders based on loaded data         │
│   - Caches frequently used resources     │
└─────────────────────────────────────────┘
```

### 2. Code Responsibilities

**C++ Code (Emulation)**:
- Chip functional behavior
- Signal processing  
- Timing and synchronization
- Memory management
- Bus operations
- ONLY: Chip type identifiers ("MOS6510", "VIC-II")

**TOML Files (Visualization)**:
- Physical dimensions (mm, inches)
- Colors (RGB, hex codes)
- Positions (X, Y, Z coordinates)
- Orientations (degrees, quaternions)
- Package types (DIP-40, PLCC-44)
- PCB characteristics
- 3D model references
- Texture paths

---

## Data-Driven Visualization

### Directory Structure

```
data/
├── hardware/
│   ├── chips/
│   │   ├── cpu/
│   │   │   ├── mos6510.toml
│   │   │   ├── mos6502.toml
│   │   │   └── ...
│   │   ├── video/
│   │   │   ├── mos6569.toml
│   │   │   ├── mos6567.toml
│   │   │   └── ...
│   │   ├── audio/
│   │   │   ├── mos6581.toml
│   │   │   ├── mos8580.toml
│   │   │   └── ...
│   │   └── io/
│   │       ├── mos6526.toml
│   │       ├── mos6526a.toml
│   │       └── ...
│   │
│   ├── boards/
│   │   ├── c64/
│   │   │   ├── assy_250425_reva.toml
│   │   │   ├── assy_250469_revb.toml
│   │   │   └── ...
│   │   ├── vic20/
│   │   └── ...
│   │
│   ├── cabinets/
│   │   ├── c64_breadbin.toml
│   │   ├── c64c_new_case.toml
│   │   └── ...
│   │
│   └── connectors/
│       ├── din6.toml
│       ├── edge_connector_44pin.toml
│       └── ...
│
├── visual/
│   ├── themes/
│   │   ├── realistic.toml
│   │   ├── schematic.toml
│   │   ├── retro.toml
│   │   └── modern.toml
│   │
│   └── materials/
│       ├── pcb_green.toml
│       ├── pcb_brown.toml
│       └── ...
│
└── models/
    ├── chips/
    │   ├── dip40.glb
    │   ├── dip28.glb
    │   └── ...
    └── cases/
        ├── c64_breadbin.glb
        └── ...
```

### Example: Chip Metadata

**File:** `data/hardware/chips/cpu/mos6510.toml`

```toml
[chip]
identifier = "MOS6510"
full_name = "MOS Technology 6510 Microprocessor"
manufacturer = "MOS Technology / Commodore"
year_introduced = 1982
description = "8-bit CPU with 6-bit I/O port, used in Commodore 64"

[package]
type = "DIP"
pin_count = 40
width_mm = 15.24
length_mm = 52.07
height_mm = 4.5
material = "black_ceramic"
model_3d = "models/chips/dip40.glb"

[visual]
body_color = "#1a1a1a"
body_finish = "matte"
text_color = "#ffffff"
text_font = "monospace"
text_size_mm = 2.5

[visual.logo]
text = "MOS\n6510\nCSG"
position = "center"
lines = ["MOS", "6510", "CSG"]

[pins]
reference = "chip_layouts/mos6510_pinout.toml"
pin_1_indicator = "notch"
```

### Metadata Loading System

```cpp
class HardwareMetadataCache {
public:
    const ChipVisualData* get_chip_visual(const std::string& chip_type) {
        auto it = chip_cache_.find(chip_type);
        if (it != chip_cache_.end()) {
            return &it->second;
        }
        
        std::string path = "data/hardware/chips/" + get_chip_category(chip_type) + "/" 
                          + to_lower(chip_type) + ".toml";
        
        ChipVisualData data = load_toml_chip_data(path);
        chip_cache_[chip_type] = data;
        
        return &chip_cache_[chip_type];
    }
    
    void preload_system(const std::string& system_name) {
        std::thread([this, system_name]() {
            auto chips = get_chips_for_system(system_name);
            for (const auto& chip : chips) {
                get_chip_visual(chip);
            }
        }).detach();
    }

private:
    std::unordered_map<std::string, ChipVisualData> chip_cache_;
    std::unordered_map<std::string, BoardVisualData> board_cache_;
};
```

---

## Board Layout System

### Architecture Layers

```
Cabinet Level (Physical Machine)
    ↓
Board Revision (PCB Version)
    ↓
Chip Sockets (U1, U2, U3, etc.)
    ↓
Chip Instances (VIC-II, SID, CIA, etc.)
    ↓
Chip Layout (Package pins - DIP-40, etc.)
```

### Chip Socket with Orientation

```cpp
enum class ChipOrientation {
    NORTH = 0,      // Pin 1 at top (standard)
    EAST = 90,      // Pin 1 at right
    SOUTH = 180,    // Pin 1 at bottom (upside down)
    WEST = 270      // Pin 1 at left
};

struct ChipSocket {
    // Identity
    std::string socket_id;           // "U1", "U2", "U19"
    std::string chip_type;           // "MOS6510", "MOS6569"
    std::string silk_screen_label;   // PCB label
    
    // Physical position
    float x_position_mm;             // X from board origin
    float y_position_mm;             // Y from board origin
    float z_offset_mm;               // Height above board
    
    // Orientation (critical for rendering)
    ChipOrientation orientation;     // Pin 1 direction
    float rotation_fine_degrees;     // Fine adjustment
    
    // Properties
    bool is_socketed;                // Removable vs soldered
    int layer;                       // 0=top, 1=bottom
    std::string notes;               // Additional info
};
```

### Board Layout Structure

```cpp
struct BoardLayout {
    // Identity
    std::string board_name;          // "C64 ASSY NO. 250425"
    std::string board_revision;      // "Rev B"
    std::string manufacture_date_range; // "1982-1983"
    
    // Physical dimensions
    float board_width_mm;
    float board_height_mm;
    float thickness_mm;
    
    // Chip sockets
    std::vector<ChipSocket> sockets;
    
    // Visual characteristics
    std::string pcb_color;           // "Green", "Brown"
    std::string silk_screen_color;   // "White", "Yellow"
    
    // Changes from previous revision
    std::vector<std::string> changes_from_previous;
};
```

### Example: C64 Board Layout TOML

**File:** `data/hardware/boards/c64/assy_250425_reva.toml`

```toml
[board]
identifier = "ASSY_250425_REVA"
name = "C64 ASSY NO. 250425 Rev A"
nickname = "Breadbin"
manufacturer = "Commodore"
year_range = "1982-1983"

[physical]
width_mm = 240.0
height_mm = 200.0
thickness_mm = 1.6
layers = 2

[[physical.mounting_holes]]
x = 10
y = 10
diameter_mm = 3.2

[visual]
pcb_color = "#654321"  # Brown
silk_screen_color = "#ffffff"
solder_mask = "matte"

[[sockets]]
socket_id = "U1"
chip_type = "MOS6510"
x_position_mm = 50.0
y_position_mm = 150.0
orientation = "NORTH"
is_socketed = false
silk_screen_label = "U1 CPU"

[[sockets]]
socket_id = "U19"
chip_type = "MOS6569"
x_position_mm = 120.0
y_position_mm = 120.0
orientation = "SOUTH"  # Rotated 180°!
is_socketed = true
silk_screen_label = "U19 VIC"
notes = "Often rotated 180° in C64 layout"
```

---

## Multi-Board Architecture

### Board Connection Types

```cpp
enum class BoardConnectionType {
    MOTHERBOARD,        // Main system board
    DAUGHTERBOARD,      // Plugs into motherboard
    EXPANSION_CARD,     // Removable card
    PERIPHERAL_MAIN,    // Main board in peripheral
    PERIPHERAL_SUB,     // Secondary board in peripheral
    BACKPLANE           // Passive interconnect
};
```

### Board Connector Structure

```cpp
struct BoardConnector {
    std::string connector_id;        // "EXPANSION PORT"
    std::string connector_type;      // "Edge connector"
    int pin_count;                   // Number of pins
    
    // Source position
    std::string source_board_id;
    float source_x_mm;
    float source_y_mm;
    
    // Destination position
    std::string dest_board_id;
    float dest_x_mm;
    float dest_y_mm;
    
    // Cable information
    bool has_cable;
    float cable_length_mm;
    std::string cable_type;          // "Ribbon", "Twisted pair"
    
    // Signals
    std::vector<std::string> signals; // "D0-D7", "A0-A15"
};
```

### Multi-Board System Container

```cpp
class MultiBoardSystem {
public:
    struct BoardInstance {
        std::string board_id;
        BoardLayout* layout;
        BoardConnectionType type;
        std::string parent_board_id;
        glm::vec3 position_offset;
        glm::vec3 rotation;
        bool is_removable;
        bool is_present;
    };
    
private:
    std::string system_name_;
    std::vector<BoardInstance> boards_;
    std::vector<BoardConnector> connectors_;
    std::string cabinet_model_;
    
public:
    void add_board(const std::string& board_id, 
                   BoardLayout* layout,
                   BoardConnectionType type);
    
    void connect_boards(const std::string& board_a,
                        const std::string& board_b,
                        const BoardConnector& connector);
    
    void render_3d_view();
};
```

### Example: Commodore 1541 Disk Drive

```cpp
MultiBoardSystem floppy_1541;

// Main logic board
BoardLayout floppy_main_board = {
    .board_name = "1541-II Logic Board",
    .sockets = {
        {.socket_id = "U1", .chip_type = "MOS6502"},
        {.socket_id = "U2", .chip_type = "MOS6522_VIA1"},
        {.socket_id = "U3", .chip_type = "MOS6522_VIA2"}
    }
};

// Drive mechanism board
BoardLayout floppy_drive_board = {
    .board_name = "1541 Drive Mechanics Board",
    .sockets = {
        {.socket_id = "U1", .chip_type = "7406_BUFFER"},
        {.socket_id = "U2", .chip_type = "74LS14_SCHMITT"}
    }
};

floppy_1541.add_board("LOGIC_BOARD", &floppy_main_board, 
                      BoardConnectionType::PERIPHERAL_MAIN);

floppy_1541.add_board("DRIVE_BOARD", &floppy_drive_board,
                      BoardConnectionType::PERIPHERAL_SUB, 
                      "LOGIC_BOARD");

floppy_1541.connect_boards("LOGIC_BOARD", "DRIVE_BOARD", {
    .connector_id = "DRIVE_CABLE",
    .connector_type = "Ribbon cable",
    .pin_count = 34,
    .has_cable = true,
    .cable_length_mm = 150.0f
});
```

---

## System Integration

### C++ Emulation Code (Clean)

```cpp
class C64System : public EmulatedSystem {
private:
    // ONLY emulation state
    std::unique_ptr<vicii_t> vicii_;
    std::unique_ptr<mos6581_t> sid_;
    
    // Configuration references
    std::string board_revision_id_;      // "ASSY_250425_REVA"
    std::string cabinet_model_id_;       // "C64_BREADBIN"
    
public:
    // Get chip type for visualization lookup
    std::string get_chip_type(int chip_index) const {
        switch (chip_index) {
            case 0: return "MOS6510";
            case 1: return "MOS6569";
            case 2: return "MOS6581";
        }
    }
    
    // NO methods like get_chip_color(), get_pcb_dimensions()
    // All visual queries go through metadata system
};
```

### Rendering Code (Uses Metadata)

```cpp
class BoardRenderer {
public:
    void render_board(const C64System* system) {
        std::string board_id = system->get_board_id();
        
        const BoardVisualData* board_data = 
            metadata_cache_.get_board_visual(board_id);
        
        render_pcb_substrate(
            board_data->physical.width_mm,
            board_data->physical.height_mm,
            board_data->visual.pcb_color
        );
        
        for (const auto& socket : board_data->sockets) {
            const ChipVisualData* chip_data = 
                metadata_cache_.get_chip_visual(socket.chip_type);
            
            render_chip_3d(
                chip_data->package.model_3d,
                socket.position,
                socket.orientation,
                chip_data->visual.body_color
            );
        }
    }

private:
    HardwareMetadataCache metadata_cache_;
};
```

### 3D Visualization System

```cpp
class SystemScene3D {
public:
    struct BoardNode {
        BoardInstance* board;
        Board3DTransform transform;
        std::vector<BoardNode*> children;
    };
    
private:
    BoardNode* root_board_;
    std::vector<BoardConnector> cables_;
    CabinetModel* cabinet_;
    
public:
    void render(const Camera3D& camera) {
        if (cabinet_) {
            render_cabinet(cabinet_);
        }
        
        render_board_hierarchy(root_board_, glm::mat4(1.0f));
        
        for (const auto& cable : cables_) {
            render_cable(cable);
        }
    }
    
    void render_chip_in_socket(const ChipSocket& socket, 
                               const glm::mat4& board_transform) {
        glm::mat4 chip_transform = board_transform;
        
        chip_transform = glm::translate(chip_transform, 
            glm::vec3(socket.x_position_mm, socket.y_position_mm, socket.z_offset_mm));
        
        chip_transform = glm::rotate(chip_transform,
            glm::radians(socket.get_total_rotation()),
            glm::vec3(0, 0, 1));
        
        ChipLayout* layout = get_chip_layout(socket.chip_type);
        render_chip_package(layout, chip_transform);
    }
};
```

---

## Implementation Roadmap

### Phase 0.5: Data-Driven Metadata (2-3 days)

1. **Create Data File Structure** (Day 1)
   - Set up directory organization
   - Create TOML schemas
   - Write validation system

2. **Convert Existing Data** (Day 2)
   - Extract hardcoded visual data from C64 code
   - Create TOML files for C64 boards
   - Create chip visual data files

3. **Implement Metadata Cache** (Day 2-3)
   - Build efficient loading system
   - Add caching with LRU
   - Integrate with rendering system

### Phase 0.6: Board Layout Infrastructure (3-4 days)

1. **Define Board Structures** (Day 1)
   - Create board_layout.h
   - Define ChipSocket, BoardLayout
   - Add chip orientation support

2. **Implement Board-Chip Mapping** (Day 2)
   - Map board revisions to hardware configs
   - Add board selection logic
   - Test with C64 revisions

3. **Multi-Board Support** (Day 3)
   - Implement MultiBoardSystem class
   - Add BoardConnector support
   - Test with 1541 drive

4. **Testing** (Day 3-4)
   - Test with different board revisions
   - Verify chip orientations
   - Test board switching

### Phase 2.5: Board Visualization (Optional, 4-5 days)

1. **Basic PCB Rendering** (Day 1-2)
   - Render board outline
   - Draw chip sockets
   - Label socket IDs

2. **Chip Placement** (Day 2-3)
   - Render chips in correct positions
   - Show chip orientation
   - Highlight active chips

3. **Connection Visualization** (Day 3-4)
   - Draw traces between chips
   - Show signal flow
   - Animate bus activity

### Phase 2.6: 3D Visualization (Optional, 5-7 days)

1. **3D Rendering Infrastructure** (Day 1-2)
   - Set up 3D context
   - Implement camera system
   - Basic PCB in 3D

2. **Chip 3D Models** (Day 2-4)
   - Render DIP packages in 3D
   - Add pin rendering
   - Implement rotation

3. **Cable Visualization** (Day 4-5)
   - Render cables
   - Show cable routing
   - Animate signals

4. **Cabinet Integration** (Day 5-7)
   - Load cabinet 3D models
   - Position boards in cabinet
   - Render connectors

---

## Benefits

### For Emulation Accuracy
- ✅ Correct chip revisions for different boards
- ✅ Accurate timing (PAL vs NTSC hardware)
- ✅ Test compatibility requirements
- ✅ Behavior differences between chip versions

### For Visualization
- ✅ Realistic PCB view with chip placement
- ✅ Educational tool (understand hardware layout)
- ✅ Debugging aid (see signal flow on board)
- ✅ Board comparison (see evolution)

### For Code Quality
- ✅ Clean separation of concerns
- ✅ Massive code reduction (~1000+ lines removed)
- ✅ Zero hardcoded visual constants
- ✅ Easy to maintain and extend

### For Users
- ✅ Choose favorite system variant
- ✅ Customize appearance without recompiling
- ✅ Multiple visual themes
- ✅ Educational hardware visualization

---

## Conclusion

This unified hardware visualization and board layout architecture provides a complete solution for:

1. **Data-Driven Design** - All visual/physical data in TOML files
2. **Board-Level Accuracy** - Correct chip placement and orientation
3. **Multi-Board Support** - Handle complex peripherals and arcade systems
4. **3D Visualization** - Complete physical system representation
5. **User Customization** - Edit appearance without recompiling
6. **Clean Code** - Emulation logic separated from presentation
7. **Extensibility** - Works for any retro computer or arcade system

The architecture builds naturally on existing systems while providing a foundation for accurate visualization of complete retro computing systems, from simple single-board computers to complex multi-board arcade cabinets.

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-18  
**Author:** AI Assistant (Code Mode)  
**Review Status:** Ready for Review
