#pragma once
// drive_subsystem.hpp — Host-side adapter for attaching drive systems
//
// Bridges the gap between the host Commodore system and one or more
// cycle-accurate drive systems (C1541, future C1571, etc.).
//
// Responsibilities:
//   - Owns the shared IECBus and WarpController.
//   - Owns drive system instances (up to 3 drives: devices 8-10).
//   - Provides advance_drives() for the host tick loop.
//   - Bridges the host system's CIA IEC output pins ↔ IECBus slot 0.
//   - Monitors drive activity for warp mode.
//
// The host system creates a DriveSubsystem in its initialize() and calls:
//   - sync_host_to_iec() to push CIA output → IEC bus (after CIA tick)
//   - sync_iec_to_host() to pull IEC bus → CIA input (before CIA tick)
//   - advance_drives()   to tick all drives (after host CPU tick)
//   - warp().is_warping() to gate video and frame-sync sleep
//
// This keeps drive integration concerns out of the host system's tick loop
// and allows easy swapping between trap-based and cycle-accurate modes.

#include "core/iec_bus.hpp"
#include "core/warp_controller.hpp"
#include "devices/storage/drive_1541_system.hpp"

#include <array>
#include <memory>
#include <cstdint>

/// Maximum number of IEC drives attached simultaneously.
inline constexpr int MAX_IEC_DRIVES = 3;

/// Abstract base for any IEC drive system.
/// Provides the tick/reset/status interface that DriveSubsystem needs
/// without knowing the concrete drive type or template parameters.
class IECDriveBase {
public:
    virtual ~IECDriveBase() = default;
    virtual void tick() = 0;
    virtual void reset() = 0;
    virtual bool is_active() const = 0;
    virtual bool is_motor_on() const = 0;
    virtual bool is_led_on() const = 0;
    virtual const char* model_id() const = 0;
    virtual const char* display_name() const = 0;
    virtual void attach_iec_bus(IECBus* bus, int slot) = 0;

    // Disk image management
    virtual bool insert_disk(const char* filepath) = 0;
    virtual void eject_disk() = 0;
    virtual bool is_disk_inserted() const = 0;
};

/// Adapter that wraps a concrete C1541System<Traits> into the IECDriveBase
/// interface.  The adapter owns the drive system instance.
template <const DriveTraits& Traits>
class IECDriveAdapter : public IECDriveBase {
public:
    IECDriveAdapter() = default;

    void tick() override                            { drive_.tick(); }
    void reset() override                           { drive_.reset(); }
    bool is_active() const override                 { return drive_.is_active(); }
    bool is_motor_on() const override               { return drive_.is_motor_on(); }
    bool is_led_on() const override                 { return drive_.is_led_on(); }
    const char* model_id() const override           { return C1541System<Traits>::model_id(); }
    const char* display_name() const override       { return C1541System<Traits>::display_name(); }
    void attach_iec_bus(IECBus* bus, int slot) override { drive_.attach_iec_bus(bus, slot); }
    bool insert_disk(const char* filepath) override { return drive_.insert_disk(filepath); }
    void eject_disk() override                      { drive_.eject_disk(); }
    bool is_disk_inserted() const override          { return drive_.is_disk_inserted(); }

    C1541System<Traits>& drive() { return drive_; }
    const C1541System<Traits>& drive() const { return drive_; }

private:
    C1541System<Traits> drive_;
};

class DriveSubsystem {
public:
    DriveSubsystem() {
        iec_bus_.reset_all();
    }

    // ── Drive management ────────────────────────────────────────────────

    /// Attach a drive of the specified traits at the given device number (8-10).
    /// Returns a non-owning pointer to the type-erased drive, or nullptr on failure.
    template <const DriveTraits& Traits>
    IECDriveBase* attach_drive(uint8_t device_number = 8) {
        int slot = device_number - 8;
        if (slot < 0 || slot >= MAX_IEC_DRIVES) return nullptr;
        if (drives_[slot]) return nullptr;  // Slot occupied

        auto adapter = std::make_unique<IECDriveAdapter<Traits>>();
        adapter->attach_iec_bus(&iec_bus_, slot + 1);  // Slot 0 = host

        auto* ptr = adapter.get();
        drives_[slot] = std::move(adapter);
        return ptr;
    }

    /// Detach the drive at the given device number.
    void detach_drive(uint8_t device_number) {
        int slot = device_number - 8;
        if (slot >= 0 && slot < MAX_IEC_DRIVES)
            drives_[slot].reset();
    }

    /// Get a drive by device number (nullptr if not attached).
    IECDriveBase* get_drive(uint8_t device_number) {
        int slot = device_number - 8;
        if (slot < 0 || slot >= MAX_IEC_DRIVES) return nullptr;
        return drives_[slot].get();
    }

    // ── Per-cycle integration (called from host tick loop) ──────────────

    /// Push host CIA IEC output lines to the IEC bus (slot 0).
    ///
    /// The C64's CIA#2 port A bits:
    ///   Bit 3: ATN OUT  (directly drives IEC ATN)
    ///   Bit 4: CLK OUT  (directly drives IEC CLK via inverter)
    ///   Bit 5: DATA OUT (directly drives IEC DATA via inverter)
    ///
    /// Call this AFTER the host's CIA#2 tick, before advance_drives().
    void sync_host_to_iec(uint8_t cia2_port_a) {
        auto& host = iec_bus_.output(0);

        // ATN OUT (bit 3): CIA PA3 HIGH = ATN asserted (line LOW)
        host.set(iec::ATN, !(cia2_port_a & 0x08));

        // CLK OUT (bit 4): CIA PA4 HIGH → inverter → CLK LOW
        host.set(iec::CLK, !(cia2_port_a & 0x10));

        // DATA OUT (bit 5): CIA PA5 HIGH → inverter → DATA LOW
        host.set(iec::DATA, !(cia2_port_a & 0x20));
    }

    /// Read IEC bus combined state back into host CIA input format.
    ///
    /// CIA#2 port A input bits:
    ///   Bit 6: CLK IN  (directly from bus, active-low → bit HIGH when CLK is LOW)
    ///   Bit 7: DATA IN (from bus, active-low → bit HIGH when DATA is LOW)
    ///
    /// Call this BEFORE the host's CIA#2 tick to present current bus state.
    /// Returns the input bit pattern to OR into CIA#2 port A pins.
    uint8_t sync_iec_to_host() const {
        uint8_t combined = iec_bus_.combined();
        uint8_t pa_in = 0;

        // CLK IN (bit 6): bus CLK LOW → CIA PA6 HIGH (inverted)
        if (!(combined & (1u << iec::CLK)))
            pa_in |= 0x40;

        // DATA IN (bit 7): bus DATA LOW → CIA PA7 HIGH (inverted)
        if (!(combined & (1u << iec::DATA)))
            pa_in |= 0x80;

        return pa_in;
    }

    /// Advance all attached drives by one cycle.
    /// Also updates warp state based on drive activity.
    void advance_drives(uint64_t host_cycle) {
        bool any_active = false;

        for (auto& drive : drives_) {
            if (!drive) continue;
            drive->tick();
            any_active |= drive->is_active();
        }

        warp_.update(any_active, host_cycle);
    }

    // ── Warp control ────────────────────────────────────────────────────

    WarpController& warp() { return warp_; }
    const WarpController& warp() const { return warp_; }

    // ── Lifecycle ───────────────────────────────────────────────────────

    void reset() {
        iec_bus_.reset_all();
        warp_.reset();
        for (auto& drive : drives_)
            if (drive) drive->reset();
    }

    // ── Direct access ───────────────────────────────────────────────────

    IECBus& iec_bus() { return iec_bus_; }
    const IECBus& iec_bus() const { return iec_bus_; }

private:
    IECBus          iec_bus_;
    WarpController  warp_;

    std::array<std::unique_ptr<IECDriveBase>, MAX_IEC_DRIVES> drives_;
};
