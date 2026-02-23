#pragma once
/**
 * drive_1541.h - Commodore 1541 Disk Drive Peripheral
 *
 * Emulates a complete Commodore 1541 floppy disk drive as an IEC serial bus
 * device.  The 1541 is effectively a separate computer with its own:
 *   - MOS 6502 CPU (running at 1 MHz)
 *   - Two MOS 6522 VIA chips (VIA1 for IEC bus, VIA2 for drive mechanics)
 *   - 2 KB RAM
 *   - 16 KB ROM (DOS + controller firmware)
 *
 * ARCHITECTURE:
 * =============
 * This implementation operates at the IEC protocol level rather than at the
 * full cycle-accurate hardware level.  This provides correct behaviour for
 * standard LOAD/SAVE operations and most fast-loaders, while being far more
 * efficient than emulating the full 6502 + VIA hardware.
 *
 * The drive handles these IEC commands:
 *   LISTEN  → receive data (SAVE, channel I/O)
 *   TALK    → send data (LOAD, directory)
 *   OPEN    → open a file/channel
 *   CLOSE   → close a channel
 *   UNLISTEN / UNTALK → terminate current operation
 *
 * D64 DISK IMAGE:
 * ===============
 * The drive operates on D64 disk images (174,848 bytes for 35 tracks).
 * Files are located through the BAM (Block Availability Map) and directory
 * at track 18.
 *
 * FUTURE EXPANSION:
 * =================
 * - Full 6502 + 6522 cycle-accurate mode for custom fast-loaders
 * - G64 support for copy-protected disks
 * - Multiple drive units on the same IEC bus (device #8-11)
 */

#include "../../core/connector.h"
#include <cstdint>
#include <string>
#include <vector>
#include <array>

// ============================================================================
// IEC PROTOCOL STATES
// ============================================================================

/// IEC bus protocol state machine
enum class IECState {
    IDLE,           ///< Waiting for ATN assertion
    COMMAND,        ///< Receiving command byte under ATN
    LISTEN,         ///< Addressed as listener — receiving data
    TALK,           ///< Addressed as talker — sending data
    OPEN,           ///< OPEN secondary address — receiving filename
    CLOSE,          ///< CLOSE secondary address
};

/// IEC bus signal reading (matches ConnectorSignals::IECBit)
struct IECBusState {
    bool atn;       ///< ATN line state (true = asserted/low)
    bool clk;       ///< CLK line state
    bool data;      ///< DATA line state
};

// ============================================================================
// DRIVE CHANNEL
// ============================================================================

/// A single drive I/O channel (0-15).
struct DriveChannel {
    bool             open = false;
    std::string      filename;          ///< Opened filename
    std::vector<uint8_t> buffer;        ///< Data buffer (file contents or error channel)
    uint32_t         position = 0;      ///< Current read/write position
    bool             eof = false;       ///< End of data reached

    void clear() {
        open = false;
        filename.clear();
        buffer.clear();
        position = 0;
        eof = false;
    }
};

// ============================================================================
// DRIVE 1541
// ============================================================================

class Drive1541Device : public PeripheralDevice {
public:
    explicit Drive1541Device(uint8_t device_number = 8);
    ~Drive1541Device() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return name_.c_str(); }
    const char* get_id() const override   { return "1541"; }
    ConnectorType get_connector_type() const override { return ConnectorType::IEC_SERIAL; }
    void reset() override;
    void tick() override;
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override;

    bool has_activity() const override { return drive_led_; }

#ifdef IMGUI_VERSION
    void render_device_ui() override;
#endif

    // --- Drive-specific API --------------------------------------------

    /// Insert a D64 disk image.
    bool insert_disk(const char* filepath);

    /// Eject the current disk.
    void eject_disk();

    /// Swap disk — replaces the disk image without resetting drive state.
    /// Channels are invalidated (files become stale), but IEC protocol state,
    /// uploaded fastloader code, and VIA state are all preserved — exactly
    /// like physically swapping a floppy while the drive is still powered on.
    bool swap_disk(const char* filepath);

    /// Is a disk inserted?
    bool is_disk_inserted() const { return disk_inserted_; }

    /// Get the disk image path.
    const std::string& get_disk_path() const { return disk_path_; }

    /// Get/set device number (8-11).
    uint8_t get_device_number() const { return device_number_; }
    void set_device_number(uint8_t num);

    /// Get the current error channel message.
    const std::string& get_error_message() const { return error_message_; }

    /// Get drive busy/LED state.
    bool is_drive_led_on() const { return drive_led_; }

    // --- Disc Fliplist --------------------------------------------------
    // A pre-loaded list of disc images that can be cycled through with a
    // single keypress, like VICE's "attach next disc in fliplist" feature.
    // Ideal for multi-disc games: load disc 1, 2, 3... and press a key to
    // advance instead of navigating file dialogs for each swap prompt.

    /// Add a disc image path to the fliplist (avoids duplicates).
    void fliplist_add(const char* filepath);

    /// Remove a disc image from the fliplist by index.
    void fliplist_remove(int index);

    /// Clear the entire fliplist.
    void fliplist_clear();

    /// Swap to the next disc in the fliplist.  Wraps around.
    bool flip_next();

    /// Swap to the previous disc in the fliplist.  Wraps around.
    bool flip_prev();

    /// Get current fliplist.
    const std::vector<std::string>& get_fliplist() const { return fliplist_; }

    /// Get current fliplist index (-1 if empty or no match).
    int get_fliplist_index() const { return fliplist_index_; }

    // --- File Dialog Request (GUI communication) -----------------------

    /// Returns true if the device UI requested a file dialog (e.g. "Insert Disk...").
    bool wants_file_dialog() const { return wants_file_dialog_; }

    /// Clear the file dialog request flag (called by GUI after opening the dialog).
    void clear_file_dialog_request() { wants_file_dialog_ = false; }

    // --- Serial Trap API ------------------------------------------------
    // Called by the C64's KERNAL serial trap handlers to perform drive
    // operations without going through the IEC bit-banged protocol.
    // This mirrors VICE's serial-trap.c → fsdrive.c dispatch path.

    /// Begin OPEN on secondary address — prepare for filename accumulation.
    void trap_open(uint8_t sa);

    /// Close channel.
    void trap_close(uint8_t sa);

    /// Set secondary address for subsequent data transfer.
    void trap_second(uint8_t sa);

    /// CIOUT — receive byte from C64 (filename during OPEN, data during LISTEN).
    void trap_send(uint8_t byte);

    /// ACPTR — send byte to C64.  Returns status: 0=ok, 0x40=EOF.
    int trap_receive(uint8_t& byte);

    /// Complete pending OPEN with accumulated filename.
    void trap_unlisten();

    /// Clean up after TALK session.
    void trap_untalk();

private:
    // --- IEC Protocol --------------------------------------------------

    /// Process a complete command byte received under ATN.
    void process_command(uint8_t command);

    /// State machine for IEC bus communication.
    void iec_state_machine();

    /// Read IEC bus line states from connector port.
    IECBusState read_iec_bus() const;

    /// Drive the DATA and CLK lines (output to bus).
    void drive_iec_lines(bool data_out, bool clk_out);

    // --- D64 File Operations -------------------------------------------

    /// Load directory listing into channel buffer.
    void load_directory(DriveChannel& channel);

    /// Open a file from the D64 image and load into channel buffer.
    bool open_file(DriveChannel& channel, const std::string& filename);

    /// Set the error channel message.
    void set_error(int code, const char* message, int track = 0, int sector = 0);

    // --- D64 Low-Level -------------------------------------------------

    /// Read a sector from the D64 image.
    bool read_sector(uint8_t track, uint8_t sector, uint8_t* buffer);

    /// Convert track/sector to byte offset in the D64 image.
    uint32_t track_sector_to_offset(uint8_t track, uint8_t sector) const;

    /// Get the number of sectors on a given track.
    uint8_t sectors_per_track(uint8_t track) const;

    // --- Data members --------------------------------------------------

    std::string     name_;
    uint8_t         device_number_;     ///< IEC device number (default 8)

    // IEC protocol state
    IECState        iec_state_;
    uint8_t         current_command_;   ///< Last command byte received
    uint8_t         current_secondary_; ///< Current secondary address (0-15)
    bool            addressed_;         ///< True if this device is currently addressed
    uint32_t        output_signals_;    ///< Current output signal state (active-low)
    bool            prev_atn_;          ///< Previous ATN state for edge detection
    uint32_t        byte_counter_;      ///< Bytes transferred in current operation
    uint32_t        iec_cycle_counter_; ///< Cycle counter for IEC timing

    // IEC data transfer state
    uint8_t         shift_register_;    ///< Current byte being shifted in/out
    int8_t          bit_counter_;       ///< Current bit position (7..0)
    bool            eoi_sent_;          ///< EOI (End-Or-Identify) sent for last byte
    std::vector<uint8_t> receive_buffer_; ///< Incoming data buffer (LISTEN/OPEN)

    // Drive channels (0-15)
    static constexpr int NUM_CHANNELS = 16;
    std::array<DriveChannel, NUM_CHANNELS> channels_;

    // Disk image
    std::vector<uint8_t> disk_image_;
    std::string          disk_path_;
    bool                 disk_inserted_;

    // Drive state
    bool                 drive_led_;
    std::string          error_message_;

    // Disc fliplist
    std::vector<std::string> fliplist_;
    int                      fliplist_index_ = -1;

    // GUI communication
    bool                     wants_file_dialog_ = false;  ///< Set by render_device_ui, cleared by GUI

    // Serial trap state (used by KERNAL trap path)
    uint8_t         trap_sa_ = 0;                ///< Current secondary address for trap ops
    bool            trap_awaiting_name_ = false;  ///< True between OPEN and UNLISTEN
    std::string     trap_name_buffer_;            ///< Filename accumulator during OPEN

    // D64 constants (prefixed to avoid collision with d64_format.h macros)
    static constexpr uint32_t DRIVE_D64_STD_SIZE     = 174848;
    static constexpr uint32_t DRIVE_D64_STD_SIZE_ERR = 175531;
    static constexpr uint32_t DRIVE_D64_EXT_SIZE     = 196608;
    static constexpr uint32_t DRIVE_D64_EXT_SIZE_ERR = 197376;
    static constexpr uint8_t  DIR_TRACK = 18;
    static constexpr uint8_t  DIR_SECTOR = 1;
    static constexpr uint8_t  BAM_TRACK = 18;
    static constexpr uint8_t  BAM_SECTOR = 0;
};
