#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <cstdint>
#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"

// MOS 6522 VIA (Versatile Interface Adapter) chip structure
typedef struct {
    chip_descriptor_t* desc;
    void* bus;

    // Registers
    uint8_t registers[16];

    // I/O Ports
    uint8_t port_a_data;
    uint8_t port_b_data;
    uint8_t port_a_ddr;
    uint8_t port_b_ddr;

    // Callbacks for port input reads (used for keyboard matrix scanning)
    // These callbacks allow external devices (keyboard, joystick) to pull port lines LOW
    // Called when VIA reads from port to get external device state
    uint8_t (*port_a_read_callback)(void* context, uint8_t port_a_output);
    void* port_a_read_context;
    uint8_t (*port_b_read_callback)(void* context, uint8_t port_b_output);
    void* port_b_read_context;

    // Timers
    uint16_t timer1_latch;
    uint16_t timer1_counter;
    uint16_t timer2_latch;
    uint16_t timer2_counter;

    // Shift register
    uint8_t shift_register;
    uint8_t shift_counter;

    // Interrupt flags
    uint8_t interrupt_flags;
    uint8_t interrupt_enable;

    // Control registers
    uint8_t acr;  // Auxiliary Control Register
    uint8_t pcr;  // Peripheral Control Register
    uint8_t ifr;  // Interrupt Flag Register
    uint8_t ier;  // Interrupt Enable Register

    // Timer control
    bool timer1_running;
    bool timer2_running;
    bool timer1_continuous;
    bool timer2_continuous;

    // Interrupt state
    bool interrupt_active;
    int interrupt_line;

} mos6522_t;

// Register addresses
#define MOS6522_PORTB  0x00
#define MOS6522_PORTA  0x01
#define MOS6522_DDRB   0x02
#define MOS6522_DDRA   0x03
#define MOS6522_T1CL   0x04
#define MOS6522_T1CH   0x05
#define MOS6522_T1LL   0x06
#define MOS6522_T1LH   0x07
#define MOS6522_T2CL   0x08
#define MOS6522_T2CH   0x09
#define MOS6522_SR     0x0A
#define MOS6522_ACR    0x0B
#define MOS6522_PCR    0x0C
#define MOS6522_IFR    0x0D
#define MOS6522_IER    0x0E
#define MOS6522_PORTA_NH 0x0F  // PORTA without handshake

// Interrupt flags
#define MOS6522_IFR_IRQ      0x80
#define MOS6522_IFR_SET_CLR  0x80
#define MOS6522_IFR_T1       0x40
#define MOS6522_IFR_T2       0x20
#define MOS6522_IFR_CB1      0x10
#define MOS6522_IFR_CB2      0x08
#define MOS6522_IFR_SR       0x04
#define MOS6522_IFR_CA1      0x02
#define MOS6522_IFR_CA2      0x01

// ACR bits
#define MOS6522_ACR_SR_MODE  0x1C
#define MOS6522_ACR_SR_EXT   0x10
#define MOS6522_ACR_SR_OUT   0x08
#define MOS6522_ACR_SR_IN    0x04
#define MOS6522_ACR_T1_MODE  0x03
#define MOS6522_ACR_T1_CONT  0x40
#define MOS6522_ACR_T1_PB7   0x80
#define MOS6522_ACR_T2_MODE  0x0C
#define MOS6522_ACR_T2_CONT  0x20
#define MOS6522_ACR_T2_PB6   0x10

// PCR bits
#define MOS6522_PCR_CA2_OUT  0x80
#define MOS6522_PCR_CA1_OUT  0x40
#define MOS6522_PCR_CB2_OUT  0x20
#define MOS6522_PCR_CB1_OUT  0x10
#define MOS6522_PCR_CA2_IN   0x08
#define MOS6522_PCR_CA1_IN   0x04
#define MOS6522_PCR_CB2_IN   0x02
#define MOS6522_PCR_CB1_IN   0x01

// Function declarations
void* mos6522_create(chip_descriptor_t* desc);
void mos6522_destroy(void* chip);
void mos6522_reset(mos6522_t* via);
void mos6522_bus_attach(void* chip, void* bus);
bus_state_t mos6522_tick(void* chip, bus_state_t bus_state);
bus_state_t mos6522_registers_read(void* chip, bus_state_t bus_state);
bus_state_t mos6522_registers_write(void* chip, bus_state_t bus_state);

// Port read callback registration (used for keyboard matrix scanning, joystick, etc.)
void mos6522_set_port_a_read_callback(mos6522_t* via, uint8_t (*callback)(void*, uint8_t), void* context);
void mos6522_set_port_b_read_callback(mos6522_t* via, uint8_t (*callback)(void*, uint8_t), void* context);

// Chip descriptor
extern chip_descriptor_t mos6522_descriptor;

// GUI functions
#ifdef IMGUI_VERSION
void mos6522_render_debug_window(void* chip, bool* show_window);
void mos6522_render_settings_window(void* chip, bool* show_window);
#endif
