// ============================================================================
// CommodoreSystem serial trap template implementations
// ============================================================================
// Include this file from .cpp files that call check_serial_traps() and need
// the template bodies instantiated.  Separated from commodore_system.hpp to
// avoid pulling Drive1541Device's full definition into every translation unit.
// ============================================================================
#pragma once

#include "systems/commodore/commodore_system.hpp"
#include "devices/storage/drive_1541.hpp"

template <typename CPU>
bool CommodoreSystem::serial_trap_attention(CPU& cpu, uint8_t* ram, uint16_t resume_pc) {
    uint8_t iecdata = ram[IEC::ZP_BSOUR];

    if (iecdata == IEC::UNLISTEN) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_unlisten();
        serial_trap_.active_device = -1;
    } else if (iecdata == IEC::UNTALK) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_untalk();
        serial_trap_.active_device = -1;
    } else if ((iecdata & 0xF0) == IEC::LISTEN_MASK || (iecdata & 0xF0) == IEC::TALK_MASK) {
        serial_trap_.active_device = iecdata & IEC::DEVNR_MASK;
        serial_trap_.trap_device = iecdata;
        serial_trap_.trap_secondary = 0;
    } else if ((iecdata & 0xF0) == IEC::SECOND_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_second(iecdata & 0x0F);
    } else if ((iecdata & 0xF0) == IEC::OPEN_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_open(iecdata & 0x0F);
    } else if ((iecdata & 0xF0) == IEC::CLOSE_MASK) {
        serial_trap_.trap_secondary = iecdata;
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (drive) drive->trap_close(iecdata & 0x0F);
    }

    if (serial_trap_.active_device >= 4) {
        auto* drive = find_iec_drive(serial_trap_.active_device);
        if (!drive) ram[IEC::ZP_STATUS] |= 0x80;  // Device not present
    }

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;  // Clear carry
    p &= ~0x04;  // Clear interrupt disable
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume_pc);
    cpu.transition_to_fetch();
    return true;
}

template <typename CPU>
bool CommodoreSystem::serial_trap_send(CPU& cpu, uint8_t* ram, uint16_t resume_pc) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    uint8_t iecdata = ram[IEC::ZP_BSOUR];

    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC::SECOND_MASK;
        drive->trap_second(0);
    }

    drive->trap_send(iecdata);

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;
    p &= ~0x04;
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume_pc);
    cpu.transition_to_fetch();
    return true;
}

template <typename CPU>
bool CommodoreSystem::serial_trap_receive(CPU& cpu, uint8_t* ram, uint16_t resume_pc) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    if (serial_trap_.trap_secondary == 0) {
        serial_trap_.trap_secondary = IEC::SECOND_MASK;
        drive->trap_second(0);
    }

    uint8_t data = 0;
    int status = drive->trap_receive(data);

    ram[IEC::ZP_TMP_IN] = data;
    cpu.set(reg::A, data);

    if (status)
        ram[IEC::ZP_STATUS] |= static_cast<uint8_t>(status);

    uint8_t p = cpu.get(reg::P);
    p &= ~0x01;  // Clear carry
    p &= ~0x04;  // Clear interrupt disable
    if (data & 0x80) p |= 0x80; else p &= ~0x80;  // N flag
    if (data == 0)   p |= 0x02; else p &= ~0x02;  // Z flag
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume_pc);
    cpu.transition_to_fetch();
    return true;
}

template <typename CPU>
bool CommodoreSystem::serial_trap_ready(CPU& cpu, uint16_t resume_pc) {
    if (serial_trap_.active_device < 4) return false;
    auto* drive = find_iec_drive(serial_trap_.active_device);
    if (!drive) return false;

    cpu.set(reg::A, static_cast<uint8_t>(1));

    uint8_t p = cpu.get(reg::P);
    p &= ~0x80;  // Clear sign
    p &= ~0x02;  // Clear zero
    p &= ~0x04;  // Clear interrupt disable
    cpu.set(reg::P, p);
    cpu.set(reg::PC, resume_pc);
    cpu.transition_to_fetch();
    return true;
}
