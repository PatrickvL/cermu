# Configurable System Architecture

**Date:** 2026-01-18  
**Purpose:** Design data-driven system configuration to enable users to create custom systems from hardware components  
**Builds On:** [`BOARD_LAYOUT_ARCHITECTURE.md`](BOARD_LAYOUT_ARCHITECTURE.md), [`MULTI_BOARD_ARCHITECTURE.md`](MULTI_BOARD_ARCHITECTURE.md)

---

## Executive Summary

This document defines an architecture that transforms system emulation from hardcoded implementations (like [`Apple1System`](../code/cpp/src/systems/apple1/apple1_system.h) or C64System) into a data-driven, configurable approach. Users can define complete systems through TOML configuration files that specify hardware components, memory maps, connections, and behaviors.

### Key Benefits

- **User Extensibility**: Users can create new systems without writing C++ code
- **Rapid Prototyping**: Test hardware configurations quickly
- **Educational Tool**: Learn computer architecture by assembling systems
- **Historical Accuracy**: Recreate obscure system variants and prototypes
- **No Code Duplication**: Existing systems become configuration files

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│         System Configuration (TOML)                │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Hardware   │  │    Memory    │  │  Connections  │  │
│  │ Components  │  │     Maps     │  │   & Buses     │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│           Component Registry & Factory                  │
│  CPU    RAM    ROM    VIC-II    SID    CIA    PIA ...  │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│          GenericSystem (Runtime Builder)                │
│  • Loads configuration                                  │
│  • Instantiates components                              │
│  • Connects buses                                       │
│  • Manages execution                                    │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│            Emulated System Instance                     │
└─────────────────────────────────────────────────────────┘
```

---

## 1. Component Registry

### Component Interface

All hardware components implement a common interface:

```cpp
/**
 * Base interface for all emulatable hardware components
 */
class IHardwareComponent {
public:
    virtual ~IHardwareComponent() = default;
    
    // Component identification
    virtual const char* get_type() const = 0;        // "CPU", "RAM", "VIC-II"
    virtual const char* get_model() const = 0;       // "MOS6502", "MOS6569"
    virtual const char* get_instance_id() const = 0; // "cpu", "ram", "vic"
    
    // Lifecycle
    virtual bool initialize(const ComponentConfig& config) = 0;
    virtual void reset() = 0;
    virtual void shutdown() = 0;
    
    // Execution
    virtual void tick() = 0;                        // Execute one cycle
    virtual uint32_t get_tick_rate() const = 0;    // Component speed (Hz)
    
    // Bus interface
    virtual bool supports_bus_access() const = 0;
    virtual uint8_t read(uint32_t address) = 0;
    virtual void write(uint32_t address, uint8_t value) = 0;
    
    // Address space
    virtual uint32_t get_address_start() const = 0;
    virtual uint32_t get_address_end() const = 0;
    virtual uint32_t get_address_size() const = 0;
    
    // Signals (IRQ, NMI, etc.)
    virtual bool has_signal(const char* signal_name) const = 0;
    virtual void set_signal(const char* signal_name, bool state) = 0;
    virtual bool get_signal(const char* signal_name) const = 0;
    
    // Chip layout (for visualization)
    virtual const ChipLayout* get_chip_layout() const = 0;
};
```

### Component Factory

```cpp
/**
 * Factory for creating hardware components from type strings
 */
class ComponentFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<IHardwareComponent>(const ComponentConfig&)>;
    
    /**
     * Register a component type
     */
    static void register_component(const char* type, const char* model, CreatorFunc creator);
    
    /**
     * Create a component instance
     */
    static std::unique_ptr<IHardwareComponent> create(
        const char* type, 
        const char* model,
        const ComponentConfig& config
    );
    
    /**
     * Get list of available components
     */
    static std::vector<ComponentInfo> get_available_components();
    
private:
    static std::map<std::string, CreatorFunc> registry_;
};

// Registration macro for convenience
#define REGISTER_COMPONENT(type, model, class_name) \
    namespace { \
        struct class_name##Registrar { \
            class_name##Registrar() { \
                ComponentFactory::register_component(type, model, \
                    [](const ComponentConfig& config) { \
                        return std::make_unique<class_name>(config); \
                    }); \
            } \
        }; \
        static class_name##Registrar class_name##_registrar_instance; \
    }
```

### Example Component Implementations

```cpp
/**
 * MOS6502 CPU wrapper
 */
class MOS6502Component : public IHardwareComponent {
private:
    mos6502_t* cpu_;
    std::string instance_id_;
    
public:
    const char* get_type() const override { return "CPU"; }
    const char* get_model() const override { return "MOS6502"; }
    
    bool initialize(const ComponentConfig& config) override {
        instance_id_ = config.get_string("id");
        cpu_ = mos6502_create();
        // Configure with memory callbacks from config
        return cpu_ != nullptr;
    }
    
    void tick() override {
        mos6502_tick(cpu_, 0);
    }
    
    uint32_t get_tick_rate() const override { return 1000000; } // 1 MHz
    
    // ... other methods
};

REGISTER_COMPONENT("CPU", "MOS6502", MOS6502Component)

/**
 * RAM component
 */
class RAMComponent : public IHardwareComponent {
private:
    std::vector<uint8_t> data_;
    uint32_t start_address_;
    uint32_t size_;
    std::string instance_id_;
    
public:
    const char* get_type() const override { return "RAM"; }
    const char* get_model() const override { return "GENERIC_RAM"; }
    
    bool initialize(const ComponentConfig& config) override {
        instance_id_ = config.get_string("id");
        start_address_ = config.get_uint("address");
        size_ = config.get_uint("size");
        data_.resize(size_, 0);
        return true;
    }
    
    uint8_t read(uint32_t address) override {
        uint32_t offset = address - start_address_;
        if (offset < size_) return data_[offset];
        return 0xFF;
    }
    
    void write(uint32_t address, uint8_t value) override {
        uint32_t offset = address - start_address_;
        if (offset < size_) data_[offset] = value;
    }
    
    uint32_t get_address_start() const override { return start_address_; }
    uint32_t get_address_end() const override { return start_address_ + size_ - 1; }
    
    // ... other methods
};

REGISTER_COMPONENT("RAM", "GENERIC", RAMComponent)
```

---

## 2. System Configuration Format (TOML)

### Complete System Configuration

```toml
{
  "system": {
    "name": "Apple 1",
    "short_name": "APPLE1",
    "description": "Apple Computer 1 (1976) - Woz's first computer",
    "version": "1.0",
    "author": "User Name"
  },
  
  "hardware_traits": {
    "display": {
      "native_width": 320,
      "native_height": 192,
      "format": "RGBA8888",
      "palette": [
        [0, 0, 0, 255],
        [51, 255, 51, 255]
      ]
    },
    "audio": {
      "format": "NONE",
      "sample_rate": 0,
      "channels": 0
    },
    "timing": {
      "cpu_frequency": 1000000,
      "target_fps": 60,
      "cycles_per_frame": 16667,
      "region": "NTSC"
    }
  },
  
  "components": [
    {
      "id": "cpu",
      "type": "CPU",
      "model": "MOS6502",
      "config": {
        "frequency": 1000000,
        "start_vector": "$FFFC"
      }
    },
    {
      "id": "ram",
      "type": "RAM",
      "model": "GENERIC",
      "config": {
        "address": "$0000",
        "size": 8192,
        "description": "8KB main RAM"
      }
    },
    {
      "id": "monitor_rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$FF00",
        "size": 256,
        "file": "data/apple1/roms/apple1.rom",
        "description": "Woz Monitor ROM"
      }
    },
    {
      "id": "pia",
      "type": "PIA",
      "model": "PIA6820",
      "config": {
        "address": "$D010",
        "size": 4,
        "port_a_direction": "INPUT",
        "port_b_direction": "OUTPUT"
      }
    },
    {
      "id": "terminal",
      "type": "DISPLAY",
      "model": "TEXT_TERMINAL",
      "config": {
        "columns": 40,
        "rows": 24,
        "char_width": 8,
        "char_height": 8,
        "font_rom": "data/apple1/roms/2513.rom"
      }
    }
  ],
  
  "memory_map": {
    "description": "Apple 1 memory layout",
    "regions": [
      {
        "name": "RAM",
        "address": "$0000",
        "size": 8192,
        "component": "ram",
        "writable": true
      },
      {
        "name": "PIA",
        "address": "$D010",
        "size": 4,
        "component": "pia",
        "writable": true
      },
      {
        "name": "Monitor ROM",
        "address": "$FF00",
        "size": 256,
        "component": "monitor_rom",
        "writable": false
      }
    ]
  },
  
  "connections": [
    {
      "from": "cpu",
      "to": "ram",
      "type": "bus",
      "signals": ["ADDRESS", "DATA", "RW"]
    },
    {
      "from": "cpu",
      "to": "pia",
      "type": "bus",
      "signals": ["ADDRESS", "DATA", "RW"]
    },
    {
      "from": "cpu",
      "to": "monitor_rom",
      "type": "bus",
      "signals": ["ADDRESS", "DATA"]
    },
    {
      "from": "pia",
      "to": "terminal",
      "type": "port",
      "signals": ["PORT_B"],
      "mapping": {
        "pia.port_b": "terminal.data_in"
      }
    }
  ],
  
  "board_layout": {
    "name": "Apple Computer 1 (Original)",
    "revision": "Rev 0",
    "manufacture_date": "1976-1977",
    "width_mm": 210,
    "height_mm": 270,
    "sockets": [
      {
        "socket_id": "A1",
        "component": "cpu",
        "x": 50,
        "y": 150,
        "orientation": "NORTH",
        "is_socketed": true
      },
      {
        "socket_id": "A2",
        "component": "pia",
        "x": 100,
        "y": 150,
        "orientation": "NORTH",
        "is_socketed": true
      }
    ]
  },
  
  "input": {
    "keyboard": {
      "type": "ASCII",
      "target_component": "pia",
      "target_port": "PORT_A",
      "mapping": "APPLE1_KEYBOARD"
    }
  },
  
  "file_support": {
    "extensions": [".bin", ".hex", ".txt"],
    "loaders": [
      {
        "extension": ".bin",
        "type": "BINARY",
        "load_address": "$0280"
      }
    ]
  }
}
```

### C64 System Configuration Example

```toml
{
  "system": {
    "name": "Commodore 64",
    "short_name": "C64",
    "description": "Commodore 64 (1982) - Best-selling computer of all time",
    "version": "1.0"
  },
  
  "components": [
    {
      "id": "cpu",
      "type": "CPU",
      "model": "MOS6510",
      "config": {
        "frequency": 1022727,
        "has_port": true
      }
    },
    {
      "id": "ram",
      "type": "RAM",
      "model": "GENERIC",
      "config": {
        "address": "$0000",
        "size": 65536
      }
    },
    {
      "id": "vic",
      "type": "VIC",
      "model": "MOS6569",
      "config": {
        "address": "$D000",
        "size": 1024,
        "revision": "R3",
        "pal": true
      }
    },
    {
      "id": "sid",
      "type": "SID",
      "model": "MOS6581",
      "config": {
        "address": "$D400",
        "size": 1024,
        "revision": "R2"
      }
    },
    {
      "id": "cia1",
      "type": "CIA",
      "model": "MOS6526",
      "config": {
        "address": "$DC00",
        "size": 256,
        "irq_output": "cpu.irq"
      }
    },
    {
      "id": "cia2",
      "type": "CIA",
      "model": "MOS6526",
      "config": {
        "address": "$DD00",
        "size": 256,
        "nmi_output": "cpu.nmi"
      }
    },
    {
      "id": "color_ram",
      "type": "RAM",
      "model": "MOS2114",
      "config": {
        "address": "$D800",
        "size": 1024,
        "nibble_mode": true
      }
    },
    {
      "id": "basic_rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$A000",
        "size": 8192,
        "file": "data/c64/roms/basic.rom"
      }
    },
    {
      "id": "kernal_rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$E000",
        "size": 8192,
        "file": "data/c64/roms/kernal.rom"
      }
    },
    {
      "id": "char_rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$D000",
        "size": 4096,
        "file": "data/c64/roms/characters.rom"
      }
    }
  ],
  
  "memory_map": {
    "type": "BANKED",
    "banking_component": "cpu",
    "banking_register": "port",
    "banks": [
      {
        "id": 0,
        "description": "All RAM",
        "config": "$37",
        "regions": [
          {"address": "$0000", "size": 65536, "component": "ram"}
        ]
      },
      {
        "id": 1,
        "description": "BASIC + KERNAL",
        "config": "$36",
        "regions": [
          {"address": "$0000", "size": 40960, "component": "ram"},
          {"address": "$A000", "size": 8192, "component": "basic_rom"},
          {"address": "$E000", "size": 8192, "component": "kernal_rom"}
        ]
      }
    ]
  },
  
  "board_layout": {
    "name": "C64 ASSY NO. 250425",
    "revision": "Rev A",
    "width_mm": 240,
    "height_mm": 200,
    "sockets": [
      {
        "socket_id": "U1",
        "component": "cpu",
        "x": 50,
        "y": 150,
        "orientation": "NORTH"
      },
      {
        "socket_id": "U19",
        "component": "vic",
        "x": 120,
        "y": 120,
        "orientation": "SOUTH"
      },
      {
        "socket_id": "U18",
        "component": "sid",
        "x": 120,
        "y": 150,
        "orientation": "NORTH"
      }
    ]
  }
}
```

---

## 3. GenericSystem Implementation

```cpp
/**
 * Generic configurable system that loads from TOML configuration
 */
class GenericSystem : public EmulatedSystem {
private:
    // Configuration
    SystemConfig config_;
    std::string config_path_;
    
    // Components
    std::vector<std::unique_ptr<IHardwareComponent>> components_;
    std::map<std::string, IHardwareComponent*> component_map_;
    
    // Memory management
    std::vector<MemoryRegion> memory_regions_;
    MemoryBankingController* banking_controller_;
    
    // Connections
    std::vector<ComponentConnection> connections_;
    
    // Board layout
    std::unique_ptr<BoardLayout> board_layout_;
    
public:
    GenericSystem(const char* config_path);
    ~GenericSystem() override;
    
    /**
     * Load system configuration from TOML file
     */
    bool load_configuration(const char* toml_path);
    
    /**
     * Build the system from loaded configuration
     */
    bool build_system();
    
    // EmulatedSystem interface implementation
    const SystemDescriptor& get_descriptor() const override;
    bool initialize() override;
    void reset() override;
    void tick() override;
    void run_frame() override;
    bool load_file(const char* filepath) override;
    
    // Component access
    IHardwareComponent* get_component(const char* id) const;
    
    // Memory routing
    uint8_t read_memory(uint32_t address);
    void write_memory(uint32_t address, uint8_t value);
    
private:
    /**
     * Create components from configuration
     */
    bool create_components();
    
    /**
     * Setup memory map
     */
    bool setup_memory_map();
    
    /**
     * Connect components together
     */
    bool connect_components();
    
    /**
     * Setup board layout
     */
    bool setup_board_layout();
};
```

### Implementation Details

```cpp
GenericSystem::GenericSystem(const char* config_path)
    : EmulatedSystem()
    , config_path_(config_path)
    , banking_controller_(nullptr)
{
}

bool GenericSystem::load_configuration(const char* toml_path) {
    // Load TOML file
    std::ifstream file(toml_path);
    if (!file.is_open()) {
        printf("GenericSystem: Failed to open config: %s\n", toml_path);
        return false;
    }
    
    // Parse TOML (using nlohmann/toml or similar)
    nlohmann::toml config_toml;
    file >> config_toml;
    
    // Parse system info
    config_.name = config_toml["system"]["name"];
    config_.short_name = config_toml["system"]["short_name"];
    config_.description = config_toml["system"]["description"];
    
    // Parse hardware traits
    parse_hardware_traits(config_toml["hardware_traits"]);
    
    // Parse components
    config_.components = config_toml["components"];
    
    // Parse memory map
    config_.memory_map = config_toml["memory_map"];
    
    // Parse connections
    config_.connections = config_toml["connections"];
    
    // Parse board layout
    if (config_toml.contains("board_layout")) {
        config_.board_layout = config_toml["board_layout"];
    }
    
    return true;
}

bool GenericSystem::build_system() {
    printf("GenericSystem: Building system '%s'\n", config_.name.c_str());
    
    // Create all components
    if (!create_components()) {
        printf("GenericSystem: Failed to create components\n");
        return false;
    }
    
    // Setup memory map
    if (!setup_memory_map()) {
        printf("GenericSystem: Failed to setup memory map\n");
        return false;
    }
    
    // Connect components
    if (!connect_components()) {
        printf("GenericSystem: Failed to connect components\n");
        return false;
    }
    
    // Setup board layout (optional)
    if (!config_.board_layout.empty()) {
        setup_board_layout();
    }
    
    printf("GenericSystem: System built successfully\n");
    return true;
}

bool GenericSystem::create_components() {
    for (const auto& comp_config : config_.components) {
        std::string id = comp_config["id"];
        std::string type = comp_config["type"];
        std::string model = comp_config["model"];
        
        printf("  Creating component: %s (%s/%s)\n", id.c_str(), type.c_str(), model.c_str());
        
        // Create component using factory
        auto component = ComponentFactory::create(
            type.c_str(),
            model.c_str(),
            comp_config["config"]
        );
        
        if (!component) {
            printf("  ERROR: Failed to create component %s\n", id.c_str());
            return false;
        }
        
        // Initialize component
        if (!component->initialize(comp_config["config"])) {
            printf("  ERROR: Failed to initialize component %s\n", id.c_str());
            return false;
        }
        
        // Store component
        component_map_[id] = component.get();
        components_.push_back(std::move(component));
    }
    
    return true;
}

void GenericSystem::tick() {
    // Tick all components (in dependency order if needed)
    for (auto& component : components_) {
        component->tick();
    }
    total_cycles_++;
}

uint8_t GenericSystem::read_memory(uint32_t address) {
    // Find memory region for this address
    for (const auto& region : memory_regions_) {
        if (address >= region.start && address <= region.end) {
            if (region.component) {
                return region.component->read(address);
            }
        }
    }
    return 0xFF; // Unmapped
}

void GenericSystem::write_memory(uint32_t address, uint8_t value) {
    // Find memory region for this address
    for (const auto& region : memory_regions_) {
        if (address >= region.start && address <= region.end) {
            if (region.component && region.writable) {
                region.component->write(address, value);
                return;
            }
        }
    }
}
```

---

## 4. Configuration Schema Specification

### TOML Schema for System Configuration

```toml
{
  "$schema": "http://toml-schema.org/draft-07/schema#",
  "title": "Emulated System Configuration",
  "type": "object",
  "required": ["system", "hardware_traits", "components", "memory_map"],
  "properties": {
    "system": {
      "type": "object",
      "required": ["name", "short_name", "description"],
      "properties": {
        "name": {"type": "string"},
        "short_name": {"type": "string"},
        "description": {"type": "string"},
        "version": {"type": "string"},
        "author": {"type": "string"}
      }
    },
    "hardware_traits": {
      "type": "object",
      "required": ["display", "audio", "timing"],
      "properties": {
        "display": {
          "type": "object",
          "properties": {
            "native_width": {"type": "integer"},
            "native_height": {"type": "integer"},
            "format": {"type": "string", "enum": ["RGBA8888", "RGB888", "PALETTE_INDEXED_8"]},
            "palette": {
              "type": "array",
              "items": {
                "type": "array",
                "items": {"type": "integer", "minimum": 0, "maximum": 255},
                "minItems": 4,
                "maxItems": 4
              }
            }
          }
        },
        "audio": {
          "type": "object",
          "properties": {
            "format": {"type": "string"},
            "sample_rate": {"type": "integer"},
            "channels": {"type": "integer"}
          }
        },
        "timing": {
          "type": "object",
          "properties": {
            "cpu_frequency": {"type": "integer"},
            "target_fps": {"type": "integer"},
            "cycles_per_frame": {"type": "integer"},
            "region": {"type": "string", "enum": ["NTSC", "PAL", "SECAM"]}
          }
        }
      }
    },
    "components": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["id", "type", "model", "config"],
        "properties": {
          "id": {"type": "string"},
          "type": {"type": "string"},
          "model": {"type": "string"},
          "config": {"type": "object"}
        }
      }
    },
    "memory_map": {
      "type": "object",
      "required": ["regions"],
      "properties": {
        "type": {"type": "string", "enum": ["SIMPLE", "BANKED"]},
        "description": {"type": "string"},
        "regions": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["name", "address", "size", "component"],
            "properties": {
              "name": {"type": "string"},
              "address": {"type": "string"},
              "size": {"type": "integer"},
              "component": {"type": "string"},
              "writable": {"type": "boolean"}
            }
          }
        }
      }
    },
    "connections": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["from", "to", "type"],
        "properties": {
          "from": {"type": "string"},
          "to": {"type": "string"},
          "type": {"type": "string", "enum": ["bus", "port", "signal"]},
          "signals": {"type": "array", "items": {"type": "string"}},
          "mapping": {"type": "object"}
        }
      }
    },
    "board_layout": {
      "type": "object",
      "properties": {
        "name": {"type": "string"},
        "revision": {"type": "string"},
        "width_mm": {"type": "number"},
        "height_mm": {"type": "number"},
        "sockets": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "socket_id": {"type": "string"},
              "component": {"type": "string"},
              "x": {"type": "number"},
              "y": {"type": "number"},
              "orientation": {"type": "string", "enum": ["NORTH", "EAST", "SOUTH", "WEST"]},
              "is_socketed": {"type": "boolean"}
            }
          }
        }
      }
    }
  }
}
```

---

## 5. Migration Path for Existing Systems

### Phase 1: Parallel Implementation (No Breaking Changes)

1. Keep existing hardcoded system classes ([`Apple1System`](../code/cpp/src/systems/apple1/apple1_system.cpp), C64System, etc.)
2. Add GenericSystem alongside them
3. Create TOML configurations for existing systems
4. Test that GenericSystem produces identical behavior

### Phase 2: Optional Migration

1. Users can choose to use either:
   - Hardcoded system (for maximum performance)
   - TOML-configured system (for flexibility)
2. Both approaches coexist
3. Add UI option to switch between modes

### Phase 3: Full Data-Driven (Optional Future)

1. Move all systems to TOML configuration
2. Remove hardcoded system classes
3. Keep only GenericSystem

---

## 6. Benefits & Use Cases

### For Users

**Create Custom Systems:**
```toml
{
  "system": {
    "name": "My Custom 6502 Computer",
    "description": "Homebrew system with custom peripherals"
  },
  "components": [
    {"id": "cpu", "type": "CPU", "model": "MOS6502"},
    {"id": "ram", "type": "RAM", "size": 32768},
    {"id": "my_device", "type": "CUSTOM_IO", "model": "MyDevice"}
  ]
}
```

**Test Hardware Configurations:**
- Try different RAM sizes
- Swap chip revisions
- Add/remove peripherals
- Test compatibility

**Recreate Historical Variants:**
- Apple II clones
- C64 board revisions
- Prototype systems
- Rare configurations

### For Developers

- **Rapid Prototyping**: Test new chip combinations quickly
- **Educational**: Teach computer architecture concepts
- **Debugging**: Isolate component behaviors
- **Documentation**: Configuration files serve as documentation

---

## 7. Implementation Roadmap

### Phase 1: Foundation (5-7 days)

1. **Component Interface** (Day 1-2)
   - Define [`IHardwareComponent`](../code/cpp/src/core/hardware_component.h) interface
   - Create base implementations for common types
   - Implement component factory

2. **Configuration Parser** (Day 2-3)
   - TOML parsing infrastructure
   - Schema validation
   - Configuration structures

3. **GenericSystem Core** (Day 3-5)
   - Basic system builder
   - Component instantiation
   - Memory routing

4. **Testing** (Day 5-7)
   - Create Apple1 TOML configuration
   - Verify identical behavior to hardcoded version
   - Test configuration variations

### Phase 2: Component Library (7-10 days)

1. **CPU Components** (Day 1-2)
   - MOS6502
   - MOS6510
   - Z80

2. **Memory Components** (Day 2-3)
   - Generic RAM
   - Generic ROM
   - Banked memory

3. **I/O Components** (Day 3-5)
   - PIA6820
   - CIA6526
   - VIA6522

4. **Video Components** (Day 5-8)
   - VIC-II
   - Text terminal
   - Character ROM

5. **Audio Components** (Day 8-10)
   - SID 6581
   - Generic PSG

### Phase 3: Advanced Features (5-7 days)

1. **Memory Banking** (Day 1-2)
   - Banking controller
   - C64-style PLA emulation

2. **Signal Routing** (Day 2-3)
   - IRQ/NMI connections
   - Clock distribution
   - Reset lines

3. **Board Layout** (Day 3-5)
   - Board definition parsing
   - Socket placement
   - Visual integration

4. **File Loaders** (Day 5-7)
   - Configurable file loaders
   - Format detection
   - Auto-loading

---

## 8. Example: Building Apple 1 from Components

### Step-by-Step Process

1. **User creates** [`apple1.toml`](../data/systems/apple1.toml)
2. **System loads configuration** and validates schema
3. **ComponentFactory creates components:**
   - MOS6502 CPU @ 1MHz
   - 8KB RAM at $0000
   - 256B ROM at $FF00
   - PIA6820 at $D010
   - TextTerminal display
4. **Memory mapper** connects components to address space
5. **Signal router** connects PIA Port B to terminal input
6. **System runs** identically to hardcoded [`Apple1System`](../code/cpp/src/systems/apple1/apple1_system.cpp)

### Configuration Variations

Users can then create variants:
- `apple1_4k.toml` - 4KB RAM version
- `apple1_basic.toml` - With BASIC ROM
- `apple1_custom.toml` - Custom peripherals

---

## 9. Advanced Use Cases

### Hybrid Systems

Combine chips from different eras:
```toml
{
  "system": {
    "name": "Franken-Computer",
    "description": "Z80 + VIC-II + SID"
  },
  "components": [
    {"id": "cpu", "type": "CPU", "model": "Z80"},
    {"id": "video", "type": "VIC", "model": "MOS6569"},
    {"id": "audio", "type": "SID", "model": "MOS6581"}
  ]
}
```

### Multi-Board Systems

```toml
{
  "system": {
    "name": "C64 + 1541 Drive"
  },
  "boards": [
    {
      "id": "c64_board",
      "config": "configs/c64.toml"
    },
    {
      "id": "drive_board",
      "config": "configs/1541.toml"
    }
  ],
  "connections": [
    {
      "from": "c64_board.serial_port",
      "to": "drive_board.serial_port",
      "type": "cable"
    }
  ]
}
```

### Educational Scenarios

Progressive complexity:
1. `minimal_6502.toml` - Just CPU + RAM
2. `basic_computer.toml` - Add ROM + I/O
3. `simple_graphics.toml` - Add character display
4. `full_system.toml` - Complete retro computer

---

## Conclusion

The configurable system architecture provides:

1. **User Empowerment** - Create systems without coding
2. **Flexibility** - Mix and match components freely
3. **Education** - Learn by building systems
4. **Preservation** - Document rare configurations
5. **No Breaking Changes** - Coexists with hardcoded systems

This approach transforms the emulator from a fixed set of systems into a flexible platform for exploring computer architecture and creating custom retro computing experiences.

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-18  
**Author:** AI Assistant (Code Mode)  
**Review Status:** Ready for Implementation
