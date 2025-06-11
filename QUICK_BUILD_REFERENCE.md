# Build Best Practices for Future Sessions

## ✅ PROVEN BUILD COMMAND
This command has been tested and works reliably:

```powershell
cd "c:\Workspaces\Mine\aiemu\code\c"; if ($?) { & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" aiemuc.sln /p:Configuration=Release }
```

## 🔧 KEY ENVIRONMENT DETAILS

### Build Tools Location
- **MSBuild**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`
- **Version**: MSBuild 17.14.10+8b8e13593 for .NET Framework

### Project Paths
- **Solution**: `c:\Workspaces\Mine\aiemu\code\c\aiemuc.sln`
- **Output**: `c:\Workspaces\Mine\aiemu\code\c\bin\Release\`

### Critical PowerShell Syntax
- ✅ Use `;` for command chaining (NOT `&&`)
- ✅ Use `if ($?)` for conditional execution
- ✅ Use `& "path"` for executables with spaces

## 🎯 EXECUTABLES PRODUCED
- `c64emu.exe` - Console C64 emulator
- `c64emu_gui.exe` - GUI C64 emulator (MAIN TARGET)
- `test_mos6510_basic.exe` - CPU tests

## 🚀 QUICK BUILD COMMAND (Copy-Paste Ready)
```powershell
# Full build from scratch
cd "c:\Workspaces\Mine\aiemu\code\c"; if ($?) { & "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" aiemuc.sln /p:Configuration=Release /verbosity:minimal }

# Run GUI after build
cd "c:\Workspaces\Mine\aiemu\code\c\bin\Release"; if (Test-Path "c64emu_gui.exe") { .\c64emu_gui.exe } else { Write-Host "Build failed - executable not found" }
```

## 🔍 VERIFICATION COMMANDS
```powershell
# Check if MSBuild exists
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"

# List built executables
Get-ChildItem "c:\Workspaces\Mine\aiemu\code\c\bin\Release\*.exe"

# Check PowerShell execution policy
Get-ExecutionPolicy
```

## ⚠️ COMMON ISSUES & SOLUTIONS

### Issue: "msbuild is not recognized"
**Solution**: Always use full path to MSBuild

### Issue: Build errors with C4244 warnings
**Solution**: Add explicit type casts like `(uint8_t)i`

### Issue: Function parameter mismatches
**Solution**: Check function signatures - parameters may be in wrong order

### Issue: PowerShell script execution
**Solution**: Use direct commands instead of script files for reliability

## 📋 PRE-BUILD CHECKLIST
1. ✅ Verify MSBuild path exists
2. ✅ Use PowerShell (not cmd or bash)
3. ✅ Use correct PowerShell syntax (`;` not `&&`)
4. ✅ Build from correct directory
5. ✅ Check for compilation errors

## 💡 NOTES FOR FUTURE ME
- The emulation thread hanging issue has been RESOLVED
- CPU execution now safely handles missing ROM files
- Build environment is stable and working
- GUI application starts correctly and emulation controls work
- Focus on ROM loading functionality for next development phase
