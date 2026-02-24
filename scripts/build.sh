#!/bin/bash
# Build Script for cermu Multi-System Emulator
#
# Usage:
#   ./build.sh [gui|console|all] [release|debug] [--skip-deps] [--clean]
#
# This script:
#   1. Fetches missing external dependencies (SDL2, Dear ImGui)
#   2. Configures CMake
#   3. Builds the requested target(s)

set -e

# ============================================================================
# Paths & versions
# ============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
EXTERNAL_DIR="${PROJECT_DIR}/external"
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"

SDL2_VERSION="2.30.10"
IMGUI_VERSION="v1.92.1"
SDL2_DIR="${EXTERNAL_DIR}/SDL2/SDL2-${SDL2_VERSION}"
IMGUI_DIR="${EXTERNAL_DIR}/imgui"

# Defaults
TARGET="${1:-gui}"
BUILD_TYPE="${2:-release}"
SKIP_DEPS=false
CLEAN=false

# Parse flags (can appear anywhere)
for arg in "$@"; do
    case "$arg" in
        --skip-deps) SKIP_DEPS=true ;;
        --clean)     CLEAN=true ;;
    esac
done

# ============================================================================
# Helpers
# ============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

step()    { echo -e "\n${CYAN}==> $1${NC}"; }
ok()      { echo -e "    ${GREEN}OK:${NC} $1"; }
skip_msg(){ echo -e "    ${YELLOW}SKIP:${NC} $1"; }
err()     { echo -e "    ${RED}ERROR:${NC} $1"; exit 1; }

detect_platform() {
    case "$(uname -s)" in
        Linux*)                  PLATFORM=linux ;;
        Darwin*)                 PLATFORM=macos ;;
        CYGWIN*|MINGW*|MSYS*)   PLATFORM=windows ;;
        *)                       PLATFORM=unknown ;;
    esac
}

nproc_safe() { nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4; }

# ============================================================================
# Dependency: SDL2
# ============================================================================
fetch_sdl2() {
    step "Checking SDL2 ${SDL2_VERSION}..."

    if [ -f "${SDL2_DIR}/include/SDL.h" ]; then
        ok "SDL2 already present at ${SDL2_DIR}"
        return
    fi

    detect_platform

    case "$PLATFORM" in
        linux)
            # Prefer system package
            if pkg-config --exists sdl2 2>/dev/null; then
                ok "System SDL2 found: $(pkg-config --modversion sdl2)"
                return
            fi
            echo "    SDL2 not found. Installing via package manager..."
            if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get update -qq && sudo apt-get install -y libsdl2-dev
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y SDL2-devel
            elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --noconfirm sdl2
            else
                err "Cannot auto-install SDL2. Please install libsdl2-dev manually."
            fi
            ok "SDL2 installed via system package manager"
            ;;
        macos)
            if pkg-config --exists sdl2 2>/dev/null; then
                ok "System SDL2 found: $(pkg-config --modversion sdl2)"
                return
            fi
            echo "    Installing SDL2 via Homebrew..."
            command -v brew >/dev/null 2>&1 || err "Homebrew not found. Install from https://brew.sh"
            brew install sdl2
            ok "SDL2 installed via Homebrew"
            ;;
        windows)
            # Download VC dev package (same as build.ps1)
            echo "    Downloading SDL2 dev package..."
            local zip_name="SDL2-devel-${SDL2_VERSION}-VC.zip"
            local url="https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/${zip_name}"
            local tmp_zip="/tmp/${zip_name}"

            curl -fSL -o "$tmp_zip" "$url" || wget -q -O "$tmp_zip" "$url" || err "Failed to download SDL2"

            local extract_tmp="/tmp/sdl2_extract"
            rm -rf "$extract_tmp"
            unzip -q "$tmp_zip" -d "$extract_tmp"

            mkdir -p "$(dirname "$SDL2_DIR")"
            rm -rf "$SDL2_DIR"
            mv "${extract_tmp}/SDL2-${SDL2_VERSION}" "$SDL2_DIR"
            rm -f "$tmp_zip"
            rm -rf "$extract_tmp"

            if [ -f "${SDL2_DIR}/include/SDL.h" ]; then
                ok "SDL2 installed to ${SDL2_DIR}"
            else
                err "SDL2 installation failed"
            fi
            ;;
        *)
            err "Unsupported platform for SDL2 auto-install: $PLATFORM"
            ;;
    esac
}

# ============================================================================
# Dependency: Dear ImGui
# ============================================================================
fetch_imgui() {
    step "Checking Dear ImGui ${IMGUI_VERSION}..."

    if [ -f "${IMGUI_DIR}/imgui.h" ]; then
        ok "Dear ImGui already present at ${IMGUI_DIR}"
        return
    fi

    if command -v git >/dev/null 2>&1; then
        echo "    Cloning Dear ImGui ${IMGUI_VERSION}..."
        git clone --depth 1 --branch "$IMGUI_VERSION" \
            "https://github.com/ocornut/imgui.git" "$IMGUI_DIR"
    else
        local tag_bare="${IMGUI_VERSION#v}"
        local url="https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.tar.gz"
        local tmp_tar="/tmp/imgui-${IMGUI_VERSION}.tar.gz"
        echo "    Downloading Dear ImGui ${IMGUI_VERSION}..."

        curl -fSL -o "$tmp_tar" "$url" || wget -q -O "$tmp_tar" "$url" || err "Failed to download ImGui"

        local extract_tmp="/tmp/imgui_extract"
        rm -rf "$extract_tmp"
        mkdir -p "$extract_tmp"
        tar xzf "$tmp_tar" -C "$extract_tmp"

        rm -rf "$IMGUI_DIR"
        mv "${extract_tmp}/imgui-${tag_bare}" "$IMGUI_DIR"
        rm -f "$tmp_tar"
        rm -rf "$extract_tmp"
    fi

    if [ -f "${IMGUI_DIR}/imgui.h" ]; then
        ok "Dear ImGui installed to ${IMGUI_DIR}"
    else
        err "Dear ImGui installation failed"
    fi
}

# ============================================================================
# CMake configure & build
# ============================================================================
build_project() {
    # CMake build type
    local cmake_type="Release"
    case "$BUILD_TYPE" in
        debug)   cmake_type="Debug" ;;
        release) cmake_type="Release" ;;
    esac

    step "Configuring CMake (${cmake_type})..."

    command -v cmake >/dev/null 2>&1 || err "CMake not found. Please install CMake 3.16 or later."

    if $CLEAN; then
        echo "    Cleaning build directory..."
        rm -f "${BUILD_DIR}/CMakeCache.txt"
        rm -rf "${BUILD_DIR}/CMakeFiles"
    fi

    mkdir -p "$BUILD_DIR"

    cd "$PROJECT_DIR"
    cmake -B build -S . -DCMAKE_BUILD_TYPE="$cmake_type"

    # Determine targets
    local targets=()
    case "$TARGET" in
        gui)     targets=(cermu) ;;
        console) targets=(cermu_console) ;;
        all)     targets=(cermu cermu_console) ;;
        *)       err "Unknown target: $TARGET (use gui, console, or all)" ;;
    esac

    local jobs
    jobs=$(nproc_safe)

    for t in "${targets[@]}"; do
        step "Building target: ${t} (${cmake_type})..."
        cmake --build build --config "$cmake_type" --target "$t" -j"$jobs"
        ok "Target '${t}' built successfully"
    done

    # Show results
    step "Build results:"
    if [ -d "$BIN_DIR" ]; then
        find "$BIN_DIR" -maxdepth 1 -type f \( -name "*.exe" -o -executable \) 2>/dev/null | sort | while read -r f; do
            echo "    $f"
        done
    else
        skip_msg "No bin directory found at ${BIN_DIR}"
    fi
}

# ============================================================================
# Usage
# ============================================================================
show_usage() {
    echo "Usage: $0 [TARGET] [BUILD_TYPE] [OPTIONS]"
    echo ""
    echo "Arguments:"
    echo "  TARGET       gui|console|all  (default: gui)"
    echo "  BUILD_TYPE   debug|release    (default: release)"
    echo ""
    echo "Options:"
    echo "  --skip-deps  Skip dependency fetching"
    echo "  --clean      Remove CMake cache before configuring"
    echo "  -h, --help   Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build GUI (Release)"
    echo "  $0 gui debug          # Build GUI (Debug)"
    echo "  $0 all release        # Build everything"
    echo "  $0 console --clean    # Clean + build console"
}

# ============================================================================
# Main
# ============================================================================
for arg in "$@"; do
    case "$arg" in
        -h|--help|help) show_usage; exit 0 ;;
    esac
done

echo -e "${CYAN}============================================${NC}"
echo -e "${CYAN} cermu Build Script${NC}"
echo -e "${CYAN} Target: ${TARGET} | Config: ${BUILD_TYPE}${NC}"
echo -e "${CYAN}============================================${NC}"

# Fetch dependencies
if ! $SKIP_DEPS; then
    fetch_sdl2
    fetch_imgui
else
    skip_msg "Dependency fetch skipped (--skip-deps)"
fi

# Build
build_project

echo -e "\n${GREEN}============================================${NC}"
echo -e "${GREEN} Build complete!${NC}"
echo -e "${GREEN}============================================${NC}"