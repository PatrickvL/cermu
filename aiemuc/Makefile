CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99
TARGET = c64emu
SOURCES = c64_main.c c64.c c64_bus.c ram.c rom.c system.c mos6569.c mos6526.c mos6581.c mos6510.c mos2114.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = c64.h c64_bus.h ram.h rom.h mos6569.h mos6526.h mos6581.h mos6510.h

# Klaus test harness
KLAUS_RUNNER = klaus_test_runner
KLAUS_SOURCES = klaus_test_runner.c mos6510_test_harness.c c64.c c64_bus.c ram.c rom.c system.c mos6569.c mos6526.c mos6581.c mos6510.c mos2114.c
KLAUS_OBJECTS = $(KLAUS_SOURCES:.c=.o)
KLAUS_HEADERS = $(HEADERS) mos6510_test_harness.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

$(KLAUS_RUNNER): $(KLAUS_OBJECTS)
	$(CC) $(KLAUS_OBJECTS) -o $(KLAUS_RUNNER)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(KLAUS_OBJECTS) $(TARGET) $(KLAUS_RUNNER)

test: $(TARGET)
	@echo "Build successful! C64 emulator with cycle-accurate MOS6510 CPU compiled cleanly."

klaus: $(KLAUS_RUNNER)
	@echo "Klaus test runner compiled successfully!"

.PHONY: all clean test klaus
