#!/bin/bash
# Cross-platform build script for C64 Emulator
# Supports: CMake, Make, MSBuild (via Wine/MinGW)
#
# Usage:
#   ./build.sh [console|gui] [debug|release] [cmake|make|msbuild]

set -e

# Configuration
PROJECT_NAME="c64emu"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CODE_DIR="${PROJECT_DIR}/code/c"
BUILD_DIR="${CODE_DIR}/build"
BIN_DIR="${CODE_DIR}/bin"

# Default values
TARGET="${1:-console}"
BUILD_TYPE="${2:-release}"
BUILD_SYSTEM="${3:-auto}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
info() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Platform detection
detect_platform() {
    case "$(uname -s)" in
        Linux*)     PLATFORM=linux;;
        Darwin*)    PLATFORM=macos;;
        CYGWIN*|MINGW*|MSYS*) PLATFORM=windows;;
        *)          PLATFORM=unknown;;
    esac
    info "Detected platform: $PLATFORM"
}

# Build system detection
detect_build_system() {
    if [ "$BUILD_SYSTEM" != "auto" ]; then
        return
    fi
    
    if [ "$PLATFORM" = "windows" ]; then
        # On Windows, prefer MSBuild if available, otherwise CMake
        if command -v msbuild.exe >/dev/null 2>&1; then
            BUILD_SYSTEM="msbuild"
        elif [ -f "${CODE_DIR}/aiemuc.sln" ]; then
            BUILD_SYSTEM="msbuild"
        else
            BUILD_SYSTEM="cmake"
        fi
    else
        # On Unix-like systems, prefer Make if Makefile exists, otherwise CMake
        if [ -f "${CODE_DIR}/Makefile" ] && [ ! -f "${CODE_DIR}/CMakeCache.txt" ]; then
            BUILD_SYSTEM="make"
        else
            BUILD_SYSTEM="cmake"
        fi
    fi
    
    info "Selected build system: $BUILD_SYSTEM"
}

# Check dependencies
check_dependencies() {
    case "$BUILD_SYSTEM" in
        cmake)
            command -v cmake >/dev/null 2>&1 || error "CMake not found. Please install CMake 3.16 or later."
            ;;
        make)
            command -v make >/dev/null 2>&1 || error "Make not found. Please install GNU Make."
            command -v gcc >/dev/null 2>&1 || command -v clang >/dev/null 2>&1 || error "No C compiler found. Please install GCC or Clang."
            ;;
        msbuild)
            if [ "$PLATFORM" = "windows" ]; then
                command -v msbuild.exe >/dev/null 2>&1 || error "MSBuild not found. Please install Visual Studio Build Tools 2022."
            else
                error "MSBuild is only supported on Windows"
            fi
            ;;
    esac
}

# Check SDL2 availability for GUI builds
check_gui_dependencies() {
    if [ "$TARGET" != "gui" ]; then
        return
    fi
    
    info "Checking GUI dependencies..."
    
    # Check for SDL2
    SDL2_FOUND=false
    if command -v sdl2-config >/dev/null 2>&1; then
        SDL2_FOUND=true
        info "Found system SDL2: $(sdl2-config --version)"
    elif [ -f "${CODE_DIR}/deps/sdl2/SDL2-2.30.10/include/SDL.h" ]; then
        SDL2_FOUND=true
        info "Found local SDL2 in deps/"
    fi
    
    if [ "$SDL2_FOUND" = false ]; then
        warning "SDL2 not found. GUI build may fail."
        case "$PLATFORM" in
            linux)
                info "Install with: sudo apt install libsdl2-dev (Ubuntu/Debian)"
                info "            or: sudo yum install SDL2-devel (CentOS/RHEL)"
                ;;
            macos)
                info "Install with: brew install sdl2"
                ;;
        esac
    fi
}

# CMake build
build_cmake() {
    info "Building with CMake..."
    
    cd "$CODE_DIR"
    
    # Determine CMake build type
    CMAKE_BUILD_TYPE="Release"
    if [ "$BUILD_TYPE" = "debug" ]; then
        CMAKE_BUILD_TYPE="Debug"
    fi
    
    # Create and enter build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure
    info "Configuring with CMake..."
    cmake .. -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
    
    # Build
    info "Building..."
    if [ "$TARGET" = "gui" ]; then
        cmake --build . --config "$CMAKE_BUILD_TYPE" --target c64emu_gui -j$(nproc 2>/dev/null || echo 4)
    else
        cmake --build . --config "$CMAKE_BUILD_TYPE" --target c64emu -j$(nproc 2>/dev/null || echo 4)
    fi
    
    success "CMake build completed"
}

# Make build
build_make() {
    info "Building with Make..."
    
    cd "$CODE_DIR"
    
    # Set build type
    MAKE_BUILD_TYPE="Release"
    if [ "$BUILD_TYPE" = "debug" ]; then
        MAKE_BUILD_TYPE="Debug"
    fi
    
    # Build
    if [ "$TARGET" = "gui" ]; then
        make gui BUILD_TYPE="$MAKE_BUILD_TYPE" -j$(nproc 2>/dev/null || echo 4)
    else
        make console BUILD_TYPE="$MAKE_BUILD_TYPE" -j$(nproc 2>/dev/null || echo 4)
    fi
    
    success "Make build completed"
}

# MSBuild build
build_msbuild() {
    info "Building with MSBuild..."
    
    cd "$CODE_DIR"
    
    # Determine configuration
    MSBUILD_CONFIG="Release"
    if [ "$BUILD_TYPE" = "debug" ]; then
        MSBUILD_CONFIG="Debug"
    fi
    
    # Build
    if command -v msbuild.exe >/dev/null 2>&1; then
        msbuild.exe aiemuc.sln /p:Configuration="$MSBUILD_CONFIG" /verbosity:minimal
    else
        error "MSBuild not found in PATH"
    fi
    
    success "MSBuild build completed"
}

# Show build results
show_results() {
    info "Build completed successfully!"
    echo ""
    
    # Find built executables
    EXECUTABLES=()
    
    # Check different possible output locations
    for dir in "$BIN_DIR" "$BUILD_DIR/bin" "$CODE_DIR/bin/Release" "$CODE_DIR/bin/Debug"; do
        if [ -d "$dir" ]; then
            while IFS= read -r -d '' exe; do
                EXECUTABLES+=("$exe")
            done < <(find "$dir" -name "*.exe" -o -name "*emu*" -type f -executable 2>/dev/null | grep -E "(c64emu|test_)" | sort | tr '\n' '\0' 2>/dev/null || true)
        fi
    done
    
    if [ ${#EXECUTABLES[@]} -gt 0 ]; then
        echo "Built executables:"
        for exe in "${EXECUTABLES[@]}"; do
            echo "  - $exe"
        done
        echo ""
        
        # Show how to run
        if [ "$TARGET" = "gui" ]; then
            GUI_EXE=$(printf '%s\n' "${EXECUTABLES[@]}" | grep -E "(gui|GUI)" | head -1 || true)
            if [ -n "$GUI_EXE" ]; then
                echo "Run GUI version:"
                echo "  $GUI_EXE"
            fi
        else
            CONSOLE_EXE=$(printf '%s\n' "${EXECUTABLES[@]}" | grep -v -E "(gui|GUI|test)" | head -1 || true)
            if [ -n "$CONSOLE_EXE" ]; then
                echo "Run console version:"
                echo "  $CONSOLE_EXE"
            fi
        fi
    else
        warning "No executables found. Build may have failed."
    fi
}

# Show usage
show_usage() {
    echo "Usage: $0 [TARGET] [BUILD_TYPE] [BUILD_SYSTEM]"
    echo ""
    echo "Arguments:"
    echo "  TARGET      console|gui (default: console)"
    echo "  BUILD_TYPE  debug|release (default: release)"
    echo "  BUILD_SYSTEM cmake|make|msbuild|auto (default: auto)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build console version with auto-detected build system"
    echo "  $0 gui                # Build GUI version"
    echo "  $0 console debug      # Build debug console version"
    echo "  $0 gui release cmake  # Build GUI with CMake specifically"
    echo ""
    echo "Platform: $PLATFORM"
    echo "Auto-detected build system: $BUILD_SYSTEM"
}

# Main execution
main() {
    # Show usage if help requested
    if [ "$1" = "-h" ] || [ "$1" = "--help" ] || [ "$1" = "help" ]; then
        detect_platform
        detect_build_system
        show_usage
        exit 0
    fi
    
    info "Starting build for $PROJECT_NAME"
    info "Target: $TARGET, Build Type: $BUILD_TYPE"
    
    # Detect platform and build system
    detect_platform
    detect_build_system
    
    # Check dependencies
    check_dependencies
    check_gui_dependencies
    
    # Ensure we're in the right directory
    if [ ! -f "$CODE_DIR/CMakeLists.txt" ] && [ ! -f "$CODE_DIR/Makefile" ] && [ ! -f "$CODE_DIR/aiemuc.sln" ]; then
        error "Build files not found in $CODE_DIR. Please run from project root."
    fi
    
    # Build based on selected system
    case "$BUILD_SYSTEM" in
        cmake)
            build_cmake
            ;;
        make)
            build_make
            ;;
        msbuild)
            build_msbuild
            ;;
        *)
            error "Unknown build system: $BUILD_SYSTEM"
            ;;
    esac
    
    # Show results
    show_results
}

# Run main function
main "$@"