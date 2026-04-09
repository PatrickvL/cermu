#pragma once
/*
 * mk68901.hpp — Motorola MK68901 MFP (Multi-Function Peripheral) — Stub
 *
 * The MK68901 is a versatile I/O controller used in the Atari ST providing:
 *   - 8 GPIO pins (active-edge interrupt triggering)
 *   - 16-source interrupt controller (8 external, 4 timer, 4 other)
 *   - 4 timers (A, B, C, D) with prescaler, count, and event modes
 *   - Full-duplex USART (async serial, used for MIDI + RS-232)
 *
 * Register map ($FFFA01–$FFFA2F, odd bytes only in Atari ST):
 *   $01 GPIP   — GPIO data
 *   $03 AER    — Active Edge Register
 *   $05 DDR    — Data Direction Register
 *   $07 IERA   — Interrupt Enable A
 *   $09 IERB   — Interrupt Enable B
 *   $0B IPRA   — Interrupt Pending A
 *   $0D IPRB   — Interrupt Pending B
 *   $0F ISRA   — Interrupt In-Service A
 *   $11 ISRB   — Interrupt In-Service B
 *   $13 IMRA   — Interrupt Mask A
 *   $15 IMRB   — Interrupt Mask B
 *   $17 VR     — Vector Register
 *   $19 TACR   — Timer A Control
 *   $1B TBCR   — Timer B Control
 *   $1D TCDCR  — Timer C/D Control
 *   $1F TADR   — Timer A Data
 *   $21 TBDR   — Timer B Data
 *   $23 TCDR   — Timer C Data
 *   $25 TDDR   — Timer D Data
 *   $27 SCR    — Sync Character
 *   $29 UCR    — USART Control
 *   $2B RSR    — Receiver Status
 *   $2D TSR    — Transmitter Status
 *   $2F UDR    — USART Data
 */

#include "chip/io/io_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define MK68901_DECL(REG, FLD, CMP) \
    REG(0x00, GPIP,   "GPIO Data")                                              \
    REG(0x01, AER,    "Active Edge Register")                                   \
    REG(0x02, DDR,    "Data Direction Register")                                \
    REG(0x03, IERA,   "Interrupt Enable A")                                     \
    REG(0x04, IERB,   "Interrupt Enable B")                                     \
    REG(0x05, IPRA,   "Interrupt Pending A")                                    \
    REG(0x06, IPRB,   "Interrupt Pending B")                                    \
    REG(0x07, ISRA,   "Interrupt In-Service A")                                 \
    REG(0x08, ISRB,   "Interrupt In-Service B")                                 \
    REG(0x09, IMRA,   "Interrupt Mask A")                                       \
    REG(0x0A, IMRB,   "Interrupt Mask B")                                       \
    REG(0x0B, VR,     "Vector Register")                                        \
      FLD(VR, VEC_BASE,   7:4, "Vector base",             Value, 0, 0)          \
      FLD(VR, S_BIT,      3:3, "Software end-of-interrupt",Flag, 0, 0)          \
    REG(0x0C, TACR,   "Timer A Control")                                        \
      FLD(TACR, PRESCALE_A, 3:0, "Timer A prescale/mode", Value, 0, 0)          \
    REG(0x0D, TBCR,   "Timer B Control")                                        \
      FLD(TBCR, PRESCALE_B, 3:0, "Timer B prescale/mode", Value, 0, 0)          \
    REG(0x0E, TCDCR,  "Timer C/D Control")                                      \
      FLD(TCDCR, PRESCALE_C, 6:4, "Timer C prescale",     Value, 0, 0)          \
      FLD(TCDCR, PRESCALE_D, 2:0, "Timer D prescale",     Value, 0, 0)          \
    REG(0x0F, TADR,   "Timer A Data")                                           \
    REG(0x10, TBDR,   "Timer B Data")                                           \
    REG(0x11, TCDR,   "Timer C Data")                                           \
    REG(0x12, TDDR,   "Timer D Data")                                           \
    REG(0x13, SCR,    "Sync Character")                                         \
    REG(0x14, UCR,    "USART Control")                                          \
    REG(0x15, RSR,    "Receiver Status")                                        \
    REG(0x16, TSR,    "Transmitter Status")                                     \
      FLD(TSR, BUF_EMPTY,  7:7, "Buffer empty",           Flag,  0, 0)          \
    REG(0x17, UDR,    "USART Data")

namespace mk68901 {
    namespace reg {
        MK68901_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 24;
    }
    using namespace reg;

    // Timer prescaler divisors (index by control register value)
    inline constexpr uint16_t PRESCALER[] = {
        0, 4, 10, 16, 50, 64, 100, 200
    };
}

DECL_EXTRACT(MK68901, MK68901_DECL)

// ============================================================================
// MK68901 MFP — Stub Implementation
// ============================================================================

struct mk68901_t : public IoChipBase {

    mk68901_t()
        : IoChipBase(ChipInfo{"MK68901", "Motorola", "Multi-Function Peripheral"})
    {
        init_regs(mk68901::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(MK68901_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = (BUS_GET_ADDR(bus) >> 1) & 0x1F;  // Odd-byte addressing
        if (addr < mk68901::reg::REG_COUNT) {
            uint8_t val = regs_.data[addr];
            // Timer data registers return current counter value
            if (addr == mk68901::TADR) val = timer_counter_[0];
            else if (addr == mk68901::TBDR) val = timer_counter_[1];
            else if (addr == mk68901::TCDR) val = timer_counter_[2];
            else if (addr == mk68901::TDDR) val = timer_counter_[3];
            else if (addr == mk68901::TSR) val |= 0x80;  // Buffer always empty (stub)
            BUS_SET_DATA(bus, val);
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = (BUS_GET_ADDR(bus) >> 1) & 0x1F;
        uint8_t data = BUS_GET_DATA(bus);
        if (addr < mk68901::reg::REG_COUNT) {
            regs_.data[addr] = data;
            // Reload timer data registers
            if (addr == mk68901::TADR) timer_reload_[0] = data;
            else if (addr == mk68901::TBDR) timer_reload_[1] = data;
            else if (addr == mk68901::TCDR) timer_reload_[2] = data;
            else if (addr == mk68901::TDDR) timer_reload_[3] = data;
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }

        // Timer ticks (prescaler-based countdown)
        tick_timer(0, (regs_.data[mk68901::TACR] & 0x07));
        tick_timer(1, (regs_.data[mk68901::TBCR] & 0x07));
        tick_timer(2, (regs_.data[mk68901::TCDCR] >> 4) & 0x07);
        tick_timer(3, (regs_.data[mk68901::TCDCR] & 0x07));

        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, mk68901::reg::REG_COUNT);
        regs_.data[mk68901::VR] = 0x0F;  // Default vector base
        regs_.data[mk68901::TSR] = 0x80;  // Transmitter buffer empty
        for (int i = 0; i < 4; i++) {
            timer_reload_[i] = 0;
            timer_counter_[i] = 0;
            timer_prescale_count_[i] = 0;
        }
    }

    // ── Timer IRQ pending check ──────────────────────────────────
    bool timer_irq_pending() const noexcept {
        // Timer C (200 Hz system tick) is in IPRA bit 5
        return (regs_.data[mk68901::IPRA] & regs_.data[mk68901::IMRA]) != 0 ||
               (regs_.data[mk68901::IPRB] & regs_.data[mk68901::IMRB]) != 0;
    }

    uint8_t interrupt_vector() const noexcept {
        return regs_.data[mk68901::VR] & 0xF0;
    }

    // ── State ────────────────────────────────────────────────────
    uint8_t timer_reload_[4] = {};
    uint8_t timer_counter_[4] = {};
    uint16_t timer_prescale_count_[4] = {};

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    void tick_timer(int idx, uint8_t mode) noexcept {
        if (mode == 0) return;  // Stopped
        if (mode > 7) return;   // Event mode (not implemented)

        uint16_t prescale = mk68901::PRESCALER[mode];
        timer_prescale_count_[idx]++;
        if (timer_prescale_count_[idx] >= prescale) {
            timer_prescale_count_[idx] = 0;
            if (timer_counter_[idx] == 0) {
                timer_counter_[idx] = timer_reload_[idx];
                // Set pending interrupt bit for this timer
                // Timer A=IPRA.5, Timer B=IPRA.0, Timer C=IPRB.5, Timer D=IPRB.4
                static constexpr uint8_t timer_ipra_bits[] = {0x20, 0x01, 0x00, 0x00};
                static constexpr uint8_t timer_iprb_bits[] = {0x00, 0x00, 0x20, 0x10};
                regs_.data[mk68901::IPRA] |= timer_ipra_bits[idx];
                regs_.data[mk68901::IPRB] |= timer_iprb_bits[idx];
            } else {
                timer_counter_[idx]--;
            }
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("MFP — Interrupts")
            .value("IERA", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->regs_.data[mk68901::IERA]; })
            .value("IERB", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->regs_.data[mk68901::IERB]; })
            .value("IPRA", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->regs_.data[mk68901::IPRA]; })
            .value("IPRB", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->regs_.data[mk68901::IPRB]; })
            .category("MFP — Timers")
            .value("Timer A", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->timer_counter_[0]; })
            .value("Timer B", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->timer_counter_[1]; })
            .value("Timer C", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->timer_counter_[2]; })
            .value("Timer D", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const mk68901_t*>(c)->timer_counter_[3]; });
    }
#endif
};
