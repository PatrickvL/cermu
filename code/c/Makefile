# Cross-Platform Makefile for C64 Emulator
# Compatible with Linux, macOS, and Windows (via MinGW/MSYS2)
#
# Usage:
#   make                    # Build console version (Release)
#   make gui                # Build GUI version (Release)  
#   make debug              # Build console version (Debug)
#   make gui-debug          # Build GUI version (Debug)
#   make clean              # Clean build artifacts
#   make install            # Install to system
#   make test               # Run tests
#
# Configuration:
#   make CC=clang           # Use specific compiler
#   make PREFIX=/usr/local  # Set install prefix
#   make V=1                # Verbose output

# Project configuration
PROJECT_NAME := c64emu
VERSION := 0.1.0

# Build configuration
CC ?= gcc
CXX ?= g++
PREFIX ?= /usr/local
DESTDIR ?=

# Build directories
BUILD_DIR := build
BIN_DIR := bin
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/deps

# Source directories
SRC_DIR := src
CORE_DIR := $(SRC_DIR)/core
CHIP_DIR := $(SRC_DIR)/chip
SYSTEMS_DIR := $(SRC_DIR)/systems
GUI_DIR := $(SRC_DIR)/gui
MAIN_DIR := $(SRC_DIR)/main

# External dependencies
EXTERNAL_DIR := external
CIMGUI_DIR := $(EXTERNAL_DIR)/cimgui
SDL2_DIR := deps/sdl2/SDL2-2.30.10

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
UNAME_M := $(shell uname -m 2>/dev/null || echo x86_64)

# Platform-specific settings
ifeq ($(UNAME_S),Linux)
    PLATFORM := linux
    SHARED_EXT := .so
    EXE_EXT :=
    PLATFORM_LIBS := -lm -ldl -lpthread
endif

ifeq ($(UNAME_S),Darwin)
    PLATFORM := macos
    SHARED_EXT := .dylib
    EXE_EXT :=
    PLATFORM_LIBS := -lm -ldl -lpthread
    PLATFORM_CFLAGS := -mmacosx-version-min=10.14
endif

ifneq ($(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)),)
    PLATFORM := windows
    SHARED_EXT := .dll
    EXE_EXT := .exe
    PLATFORM_LIBS := -lm -limm32
    PLATFORM_CFLAGS := -D_WIN32_WINNT=0x0600
endif

# Default to Linux if detection fails
PLATFORM ?= linux
SHARED_EXT ?= .so
EXE_EXT ?=
PLATFORM_LIBS ?= -lm -ldl -lpthread

# Build type (Release or Debug)
BUILD_TYPE ?= Release
ifeq ($(BUILD_TYPE),Debug)
    OPT_FLAGS := -g -O0 -DDEBUG
    BUILD_SUFFIX := _debug
else
    OPT_FLAGS := -O2 -DNDEBUG
    BUILD_SUFFIX :=
endif

# Compiler flags
CFLAGS := -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable $(OPT_FLAGS) $(PLATFORM_CFLAGS)
CXXFLAGS := -std=c++11 -Wall -Wextra -Wno-unused-parameter $(OPT_FLAGS) $(PLATFORM_CFLAGS)

# Enable redesign CPU dispatch
CFLAGS += -DREDESIGN=1
CXXFLAGS += -DREDESIGN=1

# Include directories
INCLUDES := -I$(CORE_DIR) \
           -I$(CHIP_DIR)/cpu/mos6510 \
           -I$(CHIP_DIR)/io \
           -I$(CHIP_DIR)/logic \
           -I$(CHIP_DIR)/memory \
           -I$(CHIP_DIR)/sound \
           -I$(CHIP_DIR)/video \
           -I$(SYSTEMS_DIR)/c64 \
           -I$(MAIN_DIR) \
           -I$(GUI_DIR)

# Libraries
LIBS := $(PLATFORM_LIBS)

# SDL2 and GUI detection
SDL2_CONFIG := $(shell command -v sdl2-config 2> /dev/null)
SDL2_LOCAL := $(wildcard $(SDL2_DIR)/include/SDL.h)

# Use local SDL2 if available, otherwise try system SDL2
ifdef SDL2_LOCAL
    SDL2_INCLUDES := -I$(SDL2_DIR)/include
    ifeq ($(UNAME_M),x86_64)
        SDL2_LIBS := -L$(SDL2_DIR)/lib/x64 -lSDL2
    else
        SDL2_LIBS := -L$(SDL2_DIR)/lib/x86 -lSDL2
    endif
    HAS_SDL2 := 1
else ifdef SDL2_CONFIG
    SDL2_INCLUDES := $(shell $(SDL2_CONFIG) --cflags)
    SDL2_LIBS := $(shell $(SDL2_CONFIG) --libs)
    HAS_SDL2 := 1
else
    HAS_SDL2 := 0
endif

# OpenGL detection
ifeq ($(PLATFORM),linux)
    OPENGL_LIBS := -lGL -lGLU
    HAS_OPENGL := 1
endif

ifeq ($(PLATFORM),macos)
    OPENGL_LIBS := -framework OpenGL
    HAS_OPENGL := 1
endif

ifeq ($(PLATFORM),windows)
    OPENGL_LIBS := -lopengl32 -lglu32
    HAS_OPENGL := 1
endif

# GUI build capability
ifeq ($(HAS_SDL2)$(HAS_OPENGL),11)
    CAN_BUILD_GUI := 1
    GUI_INCLUDES := $(SDL2_INCLUDES) -I$(CIMGUI_DIR) -I$(CIMGUI_DIR)/imgui -I$(CIMGUI_DIR)/imgui/backends
    GUI_LIBS := $(SDL2_LIBS) $(OPENGL_LIBS)
    GUI_CFLAGS := -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS=1
else
    CAN_BUILD_GUI := 0
endif

# Core source files
CORE_SOURCES := $(CORE_DIR)/system.c \
               $(CORE_DIR)/chip.c \
               $(CORE_DIR)/ioport.c

# Chip source files
CHIP_SOURCES := $(CHIP_DIR)/cpu/fam65xx/fam65xx_core.c \
               $(CHIP_DIR)/cpu/fam65xx/fam65xx_opcodes.c \
               $(CHIP_DIR)/cpu/mos6510/mos6510.c \
               $(CHIP_DIR)/io/mos6526.c \
               $(CHIP_DIR)/logic/pla.c \
               $(CHIP_DIR)/memory/ram.c \
               $(CHIP_DIR)/memory/rom.c \
               $(CHIP_DIR)/memory/mos2114.c \
               $(CHIP_DIR)/sound/mos6581.c \
               $(CHIP_DIR)/video/vic_ii/mos6569.c \
               $(CHIP_DIR)/video/vic_ii/mos6567.c \
               $(CHIP_DIR)/video/vic_ii/vicii_common.c

# System source files
SYSTEM_SOURCES := $(SYSTEMS_DIR)/c64/c64.c \
                 $(SYSTEMS_DIR)/c64/c64_bus.c \
                 $(SYSTEMS_DIR)/c64/c64_config.c

# Utility source files
UTIL_SOURCES := $(CORE_DIR)/storage/rom_loader.c \
               $(CORE_DIR)/config/path_discovery.c

# GUI source files (only if GUI can be built)
ifeq ($(CAN_BUILD_GUI),1)
GUI_SOURCES := $(GUI_DIR)/cimgui_interface.c \
              $(GUI_DIR)/cimgui_backends.cpp \
              $(CHIP_DIR)/video/vic_ii/vicii_gui.c \
              $(CHIP_DIR)/cpu/fam65xx/fam65xx_gui.c \
              $(CHIP_DIR)/cpu/mos6510/mos6510_gui.c \
              $(CHIP_DIR)/logic/pla_gui.c
endif

# All common sources
COMMON_SOURCES := $(CORE_SOURCES) $(CHIP_SOURCES) $(SYSTEM_SOURCES) $(UTIL_SOURCES)

# Test source files
TEST_SOURCES := test_mos6510_basic.c

# Object files
COMMON_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(filter %.c,$(COMMON_SOURCES)))
COMMON_OBJECTS += $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(filter %.cpp,$(COMMON_SOURCES)))

GUI_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(filter %.c,$(GUI_SOURCES)))
GUI_OBJECTS += $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(filter %.cpp,$(GUI_SOURCES)))

TEST_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SOURCES))

# Dependency files
DEPS := $(patsubst %.o,$(DEP_DIR)/%.d,$(COMMON_OBJECTS) $(GUI_OBJECTS) $(TEST_OBJECTS))

# Target executables
CONSOLE_TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(BUILD_SUFFIX)$(EXE_EXT)
GUI_TARGET := $(BIN_DIR)/$(PROJECT_NAME)_gui$(BUILD_SUFFIX)$(EXE_EXT)
TEST_TARGET := $(BIN_DIR)/test_mos6510_basic$(BUILD_SUFFIX)$(EXE_EXT)

# Verbose output control
V ?= 0
ifeq ($(V),1)
    Q :=
    ECHO := @true
else
    Q := @
    ECHO := @echo
endif

# Default target
.PHONY: all
all: console

# Console target
.PHONY: console
console: $(CONSOLE_TARGET)

# GUI target
.PHONY: gui
gui: 
ifeq ($(CAN_BUILD_GUI),1)
	$(MAKE) $(GUI_TARGET)
else
	@echo "Error: Cannot build GUI - SDL2 or OpenGL not found"
	@echo "Install SDL2 development libraries to enable GUI build"
	@exit 1
endif

# Debug targets
.PHONY: debug gui-debug
debug:
	$(MAKE) console BUILD_TYPE=Debug

gui-debug:
	$(MAKE) gui BUILD_TYPE=Debug

# Test target
.PHONY: test
test: $(TEST_TARGET)
	$(ECHO) "Running basic tests..."
	$(Q)$(TEST_TARGET)

# Build console executable
$(CONSOLE_TARGET): $(COMMON_OBJECTS) $(OBJ_DIR)/$(MAIN_DIR)/c64_main.o | $(BIN_DIR)
	$(ECHO) "Linking $@"
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Build GUI executable
$(GUI_TARGET): $(COMMON_OBJECTS) $(GUI_OBJECTS) $(OBJ_DIR)/$(MAIN_DIR)/c64_main_gui.o | $(BIN_DIR)
	$(ECHO) "Linking $@"
	$(Q)$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS) $(GUI_LIBS)

# Build test executable
$(TEST_TARGET): $(COMMON_OBJECTS) $(TEST_OBJECTS) | $(BIN_DIR)
	$(ECHO) "Linking $@"
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile C source files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR) $(DEP_DIR)
	$(ECHO) "Compiling $<"
	$(Q)mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*.d)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -MMD -MF $(DEP_DIR)/$*.d -c $< -o $@

# Compile C++ source files  
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR) $(DEP_DIR)
	$(ECHO) "Compiling $<"
	$(Q)mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*.d)
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDES) $(GUI_INCLUDES) $(GUI_CFLAGS) -MMD -MF $(DEP_DIR)/$*.d -c $< -o $@

# Create build directories
$(BUILD_DIR) $(BIN_DIR) $(OBJ_DIR) $(DEP_DIR):
	$(Q)mkdir -p $@

# Clean build artifacts
.PHONY: clean
clean:
	$(ECHO) "Cleaning build artifacts"
	$(Q)rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install targets
.PHONY: install install-console install-gui
install: install-console
ifeq ($(CAN_BUILD_GUI),1)
	$(MAKE) install-gui
endif

install-console: $(CONSOLE_TARGET)
	$(ECHO) "Installing console executable"
	$(Q)install -d $(DESTDIR)$(PREFIX)/bin
	$(Q)install -m 755 $(CONSOLE_TARGET) $(DESTDIR)$(PREFIX)/bin/

install-gui: $(GUI_TARGET)
	$(ECHO) "Installing GUI executable"
	$(Q)install -d $(DESTDIR)$(PREFIX)/bin
	$(Q)install -m 755 $(GUI_TARGET) $(DESTDIR)$(PREFIX)/bin/

# Uninstall
.PHONY: uninstall
uninstall:
	$(ECHO) "Uninstalling"
	$(Q)rm -f $(DESTDIR)$(PREFIX)/bin/$(PROJECT_NAME)$(EXE_EXT)
	$(Q)rm -f $(DESTDIR)$(PREFIX)/bin/$(PROJECT_NAME)_gui$(EXE_EXT)

# Show build configuration
.PHONY: config
config:
	@echo "Build Configuration:"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Compiler: $(CC) $(CXX)"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Install Prefix: $(PREFIX)"
	@echo "  SDL2 Available: $(HAS_SDL2)"
	@echo "  OpenGL Available: $(HAS_OPENGL)"
	@echo "  Can Build GUI: $(CAN_BUILD_GUI)"
	@echo "  Targets:"
	@echo "    Console: $(CONSOLE_TARGET)"
ifeq ($(CAN_BUILD_GUI),1)
	@echo "    GUI: $(GUI_TARGET)"
endif
	@echo "    Test: $(TEST_TARGET)"

# Help target
.PHONY: help
help:
	@echo "C64 Emulator Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all, console    Build console version (default)"
	@echo "  gui             Build GUI version (requires SDL2+OpenGL)"
	@echo "  debug           Build console version with debug info"
	@echo "  gui-debug       Build GUI version with debug info"
	@echo "  test            Build and run tests"
	@echo "  clean           Clean build artifacts"
	@echo "  install         Install to system"
	@echo "  uninstall       Remove from system"
	@echo "  config          Show build configuration"
	@echo "  help            Show this help"
	@echo ""
	@echo "Options:"
	@echo "  CC=compiler     Set C compiler"
	@echo "  CXX=compiler    Set C++ compiler"
	@echo "  PREFIX=path     Set install prefix (default: /usr/local)"
	@echo "  BUILD_TYPE=type Set build type (Release/Debug)"
	@echo "  V=1             Enable verbose output"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build console version"
	@echo "  make gui                # Build GUI version"
	@echo "  make CC=clang gui       # Build GUI with Clang"
	@echo "  make PREFIX=/opt/c64emu install"

# Include dependency files
-include $(DEPS)

# Mark directories as phony to avoid conflicts with files of same name
.PHONY: $(BUILD_DIR) $(BIN_DIR) $(OBJ_DIR) $(DEP_DIR)