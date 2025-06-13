# Makefile for MOS6510 Comprehensive Tests
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
INCLUDES = -I../

# Source directories
SRC_DIR = ../src
CPU_DIR = $(SRC_DIR)/chip/cpu/mos6510
CORE_DIR = $(SRC_DIR)/core

# Source files
CPU_SOURCES = \
    $(CPU_DIR)/mos6510.c \
    $(CPU_DIR)/mos6510_arithmetic.c \
    $(CPU_DIR)/mos6510_control.c \
    $(CPU_DIR)/mos6510_flags.c \
    $(CPU_DIR)/mos6510_illegal.c \
    $(CPU_DIR)/mos6510_memory.c \
    $(CPU_DIR)/mos6510_misc.c \
    $(CPU_DIR)/mos6510_registers.c \
    $(CPU_DIR)/mos6510_shifts.c \
    $(CPU_DIR)/mos6510_stack.c

CORE_SOURCES = \
    $(CORE_DIR)/chip.c

ALL_SOURCES = $(CPU_SOURCES) $(CORE_SOURCES)

# Test executables
TEST_BASIC = test_mos6510_basic
TEST_COMPREHENSIVE = test_mos6510_comprehensive

# Build directory
BUILD_DIR = build

.PHONY: all clean test basic comprehensive

all: $(BUILD_DIR) $(TEST_BASIC) $(TEST_COMPREHENSIVE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BASIC): $(BUILD_DIR)/$(TEST_BASIC)

$(TEST_COMPREHENSIVE): $(BUILD_DIR)/$(TEST_COMPREHENSIVE)

$(BUILD_DIR)/$(TEST_BASIC): ../test_mos6510_basic.c $(ALL_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(BUILD_DIR)/$(TEST_COMPREHENSIVE): test_mos6510_comprehensive.c $(ALL_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test: comprehensive
	@echo "Running comprehensive test suite..."
	./$(BUILD_DIR)/$(TEST_COMPREHENSIVE)

basic: $(TEST_BASIC)
	@echo "Running basic test suite..."
	./$(BUILD_DIR)/$(TEST_BASIC)

comprehensive: $(TEST_COMPREHENSIVE)

clean:
	rm -rf $(BUILD_DIR)
	rm -f *.o

.SUFFIXES: .c .o

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: all

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build all tests"
	@echo "  basic        - Build and run basic tests"
	@echo "  comprehensive - Build comprehensive tests"
	@echo "  test         - Build and run comprehensive tests"
	@echo "  clean        - Remove build files"
	@echo "  debug        - Build with debug flags"
	@echo "  help         - Show this help message"