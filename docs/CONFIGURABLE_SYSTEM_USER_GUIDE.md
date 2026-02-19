# Configurable System Architecture - User Guide

**Create your own retro computer systems without writing code!**

This guide shows you how to create custom emulated computer systems using TOML configuration files. You can mix and match hardware components, create variants of existing systems, or design completely new systems.

---

## Quick Start

### 1. Choose a Template

Start with an existing system configuration:

- **Apple 1**: [`data/systems/apple1.toml`](../data/systems/apple1.toml) - Simple 6502-based system
- **Commodore 64**: [`data/systems/c64.toml`](../data/systems/c64.toml) - Complex system with banking

### 2. Create Your Configuration

Copy a template and modify it:

```bash
cp data/systems/apple1.toml data/systems/my_system.toml
```

Edit `my_system.toml` to customize your system.

### 3. Load Your System

```bash
./multi_emu_console --config data/systems/my_system.toml
```

---

## Configuration Structure

A system configuration consists of these main sections:

```
┌─────────────────────────────────────────┐
│  System Information                     │
│  • Name, description, metadata          │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  Hardware Traits                        │
│  • Display settings (resolution, colors)│
│  • Audio settings                       │
│  • Timing (CPU speed, FPS)              │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  Components                             │
│  • CPU, RAM, ROM, I/O chips             │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  Memory Map                             │
│  • Address space layout                 │
│  • Banking (optional)                   │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  Connections                            │
│  • How components talk to each other    │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│  Board Layout (optional)                │
│  • Physical PCB appearance              │
│  • Chip placement                       │
└─────────────────────────────────────────┘
```

---

## Example 1: Simple 6502 Computer

Let's build a minimal 6502-based computer:

```toml
{
  "system": {
    "name": "Simple 6502",
    "short_name": "SIMPLE6502",
    "description": "Minimal 6502 computer with 32KB RAM",
    "version": "1.0"
  },
  
  "hardware_traits": {
    "display": {
      "native_width": 640,
      "native_height": 480,
      "format": "RGBA8888"
    },
    "audio": {
      "format": "NONE"
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
        "frequency": 1000000
      }
    },
    {
      "id": "ram",
      "type": "RAM",
      "model": "GENERIC",
      "config": {
        "address": "$0000",
        "size": 32768
      }
    },
    {
      "id": "rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$8000",
        "size": 32768,
        "file": "data/my_system/monitor.rom"
      }
    }
  ],
  
  "memory_map": {
    "type": "SIMPLE",
    "regions": [
      {"name": "RAM", "address": "$0000", "size": 32768, "component": "ram", "writable": true},
      {"name": "ROM", "address": "$8000", "size": 32768, "component": "rom", "writable": false}
    ]
  }
}
```

---

## Example 2: Apple 1 Variant with More RAM

Customize an existing system (Apple 1 with 64KB instead of 8KB):

```toml
{
  "$schema": "../docs/system_config_schema.toml",
  "system": {
    "name": "Apple 1 Enhanced",
    "short_name": "APPLE1_64K",
    "description": "Apple 1 with 64KB RAM upgrade"
  },
  
  "components": [
    {
      "id": "cpu",
      "type": "CPU",
      "model": "MOS6502",
      "config": {"frequency": 1000000}
    },
    {
      "id": "ram",
      "type": "RAM",
      "model": "GENERIC",
      "description": "64KB RAM upgrade!",
      "config": {
        "address": "$0000",
        "size": 65536
      }
    },
    {
      "id": "monitor_rom",
      "type": "ROM",
      "model": "GENERIC",
      "config": {
        "address": "$FF00",
        "size": 256,
        "file": "data/apple1/roms/apple1.rom"
      }
    }
  ],
  
  "memory_map": {
    "type": "SIMPLE",
    "regions": [
      {"name": "RAM", "address": "$0000", "size": 65280, "component": "ram", "writable": true},
      {"name": "ROM", "address": "$FF00", "size": 256, "component": "monitor_rom", "writable": false}
    ]
  }
}
```

---

## Available Components

### CPUs
- **MOS6502** - Original 6502 (Apple II, Atari, NES)
- **MOS6510** - 6502 with I/O port (Commodore 64)
- **Z80** - Zilog Z80 (ZX Spectrum, Game Boy)

### Memory
- **GENERIC RAM** - Simple RAM array
- **GENERIC ROM** - ROM loaded from file
- **MOS2114** - 1KB color RAM (C64-style nibble mode)

### I/O Chips
- **PIA6820** - Peripheral Interface Adapter (Apple 1, Atari)
- **CIA6526** - Complex Interface Adapter (Commodore 64)
- **VIA6522** - Versatile Interface Adapter (VIC-20, BBC Micro)

### Video Chips
- **MOS6569** - VIC-II PAL (Commodore 64)
- **MOS6567** - VIC-II NTSC (Commodore 64)
- **TEXT_TERMINAL** - Simple character display

### Audio Chips
- **MOS6581** - SID sound chip (Commodore 64)

---

## Configuration Fields Explained

### System Information

```toml
"system": {
  "name": "My Computer",           // Full name displayed in UI
  "short_name": "MYCOMP",          // Identifier (uppercase, no spaces)
  "description": "...",             // Brief description
  "version": "1.0",                 // Config version
  "author": "Your Name"             // Who created this config
}
```

### Hardware Traits

**Display:**
```toml
"display": {
  "native_width": 320,              // Screen width in pixels
  "native_height": 200,             // Screen height in pixels
  "format": "RGBA8888",             // Color format
  "palette": [                       // Color palette (if indexed)
    [0, 0, 0, 255],                 // Color 0: Black
    [255, 255, 255, 255]            // Color 1: White
  ]
}
```

**Timing:**
```toml
"timing": {
  "cpu_frequency": 1000000,         // CPU Hz (1MHz = 1000000)
  "target_fps": 60,                 // Frames per second
  "cycles_per_frame": 16667,        // CPU cycles per frame
  "region": "NTSC"                  // Video standard
}
```

### Components

Each component needs:
- **id**: Unique identifier (lowercase, no spaces)
- **type**: Component category (CPU, RAM, ROM, etc.)
- **model**: Specific chip model
- **config**: Component-specific settings

```toml
{
  "id": "cpu",                      // How to reference this component
  "type": "CPU",                    // Component category
  "model": "MOS6502",               // Specific chip
  "description": "Main CPU",        // Optional description
  "config": {                        // Component-specific config
    "frequency": 1000000,
    "address": "$0000"              // $ prefix for hex
  }
}
```

### Memory Map

**Simple (flat address space):**
```toml
"memory_map": {
  "type": "SIMPLE",
  "regions": [
    {
      "name": "RAM",
      "address": "$0000",           // Start address
      "size": 32768,                // Size in bytes
      "component": "ram",           // Which component
      "writable": true              // Read/write
    }
  ]
}
```

**Banked (switchable memory):**
```toml
"memory_map": {
  "type": "BANKED",
  "banking_component": "cpu",       // What controls banking
  "banking_register": "port",       // Which register
  "banks": [
    {
      "id": 0,
      "description": "All RAM",
      "config": "$30",              // Banking register value
      "regions": [...]              // Memory layout for this bank
    }
  ]
}
```

### Connections

Define how components communicate:

```toml
"connections": [
  {
    "from": "cpu",                  // Source component
    "to": "ram",                    // Destination component
    "type": "bus",                  // Connection type
    "signals": ["ADDRESS", "DATA", "RW"]  // Signals on connection
  }
]
```

**Connection types:**
- **bus**: Address/data bus (memory access)
- **port**: Parallel port connection
- **signal**: Single signal line (IRQ, NMI, etc.)
- **cable**: Physical cable between boards

---

## Creating Custom Systems

### Step 1: Define Your System

Decide on:
- **CPU**: Which processor?
- **Memory**: How much RAM and ROM?
- **I/O**: Keyboard, display, sound?
- **Speed**: CPU frequency, frame rate?

### Step 2: Add Components

Add each hardware chip as a component:

```toml
"components": [
  {"id": "cpu", "type": "CPU", "model": "MOS6502", ...},
  {"id": "ram", "type": "RAM", "model": "GENERIC", ...},
  {"id": "rom", "type": "ROM", "model": "GENERIC", ...}
]
```

### Step 3: Map Memory

Define where each component appears in memory:

```toml
"memory_map": {
  "regions": [
    {"name": "RAM", "address": "$0000", "size": 8192, "component": "ram"},
    {"name": "ROM", "address": "$F000", "size": 4096, "component": "rom"}
  ]
}
```

### Step 4: Connect Components

Wire components together:

```toml
"connections": [
  {"from": "cpu", "to": "ram", "type": "bus"},
  {"from": "cpu", "to": "rom", "type": "bus"}
]
```

### Step 5: Test

Load and test your configuration:

```bash
./multi_emu_console --config data/systems/my_system.toml
```

---

## Common Patterns

### Pattern 1: Add More RAM

```toml
"components": [
  {
    "id": "ram",
    "type": "RAM",
    "model": "GENERIC",
    "config": {
      "address": "$0000",
      "size": 65536  // Change this!
    }
  }
]
```

### Pattern 2: Swap CPU

```toml
"components": [
  {
    "id": "cpu",
    "type": "CPU",
    "model": "Z80",  // Change from MOS6502 to Z80
    "config": {
      "frequency": 4000000  // 4 MHz
    }
  }
]
```

### Pattern 3: Add Display

```toml
"components": [
  {
    "id": "terminal",
    "type": "DISPLAY",
    "model": "TEXT_TERMINAL",
    "config": {
      "columns": 80,
      "rows": 25,
      "char_width": 8,
      "char_height": 16
    }
  }
]
```

### Pattern 4: Load ROMs

```toml
"components": [
  {
    "id": "rom",
    "type": "ROM",
    "model": "GENERIC",
    "config": {
      "address": "$E000",
      "size": 8192,
      "file": "path/to/rom.bin"  // Specify your ROM file
    }
  }
]
```

---

## Troubleshooting

### "Component not found"

**Problem**: Component type/model doesn't exist

**Solution**: Check available components list above, ensure type and model names are correct

### "Memory overlap"

**Problem**: Two components overlap in address space

**Solution**: Check memory map addresses, ensure regions don't conflict

### "ROM file not found"

**Problem**: ROM file path is incorrect

**Solution**: Use paths relative to emulator executable, check file exists

### "Invalid TOML"

**Problem**: Syntax error in TOML file

**Solution**: Validate TOML at https://tomllint.com/ or use schema validation

---

## Advanced Features

### Board Visualization

Add physical board layout for visual representation:

```toml
"board_layout": {
  "name": "My Custom Board",
  "width_mm": 200,
  "height_mm": 150,
  "sockets": [
    {
      "socket_id": "U1",
      "component": "cpu",
      "x": 50,
      "y": 75,
      "orientation": "NORTH",
      "is_socketed": true
    }
  ]
}
```

### Multi-Board Systems

Create systems with multiple boards (coming soon):

```toml
"boards": [
  {"id": "main", "config": "main_board.toml"},
  {"id": "expansion", "config": "expansion.toml"}
]
```

---

## Examples in the Wild

Check [`data/systems/`](../data/systems/) for complete examples:

- **apple1.toml** - Simple system with PIA and text terminal
- **c64.toml** - Complex system with memory banking and multiple chips

---

## Schema Validation

Validate your configuration against the schema:

```bash
# Install tomlschema validator
pip install tomlschema

# Validate your config
tomlschema -i data/systems/my_system.toml docs/system_config_schema.toml
```

The schema is at [`docs/system_config_schema.toml`](system_config_schema.toml)

---

## Best Practices

1. **Start Simple**: Begin with a minimal system and add complexity gradually
2. **Use Templates**: Copy existing configs rather than starting from scratch
3. **Test Incrementally**: Test after each major change
4. **Document**: Add descriptions to components and regions
5. **Validate**: Use schema validation to catch errors early
6. **Version**: Use semantic versioning for your configs (1.0, 1.1, 2.0)

---

## Next Steps

- Review the [Architecture Documentation](CONFIGURABLE_SYSTEM_ARCHITECTURE.md) for technical details
- Check [available chip components](../code/cpp/src/chip/) for more options
- Join the community to share your custom systems
- Contribute new component types

---

## FAQ

**Q: Can I create a system that never existed in real life?**  
A: Yes! Mix chips from different eras, create hybrid systems, experiment freely.

**Q: Do I need to know C++ to create a system?**  
A: No! Just edit TOML files. C++ knowledge is only needed to add new component types.

**Q: Can I contribute my configuration?**  
A: Yes! Submit a pull request with your config in `data/systems/`

**Q: How do I add a new chip that's not in the component list?**  
A: That requires C++ implementation. See the developer documentation.

**Q: Can I create multi-board systems like a C64 + disk drive?**  
A: Not yet, but it's planned! Check the roadmap in the architecture docs.

---

**Happy system building!** 🎮

For questions or help, open an issue on GitHub or join the community discussion.
