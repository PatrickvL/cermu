/*
 * atari_vector_system.cpp — Atari vector arcade system implementation
 *
 * Shared implementation for Asteroids and Lunar Lander.
 *
 * Tick loop (per CPU cycle at 1.512 MHz):
 *   1. CPU PHI2 → address + data on bus
 *   2. Address decode:
 *      - $0000-$03FF → Work RAM (via MemoryBus)
 *      - $2000-$2FFF → I/O reads (manual dispatch)
 *      - $3000-$3FFF → I/O writes (manual dispatch)
 *      - $4000-$47FF → Vector RAM (via MemoryBus)
 *      - $5000-$57FF → Vector ROM (via MemoryBus)
 *      - $6000/$6800+ → Program ROM (via MemoryBus)
 *      - Unmapped → open bus
 *   3. CPU PHI1
 *   4. DVG tick (runs concurrently)
 *   5. NMI timer (fires every ~6048 CPU cycles ≈ 250 Hz)
 *
 * ROM file format:
 *   Raw binary ROM dumps.  The system expects the full program ROM
 *   concatenated with vector ROM if separate.
 */

#include "systems/arcade/atari_vector/atari_vector_system.hpp"
#include "core/dip_switch.hpp"
#include "core/rom_set.hpp"
#include "core/system_registry.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <type_traits>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

namespace atv = atari_vector_constants;

// ============================================================================
// ROM SET DESCRIPTORS
// ============================================================================
//
// Each Atari vector game shipped with multiple ROM chips.  These descriptors
// map the individual ROM files (identified by part-number substrings) to their
// memory addresses so the generic rom_set loader can place them correctly.
//
// Asteroids:  3 × 2 KB program ROMs ($6800-$7FFF) + 1 × 2 KB vector ROM ($5000)
// Lunar Lander: 4 × 2 KB program ROMs ($6000-$7FFF) + 1–2 × 2 KB vector ROMs ($5000+)

// ── Asteroids Rev 1 ──────────────────────────────────────────────────────────

static const RomEntryDescriptor ast_v1_entries[] = {
    // Vector ROM — DVG display list ROM at $5000
    { {"035127.01", "035127-01"},   0x5000, 2048, true  },
    // Program ROMs — three 2 KB chips covering $6800-$7FFF
    { {"035145.01", "035145-01"},   0x6800, 2048, true  },   // socket ef2
    { {"035144.01", "035144-01"},   0x7000, 2048, true  },   // socket h2
    { {"035143.01", "035143-01"},   0x7800, 2048, true  },   // socket j2 (reset vector)
};

static const RomSetDescriptor ast_v1_romset = {
    "Asteroids Rev 1", "Asteroids",
    ast_v1_entries, 4
};

// ── Asteroids Rev 2 ──────────────────────────────────────────────────────────

static const RomEntryDescriptor ast_v2_entries[] = {
    { {"035127.02", "035127-02"},   0x5000, 2048, true  },
    { {"035145.02", "035145-02"},   0x6800, 2048, true  },
    { {"035144.02", "035144-02"},   0x7000, 2048, true  },
    { {"035143.02", "035143-02"},   0x7800, 2048, true  },
};

static const RomSetDescriptor ast_v2_romset = {
    "Asteroids Rev 2", "Asteroids",
    ast_v2_entries, 4
};

// ── Lunar Lander Rev 1 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v1_entries[] = {
    // Vector ROMs — 034599 at $4800, 034598 at $5000
    { {"034599.01", "034599-01", "LLVROM1"},  0x4800, 2048, true  },
    { {"034598.01", "034598-01", "LLVROM0"},  0x5000, 2048, true  },
    // Program ROMs — four 2 KB chips covering $6000-$7FFF
    { {"034572.01", "034572-01"},             0x6000, 2048, true  },   // socket c1
    { {"034571.01", "034571-01", "LLPROM2"},  0x6800, 2048, true  },   // socket de1
    { {"034570.01", "034570-01", "LLPROM1"},  0x7000, 2048, true  },   // socket f1
    { {"034569.01", "034569-01", "LLPROM0"},  0x7800, 2048, true  },   // socket j1 (reset vector)
    // Language PROM (optional)
    { {"034597.01", "034597-01"},             0x0000, 2048, false },
};

static const RomSetDescriptor ll_v1_romset = {
    "Lunar Lander Rev 1", "LunarLander",
    ll_v1_entries, 7
};

// ── Lunar Lander Rev 2 ──────────────────────────────────────────────────────

static const RomEntryDescriptor ll_v2_entries[] = {
    // Vector ROMs — same layout as v1: 034599 at $4800, 034598 at $5000
    { {"034599.01", "034599-01", "LLVROM1"},  0x4800, 2048, true  },
    { {"034598.01", "034598-01", "LLVROM0"},  0x5000, 2048, true  },
    // Program ROMs — rev 2 chips
    { {"034572.02", "034572-02"},             0x6000, 2048, true  },
    { {"034571.02", "034571-02"},             0x6800, 2048, true  },
    { {"034570.02", "034570-02"},             0x7000, 2048, true  },
    { {"034569.02", "034569-02"},             0x7800, 2048, true  },
    { {"034597.01", "034597-01"},             0x0000, 2048, false },
};

static const RomSetDescriptor ll_v2_romset = {
    "Lunar Lander Rev 2", "LunarLander",
    ll_v2_entries, 7
};

// ── Asteroids Deluxe Rev 1 ───────────────────────────────────────────────
//
// Program ROMs: 4 × 2 KB at $6000-$7FFF
// Vector ROMs:  2 × 2 KB
//   036800 at DVG offset $0800 → CPU $4800 (load_address $4800)
//   036799 at DVG offset $1000 → CPU $5000 (load_address $5000)

static const RomEntryDescriptor ad_v1_entries[] = {
    // Vector ROMs
    { {"036800.01", "036800-01"},   0x4800, 2048, true  },   // DVG offset $0800
    { {"036799.01", "036799-01"},   0x5000, 2048, true  },   // DVG offset $1000
    // Program ROMs
    { {"036430.01", "036430-01"},   0x6000, 2048, true  },   // socket c1
    { {"036431.01", "036431-01"},   0x6800, 2048, true  },   // socket de1
    { {"036432.01", "036432-01"},   0x7000, 2048, true  },   // socket f1
    { {"036433.02", "036433-02"},   0x7800, 2048, true  },   // socket j1 (reset vector)
};

static const RomSetDescriptor ad_v1_romset = {
    "Asteroids Deluxe Rev 1", "AsteroidsDeluxe",
    ad_v1_entries, 6
};

// ── Asteroids Deluxe Rev 2 ───────────────────────────────────────────────

static const RomEntryDescriptor ad_v2_entries[] = {
    // Vector ROMs (036800 updated, 036799 same as v1)
    { {"036800.02", "036800-02"},   0x4800, 2048, true  },
    { {"036799.01", "036799-01"},   0x5000, 2048, true  },
    // Program ROMs
    { {"036430.02", "036430-02"},   0x6000, 2048, true  },
    { {"036431.02", "036431-02"},   0x6800, 2048, true  },
    { {"036432.02", "036432-02"},   0x7000, 2048, true  },
    { {"036433.03", "036433-03"},   0x7800, 2048, true  },
};

static const RomSetDescriptor ad_v2_romset = {
    "Asteroids Deluxe Rev 2", "AsteroidsDeluxe",
    ad_v2_entries, 6
};

// ── Battlezone Rev 1 ─────────────────────────────────────────────────────
//
// Battlezone: 6 × 2 KB program ROMs ($5000-$7FFF), 2 × 2 KB vector ROMs ($3000-$3FFF)

static const RomEntryDescriptor bz_v1_entries[] = {
    // Vector ROMs
    { {"036422.01", "036422-01"},   0x3000, 2048, true  },   // vector ROM 1
    { {"036421.01", "036421-01"},   0x3800, 2048, true  },   // vector ROM 2
    // Program ROMs ($5000-$7FFF)
    { {"036414.01", "036414-01"},   0x5000, 2048, true  },
    { {"036413.01", "036413-01"},   0x5800, 2048, true  },
    { {"036412.01", "036412-01"},   0x6000, 2048, true  },
    { {"036411.01", "036411-01"},   0x6800, 2048, true  },
    { {"036410.01", "036410-01"},   0x7000, 2048, true  },
    { {"036409.01", "036409-01"},   0x7800, 2048, true  },
};

static const RomSetDescriptor bz_v1_romset = {
    "Battlezone Rev 1", "Battlezone",
    bz_v1_entries, 8
};

// ── Battlezone Rev 2 ─────────────────────────────────────────────────────

static const RomEntryDescriptor bz_v2_entries[] = {
    { {"036422.01", "036422-01"},   0x3000, 2048, true  },
    { {"036421.01", "036421-01"},   0x3800, 2048, true  },
    { {"036414.02", "036414-02"},   0x5000, 2048, true  },
    { {"036413.02", "036413-02"},   0x5800, 2048, true  },
    { {"036412.02", "036412-02"},   0x6000, 2048, true  },
    { {"036411.02", "036411-02"},   0x6800, 2048, true  },
    { {"036410.02", "036410-02"},   0x7000, 2048, true  },
    { {"036409.02", "036409-02"},   0x7800, 2048, true  },
};

static const RomSetDescriptor bz_v2_romset = {
    "Battlezone Rev 2", "Battlezone",
    bz_v2_entries, 8
};

// ── Red Baron ────────────────────────────────────────────────────────────
//
// Red Baron: 6 × 2 KB program ROMs ($5000-$7FFF), 2 × 2 KB vector ROMs ($3000-$3FFF)

static const RomEntryDescriptor rb_entries[] = {
    // Vector ROMs
    { {"037006.01", "037006-01"},   0x3000, 2048, true  },   // vector ROM 1
    { {"037007.01", "037007-01"},   0x3800, 2048, true  },   // vector ROM 2
    // Program ROMs ($5000-$7FFF)
    { {"037001.01", "037001-01"},   0x5000, 2048, true  },
    { {"037000.01", "037000-01"},   0x5800, 2048, true  },
    { {"036999.01", "036999-01"},   0x6000, 2048, true  },
    { {"036998.01", "036998-01"},   0x6800, 2048, true  },
    { {"036997.01", "036997-01"},   0x7000, 2048, true  },
    { {"036996.01", "036996-01"},   0x7800, 2048, true  },
};

static const RomSetDescriptor rb_romset = {
    "Red Baron", "RedBaron",
    rb_entries, 8
};

// ── Tempest Rev 3 ────────────────────────────────────────────────────────
//
// Tempest (1980): revision 3 PCBs (-03/-04) use 2532 EPROMs (4 KB).
// Older PCBs (-01/-02) use 2716 EPROMs (2 KB) — separate descriptor.
//
// PCB layout from tempest3_readme.txt (2532 config):
//   VECTOR ROM: .138 at N/P3 (4 KB)
//   PROGRAM ROMs: .133 D1, .134 F1, .235 J1, .136 L/M1, .237 P1 (5 × 4 KB)
//
// MAME ROM addresses:
//   Vector ROM:  $3000-$3FFF (4 KB)
//   Program ROM: $9000-$DFFF (20 KB), $E000-$FFFF mirrors $C000-$DFFF

static const RomEntryDescriptor tempest_v3_entries[] = {
    // Vector ROM (2532 at N/P3 — spans both 2716 sockets N/P3 + R3)
    { {"136002.138", "136002-138"},  0x3000, 4096, true  },
    // Program ROMs (5 × 2532 = 20 KB at $9000-$DFFF)
    { {"136002.133", "136002-133"},  0x9000, 4096, true  },   // D1
    { {"136002.134", "136002-134"},  0xA000, 4096, true  },   // F1
    { {"136002.235", "136002-235"},  0xB000, 4096, true  },   // J1 (V3)
    { {"136002.136", "136002-136"},  0xC000, 4096, true  },   // L/M1
    { {"136002.237", "136002-237"},  0xD000, 4096, true  },   // P1 (V3)
};

static const RomSetDescriptor tempest_v3_romset = {
    "Tempest Rev 3 (2532)", "Tempest",
    tempest_v3_entries, 6
};

// ── Tempest Rev 1 (2716 EPROMs) ─────────────────────────────────────────
//
// Original Tempest release.  12 × 2716 (2 KB each):
//   .111, .112 = vector ROMs ($3000-$3FFF)
//   .113-.122  = program ROMs ($9000-$DFFF)
// No revision-specific replacements — all base part numbers.

static const RomEntryDescriptor tempest_v1_entries[] = {
    // Vector ROMs (2 × 2716)
    { {"136002.111", "136002-111"},  0x3000, 2048, true  },   // N/P3
    { {"136002.112", "136002-112"},  0x3800, 2048, true  },   // R3
    // Program ROMs (10 × 2716 = 20 KB at $9000-$DFFF)
    { {"136002.113", "136002-113"},  0x9000, 2048, true  },   // D1
    { {"136002.114", "136002-114"},  0x9800, 2048, true  },   // E1
    { {"136002.115", "136002-115"},  0xA000, 2048, true  },   // F1
    { {"136002.116", "136002-116"},  0xA800, 2048, true  },   // H1 (V1)
    { {"136002.117", "136002-117"},  0xB000, 2048, true  },   // J1 (V1)
    { {"136002.118", "136002-118"},  0xB800, 2048, true  },   // K1
    { {"136002.119", "136002-119"},  0xC000, 2048, true  },   // L/M1
    { {"136002.120", "136002-120"},  0xC800, 2048, true  },   // M/N1
    { {"136002.121", "136002-121"},  0xD000, 2048, true  },   // P1
    { {"136002.122", "136002-122"},  0xD800, 2048, true  },   // R1 (V1)
};

static const RomSetDescriptor tempest_v1_romset = {
    "Tempest Rev 1 (2716)", "Tempest",
    tempest_v1_entries, 12
};

// ── Tempest Rev 2 (2716 EPROMs) ─────────────────────────────────────────
//
// V2 replaces two chips from v1:
//   .117 → .217 at J1 ($B000)
//   .122 → .222 at R1 ($D800)
// All other chips same as v1.

static const RomEntryDescriptor tempest_v2_entries[] = {
    // Vector ROMs (2 × 2716)
    { {"136002.111", "136002-111"},  0x3000, 2048, true  },   // N/P3
    { {"136002.112", "136002-112"},  0x3800, 2048, true  },   // R3
    // Program ROMs (10 × 2716 = 20 KB at $9000-$DFFF)
    { {"136002.113", "136002-113"},  0x9000, 2048, true  },   // D1
    { {"136002.114", "136002-114"},  0x9800, 2048, true  },   // E1
    { {"136002.115", "136002-115"},  0xA000, 2048, true  },   // F1
    { {"136002.116", "136002-116"},  0xA800, 2048, true  },   // H1 (shared V1/V2)
    { {"136002.217", "136002-217"},  0xB000, 2048, true  },   // J1 (V2)
    { {"136002.118", "136002-118"},  0xB800, 2048, true  },   // K1
    { {"136002.119", "136002-119"},  0xC000, 2048, true  },   // L/M1
    { {"136002.120", "136002-120"},  0xC800, 2048, true  },   // M/N1
    { {"136002.121", "136002-121"},  0xD000, 2048, true  },   // P1
    { {"136002.222", "136002-222"},  0xD800, 2048, true  },   // R1 (V2)
};

static const RomSetDescriptor tempest_v2_romset = {
    "Tempest Rev 2 (2716)", "Tempest",
    tempest_v2_entries, 12
};

// ── Tempest Rev 3 (2716 upgrade) ────────────────────────────────────────
//
// For -01/-02 PCBs: same v3 code in 2716 EPROMs (2 KB each).
// V3-specific replacements: .316 (H1), .217 (J1), .222 (R1).
// All other chips (.113-.121) are shared base v1/v2 ROMs.

static const RomEntryDescriptor tempest_v3_2716_entries[] = {
    // Vector ROMs (2 × 2716)
    { {"136002.111", "136002-111"},  0x3000, 2048, true  },   // N/P3
    { {"136002.112", "136002-112"},  0x3800, 2048, true  },   // R3
    // Program ROMs (10 × 2716 = 20 KB at $9000-$DFFF)
    { {"136002.113", "136002-113"},  0x9000, 2048, true  },   // D1
    { {"136002.114", "136002-114"},  0x9800, 2048, true  },   // E1
    { {"136002.115", "136002-115"},  0xA000, 2048, true  },   // F1
    { {"136002.316", "136002-316"},  0xA800, 2048, true  },   // H1 (V3)
    { {"136002.217", "136002-217"},  0xB000, 2048, true  },   // J1 (V2/V3)
    { {"136002.118", "136002-118"},  0xB800, 2048, true  },   // K1
    { {"136002.119", "136002-119"},  0xC000, 2048, true  },   // L/M1
    { {"136002.120", "136002-120"},  0xC800, 2048, true  },   // M/N1
    { {"136002.121", "136002-121"},  0xD000, 2048, true  },   // P1
    { {"136002.222", "136002-222"},  0xD800, 2048, true  },   // R1 (V2/V3)
};

static const RomSetDescriptor tempest_v3_2716_romset = {
    "Tempest Rev 3 (2716)", "Tempest",
    tempest_v3_2716_entries, 12
};

// ── Gravitar Rev 2 ───────────────────────────────────────────────────────
//
// Gravitar (1982): "bwidow" board (MAME bwidow.cpp).
// PCB layout from README.gravitar:
//   CODE  ROMs: .201-.206 at sockets D1–M1 (row 1) — 6 × 4 KB
//   VECTOR ROMs: .207-.210 at sockets M/N7–L7 (row 7) — 3 × 4 KB + 1 × 2 KB
//
// MAME ROM addresses (single 64 KB CPU address space):
//   Vector ROM:  $2800-$5FFF (14 KB)
//   Program ROM: $9000-$EFFF (24 KB), $F000-$FFFF mirrors $E000

static const RomEntryDescriptor gravitar_v2_entries[] = {
    // Vector ROMs — AVG display list ROM at $2800-$5FFF
    { {"136010.210", "136010-210"},  0x2800, 2048, true  },   // L7    (2 KB)
    { {"136010.207", "136010-207"},  0x3000, 4096, true  },   // M/N7  (4 KB)
    { {"136010.208", "136010-208"},  0x4000, 4096, true  },   // N/P7  (4 KB)
    { {"136010.209", "136010-209"},  0x5000, 4096, true  },   // R7    (4 KB)
    // Program ROMs — CPU code at $9000-$EFFF
    { {"136010.201", "136010-201"},  0x9000, 4096, true  },   // D1
    { {"136010.202", "136010-202"},  0xA000, 4096, true  },   // E/F1
    { {"136010.203", "136010-203"},  0xB000, 4096, true  },   // H1
    { {"136010.204", "136010-204"},  0xC000, 4096, true  },   // J1
    { {"136010.205", "136010-205"},  0xD000, 4096, true  },   // K/L1
    { {"136010.206", "136010-206"},  0xE000, 4096, true  },   // M1 (reset vector via mirror)
};

static const RomSetDescriptor gravitar_v2_romset = {
    "Gravitar Rev 2", "Gravitar",
    gravitar_v2_entries, 10
};

// ── Gravitar Rev 3 ───────────────────────────────────────────────────────
//
// Rev 3 replaces .209→.309 (vector) and .201-.206→.301-.306 (program).
// Shared with v2: .210, .207, .208 (vector ROMs).

static const RomEntryDescriptor gravitar_v3_entries[] = {
    // Vector ROMs — same layout as v2 except .209→.309
    { {"136010.210", "136010-210"},  0x2800, 0, true  },   // L7    (2 KB, but v3 dumps may be padded to 4 KB)
    { {"136010.207", "136010-207"},  0x3000, 4096, true  },   // M/N7  (4 KB)
    { {"136010.208", "136010-208"},  0x4000, 4096, true  },   // N/P7  (4 KB)
    { {"136010.309", "136010-309"},  0x5000, 4096, true  },   // R7    (4 KB, replaces .209)
    // Program ROMs — .301-.306 replace .201-.206
    { {"136010.301", "136010-301"},  0x9000, 4096, true  },   // D1
    { {"136010.302", "136010-302"},  0xA000, 4096, true  },   // E/F1
    { {"136010.303", "136010-303"},  0xB000, 4096, true  },   // H1
    { {"136010.304", "136010-304"},  0xC000, 4096, true  },   // J1
    { {"136010.305", "136010-305"},  0xD000, 4096, true  },   // K/L1
    { {"136010.306", "136010-306"},  0xE000, 4096, true  },   // M1 (reset vector via mirror)
};

static const RomSetDescriptor gravitar_v3_romset = {
    "Gravitar Rev 3", "Gravitar",
    gravitar_v3_entries, 10
};

// ── Space Duel ───────────────────────────────────────────────────────────
//
// Space Duel (1982): different board from bwidow (spacduel_map in MAME).
// PCB layout from space_duel_readme.txt:
//   CODE  ROMs: .102-.105 at sockets N/P1–J1 (row 1) + .201 at R1 — 5 × 4 KB
//   VECTOR ROMs: .106 at R7 (2 KB) + .107 at N/P7 (4 KB)
//
// MAME ROM addresses:
//   Vector ROM:  $2800-$3FFF (6 KB)
//   Program ROM: $4000-$8FFF (20 KB)

static const RomEntryDescriptor spaceduel_entries[] = {
    // Vector ROMs — AVG display list ROM at $2800-$3FFF
    { {"136006.106", "136006-106"},  0x2800, 2048, true  },   // R7    (2 KB)
    { {"136006.107", "136006-107"},  0x3000, 4096, true  },   // N/P7  (4 KB)
    // Program ROMs — CPU code at $4000-$8FFF
    { {"136006.201", "136006-201"},  0x4000, 4096, true  },   // R1
    { {"136006.102", "136006-102"},  0x5000, 4096, true  },   // N/P1
    { {"136006.103", "136006-103"},  0x6000, 4096, true  },   // M1
    { {"136006.104", "136006-104"},  0x7000, 4096, true  },   // K/L1
    { {"136006.105", "136006-105"},  0x8000, 4096, true  },   // J1
};

static const RomSetDescriptor spaceduel_romset = {
    "Space Duel", "SpaceDuel",
    spaceduel_entries, 7
};

// ── Black Widow ──────────────────────────────────────────────────────────
//
// Black Widow (1982): same "bwidow" board as Gravitar.
// PCB layout from black_widow_readme.txt:
//   CODE  ROMs: .101-.106 at sockets D1–M1 (row 1) — 6 × 4 KB
//   VECTOR ROMs: .107-.110 at sockets L7–R7 (row 7) — 1 × 2 KB + 3 × 4 KB
//
// MAME ROM addresses (single 64 KB CPU address space):
//   Vector ROM:  $2800-$5FFF (14 KB)
//   Program ROM: $9000-$EFFF (24 KB), $F000-$FFFF mirrors $E000

static const RomEntryDescriptor blackwidow_entries[] = {
    // Vector ROMs — AVG display list ROM at $2800-$5FFF
    { {"136017.107", "136017-107"},  0x2800, 2048, true  },   // L7    (2 KB)
    { {"136017.108", "136017-108"},  0x3000, 4096, true  },   // M/N7  (4 KB)
    { {"136017.109", "136017-109"},  0x4000, 4096, true  },   // N/P7  (4 KB)
    { {"136017.110", "136017-110"},  0x5000, 4096, true  },   // R7    (4 KB)
    // Program ROMs — CPU code at $9000-$EFFF
    { {"136017.101", "136017-101"},  0x9000, 4096, true  },   // D1
    { {"136017.102", "136017-102"},  0xA000, 4096, true  },   // E/F1
    { {"136017.103", "136017-103"},  0xB000, 4096, true  },   // H1
    { {"136017.104", "136017-104"},  0xC000, 4096, true  },   // J1
    { {"136017.105", "136017-105"},  0xD000, 4096, true  },   // K/L1
    { {"136017.106", "136017-106"},  0xE000, 4096, true  },   // M1 (reset vector via mirror)
};

static const RomSetDescriptor blackwidow_romset = {
    "Black Widow", "BlackWidow",
    blackwidow_entries, 10
};

// ── Major Havoc Rev 3 ────────────────────────────────────────────────────
//
// Major Havoc (1983): SIGNIFICANTLY more complex than other AVG games.
//
// *** KNOWN BROKEN — needs dedicated architectural work ***
//
// Issues requiring per-system implementation:
//   1. Memory map constants (MH_VECRAM/VECROM/PROGROM_BASE) are all wrong
//   2. CPU program ROM bank switching ($1740, 4 × 8 KB pages at $2000-$3FFF)
//   3. CPU RAM bank switching ($1780)
//   4. AVG paged vector ROM ("avg" region, 32 KB, 4 × 8 KB pages via STROBE2)
//   5. Separate "vectorrom" region (8 KB at CPU $5000-$6FFF)
//   6. Full 16-bit addressing (USES_15BIT_ADDR should be false)
//   7. Gamma CPU (second 6502 for sound/input via quad POKEY)
//   8. Fixed program ROM at $8000-$FFFF (32 KB)
//   9. I/O addresses all wrong (VGGO=$1640, VGRST=$16C0, WD=$1680)
//
// Rev 3 zip contents: .106,.107,.108 (AVG ROMs, 16KB each),
//   .210 (vectorrom, 8KB), .215-.217,.318 (program ROMs, 16KB each)
//
// Descriptor below uses APPROXIMATE addresses to allow ROM set matching.
// The game will NOT run correctly until the above issues are addressed.

static const RomEntryDescriptor majorhavoc_v3_entries[] = {
    // AVG-only ROMs — not CPU-addressable, need AVG banking support
    { {"136025.106", "136025-106"},  0x3000, 0, true  },   // 6H  (16 KB, avg bank)
    { {"136025.107", "136025-107"},  0x3000, 0, false },   // 6JK (16 KB, avg bank — overlaps, deferred)
    { {"136025.108", "136025-108"},  0x3000, 0, false },   // 9S  (16 KB, avg bank — overlaps, deferred)
    // CPU-visible vector ROM (vectorrom region)
    { {"136025.210", "136025-210"},  0x3000, 0, true  },   // 6KL (8 KB, CPU $5000-$6FFF)
    // Fixed program ROMs — should be at $8000-$FFFF (32 KB)
    { {"136025.215", "136025-215"},  0x4000, 0, true  },   // 1Q   (16 KB)
    { {"136025.216", "136025-216"},  0x4000, 0, false },   // 1M/N (16 KB, bank page)
    { {"136025.217", "136025-217"},  0x4000, 0, false },   // 1L   (16 KB, bank page)
    { {"136025.318", "136025-318"},  0x4000, 0, false },   // 1N/P (16 KB, bank page, v3 replaces .218)
};

static const RomSetDescriptor majorhavoc_v3_romset = {
    "Major Havoc Rev 3", "MajorHavoc",
    majorhavoc_v3_entries, 8
};

// ============================================================================
// DIP SWITCH DEFINITIONS (per-game, from MAME schematics)
// ============================================================================
//
// Each game has 1–2 DIP switch banks on the PCB. The switch positions,
// bit masks, and named settings mirror MAME's PORT_DIPNAME definitions.
// Default values match MAME factory defaults (typically English, 3 lives,
// 1 coin / 1 credit).

// ── Shared language settings ─────────────────────────────────────────────────

static constexpr DipSetting kLang4[] = {
    { "English",  0x00 },
    { "German",   0x01 },
    { "French",   0x02 },
    { "Spanish",  0x03 },
};

static constexpr DipSetting kLang4_Upper[] = {
    { "English",  0x00 },
    { "German",   0x40 },
    { "French",   0x80 },
    { "Spanish",  0xC0 },
};

// ── Asteroids DSW1 (4-bit, multiplexed at $2800–$2803) ──────────────────────

static constexpr DipSetting kAstLives[] = {
    { "4",  0x00 },
    { "3",  0x04 },   // default
};

static constexpr DipSwitch kAstSwitches[] = {
    { "Language",  0x03, 0, kLang4,    4 },
    { "Lives",     0x04, 1, kAstLives, 2 },
};

static constexpr DipSwitchBankDescriptor kAstDSW1 = {
    "DSW1 (N10)", kAstSwitches, 2
};

// ── Asteroids Deluxe DSW1 (4-bit, multiplexed at $2800–$2803) ────────────────

static constexpr DipSetting kAdLives[] = {
    { "2–4 (bonus dependent)", 0x00 },
    { "3",                     0x04 },   // default
    { "4",                     0x08 },
    { "5",                     0x0C },
};

static constexpr DipSwitch kAdSwitches[] = {
    { "Language",  0x03, 0, kLang4,   4 },
    { "Lives",     0x0C, 1, kAdLives, 4 },
};

static constexpr DipSwitchBankDescriptor kAdDSW1 = {
    "DSW1 (N10)", kAdSwitches, 2
};

// ── Lunar Lander DSW1 (4-bit, multiplexed at $2800–$2803) ───────────────────

static constexpr DipSetting kLlRightCoin[] = {
    { "*1",  0x00 },
    { "*4",  0x01 },   // default
    { "*5",  0x02 },
    { "*6",  0x03 },
};

static constexpr DipSetting kLlLang[] = {
    { "English",  0x00 },   // default
    { "French",   0x04 },
    { "Spanish",  0x08 },
    { "German",   0x0C },
};

static constexpr DipSwitch kLlSwitches[] = {
    { "Right Coin",  0x03, 1, kLlRightCoin, 4 },
    { "Language",    0x0C, 0, kLlLang,      4 },
};

static constexpr DipSwitchBankDescriptor kLlDSW1 = {
    "DSW1 (B4)", kLlSwitches, 2
};

// ── Battlezone DSW0 (full byte at $0A00) ─────────────────────────────────────

static constexpr DipSetting kBzLives[] = {
    { "2",  0x00 },
    { "3",  0x01 },   // default
    { "4",  0x02 },
    { "5",  0x03 },
};

static constexpr DipSetting kBzMissile[] = {
    { "5000",   0x00 },
    { "15000",  0x04 },   // default
    { "25000",  0x08 },
    { "50000",  0x0C },
};

static constexpr DipSetting kBzBonus[] = {
    { "25000 and 100000",  0x00 },
    { "15000 and 100000",  0x10 },   // default
    { "50000 and 100000",  0x20 },
    { "None",              0x30 },
};

static constexpr DipSwitch kBzDsw0Switches[] = {
    { "Lives",              0x03, 1, kBzLives,     4 },
    { "Missile appears at", 0x0C, 1, kBzMissile,   4 },
    { "Bonus Life",         0x30, 1, kBzBonus,     4 },
    { "Language",           0xC0, 0, kLang4_Upper, 4 },
};

static constexpr DipSwitchBankDescriptor kBzDSW0 = {
    "DSW0 (N11)", kBzDsw0Switches, 4
};

// ── Battlezone DSW1 (full byte at $0C00) ─────────────────────────────────────

static constexpr DipSetting kBzCoinB[] = {
    { "2 Coins / 1 Credit",  0x00 },
    { "1 Coin / 1 Credit",   0x01 },   // default
};

static constexpr DipSetting kBzCoinA[] = {
    { "4 Coins / 1 Credit",  0x00 },
    { "3 Coins / 1 Credit",  0x02 },
    { "2 Coins / 1 Credit",  0x04 },
    { "1 Coin / 1 Credit",   0x06 },   // default
};

static constexpr DipSwitch kBzDsw1Switches[] = {
    { "Coin B",  0x01, 1, kBzCoinB, 2 },
    { "Coin A",  0x06, 3, kBzCoinA, 4 },
};

static constexpr DipSwitchBankDescriptor kBzDSW1 = {
    "DSW1 (P12)", kBzDsw1Switches, 2
};

// ── Red Baron DSW0 (full byte at $0A00) ──────────────────────────────────────

static constexpr DipSetting kRbBonus[] = {
    { "2000",  0x00 },
    { "4000",  0x04 },   // default
    { "6000",  0x08 },
    { "None",  0x0C },
};

static constexpr DipSetting kRbLives[] = {
    { "2",  0x00 },
    { "3",  0x10 },   // default
    { "4",  0x20 },
    { "5",  0x30 },
};

static constexpr DipSwitch kRbDsw0Switches[] = {
    { "Language",    0x03, 0, kLang4,    4 },
    { "Bonus Life",  0x0C, 1, kRbBonus, 4 },
    { "Lives",       0x30, 1, kRbLives, 4 },
};

static constexpr DipSwitchBankDescriptor kRbDSW0 = {
    "DSW0 (N11)", kRbDsw0Switches, 3
};

// Red Baron DSW1 — same as Battlezone DSW1 (coinage)
static constexpr DipSwitchBankDescriptor kRbDSW1 = {
    "DSW1 (P12)", kBzDsw1Switches, 2
};

// ── Tempest IN2 (bits 0–6 at $0E00; bit 7 = VBLANK, added dynamically) ─────

static constexpr DipSetting kTmpCoinage[] = {
    { "Free Play",            0x00 },
    { "1 Coin / 2 Credits",   0x01 },
    { "2 Coins / 1 Credit",   0x02 },
    { "1 Coin / 1 Credit",    0x03 },   // default
};

static constexpr DipSetting kTmpRightCoin[] = {
    { "*1",  0x00 },   // default
    { "*4",  0x04 },
    { "*5",  0x08 },
    { "*6",  0x0C },
};

static constexpr DipSetting kTmpLang[] = {
    { "English",  0x00 },   // default
    { "French",   0x10 },
    { "German",   0x20 },
    { "Spanish",  0x30 },
};

static constexpr DipSetting kTmpBonusCoin[] = {
    { "None",     0x00 },   // default
    { "Enabled",  0x40 },
};

static constexpr DipSwitch kTmpSwitches[] = {
    { "Coinage",          0x03, 3, kTmpCoinage,   4 },
    { "Right Coin",       0x0C, 0, kTmpRightCoin, 4 },
    { "Language",         0x30, 0, kTmpLang,       4 },
    { "Bonus Coin Adder", 0x40, 0, kTmpBonusCoin, 2 },
};

static constexpr DipSwitchBankDescriptor kTmpIN2 = {
    "IN2 / DIP (R8)", kTmpSwitches, 4
};

// ── Gravitar IN4 (full byte at $8800) ────────────────────────────────────────

static constexpr DipSetting kGravLives[] = {
    { "1",         0x00 },
    { "3",         0x01 },   // default
    { "5",         0x02 },
    { "Infinite",  0x03 },
};

static constexpr DipSetting kGravDifficulty[] = {
    { "Easy",     0x00 },
    { "Medium",   0x04 },   // default
    { "Hard",     0x08 },
    { "Hardest",  0x0C },
};

static constexpr DipSetting kGravBonus[] = {
    { "10000",  0x00 },   // default
    { "20000",  0x10 },
    { "30000",  0x20 },
    { "None",   0x30 },
};

static constexpr DipSwitch kGravSwitches[] = {
    { "Lives",       0x03, 1, kGravLives,      4 },
    { "Difficulty",  0x0C, 1, kGravDifficulty, 4 },
    { "Bonus Life",  0x30, 0, kGravBonus,      4 },
    { "Language",    0xC0, 0, kLang4_Upper,    4 },
};

static constexpr DipSwitchBankDescriptor kGravIN4 = {
    "IN4 / DIP (M12)", kGravSwitches, 4
};

// ── Black Widow IN4 (full byte at $8800) ─────────────────────────────────────

static constexpr DipSetting kBwCoinage[] = {
    { "1 Coin / 1 Credit",   0x00 },   // default
    { "2 Coins / 1 Credit",  0x01 },
    { "Free Play",            0x02 },
    { "1 Coin / 2 Credits",   0x03 },
};

static constexpr DipSetting kBwRightCoin[] = {
    { "*1",  0x00 },   // default
    { "*4",  0x04 },
    { "*5",  0x08 },
    { "*6",  0x0C },
};

static constexpr DipSetting kBwLeftCoin[] = {
    { "*1",  0x00 },   // default
    { "*2",  0x10 },
};

static constexpr DipSetting kBwBonusCoin[] = {
    { "None",     0x00 },   // default
    { "Enabled",  0x20 },
};

static constexpr DipSwitch kBwSwitches[] = {
    { "Coinage",          0x03, 0, kBwCoinage,   4 },
    { "Right Coin",       0x0C, 0, kBwRightCoin, 4 },
    { "Left Coin",        0x10, 0, kBwLeftCoin,  2 },
    { "Bonus Coin Adder", 0x20, 0, kBwBonusCoin, 2 },
    { "Language",         0xC0, 0, kLang4_Upper, 4 },
};

static constexpr DipSwitchBankDescriptor kBwIN4 = {
    "IN4 / DIP (M12)", kBwSwitches, 5
};

// ── Space Duel IN3 (bit-indexed at $0900–$0907) ─────────────────────────────
// MAME: active-LOW DIP convention — default values have bits SET.

static constexpr DipSetting kSdLives[] = {
    { "3",  0x01 },   // default (bit set)
    { "4",  0x00 },
};

static constexpr DipSetting kSdDifficulty[] = {
    { "Easy",  0x02 },   // default (bit set)
    { "Hard",  0x00 },
};

static constexpr DipSetting kSdLang[] = {
    { "English",  0x08 },   // default (bit set)
    { "German",   0x00 },
};

static constexpr DipSetting kSdBonus[] = {
    { "8000",   0x10 },   // default (bit set)
    { "10000",  0x00 },
};

static constexpr DipSetting kSdCabinet[] = {
    { "Upright",   0x00 },   // default
    { "Cocktail",  0x20 },
};

static constexpr DipSetting kSdCoinage[] = {
    { "1 Coin / 1 Credit",   0x00 },   // default
    { "2 Coins / 1 Credit",  0x40 },
    { "Free Play",            0x80 },
    { "1 Coin / 2 Credits",   0xC0 },
};

static constexpr DipSwitch kSdSwitches[] = {
    { "Lives",       0x01, 0, kSdLives,      2 },
    { "Difficulty",  0x02, 0, kSdDifficulty, 2 },
    { "Language",    0x08, 0, kSdLang,       2 },
    { "Bonus Life",  0x10, 0, kSdBonus,     2 },
    { "Cabinet",     0x20, 0, kSdCabinet,   2 },
    { "Coinage",     0xC0, 0, kSdCoinage,   4 },
};

static constexpr DipSwitchBankDescriptor kSdIN3 = {
    "IN3 / DIP (D4)", kSdSwitches, 6
};

// ── Per-game DIP switch bank descriptor lookup ───────────────────────────────

template<AtariVectorVariant V>
static const DipSwitchBankDescriptor* get_dip_descriptor_0() {
    if constexpr (V == AtariVectorVariant::ASTEROIDS)          return &kAstDSW1;
    else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) return &kAdDSW1;
    else if constexpr (V == AtariVectorVariant::LUNAR_LANDER)  return &kLlDSW1;
    else if constexpr (V == AtariVectorVariant::BATTLEZONE)    return &kBzDSW0;
    else if constexpr (V == AtariVectorVariant::RED_BARON)     return &kRbDSW0;
    else if constexpr (V == AtariVectorVariant::TEMPEST)       return &kTmpIN2;
    else if constexpr (V == AtariVectorVariant::GRAVITAR)      return &kGravIN4;
    else if constexpr (V == AtariVectorVariant::BLACK_WIDOW)   return &kBwIN4;
    else if constexpr (V == AtariVectorVariant::SPACE_DUEL)    return &kSdIN3;
    else return nullptr;  // Major Havoc: deferred
}

template<AtariVectorVariant V>
static const DipSwitchBankDescriptor* get_dip_descriptor_1() {
    if constexpr (V == AtariVectorVariant::BATTLEZONE)  return &kBzDSW1;
    else if constexpr (V == AtariVectorVariant::RED_BARON) return &kRbDSW1;
    else return nullptr;  // Most games: single DIP bank
}

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<AtariVectorVariant V>
static HardwareTraits create_vector_hardware_traits() {

    HardwareTraits ht = {};

    // Display — per-game visible area matching MAME set_visarea.
    // DVG games keep the default 1024×1024; AVG games use game-specific sizes.
    int disp_w = atv::DISPLAY_WIDTH;
    int disp_h = atv::DISPLAY_HEIGHT;
    if constexpr (V == AtariVectorVariant::BATTLEZONE)  { disp_w = 580; disp_h = 400; }
    if constexpr (V == AtariVectorVariant::RED_BARON)   { disp_w = 520; disp_h = 400; }
    if constexpr (V == AtariVectorVariant::TEMPEST)     { disp_w = 580; disp_h = 570; }
    if constexpr (V == AtariVectorVariant::GRAVITAR)    { disp_w = 420; disp_h = 400; }
    if constexpr (V == AtariVectorVariant::SPACE_DUEL)  { disp_w = 540; disp_h = 400; }
    if constexpr (V == AtariVectorVariant::BLACK_WIDOW) { disp_w = 480; disp_h = 440; }
    if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) { disp_w = 300; disp_h = 260; }

    ht.display.native_width    = disp_w;
    ht.display.native_height   = disp_h;
    ht.display.visible_width   = disp_w;
    ht.display.visible_height  = disp_h;
    ht.display.format          = FramebufferFormat::RGBA8888;
    ht.display.palette_size    = 0;  // Vector display — no palette
    ht.display.pixel_aspect_ratio = 1.0f;
    ht.display.has_overscan    = false;

    // Audio
    ht.audio.format             = AudioFormat::CUSTOM;
    ht.audio.sample_rate_hz     = atv::DEFAULT_SAMPLE_RATE;
    ht.audio.channels           = 1;
    ht.audio.chip_name          = "Discrete";

    // Timing
    ht.timing.cpu_frequency_hz   = atv::CPU_FREQ_HZ;
    ht.timing.video_frequency_hz = atv::CPU_FREQ_HZ;
    ht.timing.audio_sample_rate_hz = atv::DEFAULT_SAMPLE_RATE;
    ht.timing.target_fps         = atv::TARGET_FPS;
    ht.timing.cycles_per_frame   = atv::CYCLES_PER_FRAME;
    ht.timing.standard           = VideoStandard::CUSTOM;

    return ht;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<AtariVectorVariant V>
AtariVectorSystem<V>::AtariVectorSystem()
    : System()
    , pins_(MOS6502::default_bus_state())
    , nmi_counter_(atv::NMI_PERIOD_CYCLES)
{
    hardware_traits_ = create_vector_hardware_traits<V>();
    current_palette_ = hardware_traits_.display.default_palette;
}

template<AtariVectorVariant V>
AtariVectorSystem<V>::~AtariVectorSystem() = default;

// ============================================================================
// SYSTEM DESCRIPTORS — compile-time traits → runtime descriptor
// ============================================================================

template<AtariVectorVariant V>
static SystemDescriptor create_system_descriptor() {
    using T = AtariVectorTraits<V>;
    return {
        T::NAME, T::SHORT_NAME, T::DESCRIPTION, T::DATA_FOLDER,
        std::vector<const char*>(std::begin(T::ALIASES), std::end(T::ALIASES)),
        nullptr,
        create_vector_hardware_traits<V>(),
        [](const format_descriptor_t*, const char* filepath,
           const uint8_t*, size_t size) -> SystemProbeResult {
            SystemProbeResult result = { 0.0f, {} };
            const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
            if (!ext) return result;
            if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
                for (auto expected : T::PROBE_ROM_SIZES) {
                    if (size == expected) {
                        result.confidence = 0.3f;
                        break;
                    }
                }
            }
            return result;
        },
        "Atari", T::YEAR, fam65xx::MOS6502Traits.display_name, SystemType::Arcade
    };
}

template<AtariVectorVariant V>
static SystemDescriptor& descriptor_instance() {
    static SystemDescriptor desc = create_system_descriptor<V>();
    return desc;
}

template<AtariVectorVariant V>
const SystemDescriptor& AtariVectorSystem<V>::get_descriptor() const {
    return descriptor_instance<V>();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;

    // Apply DIP switch settings from custom_settings
    for (int b = 0; b < 2; b++) {
        if (!dip_bank_[b].descriptor) continue;
        for (int i = 0; i < dip_bank_[b].descriptor->num_switches; i++) {
            const auto& sw = dip_bank_[b].descriptor->switches[i];
            std::string key = std::string("dip.") + dip_bank_[b].descriptor->name + "." + sw.name;
            auto it = config_.custom_settings.find(key);
            if (it != config_.custom_settings.end()) {
                int idx = DipSwitchBank::find_setting(sw, it->second.c_str());
                if (idx >= 0) dip_bank_[b].set_selection(i, idx);
            }
        }
    }

    return true;
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::initialize() {
    using Traits = AtariVectorTraits<V>;
    printf("%s: Initializing system\n", Traits::NAME);

    register_board(&board_);

    // Bind all chips from superset chipset — order matches make_atv_manifest<V>().
    {   size_t slot_idx_ = 0;
        // Core chips (always present)
        board_.bind_chip(slot_idx_++, &board_.work_ram);
        board_.bind_chip(slot_idx_++, &board_.vec_ram);
        board_.bind_chip(slot_idx_++, &board_.vec_rom);
        board_.bind_chip(slot_idx_++, &board_.prog_rom);
        board_.bind_chip(slot_idx_++, &board_.m6502);
        board_.bind_chip(slot_idx_++, &board_.vg);
        board_.bind_chip(slot_idx_++, &board_.latch);
        // Optional chips — order matches make_atv_manifest<V>() extra tuple
        if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            board_.bind_chip(slot_idx_++, &board_.prog_rom_hi);
            board_.bind_chip(slot_idx_++, &board_.pokey1);
        } else if constexpr (Traits::HAS_POKEY && Traits::HAS_EAROM) {
            board_.bind_chip(slot_idx_++, &board_.pokey1);
            board_.bind_chip(slot_idx_++, &board_.earom);
        } else if constexpr (Traits::HAS_POKEY) {
            board_.bind_chip(slot_idx_++, &board_.pokey1);
        }
    }
    board_.create_chips(&pins_);
    vec_ram_  = &board_.vec_ram;
    vec_rom_  = &board_.vec_rom;
    prog_rom_ = &board_.prog_rom;

    // 16-bit address games have a 3rd ROMChip for upper address space ($8000-$FFFF)
    if constexpr (!Traits::USES_15BIT_ADDR) {
        prog_rom_hi_ = &board_.prog_rom_hi;
    }

    // Wire MemoryBus page tables
    board_.apply(bus_);

    // Initialize CPU
    board_.m6502.init();
    board_.m6502.reset();

    // Initialize vector generator (DVG or AVG via ChipSet)
    vg().init();

    // Tempest-specific AVG configuration:
    //  - STAT uses bit 11 to select color vs intensity update
    //  - Monitor is rotated 90°, so X/Y axes are swapped
    if constexpr (V == AtariVectorVariant::TEMPEST) {
        vg().set_tempest_stat(true);
        vg().set_swap_xy(true);
    }

    // Per-game display area (matches MAME set_visarea dimensions).
    // AVG games get exact visible areas; DVG games keep the default 1024×1024.
    if constexpr (std::is_same_v<VideoChip, avg_t>) {
        if constexpr (V == AtariVectorVariant::BATTLEZONE) {
            vg().set_display_area(580, 400);
        } else if constexpr (V == AtariVectorVariant::RED_BARON) {
            vg().set_display_area(520, 400);
        } else if constexpr (V == AtariVectorVariant::TEMPEST) {
            vg().set_display_area(580, 570);
        } else if constexpr (V == AtariVectorVariant::GRAVITAR) {
            vg().set_display_area(420, 400);
        } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            vg().set_display_area(540, 400);
        } else if constexpr (V == AtariVectorVariant::BLACK_WIDOW) {
            vg().set_display_area(480, 440);
        } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
            vg().set_display_area(300, 260);
        }
    }

    // Register all manifest-created chips for the Hardware menu
    register_bus_chips(board_);

    // Register vector generator for the Hardware menu
    register_chip(&vg(), Traits::VIDEO_CHIP_NAME, Traits::VIDEO_CHIP_NAME, "Video");

    // Initialize POKEY (for games that have it)
    if constexpr (Traits::HAS_POKEY) {
        board_.pokey1.init();
        register_chip(&board_.pokey1, "POKEY", "POKEY", "Sound");
    }

    // Video port — VectorVideoPort for signal-based rendering
    video_port_ = std::make_unique<VectorVideoPort>();
    video_port_->bind_frame_output(&last_frame_data_);

    // Wire vector generator to the video output
    vg().set_video_out(&video_port_->output());

    // Wire vector generator memory — pointers are stable after board_.create_chips()
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atv::CPU_FREQ_HZ, atv::DEFAULT_SAMPLE_RATE);

    // Wire POKEY audio output
    if constexpr (Traits::HAS_POKEY) {
        board_.pokey1.set_audio_port(audio_port_.get());
    }

    // Space Duel: DIP switches are wired to POKEY1 pot inputs.
    // MAME overrides ALLPOT to return the DIP bank value directly.
    // Individual POT reads also return per-bit values (228 = open, 0 = grounded).
    if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
        board_.pokey1.allpot_read_callback = [](void* ctx) -> uint8_t {
            return static_cast<AtariVectorSystem*>(ctx)->dip_bank_[0].value;
        };
        board_.pokey1.allpot_read_context = this;
        board_.pokey1.pot_read_callback = [](void* ctx, uint8_t pot_index) -> uint8_t {
            auto* sys = static_cast<AtariVectorSystem*>(ctx);
            // Pot line grounded (0) when switch active, open (228) when inactive
            return (sys->dip_bank_[0].value & (1 << pot_index)) ? 228 : 0;
        };
        board_.pokey1.pot_read_context = this;
    }

    // Initialize DIP switch banks with per-game descriptors (MAME factory defaults)
    dip_bank_[0].init(get_dip_descriptor_0<V>());
    dip_bank_[1].init(get_dip_descriptor_1<V>());

    // Apply any saved DIP switch settings from the configuration
    for (int b = 0; b < 2; b++) {
        if (!dip_bank_[b].descriptor) continue;
        for (int i = 0; i < dip_bank_[b].descriptor->num_switches; i++) {
            const auto& sw = dip_bank_[b].descriptor->switches[i];
            std::string key = std::string("dip.") + dip_bank_[b].descriptor->name + "." + sw.name;
            auto it = config_.custom_settings.find(key);
            if (it != config_.custom_settings.end()) {
                int idx = DipSwitchBank::find_setting(sw, it->second.c_str());
                if (idx >= 0) dip_bank_[b].set_selection(i, idx);
            }
        }
    }

    printf("%s: System initialized\n", Traits::NAME);
    return true;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::shutdown() {
    printf("%s: Shutting down\n", AtariVectorTraits<V>::NAME);
    System::shutdown();
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::reset() {
    printf("%s: Reset\n", AtariVectorTraits<V>::NAME);

    board_.reset_chips();
    board_.m6502.reset();

    pins_ = MOS6502::default_bus_state();
    total_cycles_ = 0;

    vg().reset();
    nmi_counter_ = atv::NMI_PERIOD_CYCLES;

    if constexpr (Traits::HAS_POKEY) {
        board_.pokey1.reset();
    }

    // Internal button state uses active-HIGH convention (1=pressed, 0=not pressed).
    in0_ = 0x00;
    in1_ = 0x00;
    thrust_ = 0x00;
    snd_latch_ = 0x00;
    board_.latch.reset();    // All Q outputs LOW (NMI gated off)
    irq_asserted_ = false; // IRQ starts inactive
}

// ============================================================================
// EXECUTION
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick() {
    // CPU tick
    tick_cpu();

    // Vector generator tick — runs at the same frequency as the CPU
    vg().tick();

    // POKEY tick (runs at CPU clock for games with POKEY)
    if constexpr (Traits::HAS_POKEY) {
        board_.pokey1.tick(0);  // Arcade POKEY: no bus-driven memory access
    }

    // ── Periodic interrupt timer ────────────────────────────────────────────
    //
    // All Atari vector games derive their periodic interrupt from the same
    // clock divider chain: MASTER_CLOCK / 4096 / 12 ≈ 246 Hz.
    //
    // Interrupt type per game (confirmed via ROM vector analysis + MAME):
    //   Asteroids / Lunar Lander : Pulsed NMI, unconditional.
    //   Asteroids Deluxe         : Pulsed NMI, gated by 74LS259 Q4 ($3C04).
    //   Battlezone / Red Baron   : Pulsed NMI, gated by 74LS259 Q5 ($1005).
    //   Tempest                  : Level IRQ, cleared by $5000 write.
    //   Gravitar / Black Widow   : Level IRQ, cleared by $88C0 write.
    //   Space Duel               : Level IRQ, cleared by $0E00 write.
    //   Major Havoc              : Deferred (dual CPU architecture).
    //
    // Evidence: DVG/BZ/RB/AD ROMs have NMI vector → real handler, IRQ vector
    // → reset address.  AVG games (Tempest+) have IRQ vectors → real handlers.
    //
    if (nmi_counter_ > 0) {
        --nmi_counter_;
        // De-assert NMI between pulses (edge-triggered: needs high→low edge).
        if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                      V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                      V == AtariVectorVariant::LUNAR_LANDER ||
                      V == AtariVectorVariant::BATTLEZONE ||
                      V == AtariVectorVariant::RED_BARON) {
            BUS_SET_BIT(pins_, BUS_NMI_BIT);   // NMI inactive (high)
        }
    } else {
        nmi_counter_ = atv::NMI_PERIOD_CYCLES;

        if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                      V == AtariVectorVariant::LUNAR_LANDER) {
            // Unconditional NMI pulse
            BUS_CLR_BIT(pins_, BUS_NMI_BIT);

        } else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                             V == AtariVectorVariant::BATTLEZONE ||
                             V == AtariVectorVariant::RED_BARON) {
            // Gated NMI: only fires when enabled via 74LS259 latch output.
            // AD: Q4 ($3C04), BZ/RB: Q5 ($1005).
            if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
                if (board_.latch.q(4))
                    BUS_CLR_BIT(pins_, BUS_NMI_BIT);
            } else {
                if (board_.latch.q(5))
                    BUS_CLR_BIT(pins_, BUS_NMI_BIT);
            }

        } else if constexpr (V == AtariVectorVariant::TEMPEST ||
                             V == AtariVectorVariant::GRAVITAR ||
                             V == AtariVectorVariant::BLACK_WIDOW ||
                             V == AtariVectorVariant::SPACE_DUEL) {
            // Level-sensitive IRQ: held until game writes to IRQ ack register.
            irq_asserted_ = true;
        }
    }

    // ── IRQ line management (Tempest, Gravitar, BW, SD) ─────────────────
    if constexpr (V == AtariVectorVariant::TEMPEST ||
                  V == AtariVectorVariant::GRAVITAR ||
                  V == AtariVectorVariant::BLACK_WIDOW ||
                  V == AtariVectorVariant::SPACE_DUEL) {
        if (irq_asserted_)
            BUS_CLR_BIT(pins_, BUS_IRQ_BIT);   // IRQ active (held until ack)
        else
            BUS_SET_BIT(pins_, BUS_IRQ_BIT);   // IRQ inactive
    }

    total_cycles_++;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::run_frame() {
    if (!video_port_) return;

    if (!system_ready_) {
        // No ROM loaded — nothing to draw
        video_port_->swap_frame();
        return;
    }

    // Run one frame's worth of CPU cycles
    for (uint32_t i = 0; i < atv::CYCLES_PER_FRAME; ++i) {
        tick();
    }

    // Swap frame — produces FrameData with VideoSignalType::Vector for the GPU
    video_port_->swap_frame();

    // Tick peripherals
    tick_peripherals();
}

// ============================================================================
// CPU TICK
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::tick_cpu() {
    auto& cpu = board_.m6502;

    pins_ = cpu.template tick<MOS6502::Phase::PHI2>(pins_);

    uint16_t raw_addr = BUS_GET_ADDR(pins_);
    uint16_t addr;
    if constexpr (Traits::USES_15BIT_ADDR) {
        // DVG games + BZ/RB: A15 not connected, $8000-$FFFF mirrors $0000-$7FFF.
        addr = raw_addr & 0x7FFF;
    } else {
        // Tempest, Gravitar, BW, SD: full 16-bit address space.
        addr = raw_addr;
    }
    bool is_write = !BUS_GET_BIT(pins_, BUS_RW_BIT);

    // I/O region varies by game family:
    //   Asteroids/LL/AD:  $2000-$3FFF
    //   Battlezone/RB:    $0800-$1FFF (MAME bzone.cpp)
    //   Gravitar/BW:      $0800-$1FFF (MAME bwidow.cpp — $8800 masked to $0800)
    //   Space Duel:       $0800-$1FFF (MAME spacduel_map)
    //   Major Havoc:      $0800-$1FFF (MAME mhavoc.cpp)
    //   Tempest:          $0800-$1FFF (inputs) + $6000-$60FF (POKEY, VGGO, etc.)
    bool is_io = false;

    if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                  V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                  V == AtariVectorVariant::LUNAR_LANDER) {
        // Original DVG games: I/O at $2000-$3FFF
        is_io = (addr >= 0x2000 && addr < 0x4000);
    } else if constexpr (V == AtariVectorVariant::TEMPEST) {
        // Tempest: I/O at $0800-$1FFF (input ports, color RAM)
        //          $4000-$5FFF (coin, VGGO, WD, VGRST)
        //          $6000-$60FF (EAROM, mathbox, POKEY, LED)
        is_io = (addr >= 0x0800 && addr < 0x2000) ||
                (addr >= 0x4000 && addr < 0x6100);
    } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                         V == AtariVectorVariant::BLACK_WIDOW) {
        // Gravitar/BW (bwidow board): full 16-bit addressing.
        //   $6000-$6FFF: POKEY1/2
        //   $7000-$7FFF: EAROM, IN0 ($7800)
        //   $8000-$8FFF: IN3, IN4, VGGO, VGRST, IRQ ack, EAROM ctrl, WD
        is_io = (addr >= 0x6000 && addr < 0x9000);
    } else {
        // BZ, RB, SD, MH: I/O at $0800-$1FFF
        is_io = (addr >= 0x0800 && addr < 0x2000);
    }

    if (is_io) {
        if (is_write) {
            pins_ = io_write(addr, BUS_GET_DATA(pins_), pins_);
        } else {
            pins_ = io_read(addr, pins_);
        }
    } else {
        // All other addresses: RAM, vector RAM/ROM, program ROM via MemoryBus.
        bus_state_t bus = pins_;
        BUS_SET_ADDR(bus, addr);
        pins_ = bus_.tick(bus);
    }

    pins_ = cpu.template tick<MOS6502::Phase::PHI1>(pins_);
    cpu.sample_nmi_pin(pins_);
}

// ============================================================================
// I/O READ
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_read(uint16_t addr, bus_state_t pins) {
    uint8_t data = 0x00;

    // ── POKEY1 read — direct register access ──────────────────────
    if constexpr (Traits::HAS_POKEY) {
        if (addr >= Traits::POKEY1_BASE && addr < Traits::POKEY1_BASE + Traits::POKEY1_SIZE) {
            BUS_SET_DATA(pins, board_.pokey1.read(static_cast<uint8_t>(addr & 0x0F)));
            return pins;
        }
        // Tempest mirrors POKEY1 at $0800 and POKEY2 at $0900
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            if (addr >= 0x0800 && addr < 0x0810) {
                BUS_SET_DATA(pins, board_.pokey1.read(static_cast<uint8_t>(addr & 0x0F)));
                return pins;
            }
        }
    }

    // ── POKEY2 read ─────────────────────────────────────────────────
    if constexpr (Traits::POKEY2_BASE != 0) {
        if (addr >= Traits::POKEY2_BASE && addr < Traits::POKEY2_BASE + Traits::POKEY2_SIZE) {
            // TODO: second POKEY instance; return open-bus for now.
            BUS_SET_DATA(pins, 0xFF);
            return pins;
        }
        // Tempest POKEY2 mirror at $0900
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            if (addr >= 0x0900 && addr < 0x0910) {
                BUS_SET_DATA(pins, 0xFF);
                return pins;
            }
        }
    }

    // ── EAROM read (unified for all EAROM games) ────────────────────
    if constexpr (Traits::HAS_EAROM) {
        // ER2055 data read: returns the data output latch
        if (addr >= Traits::EAROM_BASE && addr < Traits::EAROM_BASE + Traits::EAROM_SIZE) {
            BUS_SET_DATA(pins, board_.earom.read_data());
            return pins;
        }
        // Dedicated EAROM data output port (Tempest $6050)
        if constexpr (Traits::EAROM_READ_ADDR != 0) {
            if (addr == Traits::EAROM_READ_ADDR) {
                BUS_SET_DATA(pins, board_.earom.read_data());
                return pins;
            }
        }
        // EAROM control / mathbox status register read (Tempest $6040)
        // bit 7 = 1 when mathbox idle/done. We have no mathbox — always done.
        if constexpr (Traits::EAROM_CTRL_ADDR != 0) {
            if (addr == Traits::EAROM_CTRL_ADDR) {
                BUS_SET_DATA(pins, 0x80);
                return pins;
            }
        }
    }

    // ── Game-specific input port reads ──────────────────────────────

    if constexpr (V == AtariVectorVariant::BATTLEZONE ||
                  V == AtariVectorVariant::RED_BARON) {
        // ── Battlezone / Red Baron I/O reads ────────────────────────────
        //
        // I/O at $0800-$1FFF (MAME bzone.cpp / redbaron_map):
        //   $0800   IN0  (direct, full byte — HALT, clock, coins, start)
        //   $0A00   DSW0 (DIP switches)
        //   $0C00   DSW1 (DIP switches)

        if (addr >= 0x0800 && addr < 0x0A00) {
            // IN0 — full byte with live HW signals
            // XOR converts internal active-HIGH → hardware active-LOW for
            // coin, self-test, diagnostic step bits.
            data = in0_ ^ atv::BZ_IN0_ACTIVE_LOW_MASK;
            // bit 6: VG HALT (IP_ACTIVE_LOW: halted → bit clear, running → bit set)
            if (vg().is_halted())
                data &= ~atv::BZ_IN0_HALT;
            else
                data |= atv::BZ_IN0_HALT;
            // bit 7: 3 KHz clock
            if (total_cycles_ & 0x100)
                data |= atv::BZ_IN0_CLOCK;
            else
                data &= ~atv::BZ_IN0_CLOCK;

        } else if (addr >= 0x0A00 && addr < 0x0C00) {
            data = dip_bank_[0].value;

        } else if (addr >= 0x0C00 && addr < 0x0E00) {
            data = dip_bank_[1].value;

        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                  V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        // ── Asteroids / Asteroids Deluxe I/O reads ──────────────────────
        //
        // Both use MULTIPLEXED input reads via 74LS244 buffers.
        // Reading address $200X returns bit X of the IN0 port placed at D7.
        //
        // Address decode:
        //   $2000-$2007   IN0   (multiplexed, 8 bits)
        //   $2400-$2407   IN1   (multiplexed, 8 bits)
        //   $2600-$260F   POKEY (AD — handled above)
        //   $2800-$2803   DSW1  (multiplexed, 4 bits)
        //   $2C00-$2C3F   EAROM (AD — handled above)

        // Multiplexed input port reads (shared Asteroids / AD)
        uint16_t port_base = addr & 0x2C00;  // A13, A11, A10 select port
        uint8_t offset = addr & 0x07;        // A0-A2 select which bit

        uint8_t port_val = 0x00;

        if (port_base == 0x2000) {
            // IN0: build live value from button state + hw signals
            port_val = in0_;

            // bit 1: 3 KHz clock — toggle every 256 CPU cycles (matches MAME clock_r)
            if (total_cycles_ & 0x100)
                port_val |= atv::AST_IN0_CLOCK;
            else
                port_val &= ~atv::AST_IN0_CLOCK;

            // bit 2: DVG done_r (IP_ACTIVE_LOW: halted=0, running=1)
            if (vg().is_halted())
                port_val &= ~atv::AST_IN0_HALT;
            else
                port_val |= atv::AST_IN0_HALT;

        } else if (port_base == 0x2400) {
            // IN1: player controls, coins, start
            port_val = in1_;

        } else if (port_base == 0x2800) {
            // DSW1: DIP switches
            port_val = dip_bank_[0].value;
            offset &= 0x03;  // only 4 switches via this multiplexer

        } else {
            // Unmapped read
            data = 0xFF;
            BUS_SET_DATA(pins, data);
            return pins;
        }

        // Multiplexed read: extract bit[offset], return at D7
        data = (port_val & (1 << offset)) ? 0x80 : 0x7F;

    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        // ── Lunar Lander I/O reads ───────────────────────────────────────
        //
        // Lunar Lander has a different I/O layout (MAME llander_map):
        //   $2000        IN0   (direct full-byte read, NOT multiplexed)
        //   $2400-$2407  IN1   (multiplexed, 8 bits)
        //   $2800-$2803  DSW1  (multiplexed, 4 bits)
        //   $2C00        THRUST (direct ADC value)

        uint16_t port_base = addr & 0x2C00;

        if (port_base == 0x2000 && (addr & 0x03FF) == 0x0000) {
            // IN0: direct read (non-multiplexed), full byte.
            // XOR converts internal active-HIGH state → hardware active-LOW
            // for IP_ACTIVE_LOW bits (self-test, tilt, diag step, etc.).
            // HALT (bit 0) and CLOCK (bit 6) are active-HIGH → unaffected.
            data = in0_ ^ atv::LL_IN0_ACTIVE_LOW_MASK;

            // bit 0: DVG HALT (IP_ACTIVE_HIGH in LL: done_r → bit set when halted)
            if (vg().is_halted())
                data |= atv::LL_IN0_HALT;
            else
                data &= ~atv::LL_IN0_HALT;

            // bit 6: 3 KHz clock
            if (total_cycles_ & 0x100)
                data |= atv::LL_IN0_CLOCK;
            else
                data &= ~atv::LL_IN0_CLOCK;

        } else if (port_base == 0x2400) {
            // IN1: multiplexed. XOR for active-LOW polarity before bit extract.
            uint8_t offset = addr & 0x07;
            uint8_t port_val = in1_ ^ atv::LL_IN1_ACTIVE_LOW_MASK;
            data = (port_val & (1 << offset)) ? 0x80 : 0x7F;

        } else if (port_base == 0x2800) {
            // DSW1: multiplexed (4 bits)
            uint8_t offset = addr & 0x03;
            data = (dip_bank_[0].value & (1 << offset)) ? 0x80 : 0x7F;

        } else if (port_base == 0x2C00) {
            // Thrust lever ADC
            data = thrust_;

        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::TEMPEST) {
        // ── Tempest input port reads ─────────────────────────────────
        // (POKEY and EAROM reads handled in unified sections above)
        if (addr >= 0x0C00 && addr < 0x0D00) {
            // IN0 at $0C00 (MAME tempest.cpp):
            //   bits 0-5: coins, tilt, self-test, slam (IP_ACTIVE_LOW → idle high)
            //   bit 6 (0x40): VG done_r (IP_ACTIVE_HIGH: 1=halted, 0=running)
            //   bit 7 (0x80): VBLANK (IP_ACTIVE_HIGH: 1=vblank, 0=active)
            data = 0xFF;  // all idle (active-low buttons = high)
            if (vg().is_halted()) data &= ~0x40;  // halted → clear bit 6 (IP_ACTIVE_LOW)
            // VBLANK (IP_ACTIVE_LOW): bit low during VBLANK, high during active display
            if ((total_cycles_ % atv::CYCLES_PER_FRAME) > (atv::CYCLES_PER_FRAME * 4 / 5))
                data &= ~0x80;  // VBLANK → clear bit 7
        } else if (addr >= 0x0D00 && addr < 0x0E00) {
            // IN1 at $0D00 (MAME tempest.cpp: portr("IN1"))
            // All bits are IP_ACTIVE_LOW: idle = 0xFF, pressed = bit cleared.
            data = 0xFF;  // TODO: wire host inputs
        } else if (addr >= 0x0E00 && addr < 0x0F00) {
            // IN2 at $0E00 (MAME tempest.cpp: portr("IN2") — DIP switches + VBLANK)
            // Bits 0-6: DIP switches (from DIP bank)
            // Bit 7: VBLANK (IP_ACTIVE_HIGH) — 1 during vertical blank
            data = dip_bank_[0].value & 0x7F;
            // Simulate VBLANK: high during last ~20% of each frame
            if ((total_cycles_ % atv::CYCLES_PER_FRAME) > (atv::CYCLES_PER_FRAME * 4 / 5))
                data |= 0x80;
        } else if (addr == 0x6060) {
            data = 0x00;  // Mathbox lo result (stub)
        } else if (addr == 0x6070) {
            data = 0x00;  // Mathbox hi result (stub)
        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                         V == AtariVectorVariant::BLACK_WIDOW) {
        // ── Gravitar/BW input port reads (bwidow board) ─────────────
        // (POKEY reads handled in unified section above)
        //   $7800: IN0, $8000: IN3, $8800: IN4
        //
        // IN0 bit layout (all IP_ACTIVE_LOW — idle = 0xFF):
        //   0: Coin 2       1: Coin 1       2-3: Unused
        //   4: Service 1    5: Tilt         6: Test/Service switch
        //   7: AVG done_r   (0 = halted/done, 1 = running)
        if (addr == 0x7800) {
            data = 0xFF;  // all idle (active-low buttons = high)
            // Bit 7: AVG done_r (IP_ACTIVE_LOW) — 0 when halted, 1 when running
            if (vg().is_halted()) data &= ~0x80;
        } else if (addr == 0x8000) {
            data = in1_;
        } else if (addr == 0x8800) {
            data = dip_bank_[0].value;
        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
        // ── Space Duel input port reads ──────────────────────────────
        // (POKEY reads handled in unified section above)
        //   $0800: IN0, $0900: IN3
        //
        // IN0 bit layout (all IP_ACTIVE_LOW — idle = 0xFF):
        //   0: Coin 2       1: Coin 1       2: Coin 3     3: Unused
        //   4: Tilt          5: Service 1    6: Test/Service switch
        //   7: AVG done_r   (0 = halted/done, 1 = running)
        if (addr == 0x0800) {
            data = 0xFF;  // all idle (active-low buttons = high)
            // Bit 7: AVG done_r (IP_ACTIVE_LOW) — 0 when halted, 1 when running
            if (vg().is_halted()) data &= ~0x80;
        } else if (addr >= 0x0900 && addr < 0x0A00) {
            // IN3: DIP switches (multiplexed — $090X returns bit X at D7)
            uint8_t offset = addr & 0x07;
            data = (dip_bank_[0].value & (1 << offset)) ? 0x80 : 0x7F;
        } else {
            data = 0xFF;
        }

    } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
        // ── Major Havoc input port reads ─────────────────────────────
        // (POKEY reads handled in unified section above)
        // MH alpha reads IN0 at $0800; bit 1 = VG done_r (IP_ACTIVE_LOW)
        if (addr >= 0x0800 && addr < 0x0900) {
            data = 0xFF;
            if (vg().is_halted()) data &= ~0x02;  // halted → bit 1 = 0
        } else {
            data = 0xFF;
        }
    }

    BUS_SET_DATA(pins, data);
    return pins;
}

// ============================================================================
// I/O WRITE
// ============================================================================

template<AtariVectorVariant V>
bus_state_t AtariVectorSystem<V>::io_write(uint16_t addr, uint8_t data, bus_state_t pins) {
    // I/O writes are decode-by-address — the upper address bits select the register.
    // The data byte on the bus is sometimes ignored (trigger-only writes).

    // ── POKEY1 write — direct register access ──────────────────────
    if constexpr (Traits::HAS_POKEY) {
        if (addr >= Traits::POKEY1_BASE && addr < Traits::POKEY1_BASE + Traits::POKEY1_SIZE) {
            board_.pokey1.write(static_cast<uint8_t>(addr & 0x0F), data);
            return pins;
        }
        // Tempest mirrors POKEY1 at $0800
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            if (addr >= 0x0800 && addr < 0x0810) {
                board_.pokey1.write(static_cast<uint8_t>(addr & 0x0F), data);
                return pins;
            }
        }
    }

    // ── POKEY2 write ────────────────────────────────────────────────
    if constexpr (Traits::POKEY2_BASE != 0) {
        if (addr >= Traits::POKEY2_BASE && addr < Traits::POKEY2_BASE + Traits::POKEY2_SIZE) {
            // TODO: second POKEY instance
            return pins;
        }
        // Tempest POKEY2 mirror at $0900
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            if (addr >= 0x0900 && addr < 0x0910) {
                return pins;
            }
        }
    }

    // ── EAROM data write (unified for all EAROM games) ──────────────
    if constexpr (Traits::HAS_EAROM) {
        if (addr >= Traits::EAROM_BASE && addr < Traits::EAROM_BASE + Traits::EAROM_SIZE) {
            // EAROM address+data latch (no clock strobe)
            bus_state_t eb = 0;
            BUS_SET_ADDR(eb, addr & 0x3F);
            BUS_SET_DATA(eb, data);
            board_.earom.tick(eb);
            return pins;
        }
        // EAROM control write — Tempest $6040 (AD handled in DVG switch below)
        if constexpr (Traits::EAROM_CTRL_ADDR != 0) {
            if (addr == Traits::EAROM_CTRL_ADDR) {
                // CS1=bit3, CS2=tied high, C1=bit2, C2=bit1, CK=bit0
                bus_state_t eb = 0;
                BUS_SET_ADDR(eb, board_.earom.regs_.data[er2055::reg::ADDR_LATCH]);
                BUS_SET_DATA(eb, board_.earom.regs_.data[er2055::reg::DATA_IN]);
                if (data & 0x08) BUS_SET_BIT(eb, er2055::CS1_BIT);
                BUS_SET_BIT(eb, er2055::CS2_BIT);  // tied high
                if (data & 0x04) BUS_SET_BIT(eb, er2055::C1_BIT);
                if (data & 0x02) BUS_SET_BIT(eb, er2055::C2_BIT);
                if (data & 0x01) BUS_SET_BIT(eb, er2055::CK_BIT);
                board_.earom.tick(eb);
                return pins;
            }
        }
    }

    // ── IRQACK write (unified for IRQ-based games) ──────────────────
    if constexpr (Traits::IRQACK_ADDR != 0) {
        if (addr == Traits::IRQACK_ADDR) {
            irq_asserted_ = false;
            return pins;
        }
    }

    // ── VG control + game-family-specific writes ────────────────────

    if constexpr (V == AtariVectorVariant::ASTEROIDS ||
                  V == AtariVectorVariant::ASTEROIDS_DELUXE ||
                  V == AtariVectorVariant::LUNAR_LANDER) {
        // ── DVG I/O writes at $3000-$3FFF (addr & 0x3E00 decode) ────

        uint16_t reg = addr & 0x3E00;

        switch (reg) {
            case Traits::VGGO_ADDR:
                vg().trigger_go();
                break;

            case Traits::VGRST_ADDR:
                vg().trigger_reset();
                break;

            case Traits::WDCLR_ADDR:
                // Watchdog clear — no-op in emulation
                break;

            case 0x3600:   // SND — explosion/thrust sound latch
                snd_latch_ = data;
                break;

            case 0x3800:
            case 0x3A00:
                if constexpr (Traits::HAS_EAROM) {
                    // AD EAROM control write: CS1=bit3, CS2=tied high,
                    // C1=bit2, C2=bit1, CK=bit0
                    bus_state_t eb = 0;
                    BUS_SET_ADDR(eb, board_.earom.regs_.data[er2055::reg::ADDR_LATCH]);
                    BUS_SET_DATA(eb, board_.earom.regs_.data[er2055::reg::DATA_IN]);
                    if (data & 0x08) BUS_SET_BIT(eb, er2055::CS1_BIT);
                    BUS_SET_BIT(eb, er2055::CS2_BIT);  // tied high
                    if (data & 0x04) BUS_SET_BIT(eb, er2055::C1_BIT);
                    if (data & 0x02) BUS_SET_BIT(eb, er2055::C2_BIT);
                    if (data & 0x01) BUS_SET_BIT(eb, er2055::CK_BIT);
                    board_.earom.tick(eb);
                }
                break;

            case 0x3C00:   // COIN — 74LS259 addressable latch
                board_.latch.write(addr, data);
                break;

            case 0x3E00:   // NMI ACK
                break;

            default:
                break;
        }

    } else if constexpr (V == AtariVectorVariant::BATTLEZONE ||
                         V == AtariVectorVariant::RED_BARON) {
        // ── BZ/RB I/O writes at $0800-$1FFF (range decode) ─────────
        //
        // MAME bzone.cpp:
        //   $1000: coin counters   $1200: VGGO
        //   $1400: WD clear        $1600: VGRST
        //   $1840: sound latch

        if (addr >= 0x1000 && addr < 0x1008) {
            // 74LS259 addressable latch (MAME: ls259_device::write_a0)
            //   Q0 = coin counter 1, Q1 = coin counter 2, Q2 = start LED
            //   Q5 = NMI enable (game writes $1005 D0=1 to enable periodic NMI)
            board_.latch.write(addr, data);
        } else if (addr >= 0x1840 && addr < 0x1A40) {
            snd_latch_ = data;
        } else if (addr >= Traits::VGGO_ADDR && addr < Traits::VGGO_ADDR + 0x0200) {
            vg().trigger_go();
        } else if (addr >= Traits::VGRST_ADDR && addr < Traits::VGRST_ADDR + 0x0200) {
            vg().trigger_reset();
        } else if (addr >= Traits::WDCLR_ADDR && addr < Traits::WDCLR_ADDR + 0x0200) {
            // Watchdog clear — no-op
        }

    } else {
        // ── AVG-based game I/O writes (exact address match) ─────────

        if (addr == Traits::VGGO_ADDR) { vg().trigger_go(); return pins; }
        // Gravitar/BW: alternate VGGO at $8940 (partial decode — bit 8 not decoded)
        if constexpr (V == AtariVectorVariant::GRAVITAR || V == AtariVectorVariant::BLACK_WIDOW) {
            if (addr == 0x8940) { vg().trigger_go(); return pins; }
        }
        if (addr == Traits::VGRST_ADDR) { vg().trigger_reset(); return pins; }
        if (addr == Traits::WDCLR_ADDR) {
            // Tempest: wdclr also clears IRQ (MAME wdclr_w)
            if constexpr (V == AtariVectorVariant::TEMPEST) irq_asserted_ = false;
            return pins;
        }
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_file(const char* filepath) {
    using Traits = AtariVectorTraits<V>;

    bool cold_boot = system_ready_;

    if (!system_ready_) {
        if (!initialize()) return false;
    }

    printf("%s: Loading file: %s\n", Traits::NAME, filepath);

    // Archive files (zip, 7z, …) may contain a multi-file ROM set.
    // Try ROM set matching before falling back to single-blob loading.
    std::string ext = vfs_extension(filepath);
    if (!ext.empty() && vfs_is_archive_extension(ext.c_str())) {
        auto descriptors = get_rom_set_descriptors();
        if (!descriptors.empty()) {
            auto match = rom_set_scan_and_match(
                filepath, descriptors.data(), static_cast<int>(descriptors.size()));
            if (match.matched) {
                printf("%s: Archive contains ROM set '%s'\n",
                       Traits::NAME, match.rom_set ? match.rom_set->name : "?");
                return load_rom_set(match);
            }
        }
        // Archive didn't match any ROM set — cannot load raw ZIP as ROM data
        printf("%s: Archive '%s' did not match any known ROM set\n",
               Traits::NAME, filepath);
        return false;
    }

    // Read the ROM file
    size_t file_size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &file_size);
    if (!file_data) {
        printf("%s: Failed to open file: %s\n", Traits::NAME, filepath);
        return false;
    }

    if (file_size == 0) {
        printf("%s: Empty file\n", Traits::NAME);
        free(file_data);
        return false;
    }

    printf("%s: ROM file is %zu bytes\n", Traits::NAME, file_size);

    // Determine ROM layout:
    // The file may contain:
    //   a) Just the program ROM
    //   b) Program ROM + vector ROM concatenated
    //   c) A combined ROM image with everything

    if (file_size >= Traits::PROGROM_ACTUAL + Traits::VECROM_SIZE) {
        // File contains both program ROM and vector ROM
        // Layout: program ROM first, then vector ROM at the end.
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
        if (vec_rom_) {
            std::memcpy(vec_rom_->data(), file_data + Traits::PROGROM_ACTUAL,
                        Traits::VECROM_SIZE);
        }
    } else if (file_size >= Traits::PROGROM_ACTUAL) {
        // Just the program ROM — vector ROM must be loaded separately
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, Traits::PROGROM_ACTUAL);
        }
    } else {
        // Unknown size — load as much as fits into program ROM
        size_t to_copy = std::min(file_size, static_cast<size_t>(Traits::PROGROM_ACTUAL));
        if (prog_rom_) {
            std::memcpy(prog_rom_->data() + Traits::PROGROM_OFFSET,
                        file_data, to_copy);
        }
    }

    free(file_data);

    // Set program title from filename
    const char* name = strrchr(filepath, '/');
    if (!name) name = strrchr(filepath, '\\');
    program_title_ = name ? (name + 1) : filepath;

    // Wire vector generator to vector memory
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

    // Show reset vector for diagnostic — confirms ROM data is present and mapped
    if (prog_rom_) {
        uint16_t rst_offset = Traits::PROGROM_SIZE - 4;  // $FFFC relative
        uint16_t rst_lo = prog_rom_->data()[rst_offset];
        uint16_t rst_hi = prog_rom_->data()[rst_offset + 1];
        printf("%s: Reset vector = $%04X (chip offset $%04X)\n",
               Traits::NAME, rst_lo | (rst_hi << 8), rst_offset);
    }

    if (cold_boot) {
        board_.m6502.set(A, 0);
        board_.m6502.set(X, 0);
        board_.m6502.set(Y, 0);
    }

    printf("%s: ROM loaded, system ready\n", Traits::NAME);
    return true;
}

// ============================================================================
// ROM SET LOADING
// ============================================================================

template<AtariVectorVariant V>
std::vector<const RomSetDescriptor*> AtariVectorSystem<V>::get_rom_set_descriptors() const {
    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        return { &ast_v1_romset, &ast_v2_romset };
    } else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        return { &ad_v1_romset, &ad_v2_romset };
    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        return { &ll_v1_romset, &ll_v2_romset };
    } else if constexpr (V == AtariVectorVariant::BATTLEZONE) {
        return { &bz_v1_romset, &bz_v2_romset };
    } else if constexpr (V == AtariVectorVariant::RED_BARON) {
        return { &rb_romset };
    } else if constexpr (V == AtariVectorVariant::TEMPEST) {
        return { &tempest_v3_romset, &tempest_v3_2716_romset,
                 &tempest_v2_romset, &tempest_v1_romset };
    } else if constexpr (V == AtariVectorVariant::GRAVITAR) {
        return { &gravitar_v2_romset, &gravitar_v3_romset };
    } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
        return { &spaceduel_romset };
    } else if constexpr (V == AtariVectorVariant::BLACK_WIDOW) {
        return { &blackwidow_romset };
    } else if constexpr (V == AtariVectorVariant::MAJOR_HAVOC) {
        return { &majorhavoc_v3_romset };
    } else {
        return {};
    }
}

template<AtariVectorVariant V>
bool AtariVectorSystem<V>::load_rom_set(const RomSetMatch& match) {
    using Traits = AtariVectorTraits<V>;

    if (!match.matched) return false;

    if (!system_ready_) {
        if (!initialize()) return false;
    }

    printf("%s: Loading ROM set '%s' (%zu entries)\n",
           Traits::NAME, match.rom_set ? match.rom_set->name : "?",
           match.entries.size());

    bool ok = rom_set_load_matched(match, [this](uint32_t load_address,
                                                  const uint8_t* data,
                                                  size_t size,
                                                  int /*entry_index*/) -> bool {
        // Route data to the correct chip based on load address
        if (load_address >= Traits::VECROM_BASE &&
            load_address < Traits::VECROM_BASE + Traits::VECROM_SIZE) {
            // Vector ROM
            if (!vec_rom_) return false;
            uint32_t offset = load_address - Traits::VECROM_BASE;
            size_t to_copy = std::min(size, static_cast<size_t>(Traits::VECROM_SIZE - offset));
            std::memcpy(vec_rom_->data() + offset, data, to_copy);
            printf("  Vector ROM: %zu bytes at $%04X\n", to_copy, load_address);
            return true;
        }

        // Program ROM — lower chip ($4000-$7FFF for most games)
        if (load_address >= Traits::PROGROM_BASE &&
            load_address < Traits::PROGROM_BASE + Traits::PROGROM_SIZE) {
            if (!prog_rom_) return false;
            uint32_t offset = load_address - Traits::PROGROM_BASE;
            size_t to_copy = std::min(size, static_cast<size_t>(Traits::PROGROM_SIZE - offset));
            std::memcpy(prog_rom_->data() + offset, data, to_copy);
            printf("  Program ROM: %zu bytes at $%04X (offset $%04X)\n",
                   to_copy, load_address, offset);
            return true;
        }

        // 16-bit games: upper ROM chip ($8000-$FFFF)
        if constexpr (!Traits::USES_15BIT_ADDR) {
            if (prog_rom_hi_ && load_address >= 0x8000) {
                uint32_t offset = load_address - 0x8000;
                size_t to_copy = std::min(size, static_cast<size_t>(0x8000u - offset));
                std::memcpy(prog_rom_hi_->data() + offset, data, to_copy);
                printf("  Program ROM (high): %zu bytes at $%04X (offset $%04X)\n",
                       to_copy, load_address, offset);
                return true;
            }
        }

        printf("  WARNING: Unhandled ROM address $%04X (%zu bytes) — skipped\n",
               load_address, size);
        return true;  // Not a fatal error
    });

    if (!ok) {
        printf("%s: Failed to load ROM set\n", Traits::NAME);
        return false;
    }

    // 16-bit games: create reset vector mirrors
    if constexpr (!Traits::USES_15BIT_ADDR) {
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            // Tempest: prog_rom_ IS the $8000-$FFFF chip (32KB).
            // ROM fills $9000-$DFFF (offsets $1000-$5FFF in chip).
            // Hardware mirrors $C000-$DFFF at $E000-$FFFF (A13 not decoded).
            if (prog_rom_) {
                // Copy $C000-$DFFF → $E000-$FFFF (chip offset $4000-$5FFF → $6000-$7FFF)
                std::memcpy(prog_rom_->data() + 0x6000,
                            prog_rom_->data() + 0x4000, 0x2000);
                printf("  Reset vector mirror: $C000-$DFFF → $E000-$FFFF\n");
            }
        } else if constexpr (V == AtariVectorVariant::GRAVITAR ||
                             V == AtariVectorVariant::BLACK_WIDOW) {
            // Gravitar/BW: prog_rom_ is the single Program ROM chip at $9000 (32KB alloc).
            // Last loaded ROM (.206/.106) at $E000 = chip offset $5000.
            // Mirror $E000-$EFFF to $F000-$FFFF (offset $5000 → $6000) for reset vector.
            if (prog_rom_) {
                std::memcpy(prog_rom_->data() + 0x6000,
                            prog_rom_->data() + 0x5000, 0x1000);
                printf("  Reset vector mirror: $E000 → $F000\n");
            }
        } else if constexpr (V == AtariVectorVariant::SPACE_DUEL) {
            // Space Duel: prog_rom_hi_ at $8000 (32KB alloc), .105 loads at $8000 (4KB).
            // Hardware A15 decode: $FFFC → .105 offset $FFC (reset vector lives in .105).
            // Mirror .105's 4 KB across the entire 32 KB prog_rom_hi_ for vector access.
            if (prog_rom_hi_) {
                for (uint32_t off = 0x1000; off < 0x8000; off += 0x1000) {
                    std::memcpy(prog_rom_hi_->data() + off,
                                prog_rom_hi_->data(), 0x1000);
                }
                printf("  Reset vector mirror: .105 mirrored across $8000-$FFFF\n");
            }
        }
    }

    // Set program title from ROM set name
    if (match.rom_set && match.rom_set->name)
        program_title_ = match.rom_set->name;

    // Wire vector generator to vector memory
    if (vec_ram_ && vec_rom_) {
        vg().set_vector_memory(vec_ram_->data(), Traits::VECRAM_SIZE,
                              vec_rom_->data(), Traits::VECROM_SIZE,
                              Traits::VECROM_WORD_OFFSET);
    }

    system_ready_ = true;
    reset();

    // Show reset vector for diagnostic — confirms ROM data is present and mapped
    if constexpr (!Traits::USES_15BIT_ADDR) {
        // 16-bit systems: reset vector at $FFFC in prog_rom_hi_ (or prog_rom_ for Tempest)
        ROMChip* rst_chip = nullptr;
        uint32_t rst_offset = 0;
        if constexpr (V == AtariVectorVariant::TEMPEST) {
            rst_chip = prog_rom_;
            rst_offset = Traits::PROGROM_SIZE - 4;  // prog_rom_ IS the $8000-$FFFF chip
        } else {
            rst_chip = prog_rom_hi_;
            if (rst_chip) {
                rst_offset = 0x8000 - 4;  // $FFFC - $8000 = $7FFC
            } else if (prog_rom_) {
                // Gravitar/BW: single prog_rom_ chip at $9000 (32KB alloc)
                // $FFFC mirrored into chip at offset $FFFC - PROGROM_BASE
                rst_chip = prog_rom_;
                rst_offset = 0xFFFC - Traits::PROGROM_BASE;
            }
        }
        if (rst_chip) {
            uint16_t rst_lo = rst_chip->data()[rst_offset];
            uint16_t rst_hi = rst_chip->data()[rst_offset + 1];
            printf("%s: Reset vector = $%04X (chip offset $%04X)\n",
                   Traits::NAME, rst_lo | (rst_hi << 8), rst_offset);
        }
    } else if (prog_rom_) {
        uint16_t rst_offset = Traits::PROGROM_SIZE - 4;  // $FFFC relative
        uint16_t rst_lo = prog_rom_->data()[rst_offset];
        uint16_t rst_hi = prog_rom_->data()[rst_offset + 1];
        printf("%s: Reset vector = $%04X (chip offset $%04X)\n",
               Traits::NAME, rst_lo | (rst_hi << 8), rst_offset);
    }

    printf("%s: ROM set loaded, system ready\n", Traits::NAME);
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::get_display_dimensions(int* width, int* height) const {
    if (width)  *width  = hardware_traits_.display.visible_width;
    if (height) *height = hardware_traits_.display.visible_height;
}

// ============================================================================
// AUDIO
// ============================================================================

template<AtariVectorVariant V>
uint32_t AtariVectorSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    if constexpr (Traits::HAS_POKEY) {
        // Read from POKEY audio ring buffer
        if (audio_port_) {
            int got = audio_port_->ring_.pop(buffer, static_cast<int>(max_samples));
            // Pad remainder with silence if ring didn't have enough
            if (got < static_cast<int>(max_samples))
                std::memset(buffer + got, 0, (max_samples - got) * sizeof(float));
            return max_samples;
        }
    }

    // Discrete sound (Asteroids, Lunar Lander) — silence for now
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = sample_rate_hz;
}

// ============================================================================
// INPUT
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if constexpr (V == AtariVectorVariant::ASTEROIDS) {
        // Asteroids controls (active-HIGH: pressed = set bit):
        //   Arrow keys = rotate left/right, thrust
        //   Space = fire
        //   H = hyperspace
        //   1/2 = 1P/2P start
        //   5 = coin
        //
        // In the MAME mapping, player controls are on IN1 ($2400-$2407),
        // fire/hyperspace are on IN0 ($2000-$2007).
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  atv::AST_IN1_ROT_LEFT;
                else         in1_ &= ~atv::AST_IN1_ROT_LEFT;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  atv::AST_IN1_ROT_RIGHT;
                else         in1_ &= ~atv::AST_IN1_ROT_RIGHT;
                break;
            case SDLK_UP:
                if (pressed) in1_ |=  atv::AST_IN1_THRUST;
                else         in1_ &= ~atv::AST_IN1_THRUST;
                break;
            case SDLK_SPACE:
                // Fire is on IN0 bit 4 in the MAME mapping
                if (pressed) in0_ |=  atv::AST_IN0_FIRE;
                else         in0_ &= ~atv::AST_IN0_FIRE;
                break;
            case SDLK_h:
                // Hyperspace is on IN0 bit 3 in the MAME mapping
                if (pressed) in0_ |=  atv::AST_IN0_HYPERSPACE;
                else         in0_ &= ~atv::AST_IN0_HYPERSPACE;
                break;
            case SDLK_1:
                if (pressed) in1_ |=  atv::AST_IN1_1P_START;
                else         in1_ &= ~atv::AST_IN1_1P_START;
                break;
            case SDLK_2:
                if (pressed) in1_ |=  atv::AST_IN1_2P_START;
                else         in1_ &= ~atv::AST_IN1_2P_START;
                break;
            case SDLK_5:
                // Coin insert (IN1 bit 0)
                if (pressed) in1_ |=  atv::AST_IN1_COIN1;
                else         in1_ &= ~atv::AST_IN1_COIN1;
                break;
            default:
                break;
        }
    } else if constexpr (V == AtariVectorVariant::ASTEROIDS_DELUXE) {
        // Asteroids Deluxe controls:
        //   Arrow left/right = rotate (IN1 bits 5/6)
        //   Up = thrust (IN0 bit 4)
        //   Space = fire (IN1 bit 7)
        //   H = shields (IN0 bit 3)
        //   1/2 = 1P/2P start
        //   5 = coin
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  atv::AD_IN1_ROT_LEFT;
                else         in1_ &= ~atv::AD_IN1_ROT_LEFT;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  atv::AD_IN1_ROT_RIGHT;
                else         in1_ &= ~atv::AD_IN1_ROT_RIGHT;
                break;
            case SDLK_UP:
                if (pressed) in0_ |=  atv::AD_IN0_THRUST;
                else         in0_ &= ~atv::AD_IN0_THRUST;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  atv::AD_IN1_FIRE;
                else         in1_ &= ~atv::AD_IN1_FIRE;
                break;
            case SDLK_h:
                if (pressed) in0_ |=  atv::AD_IN0_SHIELDS;
                else         in0_ &= ~atv::AD_IN0_SHIELDS;
                break;
            case SDLK_1:
                if (pressed) in1_ |=  atv::AST_IN1_1P_START;
                else         in1_ &= ~atv::AST_IN1_1P_START;
                break;
            case SDLK_2:
                if (pressed) in1_ |=  atv::AST_IN1_2P_START;
                else         in1_ &= ~atv::AST_IN1_2P_START;
                break;
            case SDLK_5:
                if (pressed) in1_ |=  atv::AST_IN1_COIN1;
                else         in1_ &= ~atv::AST_IN1_COIN1;
                break;
            default:
                break;
        }
    } else if constexpr (V == AtariVectorVariant::LUNAR_LANDER) {
        // Lunar Lander controls (active-HIGH: pressed = set bit):
        //   Up/Down = thrust (adjusts ADC value)
        //   Left/Right = rotate
        //   Space = abort (IN1 bit 5)
        //   1 = start (IN1 bit 0)
        //   5 = coin (IN1 bit 1)
        switch (key) {
            case SDLK_UP:
                if (pressed) {
                    thrust_ = std::min(255, thrust_ + 32);
                }
                break;
            case SDLK_DOWN:
                if (pressed) {
                    thrust_ = std::max(0, thrust_ - 32);
                }
                break;
            case SDLK_SPACE:
                // Abort button (IN1 bit 5)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_1:
                // Start (IN1 bit 0)
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_5:
                // Coin (IN1 bit 1, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_LEFT:
                // Rotate left (IN1 bit 7, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x80;
                else         in1_ &= ~0x80;
                break;
            case SDLK_RIGHT:
                // Rotate right (IN1 bit 6, IP_ACTIVE_LOW → we store active-high)
                if (pressed) in1_ |=  0x40;
                else         in1_ &= ~0x40;
                break;
            default:
                break;
        }

    } else if constexpr (V == AtariVectorVariant::BATTLEZONE) {
        // Battlezone — twin-stick tank controls
        //   W/S = left stick forward/reverse
        //   I/K = right stick forward/reverse
        //   Space = fire
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_w:
                if (pressed) in1_ |=  0x01;  // left forward
                else         in1_ &= ~0x01;
                break;
            case SDLK_s:
                if (pressed) in1_ |=  0x02;  // left reverse
                else         in1_ &= ~0x02;
                break;
            case SDLK_i:
                if (pressed) in1_ |=  0x04;  // right forward
                else         in1_ &= ~0x04;
                break;
            case SDLK_k:
                if (pressed) in1_ |=  0x08;  // right reverse
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;  // fire
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                // Start is IN3 bit 5 in MAME (joystick register, not IN0)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  atv::BZ_IN0_COIN1;
                else         in0_ &= ~atv::BZ_IN0_COIN1;
                break;
            default:
                break;
        }

    } else if constexpr (V == AtariVectorVariant::RED_BARON) {
        // Red Baron — yoke controls
        //   Arrow keys = up/down/left/right
        //   Space = fire
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_UP:
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_DOWN:
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_LEFT:
                if (pressed) in1_ |=  0x04;
                else         in1_ &= ~0x04;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  0x08;
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;  // fire
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                // Start is IN3 bit 5 in MAME (joystick register, not IN0)
                if (pressed) in1_ |=  0x20;
                else         in1_ &= ~0x20;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  atv::BZ_IN0_COIN1;
                else         in0_ &= ~atv::BZ_IN0_COIN1;
                break;
            default:
                break;
        }

    } else {
        // ── AVG-based games — generic controls ──────────────────────────
        //   Arrow keys = directional
        //   Space = fire / primary action
        //   1 = start
        //   5 = coin
        switch (key) {
            case SDLK_LEFT:
                if (pressed) in1_ |=  0x01;
                else         in1_ &= ~0x01;
                break;
            case SDLK_RIGHT:
                if (pressed) in1_ |=  0x02;
                else         in1_ &= ~0x02;
                break;
            case SDLK_UP:
                if (pressed) in1_ |=  0x04;
                else         in1_ &= ~0x04;
                break;
            case SDLK_DOWN:
                if (pressed) in1_ |=  0x08;
                else         in1_ &= ~0x08;
                break;
            case SDLK_SPACE:
                if (pressed) in1_ |=  0x10;
                else         in1_ &= ~0x10;
                break;
            case SDLK_1:
                if (pressed) in0_ |=  0x80;  // start
                else         in0_ &= ~0x80;
                break;
            case SDLK_5:
                if (pressed) in0_ |=  0x10;  // coin
                else         in0_ &= ~0x10;
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// GUI
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    // Future: DIP switch editor, display options
#endif
}

template<AtariVectorVariant V>
void AtariVectorSystem<V>::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    using Traits = AtariVectorTraits<V>;
    ImGui::Text("%s — DIP Switches", Traits::NAME);
    ImGui::Spacing();

    for (int b = 0; b < 2; b++) {
        if (!dip_bank_[b].descriptor) continue;
        ImGui::PushID(b);
        if (render_dip_switch_bank(dip_bank_[b])) {
            // Persist changed selections into config_.custom_settings
            const auto* desc = dip_bank_[b].descriptor;
            for (int i = 0; i < desc->num_switches; i++) {
                const auto& sw = desc->switches[i];
                std::string key = std::string("dip.") + desc->name + "." + sw.name;
                int sel = dip_bank_[b].selections[i];
                if (sel >= 0 && sel < sw.num_settings) {
                    config_.custom_settings[key] = sw.settings[sel].name;
                }
            }
        }
        ImGui::PopID();
        ImGui::Spacing();
    }
#endif
}
// ============================================================================
// SPEED CONTROL
// ============================================================================

template<AtariVectorVariant V>
void AtariVectorSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATION
// ============================================================================

// DVG-based
template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS>;
template class AtariVectorSystem<AtariVectorVariant::ASTEROIDS_DELUXE>;
template class AtariVectorSystem<AtariVectorVariant::LUNAR_LANDER>;
template class AtariVectorSystem<AtariVectorVariant::BATTLEZONE>;
template class AtariVectorSystem<AtariVectorVariant::RED_BARON>;

// AVG-based
template class AtariVectorSystem<AtariVectorVariant::TEMPEST>;
template class AtariVectorSystem<AtariVectorVariant::GRAVITAR>;
template class AtariVectorSystem<AtariVectorVariant::SPACE_DUEL>;
template class AtariVectorSystem<AtariVectorVariant::BLACK_WIDOW>;
template class AtariVectorSystem<AtariVectorVariant::MAJOR_HAVOC>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

// DVG-based
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::ASTEROIDS>(), [] { return std::make_unique<AsteroidsSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::ASTEROIDS_DELUXE>(), [] { return std::make_unique<AsteroidsDeluxeSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::LUNAR_LANDER>(), [] { return std::make_unique<LunarLanderSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::BATTLEZONE>(), [] { return std::make_unique<BattlezoneSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::RED_BARON>(), [] { return std::make_unique<RedBaronSystem>(); });

// AVG-based
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::TEMPEST>(), [] { return std::make_unique<TempestSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::GRAVITAR>(), [] { return std::make_unique<GravitarSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::SPACE_DUEL>(), [] { return std::make_unique<SpaceDuelSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::BLACK_WIDOW>(), [] { return std::make_unique<BlackWidowSystem>(); });
REGISTER_SYSTEM(descriptor_instance<AtariVectorVariant::MAJOR_HAVOC>(), [] { return std::make_unique<MajorHavocSystem>(); });
