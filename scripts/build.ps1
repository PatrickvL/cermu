# Build Script for cermu Multi-System Emulator
# Usage: .\build.ps1 [-Configuration Release|Debug] [-Target gui|console|all] [-SkipDeps]
#
# This script:
#   1. Fetches missing external dependencies (SDL2, Dear ImGui)
#   2. Configures CMake
#   3. Builds the requested target(s)

param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",

    [ValidateSet("gui", "console", "all")]
    [string]$Target = "gui",

    [switch]$SkipDeps,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Paths
# ============================================================================
$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$ExternalDir = Join-Path $ProjectDir "external"
$BuildDir    = Join-Path $ProjectDir "build"
$BinDir      = Join-Path $BuildDir   "bin"

# Dependency versions / URLs
$SDL2_VERSION    = "2.30.10"
$SDL2_ZIP_NAME   = "SDL2-devel-${SDL2_VERSION}-VC.zip"
$SDL2_URL        = "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/${SDL2_ZIP_NAME}"
$SDL2_DIR        = Join-Path $ExternalDir "SDL2" "SDL2-${SDL2_VERSION}"

$IMGUI_VERSION   = "v1.92.1"
$IMGUI_DIR       = Join-Path $ExternalDir "imgui"

# ============================================================================
# Helper Functions
# ============================================================================
function Write-Step  { param([string]$msg) Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok    { param([string]$msg) Write-Host "    OK: $msg" -ForegroundColor Green }
function Write-Skip  { param([string]$msg) Write-Host "    SKIP: $msg" -ForegroundColor Yellow }
function Write-Err   { param([string]$msg) Write-Host "    ERROR: $msg" -ForegroundColor Red }

# ============================================================================
# Dependency: SDL2  (Windows VC dev package)
# ============================================================================
function Fetch-SDL2 {
    Write-Step "Checking SDL2 ${SDL2_VERSION}..."

    if (Test-Path (Join-Path $SDL2_DIR "include" "SDL.h")) {
        Write-Ok "SDL2 already present at $SDL2_DIR"
        return
    }

    Write-Host "    Downloading $SDL2_URL ..."
    $zipPath = Join-Path $env:TEMP $SDL2_ZIP_NAME

    # Download
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $SDL2_URL -OutFile $zipPath -UseBasicParsing

    # Extract to a temp folder first, then move into place
    $extractTmp = Join-Path $env:TEMP "sdl2_extract"
    if (Test-Path $extractTmp) { Remove-Item -Recurse -Force $extractTmp }
    Expand-Archive -Path $zipPath -DestinationPath $extractTmp -Force

    # The zip contains a top-level folder SDL2-<version>
    $innerDir = Join-Path $extractTmp "SDL2-${SDL2_VERSION}"
    if (-not (Test-Path $innerDir)) {
        Write-Err "Unexpected archive layout - expected $innerDir"
        exit 1
    }

    # Ensure parent exists
    $sdl2Parent = Split-Path $SDL2_DIR -Parent
    if (-not (Test-Path $sdl2Parent)) { New-Item -ItemType Directory -Path $sdl2Parent -Force | Out-Null }

    # Move into place
    if (Test-Path $SDL2_DIR) { Remove-Item -Recurse -Force $SDL2_DIR }
    Move-Item -Path $innerDir -Destination $SDL2_DIR

    # Cleanup
    Remove-Item -Force $zipPath -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $extractTmp -ErrorAction SilentlyContinue

    if (Test-Path (Join-Path $SDL2_DIR "include" "SDL.h")) {
        Write-Ok "SDL2 installed to $SDL2_DIR"
    } else {
        Write-Err "SDL2 installation failed"
        exit 1
    }
}

# ============================================================================
# Dependency: Dear ImGui  (cloned from GitHub)
# ============================================================================
function Fetch-ImGui {
    Write-Step "Checking Dear ImGui ${IMGUI_VERSION}..."

    if (Test-Path (Join-Path $IMGUI_DIR "imgui.h")) {
        Write-Ok "Dear ImGui already present at $IMGUI_DIR"
        return
    }

    # Prefer git clone (gives us exact tag); fall back to archive download
    if (Get-Command git -ErrorAction SilentlyContinue) {
        Write-Host "    Cloning Dear ImGui ${IMGUI_VERSION} ..."
        git clone --depth 1 --branch $IMGUI_VERSION "https://github.com/ocornut/imgui.git" $IMGUI_DIR
    } else {
        $archiveUrl = "https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.zip"
        $zipPath = Join-Path $env:TEMP "imgui-${IMGUI_VERSION}.zip"
        Write-Host "    Downloading $archiveUrl ..."
        Invoke-WebRequest -Uri $archiveUrl -OutFile $zipPath -UseBasicParsing

        $extractTmp = Join-Path $env:TEMP "imgui_extract"
        if (Test-Path $extractTmp) { Remove-Item -Recurse -Force $extractTmp }
        Expand-Archive -Path $zipPath -DestinationPath $extractTmp -Force

        # Archive contains imgui-<tag> (without leading 'v')
        $tagBare = $IMGUI_VERSION.TrimStart('v')
        $innerDir = Join-Path $extractTmp "imgui-${tagBare}"
        if (-not (Test-Path $innerDir)) {
            # Try with the full tag name
            $innerDir = Get-ChildItem $extractTmp -Directory | Select-Object -First 1 -ExpandProperty FullName
        }

        if (Test-Path $IMGUI_DIR) { Remove-Item -Recurse -Force $IMGUI_DIR }
        Move-Item -Path $innerDir -Destination $IMGUI_DIR

        Remove-Item -Force $zipPath -ErrorAction SilentlyContinue
        Remove-Item -Recurse -Force $extractTmp -ErrorAction SilentlyContinue
    }

    if (Test-Path (Join-Path $IMGUI_DIR "imgui.h")) {
        Write-Ok "Dear ImGui installed to $IMGUI_DIR"
    } else {
        Write-Err "Dear ImGui installation failed"
        exit 1
    }
}

# ============================================================================
# CMake configuration & build
# ============================================================================
function Build-Project {
    Write-Step "Configuring CMake ($Configuration) ..."

    # Clean build directory if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "    Cleaning build directory..."
        Remove-Item -Path (Join-Path $BuildDir "CMakeCache.txt") -Force -ErrorAction SilentlyContinue
        Remove-Item -Path (Join-Path $BuildDir "CMakeFiles") -Recurse -Force -ErrorAction SilentlyContinue
    }

    if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null }

    Push-Location $ProjectDir
    try {
        # Configure — pass vcpkg toolchain when VCPKG_ROOT is set
        $cmakeArgs = @("-B", "build", "-S", ".", "-DCMAKE_BUILD_TYPE=$Configuration")
        if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
            $toolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -replace '\\', '/'
            $tripletDir = "$ProjectDir\cmake\triplets" -replace '\\', '/'
            $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
            $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md"
            $cmakeArgs += "-DVCPKG_OVERLAY_TRIPLETS=$tripletDir"
            Write-Ok "Using vcpkg toolchain from $env:VCPKG_ROOT"
        }
        cmake @cmakeArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Err "CMake configuration failed"
            exit $LASTEXITCODE
        }

        # Determine targets
        $targets = @()
        switch ($Target) {
            "gui"     { $targets = @("cermu") }
            "console" { $targets = @("cermu_console") }
            "all"     { $targets = @("cermu", "cermu_console") }
        }

        foreach ($t in $targets) {
            Write-Step "Building target: $t ($Configuration) ..."
            cmake --build build --config $Configuration --target $t -- /m
            if ($LASTEXITCODE -ne 0) {
                Write-Err "Build of target '$t' failed (exit code $LASTEXITCODE)"
                exit $LASTEXITCODE
            }
            Write-Ok "Target '$t' built successfully"
        }

        # List built executables
        Write-Step "Build results:"
        if (Test-Path $BinDir) {
            Get-ChildItem "$BinDir\*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
                Write-Host "    $($_.FullName)" -ForegroundColor White
            }
            # Copy SDL2.dll next to executables if not already there
            $sdlDll = Join-Path $SDL2_DIR "lib" "x64" "SDL2.dll"
            $targetDll = Join-Path $BinDir "SDL2.dll"
            if ((Test-Path $sdlDll) -and -not (Test-Path $targetDll)) {
                Copy-Item $sdlDll $targetDll
                Write-Ok "Copied SDL2.dll to $BinDir"
            }
        } else {
            Write-Skip "No bin directory found at $BinDir"
        }

    } finally {
        Pop-Location
    }
}

# ============================================================================
# Main
# ============================================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " cermu Build Script" -ForegroundColor Cyan
Write-Host " Target: $Target | Config: $Configuration" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# Check cmake is available
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Err "CMake not found in PATH. Please install CMake 3.16 or later."
    exit 1
}

# Fetch dependencies
if (-not $SkipDeps) {
    Fetch-SDL2
    Fetch-ImGui
} else {
    Write-Skip "Dependency fetch skipped (-SkipDeps)"
}

# Build
Build-Project

Write-Host "`n============================================" -ForegroundColor Green
Write-Host " Build complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
