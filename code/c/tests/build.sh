#!/bin/bash

# Build script for fam65xx processor tests runner (now with parallel execution)
# Usage: ./build.sh [debug|release]

set -e  # Exit on any error

BUILD_TYPE=${1:-default}

echo "Building fam65xx processor tests runner (Parallel Edition)..."
echo "Build type: $BUILD_TYPE"
echo "=============================================================="

# Check if required files exist
if [ ! -f "fam65xx_processor_tests_runner.cpp" ]; then
    echo "ERROR: fam65xx_processor_tests_runner.cpp not found!"
    exit 1
fi

if [ ! -f "json_parser.c" ]; then
    echo "ERROR: json_parser.c not found!"
    exit 1
fi

if [ ! -f "json_parser.h" ]; then
    echo "ERROR: json_parser.h not found!"
    exit 1
fi

# Check for CPU implementation header
if [ ! -f "../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx_callbacks.h" ]; then
    echo "ERROR: fam65xx_callbacks.h not found!"
    echo "Expected location: ../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx_callbacks.h"
    exit 1
fi

# Clean previous build
echo "Cleaning previous build..."
make clean

# Build based on type
case $BUILD_TYPE in
    debug)
        echo "Building DEBUG version..."
        make debug
        ;;
    release)
        echo "Building RELEASE version (optimized)..."
        make release
        ;;
    *)
        echo "Building DEFAULT version..."
        make
        ;;
esac

# Check if build was successful
if [ -f "fam65xx_processor_tests_runner" ]; then
    echo ""
    echo "✅ Build successful!"
    echo "Executable: fam65xx_processor_tests_runner"
    echo ""
    echo "Usage examples:"
    echo "  ./fam65xx_processor_tests_runner -h                    # Show help"
    echo "  ./fam65xx_processor_tests_runner processor_tests/      # Run with auto-detected cores"
    echo "  ./fam65xx_processor_tests_runner -j 4 -v tests/       # 4 workers, verbose"
    echo "  ./fam65xx_processor_tests_runner -q -c tests/         # Quiet, continue on failures"
    echo ""
    
    # Show system info for optimal threading
    if command -v nproc >/dev/null 2>&1; then
        CORES=$(nproc)
        RECOMMENDED=$((CORES - 1))
        echo "System info:"
        echo "  CPU cores detected: $CORES"
        echo "  Recommended workers: $RECOMMENDED (cores - 1)"
        echo ""
    fi
    
    # Show performance improvement note
    echo "🚀 Performance improvements:"
    echo "  ✓ Parallel execution (up to ${RECOMMENDED}x faster on this system)"
    echo "  ✓ Compacted debug output for speed"
    echo "  ✓ Thread-safe output (no mixed stdout)"
    echo "  ✓ Smart core usage (leaves 1 core for system)"
    echo ""
else
    echo ""
    echo "❌ Build failed!"
    exit 1
fi