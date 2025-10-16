# PowerShell build script for fam65xx processor tests runner
# This script sets up the Visual Studio environment and builds with MSVC

param(
    [string]$Configuration = "Release"
)

Write-Host "Building fam65xx processor tests runner (Parallel Edition) with MSVC..." -ForegroundColor Green
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Cyan

# Check if required files exist
$requiredFiles = @(
    "fam65xx_processor_tests_runner.cpp",
    "json_parser.c", 
    "json_parser.h",
    "../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx_callbacks.h"
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path $file)) {
        Write-Error "ERROR: Required file not found: $file"
        exit 1
    }
}

# Try to initialize Visual Studio environment
$vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path $vsPath)) {
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"
}

if (Test-Path $vsPath) {
    Write-Host "Initializing Visual Studio environment..." -ForegroundColor Yellow
    & $vsPath -Arch amd64 -SkipAutomaticLocation
} else {
    Write-Error "Visual Studio 2022 not found. Please install Visual Studio 2022 with C++ support."
    exit 1
}

# Set compiler flags based on configuration
$commonFlags = "/std:c++17", "/EHsc", "/I../src", "/I.", "/DWIN32", "/D_CONSOLE"
$targetName = "fam65xx_processor_tests_runner_parallel.exe"

if ($Configuration -eq "Debug") {
    $compilerFlags = $commonFlags + @("/Od", "/Zi", "/DDEBUG", "/MDd")
    Write-Host "Building DEBUG version..." -ForegroundColor Yellow
} else {
    $compilerFlags = $commonFlags + @("/O2", "/DNDEBUG", "/MD")
    Write-Host "Building RELEASE version (optimized)..." -ForegroundColor Yellow
}

# Clean previous build
if (Test-Path $targetName) {
    Remove-Item $targetName -Force
    Write-Host "Cleaned previous executable" -ForegroundColor Gray
}

# Remove old object files
Get-ChildItem -Filter "*.obj" | Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "Compiling with MSVC..." -ForegroundColor Yellow

# Compile and link in one step
$compileCommand = @("cl") + $compilerFlags + @(
    "fam65xx_processor_tests_runner.cpp",
    "json_parser.c",
    "/Fe:$targetName",
    "/link"
)

Write-Host "Command: $($compileCommand -join ' ')" -ForegroundColor Gray

try {
    & $compileCommand[0] $compileCommand[1..($compileCommand.Length-1)]
    
    if ($LASTEXITCODE -eq 0 -and (Test-Path $targetName)) {
        Write-Host ""
        Write-Host "✅ Build successful!" -ForegroundColor Green
        Write-Host "Executable: $targetName" -ForegroundColor Cyan
        Write-Host ""
        
        # Show usage examples
        Write-Host "Usage examples:" -ForegroundColor Yellow
        Write-Host "  $targetName -h                           # Show help" -ForegroundColor Gray
        Write-Host "  $targetName processor_tests\             # Run with auto-detected cores" -ForegroundColor Gray
        Write-Host "  $targetName -j 4 -v tests\              # 4 workers, verbose" -ForegroundColor Gray
        Write-Host "  $targetName -q -c tests\                # Quiet, continue on failures" -ForegroundColor Gray
        Write-Host ""
        
        # Show system info for optimal threading
        $cores = (Get-WmiObject -Class Win32_ComputerSystem).NumberOfLogicalProcessors
        $recommended = [Math]::Max(1, $cores - 1)
        
        Write-Host "System info:" -ForegroundColor Yellow
        Write-Host "  CPU cores detected: $cores" -ForegroundColor Gray
        Write-Host "  Recommended workers: $recommended (cores - 1)" -ForegroundColor Gray
        Write-Host ""
        
        # Show performance improvement note
        Write-Host "🚀 Performance improvements:" -ForegroundColor Green
        Write-Host "  ✓ Parallel execution (up to ${recommended}x faster on this system)" -ForegroundColor Gray
        Write-Host "  ✓ Compacted debug output for speed" -ForegroundColor Gray
        Write-Host "  ✓ Thread-safe output (no mixed stdout)" -ForegroundColor Gray
        Write-Host "  ✓ Smart core usage (leaves 1 core for system)" -ForegroundColor Gray
        Write-Host ""
        
    } else {
        Write-Error "❌ Build failed!"
        exit 1
    }
} catch {
    Write-Error "❌ Build failed with exception: $_"
    exit 1
}

# Clean up object files
Get-ChildItem -Filter "*.obj" | Remove-Item -Force -ErrorAction SilentlyContinue
Write-Host "Cleaned up build artifacts" -ForegroundColor Gray