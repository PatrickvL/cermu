# AIEMU-C

C implementation of the MOS 6510 CPU emulator.

## Building

### Option 1: Using CMake (Recommended)

```bash
mkdir build
cd build
cmake ..
make
./bin/aiemuc
```

### Option 2: Using Make

```bash
make debug      # Debug build
make release    # Release build
make run        # Build and run
make clean      # Clean build artifacts
```

### Option 3: Direct compilation

```bash
gcc main.c -o aiemuc
./aiemuc
```

## Running

```bash
./aiemuc [arguments...]
```

## Features (Planned)

- Cycle-accurate MOS 6510 CPU emulation
- All 256 opcodes (official + unofficial)
- C64/C128 compatibility
- Performance optimized for embedded use

## Directory Structure

- `main.c` - Entry point
- `CMakeLists.txt` - CMake build configuration
- `Makefile` - Simple Make build configuration
- `build/` - CMake build output (created during build)
- `README.md` - This file
