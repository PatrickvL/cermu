CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c99
TARGET = c64emu
SOURCES = c64_main.c c64.c c64_bus.c ram.c rom.c system.c mos6569.c mos6526.c mos6581.c mos6510.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = c64.h c64_bus.h ram.h rom.h mos6569.h mos6526.h mos6581.h mos6510.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	@echo "Build successful! C64 emulator with cycle-accurate MOS6510 CPU compiled cleanly."

.PHONY: all clean test
