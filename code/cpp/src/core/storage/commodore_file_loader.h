#pragma once

/**
 * Commodore File Loader - System-Independent File Format Handling
 * 
 * Shared file format parsers for Commodore 8-bit systems (C64, VIC-20, C16/Plus4).
 * Each system plugs in its own memory interface; the parsing logic is universal.
 * 
 * Supported formats:
 *   PRG  - Program files (2-byte load address + data)
 *   D64  - 1541 disk images (35-track, 683 sectors)
 *   T64  - Tape archive images (directory + file data)
 *   TAP  - Raw tape pulse data (header + pulse widths)
 *   CRT  - Cartridge images (header + CHIP packets) [C64-specific header, generic loader]
 *   BIN  - Raw binary (no header, load address supplied by caller)
 * 
 * Design philosophy:
 *   - File I/O and format parsing are fully shared
 *   - Memory access stays system-specific via callbacks or caller-managed buffers
 *   - BASIC V2 SYS parsing uses a read callback to work with any memory layout
 *   - All allocations are documented; caller frees returned buffers
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Common Types
// ============================================================================

/** Memory read callback — used for BASIC parsing after data is loaded into system memory */
typedef uint8_t (*commodore_mem_read_fn)(void* ctx, uint16_t addr);

// ============================================================================
// PRG Format — Universal Commodore Program Files
// ============================================================================

/**
 * Result of loading a PRG file.
 * The caller is responsible for freeing `data` with free().
 */
typedef struct {
    uint8_t* data;          /**< Program data (excluding 2-byte header). Caller frees. */
    size_t   data_size;     /**< Size of data in bytes */
    uint16_t load_addr;     /**< Load address from PRG header (little-endian) */
    uint16_t end_addr;      /**< End address (load_addr + data_size) */
} commodore_prg_t;

/**
 * Load a PRG file from disk.
 * Reads the 2-byte little-endian load address and the remaining data.
 * 
 * @param filepath   Path to the .prg file
 * @param out_prg    Output structure (data will be heap-allocated)
 * @return true on success. Caller must free out_prg->data.
 */
bool commodore_prg_load(const char* filepath, commodore_prg_t* out_prg);

/**
 * Parse a PRG from an in-memory buffer (e.g. extracted from D64/T64).
 * 
 * @param buffer     Raw PRG data including 2-byte header
 * @param size       Size of buffer
 * @param out_prg    Output structure (data will be heap-allocated copy)
 * @return true on success. Caller must free out_prg->data.
 */
bool commodore_prg_parse(const uint8_t* buffer, size_t size, commodore_prg_t* out_prg);

/**
 * Free a previously loaded PRG structure's data.
 */
void commodore_prg_free(commodore_prg_t* prg);

// ============================================================================
// BIN Format — Raw Binary (No Header)
// ============================================================================

/**
 * Load a raw binary file. Returns heap-allocated data. Caller frees.
 * 
 * @param filepath   Path to the .bin file
 * @param out_data   Output data pointer (heap-allocated)
 * @param out_size   Output data size
 * @return true on success
 */
bool commodore_bin_load(const char* filepath, uint8_t** out_data, size_t* out_size);

// ============================================================================
// BASIC V2 SYS Address Parsing — Shared Across All Commodore Systems
// ============================================================================

/**
 * System-specific BASIC parameters.
 * Each Commodore system has a different BASIC start address but shares
 * the same BASIC V2 tokenization.
 */
typedef struct {
    uint16_t basic_start;       /**< BASIC program start address ($0801 C64, $1001 VIC-20/C16) */
    uint8_t  sys_token;         /**< SYS token value (0x9E for BASIC V2, 0x9E for BASIC 3.5) */
    uint8_t  rem_token;         /**< REM token value (0x8F for all Commodore BASIC versions) */
    uint8_t  peek_token;        /**< PEEK token value (0xC2 for BASIC V2) */
    uint16_t basic_start_ptr_lo; /**< Zero-page address of BASIC start pointer low byte */
    uint16_t basic_start_ptr_hi; /**< Zero-page address of BASIC start pointer high byte */
} commodore_basic_params_t;

/** Pre-defined BASIC parameters for common systems */
extern const commodore_basic_params_t COMMODORE_BASIC_C64;      /**< C64: BASIC V2, start $0801 */
extern const commodore_basic_params_t COMMODORE_BASIC_VIC20;    /**< VIC-20: BASIC V2, start $1001 */
extern const commodore_basic_params_t COMMODORE_BASIC_C16;      /**< C16/Plus4: BASIC 3.5, start $1001 */

/**
 * Result of BASIC SYS address parsing.
 */
typedef struct {
    uint16_t sys_address;       /**< SYS target address (0 = not found) */
    uint16_t line_number;       /**< BASIC line number containing SYS */
    bool     found;             /**< Whether a SYS statement was found */
} commodore_basic_sys_t;

/**
 * Parse BASIC program to find a SYS address.
 * 
 * Walks the BASIC line-link chain starting at `start_addr`, looking for
 * a SYS token. Handles:
 *   - Simple numeric addresses (e.g., SYS 2061)
 *   - PEEK expressions (e.g., SYS PEEK(43)+PEEK(44)*256+offset)
 *   - Skips REM lines
 *   - Scans up to max_lines lines
 * 
 * @param mem_read    Memory read callback (reads from the system's memory)
 * @param mem_ctx     Context pointer passed to mem_read
 * @param start_addr  Address of the BASIC program start in memory
 * @param params      System-specific BASIC parameters
 * @param max_lines   Maximum number of BASIC lines to scan (10 is typical)
 * @param out_sys     Output result
 * @return true if a SYS address was found
 */
bool commodore_basic_parse_sys(commodore_mem_read_fn mem_read, void* mem_ctx,
                               uint16_t start_addr,
                               const commodore_basic_params_t* params,
                               int max_lines,
                               commodore_basic_sys_t* out_sys);

// ============================================================================
// D64 Format — 1541 Disk Image
// ============================================================================

/** D64 disk geometry constants */
#define D64_STANDARD_SIZE       174848  /**< 35 tracks, no error bytes */
#define D64_STANDARD_SIZE_ERR   175531  /**< 35 tracks, with error bytes */
#define D64_EXTENDED_SIZE       196608  /**< 40 tracks, no error bytes */
#define D64_EXTENDED_SIZE_ERR   197376  /**< 40 tracks, with error bytes */
#define D64_MAX_DIR_ENTRIES     144     /**< Maximum directory entries */
#define D64_SECTOR_SIZE         256     /**< Bytes per sector */
#define D64_DIR_TRACK           18      /**< Directory track */
#define D64_DIR_SECTOR           1      /**< First directory sector */
#define D64_BAM_TRACK           18      /**< BAM track */
#define D64_BAM_SECTOR            0     /**< BAM sector */

/** Commodore DOS file type constants (lower 4 bits of file type byte) */
#define D64_FTYPE_DEL  0x00
#define D64_FTYPE_SEQ  0x01
#define D64_FTYPE_PRG  0x02
#define D64_FTYPE_USR  0x03
#define D64_FTYPE_REL  0x04

/** D64 file type byte flags */
#define D64_FTYPE_CLOSED    0x80    /**< File properly closed (bit 7) */
#define D64_FTYPE_LOCKED    0x40    /**< File locked (bit 6) */
#define D64_FTYPE_MASK      0x07    /**< File type mask (bits 0-2) */

/** D64 directory entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    uint8_t  file_type;             /**< Raw file type byte (includes closed/locked flags) */
    uint8_t  start_track;           /**< First track of file data */
    uint8_t  start_sector;          /**< First sector of file data */
    uint16_t size_blocks;           /**< File size in 254-byte blocks */
} commodore_d64_entry_t;

/** D64 directory listing */
typedef struct {
    commodore_d64_entry_t entries[D64_MAX_DIR_ENTRIES];
    int count;                      /**< Number of entries found */
    char disk_name[17];             /**< Disk name from BAM, null-terminated */
    char disk_id[6];                /**< Disk ID + DOS type, null-terminated */
} commodore_d64_directory_t;

/**
 * D64 disk image handle (opaque to callers of extract functions).
 */
typedef struct {
    uint8_t* data;                  /**< Raw disk image data */
    size_t   data_size;             /**< Size of disk image */
    int      num_tracks;            /**< 35 or 40 */
    bool     has_errors;            /**< Whether error bytes are present */
    bool     owns_data;             /**< Whether we allocated data (and should free it) */
} commodore_d64_t;

/**
 * Open a D64 disk image from file.
 * 
 * @param filepath  Path to the .d64 file
 * @param out_d64   Output disk handle
 * @return true on success. Caller must call commodore_d64_close().
 */
bool commodore_d64_open(const char* filepath, commodore_d64_t* out_d64);

/**
 * Open a D64 disk image from an in-memory buffer.
 * The buffer is NOT copied; caller must keep it alive until close.
 * 
 * @param data      Raw D64 data
 * @param size      Size of data
 * @param out_d64   Output disk handle
 * @return true on success
 */
bool commodore_d64_open_mem(const uint8_t* data, size_t size, commodore_d64_t* out_d64);

/**
 * Read the directory of a D64 disk image.
 * 
 * @param d64       Opened disk image
 * @param out_dir   Output directory listing
 * @return true on success
 */
bool commodore_d64_read_directory(const commodore_d64_t* d64, commodore_d64_directory_t* out_dir);

/**
 * Extract a file from a D64 disk image by directory index.
 * Follows the track/sector chain and returns raw file data as a PRG
 * (with 2-byte load address header if it's a PRG file type).
 * 
 * @param d64       Opened disk image
 * @param entry_idx Directory entry index (0-based)
 * @param out_data  Output data pointer (heap-allocated, caller frees)
 * @param out_size  Output data size
 * @return true on success
 */
bool commodore_d64_extract_file(const commodore_d64_t* d64, int entry_idx,
                                uint8_t** out_data, size_t* out_size);

/**
 * Extract the first PRG file from a D64 disk image.
 * Convenience function that finds the first closed PRG and extracts it.
 * 
 * @param d64       Opened disk image
 * @param out_prg   Output PRG structure
 * @return true if a PRG was found and extracted
 */
bool commodore_d64_extract_first_prg(const commodore_d64_t* d64, commodore_prg_t* out_prg);

/**
 * Close a D64 disk image and free resources.
 */
void commodore_d64_close(commodore_d64_t* d64);

// ============================================================================
// T64 Format — Tape Archive Image
// ============================================================================

/** T64 header constants */
#define T64_HEADER_SIZE     64
#define T64_ENTRY_SIZE      32
#define T64_MAX_ENTRIES     256

/** T64 directory entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    uint8_t  c64s_file_type;       /**< C64s file type (1=normal, 3=frozen memory image) */
    uint8_t  file_type;            /**< 1541 file type (0x82=PRG etc.) */
    uint16_t start_addr;           /**< Start address (load address) */
    uint16_t end_addr;             /**< End address */
    uint32_t data_offset;          /**< Offset in T64 file to actual data */
    uint32_t data_size;            /**< Computed size (end_addr - start_addr) */
} commodore_t64_entry_t;

/** T64 archive listing */
typedef struct {
    char tape_name[25];             /**< Tape name from header, null-terminated */
    uint16_t version;               /**< T64 version */
    uint16_t max_entries;           /**< Max directory entries */
    uint16_t used_entries;          /**< Used directory entries */
    commodore_t64_entry_t entries[T64_MAX_ENTRIES];
    int count;                      /**< Actual valid entry count */
} commodore_t64_directory_t;

/** T64 archive handle */
typedef struct {
    uint8_t* data;                  /**< Raw T64 data */
    size_t   data_size;             /**< Size of T64 data */
    bool     owns_data;             /**< Whether we allocated data */
} commodore_t64_t;

/**
 * Open a T64 tape archive from file.
 */
bool commodore_t64_open(const char* filepath, commodore_t64_t* out_t64);

/**
 * Read the directory of a T64 tape archive.
 */
bool commodore_t64_read_directory(const commodore_t64_t* t64, commodore_t64_directory_t* out_dir);

/**
 * Extract a file from a T64 archive as a PRG.
 */
bool commodore_t64_extract_file(const commodore_t64_t* t64, int entry_idx, commodore_prg_t* out_prg);

/**
 * Extract the first PRG from a T64 archive.
 */
bool commodore_t64_extract_first_prg(const commodore_t64_t* t64, commodore_prg_t* out_prg);

/**
 * Close a T64 archive and free resources.
 */
void commodore_t64_close(commodore_t64_t* t64);

// ============================================================================
// TAP Format — Raw Tape Pulse Data
// ============================================================================

/** TAP file header */
typedef struct {
    char     signature[12];         /**< "C64-TAPE-RAW" or "C16-TAPE-RAW" */
    uint8_t  version;               /**< 0 or 1 */
    uint8_t  platform;              /**< 0=C64, 1=VIC-20, 2=C16 */
    uint8_t  video_standard;        /**< 0=PAL, 1=NTSC */
    uint8_t  reserved;
    uint32_t data_size;             /**< Size of pulse data */
} commodore_tap_header_t;

/**
 * Read TAP file header without loading full pulse data.
 * Useful for file type detection and metadata display.
 * 
 * @param filepath   Path to the .tap file
 * @param out_header Output header structure
 * @return true on success
 */
bool commodore_tap_read_header(const char* filepath, commodore_tap_header_t* out_header);

/**
 * Identify the platform of a TAP file.
 * Checks the signature to determine if it's C64, VIC-20, or C16 tape data.
 * 
 * @param filepath   Path to the .tap file
 * @return 0=C64, 1=VIC-20, 2=C16, -1=unknown/error
 */
int commodore_tap_identify_platform(const char* filepath);

// ============================================================================
// CRT Format — Cartridge Image (Primarily C64, structure is generic)
// ============================================================================

/** CRT header (first 64 bytes) */
typedef struct {
    char     signature[16];         /**< "C64 CARTRIDGE   " */
    uint32_t header_length;         /**< Header length (usually 0x40) */
    uint16_t version;               /**< CRT version (big-endian) */
    uint16_t hardware_type;         /**< Cartridge hardware type (big-endian) */
    uint8_t  exrom;                 /**< EXROM line state */
    uint8_t  game;                  /**< GAME line state */
    uint8_t  reserved[6];
    char     name[32];              /**< Cartridge name, null-terminated */
} commodore_crt_header_t;

/** CRT CHIP packet header */
typedef struct {
    char     signature[4];          /**< "CHIP" */
    uint32_t packet_length;         /**< Total packet length (big-endian) */
    uint16_t chip_type;             /**< 0=ROM, 1=RAM, 2=Flash (big-endian) */
    uint16_t bank_number;           /**< Bank number (big-endian) */
    uint16_t load_address;          /**< Load address (big-endian) */
    uint16_t rom_size;              /**< ROM image size (big-endian) */
} commodore_crt_chip_t;

/**
 * Read CRT file header for identification and metadata.
 * 
 * @param filepath    Path to the .crt file
 * @param out_header  Output header structure
 * @return true on success
 */
bool commodore_crt_read_header(const char* filepath, commodore_crt_header_t* out_header);

// ============================================================================
// LNX Format — Lynx Archive (Multi-file CBM container)
// ============================================================================

/** Maximum files in a Lynx archive */
#define LNX_MAX_FILES           256
/** Maximum length of the BASIC dissolve header to scan */
#define LNX_MAX_BASIC_LENGTH    1024

/** Lynx archive file entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    char     file_type;             /**< 'P'=PRG, 'S'=SEQ, 'U'=USR, 'R'=REL, 'D'=DEL */
    unsigned blocks;                /**< Number of 254-byte data blocks */
    unsigned last_block_len;        /**< Bytes used in last block (or record len for REL) */
    size_t   data_offset;           /**< Byte offset of file data within the archive */
    size_t   data_length;           /**< Actual file data length in bytes */
} commodore_lynx_entry_t;

/** Lynx archive directory */
typedef struct {
    unsigned header_blocks;         /**< Blocks occupied by header+directory */
    unsigned file_count;            /**< Number of files in archive */
    commodore_lynx_entry_t entries[LNX_MAX_FILES];
} commodore_lynx_directory_t;

/** Lynx archive handle */
typedef struct {
    uint8_t* data;                  /**< Raw archive data */
    size_t   data_size;             /**< Size of archive */
    bool     owns_data;             /**< Whether we allocated data */
} commodore_lynx_t;

/**
 * Open a Lynx archive from file.
 * @param filepath  Path to the .lnx file
 * @param out_lynx  Output archive handle
 * @return true on success. Caller must call commodore_lynx_close().
 */
bool commodore_lynx_open(const char* filepath, commodore_lynx_t* out_lynx);

/**
 * Read the directory of a Lynx archive.
 * Parses the header, skips the BASIC dissolve stub, and reads all file entries.
 * @param lynx     Opened archive
 * @param out_dir  Output directory listing
 * @return true on success
 */
bool commodore_lynx_read_directory(const commodore_lynx_t* lynx, commodore_lynx_directory_t* out_dir);

/**
 * Extract a file from a Lynx archive by index as a PRG.
 * @param lynx      Opened archive
 * @param dir       Previously-read directory
 * @param entry_idx Directory entry index (0-based)
 * @param out_prg   Output PRG structure (data heap-allocated, caller frees)
 * @return true on success
 */
bool commodore_lynx_extract_file(const commodore_lynx_t* lynx,
                                 const commodore_lynx_directory_t* dir,
                                 int entry_idx, commodore_prg_t* out_prg);

/**
 * Extract ALL PRG files from a Lynx archive.
 * @param lynx       Opened archive
 * @param out_prgs   Output array of PRG structures (caller allocates, max LNX_MAX_FILES)
 * @param max_prgs   Size of out_prgs array
 * @param out_count  Number of PRGs actually extracted
 * @return true if at least one PRG was extracted
 */
bool commodore_lynx_extract_all_prgs(const commodore_lynx_t* lynx,
                                     commodore_prg_t* out_prgs, int max_prgs,
                                     int* out_count);

/**
 * Close a Lynx archive and free resources.
 */
void commodore_lynx_close(commodore_lynx_t* lynx);

// ============================================================================
// High-Level Convenience — Unified Load Function
// ============================================================================

/** Load result type */
typedef enum {
    COMMODORE_LOAD_NONE = 0,
    COMMODORE_LOAD_PRG,             /**< PRG data loaded (check prg field) */
    COMMODORE_LOAD_D64,             /**< D64 opened, first PRG extracted (check prg field) */
    COMMODORE_LOAD_T64,             /**< T64 opened, first PRG extracted (check prg field) */
    COMMODORE_LOAD_TAP,             /**< TAP identified (check tap_header field) */
    COMMODORE_LOAD_CRT,             /**< CRT identified (check crt_header field) */
    COMMODORE_LOAD_BIN,             /**< Raw binary loaded (check prg field, load_addr = 0) */
    COMMODORE_LOAD_LNX,             /**< Lynx archive: all PRGs extracted (check lynx_files) */
    COMMODORE_LOAD_ERROR            /**< Load failed */
} commodore_load_type_t;

/** Maximum PRG files returned from a Lynx archive load */
#define COMMODORE_LOAD_MAX_LNX_FILES  64

/** Unified load result */
typedef struct {
    commodore_load_type_t type;
    commodore_prg_t       prg;          /**< Valid for PRG/D64/T64/BIN results */
    commodore_tap_header_t tap_header;  /**< Valid for TAP results */
    commodore_crt_header_t crt_header;  /**< Valid for CRT results */
    commodore_prg_t       lynx_files[COMMODORE_LOAD_MAX_LNX_FILES]; /**< Valid for LNX results */
    int                   lynx_file_count;  /**< Number of files in lynx_files[] */
    char error_msg[256];                /**< Error message if type == ERROR */
} commodore_load_result_t;

/**
 * Load any supported Commodore file format.
 * Detects format by extension and content, extracts data.
 * For container formats (D64, T64), extracts the first PRG automatically.
 * 
 * @param filepath  Path to the file
 * @param out       Output result. Caller must call commodore_load_result_free().
 * @return true on success (type != ERROR)
 */
bool commodore_load_file(const char* filepath, commodore_load_result_t* out);

/**
 * Free resources in a load result.
 */
void commodore_load_result_free(commodore_load_result_t* result);

/**
 * Get human-readable name for a load type.
 */
const char* commodore_load_type_name(commodore_load_type_t type);

#ifdef __cplusplus
}
#endif
