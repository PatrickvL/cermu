#pragma once
/*
 * wd1772.hpp — Western Digital WD1772 Floppy Disk Controller — Stub
 *
 * Member of the WD FDC family (WD1770/1772/1793/2793).
 *
 * The WD1772 is the fast-seek variant used in the Atari ST.  It manages
 * 3.5" DD floppy drives (720KB) or HD (1.44MB on Mega STE / TT).
 *
 * Features:
 *   - FM and MFM encoding (single/double density)
 *   - 6 MHz (ST standard) / 8 MHz (TT / Mega STE)
 *   - Faster step rates than WD1770 (2ms minimum)
 *   - Type I (seek/step/restore), II (read/write sector),
 *     III (read/write track), IV (force interrupt) commands
 *
 * Register map (directly selected by A0–A1):
 *   0: Command (write) / Status (read)
 *   1: Track register
 *   2: Sector register
 *   3: Data register
 */

#include "chip/storage/wd_fdc/wd_fdc_traits.hpp"
#include "core/chip.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define WD1772_DECL(REG, FLD, CMP) \
    REG(0x00, STATUS, "Status / Command")                                         \
      FLD(STATUS, BUSY,       7:7, "Busy",                  Flag, 0, 0)           \
      FLD(STATUS, DRQ,        1:1, "Data Request",          Flag, 0, 0)           \
      FLD(STATUS, MOTOR_ON,   7:7, "Motor on (Type I)",     Flag, 0, 0)           \
    REG(0x01, TRACK,  "Track Register")                                           \
    REG(0x02, SECTOR, "Sector Register")                                          \
    REG(0x03, DATA,   "Data Register")

namespace wd1772 {
    namespace reg {
        WD1772_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 4;
    }
    using namespace reg;

    // ── Family-level aliases for backward compatibility ──────────────
    // These are now defined in wd_fdc_traits.hpp; kept here as aliases.
    using namespace wd_fdc;
}

DECL_EXTRACT(WD1772, WD1772_DECL)

// ============================================================================
// WD1772 FDC — Stub Implementation
// ============================================================================

struct wd1772_t : public ChipBase {

    static constexpr auto& Traits = wd_fdc::WD1772Traits;

    wd1772_t()
        : ChipBase(ChipInfo{Traits.chip_id, Traits.vendor, Traits.display_name})
    {
        init_regs(wd1772::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(WD1772_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x03;
        switch (addr) {
            case 0:  // Status
                BUS_SET_DATA(bus, status_);
                break;
            case 1:  // Track
                BUS_SET_DATA(bus, regs_.data[wd1772::TRACK]);
                break;
            case 2:  // Sector
                BUS_SET_DATA(bus, regs_.data[wd1772::SECTOR]);
                break;
            case 3:  // Data
                BUS_SET_DATA(bus, regs_.data[wd1772::DATA]);
                status_ &= ~wd_fdc::ST_DRQ;
                break;
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x03;
        uint8_t data = BUS_GET_DATA(bus);
        switch (addr) {
            case 0:  // Command
                command_ = data;
                execute_command(data);
                break;
            case 1:  // Track
                regs_.data[wd1772::TRACK] = data;
                break;
            case 2:  // Sector
                regs_.data[wd1772::SECTOR] = data;
                break;
            case 3:  // Data
                regs_.data[wd1772::DATA] = data;
                status_ &= ~wd_fdc::ST_DRQ;
                break;
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Motor spin-up timing, seek completion, sector transfer DMA
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, wd1772::reg::REG_COUNT);
        status_ = 0;
        command_ = 0;
        current_track_ = 0;
        step_direction_ = 1;
    }

    bool irq_pending() const noexcept {
        return (status_ & wd_fdc::ST_BUSY) == 0 && command_ != 0;
    }

    // ── State ────────────────────────────────────────────────────
    uint8_t status_ = 0;
    uint8_t command_ = 0;
    uint8_t current_track_ = 0;
    int8_t  step_direction_ = 1;

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    void execute_command(uint8_t cmd) noexcept {
        uint8_t type = cmd & 0xF0;
        status_ |= wd_fdc::ST_BUSY;

        if (type == wd_fdc::CMD_FORCE_INT) {
            status_ &= ~wd_fdc::ST_BUSY;
            command_ = 0;
            return;
        }

        // Type I commands: Restore, Seek, Step
        if (type <= 0x60) {
            if (type == wd_fdc::CMD_RESTORE) {
                current_track_ = 0;
                regs_.data[wd1772::TRACK] = 0;
            } else if (type == wd_fdc::CMD_SEEK) {
                current_track_ = regs_.data[wd1772::DATA];
                regs_.data[wd1772::TRACK] = current_track_;
            }
            // Complete immediately (no motor/seek timing in stub)
            status_ &= ~wd_fdc::ST_BUSY;
            return;
        }

        // Type II/III: stub — immediate completion, no data transfer
        status_ &= ~wd_fdc::ST_BUSY;
        status_ |= wd_fdc::ST_RNF;  // No disk inserted → Record Not Found
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("FDC — Status")
            .value("Status", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const wd1772_t*>(c)->status_; })
            .value("Command", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const wd1772_t*>(c)->command_; })
            .category("FDC — Position")
            .value("Track", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const wd1772_t*>(c)->regs_.data[wd1772::TRACK]; })
            .value("Sector", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const wd1772_t*>(c)->regs_.data[wd1772::SECTOR]; })
            .value("Cur Track", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const wd1772_t*>(c)->current_track_; });
    }
#endif
};
