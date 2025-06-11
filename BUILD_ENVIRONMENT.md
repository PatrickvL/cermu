# Build Environment Configuration

## System Information
- **OS**: Windows
- **Shell**: PowerShell (default)
- **Date**: June 11, 2025

## Visual Studio Build Tools
- **Location**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`
- **MSBuild Path**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`
- **Version**: MSBuild version 17.14.10+8b8e13593 for .NET Framework

## Build Commands

### PowerShell Syntax (IMPORTANT!)
- Use `;` instead of `&&` for command chaining
- Use `if ($?) { command }` for conditional execution
- Use `& "path with spaces"` for executables with spaces in path

### Standard Build Command
```powershell
cd "c:\Workspaces\Mine\aiemu\code\c"; if ($?) { & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" aiemuc.sln /p:Configuration=Release }
```

### Project Structure
- **Solution File**: `c:\Workspaces\Mine\aiemu\code\c\aiemuc.sln`
- **Output Directory**: `c:\Workspaces\Mine\aiemu\code\c\bin\Release\`
- **Main Executables**:
  - `c64emu.exe` - Console version
  - `c64emu_gui.exe` - GUI version with Dear ImGui

### Build Targets
- `c64emu` - Console-based C64 emulator
- `c64emu_gui` - GUI C64 emulator with threading support
- `test_mos6510_basic` - CPU test executable
- `cimgui` - Dear ImGui C bindings library

## Dependencies
- **SDL2**: Located in `deps/sdl2/SDL2-2.30.10/`
- **Dear ImGui**: External cimgui bindings in `external/cimgui/`
- **OpenGL**: System OpenGL libraries

## Common Issues & Solutions

### 1. MSBuild Not Found
**Problem**: `'msbuild' is not recognized`
**Solution**: Use full path to MSBuild executable

### 2. PowerShell vs Unix Syntax
**Problem**: Using `&&` instead of `;`
**Solution**: Remember this is PowerShell, not bash/zsh

### 3. Compilation Warnings as Errors
**Common Issues**:
- C4244: Conversion from 'int' to 'uint8_t' - add explicit cast `(uint8_t)`
- Function parameter order mismatches - check function signatures
- Printf format specifiers - use `%llu` for `uint64_t` with `(unsigned long long)` cast

## Last Successful Build
- **Date**: June 11, 2025
- **Status**: ✅ All projects compiled successfully
- **Runtime Test**: ✅ GUI application starts and emulation thread works correctly

## Key Files Modified
- `src/gui/cimgui_interface.c` - Enhanced emulation thread with safety checks
- `src/systems/c64/c64.c` - Fixed function parameter order and type conversions
- `src/systems/c64/c64_bus.c` - Fixed type conversion warnings

## Notes
- Build environment is stable and working
- Emulation thread hanging issue has been resolved
- CPU execution now uses safe simulation mode when ROM not loaded
- GUI threading implementation is functional
