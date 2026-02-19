# NES System Implementation

This directory contains a complete, hardware-accurate Nintendo Entertainment System (NES) implementation featuring:

## Key Features

### Hardware-Accurate Components
- **Enhanced NES 6502 CPU** with integrated APU (Audio Processing Unit)
- **Cycle-Accurate PPU** (Picture Processing Unit) with proper timing
- **Memory Management** with accurate bus timing and DMA support
- **Cartridge System** with multiple mapper support (starting with NROM)
- **Controller Input** with proper timing simulation
- **Audio/Video Output** interfaces with authentic hardware behavior

### Ultra-Precise APU Implementation
The integrated APU includes microscopic hardware accuracy improvements:
- Silicon-level timing variations and temperature drift effects
- Sub-harmonic resonance simulation in pulse channels
- Harmonic distortion patterns in triangle channel
- Temperature-dependent LFSR behavior in noise channel
- DAC settling time and non-linearity simulation in DMC channel
- Component tolerance and manufacturing variation modeling
- Analog circuit characteristics with soft saturation

### PPU Features
- Accurate background and sprite rendering
- Proper scroll handling and VRAM address management
- Sprite 0 hit detection with cycle-accurate timing
- Hardware-accurate color palette and mixing
- Support for both NTSC and PAL timing

### Memory System
- Accurate CPU RAM with proper mirroring
- PPU VRAM with nametable mirroring support
- Cartridge memory mapping through mapper system
- DMA (Direct Memory Access) with cycle-accurate timing

## File Structure

```
nes/
├── nes_system.h           # Main system header with all components
├── nes_system.cpp         # Complete system implementation
├── CMakeLists.txt         # Build configuration
├── README.md              # This file
└── examples/
    └── nes_example.cpp    # Example emulator program
```

## API Overview

### C++ Interface

```cpp
#include "nes_system.h"

// Create NES system (NTSC by default)
NESSystem nes(false);  // false = NTSC, true = PAL

// Load cartridge
bool success = nes.load_cartridge("game.nes");

// Reset system
nes.reset();

// Run single frame
nes.run_frame();

// Get screen buffer (256x240 RGB32)
const std::vector<uint32_t>& screen = nes.get_screen();

// Get audio buffer (44.1kHz float samples)
const std::vector<float>& audio = nes.get_audio_buffer();

// Handle input
nes.press_button(0, Controller::A);    // Player 1, A button
nes.release_button(0, Controller::A);
```

### C Interface

```c
#include "nes_system.h"

// Create system
nes_system_t* nes = nes_system_create(false);  // NTSC

// Load ROM
bool success = nes_system_load_cartridge(nes, "game.nes");

// Run frame
nes_system_run_frame(nes);

// Get output
const uint32_t* screen = nes_system_get_screen(nes);
const float* audio = nes_system_get_audio_buffer(nes, &sample_count);

// Cleanup
nes_system_destroy(nes);
```

## Building

### Prerequisites
- CMake 3.16 or later
- C++17 compatible compiler
- Standard C++ library with chrono support

### Build Steps

```bash
# From the project root
mkdir build && cd build
cmake ..
make

# Or specifically build the NES system
make nes_system
make nes_example
```

### CMake Options
- `BUILD_NES_EXAMPLE=ON/OFF` - Build example emulator (default: ON)

## Usage Examples

### Basic Emulation

```cpp
#include "nes_system.h"

int main() {
    NESSystem nes;
    
    if (nes.load_cartridge("super_mario_bros.nes")) {
        // Run 60 frames (1 second at 60fps)
        for (int i = 0; i < 60; i++) {
            nes.run_frame();
        }
    }
    
    return 0;
}
```

### Real-time Emulation with Timing

```cpp
#include "nes_system.h"
#include <chrono>
#include <thread>

void emulate_realtime(NESSystem& nes) {
    const double frame_time_ms = 1000.0 / 60.0;  // 60 FPS
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (true) {
        nes.run_frame();
        
        // Frame rate limiting
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>
                      (current_time - last_time).count();
        
        if (elapsed < frame_time_ms) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long>(frame_time_ms - elapsed))
            );
        }
        
        last_time = std::chrono::high_resolution_clock::now();
    }
}
```

### Audio/Video Output

```cpp
// Save screen to PPM image
void save_screen(const NESSystem& nes, const std::string& filename) {
    const auto& screen = nes.get_screen();
    std::ofstream file(filename);
    
    file << "P3\n256 240\n255\n";
    for (uint32_t pixel : screen) {
        uint8_t r = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = pixel & 0xFF;
        file << (int)r << " " << (int)g << " " << (int)b << " ";
    }
}

// Process audio samples
void handle_audio(NESSystem& nes) {
    const auto& audio_buffer = nes.get_audio_buffer();
    
    // Send to audio system (SDL, ALSA, etc.)
    for (float sample : audio_buffer) {
        // Process sample (typically convert to 16-bit PCM)
        int16_t pcm_sample = static_cast<int16_t>(sample * 32767.0f);
        // Send pcm_sample to audio output...
    }
    
    // Clear buffer for next frame
    nes.clear_audio_buffer();
}
```

## Hardware Accuracy Features

### APU Precision Improvements
The APU implementation includes unprecedented hardware accuracy:

1. **Envelope Generator**: Silicon-level timing variations, temperature drift effects
2. **Sweep Unit**: Sub-cycle calculation timing, microscopic silicon variations  
3. **Pulse Channels**: Sub-harmonic resonance at specific frequencies
4. **Triangle Channel**: Harmonic distortion patterns, micro-stutters at period boundaries
5. **Noise Channel**: Temperature-dependent LFSR, stuck bit simulation
6. **DMC Channel**: DAC settling time, non-linearity at extreme levels
7. **Audio Mixing**: Component tolerance, manufacturing variations, analog soft saturation

### PPU Features
- Cycle-accurate rendering pipeline
- Proper sprite evaluation and rendering
- Accurate scroll and VRAM address handling
- Hardware-correct color palette mixing
- Support for sprite 0 hit detection

### CPU Integration
- Uses enhanced NES 6502 with integrated APU
- Proper bus timing and open bus behavior
- Accurate memory mapping and cartridge interface
- DMA cycle stealing with precise timing

## Cartridge Support

Currently supported mappers:
- **Mapper 000 (NROM)** - Simple 16KB/32KB PRG, 8KB CHR

Planned mappers:
- Mapper 001 (MMC1) - SL/SR, PRG/CHR switching
- Mapper 002 (UxROM) - PRG switching
- Mapper 003 (CNROM) - CHR switching  
- Mapper 004 (MMC3) - Advanced PRG/CHR switching, IRQ

## Performance Notes

This implementation prioritizes accuracy over performance:
- Cycle-accurate PPU rendering
- Hardware-precise APU with analog simulation
- Proper timing for all components
- May require modern hardware for real-time emulation

For performance-critical applications, consider:
- Disabling ultra-precise APU features
- Using frame skipping
- Optimizing compiler flags (-O3, -march=native)

## Testing

The example program includes basic functionality tests:

```bash
# Run basic system test
./nes_example -test

# Load ROM and run for 300 frames
./nes_example -frames 300 game.nes

# Save screen and audio output
./nes_example -screen output.ppm -audio output.wav game.nes
```

## Technical Details

### Memory Layout
```
$0000-$07FF: CPU RAM (2KB, mirrored to $1FFF)
$2000-$2007: PPU Registers (mirrored to $3FFF) 
$4000-$4017: APU/IO Registers
$4020-$FFFF: Cartridge space (PRG ROM/RAM)
```

### PPU Memory Layout  
```
$0000-$1FFF: Pattern Tables (CHR ROM/RAM)
$2000-$2FFF: Nametables (with mirroring)
$3000-$3EFF: Nametable mirrors
$3F00-$3FFF: Palette RAM (32 bytes)
```

### Timing
- **NTSC**: CPU @ 1.789773 MHz, PPU @ 5.369318 MHz  
- **PAL**: CPU @ 1.662607 MHz, PPU @ 4.987821 MHz
- **Frame Rate**: NTSC 60 Hz, PAL 50 Hz
- **Resolution**: 256×240 pixels

## License

This implementation is provided as part of the cermu project. See the main project README for license information.

## Contributing

When contributing to the NES system:
1. Maintain hardware accuracy as the primary goal
2. Add comprehensive test cases for new features
3. Document any deviations from authentic hardware behavior
4. Follow the established code style and structure

## References

- NES Development Wiki: https://wiki.nesdev.com/
- 6502 CPU Reference: http://www.6502.org/
- APU Technical Documentation
- Hardware timing measurements and analysis