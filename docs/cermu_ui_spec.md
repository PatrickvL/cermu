# Cermu — UI Implementation Specification

## Purpose

This document is a complete reference for implementing the Cermu system/ROM selector UI in **ImGui + OpenGL 3** (C++, Debian primary, Windows secondary). It covers layout, data models, all displayed content, keyboard navigation, colour values, and ImGui API patterns.

---

## 1. Overall Layout

```
┌─ Top bar ──────────────────────────────────────────────────────────────────┐
│ ◈ CERMU  │  [All][Home][Console][Arcade][Other]  Maker▾  │  N/42 systems   │
├─ System list (248 px wide) ──┬─ Content panel (flex) ──────────────────────┤
│                              │ System header + config strip                │
│  [search input]              ├──────────────────────────────────────────────┤
│                              │ ROM filter bar (search + Region▾ + Format▾) │
│  Row: CRT | Name | Meta      ├─ Column headers (sortable) ──────────────────┤
│  Row: …                      │ ROM rows (scrollable)                        │
│  …                           │ …                                            │
├──────────────────────────────┴──────────────────────────────────────────────┤
│ Status bar: key hints                                                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

The two main panels share a horizontal split. The left panel has a **fixed width of 248 px**. The right panel takes the remainder.

---

## 2. Colours

All colours are given as `ImVec4(r, g, b, a)` with float components 0–1.

| Role | Hex | ImVec4 |
|------|-----|--------|
| Window background | `#07080e` | `(0.027, 0.031, 0.055, 1)` |
| Top / status bar bg | `#050710` | `(0.020, 0.027, 0.063, 1)` |
| Left panel bg | `#060810` | `(0.024, 0.031, 0.063, 1)` |
| System row selected | `#0c1c2e` | `(0.047, 0.110, 0.180, 1)` |
| ROM row selected | `#0a1c30` | `(0.039, 0.110, 0.188, 1)` |
| ROM row hover | `#090c18` | `(0.035, 0.047, 0.094, 1)` |
| Header / config bg | `#060810` | same as left panel |
| Search input bg | `#090c1c` | `(0.035, 0.047, 0.110, 1)` |
| Panel border default | `#0e1422` | `(0.055, 0.078, 0.133, 1)` |
| Panel border focused | `#1a3252` | `(0.102, 0.196, 0.322, 1)` |
| Filter active border | `#1a3050` | `(0.102, 0.188, 0.314, 1)` |
| Table header row bg | `#060810` | same as panel |
| Sort column active fg | `#7aaccc` | `(0.478, 0.675, 0.800, 1)` |
| Sort column inactive fg | `#1e2c40` | `(0.118, 0.173, 0.251, 1)` |
| Text primary | `#ddeeff` | `(0.867, 0.933, 1.000, 1)` |
| Text secondary | `#c0d0e4` | `(0.753, 0.816, 0.894, 1)` |
| Text muted | `#6a7e98` | `(0.416, 0.494, 0.596, 1)` |
| Text dimmed | `#374c64` | `(0.216, 0.298, 0.392, 1)` |
| Text very dim | `#263040` | `(0.149, 0.188, 0.251, 1)` |
| Text faintest | `#1e2c40` | `(0.118, 0.173, 0.251, 1)` |
| Accent teal | `#2ec4a0` | `(0.180, 0.769, 0.627, 1)` |
| Accent blue info | `#5aaee8` | `(0.353, 0.682, 0.910, 1)` |
| Tag good `[!]` text | `#2a7a3a` | `(0.165, 0.478, 0.227, 1)` |
| Tag good bg | `#0e2418` | `(0.055, 0.141, 0.094, 1)` |
| Tag crack text | `#7a2222` | `(0.478, 0.133, 0.133, 1)` |
| Tag crack bg | `#240e0e` | `(0.141, 0.055, 0.055, 1)` |
| Tag other text | `#2a3a50` | `(0.165, 0.227, 0.314, 1)` |
| Tag other bg | `#0e1828` | `(0.055, 0.094, 0.157, 1)` |
| Favourite star active | `#cc9922` | `(0.800, 0.600, 0.133, 1)` |
| ROMSET badge text | `#aa7711` | `(0.667, 0.467, 0.067, 1)` |
| ROMSET badge bg | 15% of above | `(0.667, 0.467, 0.067, 0.15)` |

**Manufacturer colours** (used for left border stripe and maker badge):

| Maker | Hex |
|-------|-----|
| Commodore | `#e8a020` |
| Nintendo | `#e01030` |
| Apple | `#7aaccc` |
| Atari | `#e05828` |
| BBC/Acorn | `#cc6600` |
| Sinclair | `#1090e0` |
| Amstrad | `#d01050` |
| Robotron | `#cc3020` |
| VTech | `#4488dd` |
| Oric | `#ee9900` |
| Arcade | `#bb22cc` |
| Various | `#8844ee` |

---

## 3. Data Structures

### 3.1 System

```cpp
enum class SystemType { Home, Console, Arcade, Other };

struct System {
    const char* id;        // short key, e.g. "C64"
    const char* name;      // display name
    const char* maker;     // manufacturer string matching colour table
    int         year;
    const char* cpu;
    SystemType  type;
    ImU32       screenBg;  // CRT background colour (ABGR for ImGui)
    bool        isRomSet;  // true = arcade romset, shows ROMSET badge
};
```

Full list (42 systems):

| id | name | maker | year | cpu | type | screenBg | romSet |
|----|------|-------|------|-----|------|----------|--------|
| C64 | Commodore 64 | Commodore | 1982 | MOS 6510 | Home | 0xFF240504 | false |
| VIC20 | Commodore VIC-20 | Commodore | 1980 | MOS 6502 | Home | 0xFF350010 | false |
| C16 | Commodore 16 | Commodore | 1984 | MOS 7501 | Home | 0xFF380303 | false |
| C116 | Commodore 116 | Commodore | 1984 | MOS 7501 | Home | 0xFF380303 | false |
| PLUS4 | Commodore Plus/4 | Commodore | 1984 | MOS 8501 | Home | 0xFF450305 | false |
| PET | Commodore PET | Commodore | 1977 | MOS 6502 | Home | 0xFF031306 | false |
| C128 | Commodore 128 | Commodore | 1985 | 8502 + Z80 | Home | 0xFF350904 | false |
| NES | Nintendo Ent. Sys. | Nintendo | 1983 | Ricoh 2A03 | Console | 0xFF2C0409 | false |
| FC | Nintendo Famicom | Nintendo | 1983 | Ricoh 2A03 | Console | 0xFF060622 | false |
| APPLE1 | Apple 1 | Apple | 1976 | MOS 6502 | Home | 0xFF030E04 | false |
| AppleII | Apple II | Apple | 1977 | MOS 6502 | Home | 0xFF030E04 | false |
| AppleIIe | Apple IIe | Apple | 1983 | 65C02 | Home | 0xFF030E04 | false |
| AppleIIc | Apple IIc | Apple | 1984 | 65C02 | Home | 0xFF030E04 | false |
| A2600 | Atari 2600 | Atari | 1977 | MOS 6507 | Console | 0xFF000713 | false |
| BBC | BBC Micro Model B | BBC/Acorn | 1981 | MOS 6502 | Home | 0xFF000C0D | false |
| BBCB+ | BBC Micro Model B+ | BBC/Acorn | 1983 | MOS 6502 | Home | 0xFF000C0D | false |
| BBCMstr | BBC Master 128 | BBC/Acorn | 1986 | 65C12 | Home | 0xFF000C0D | false |
| Atom | Acorn Atom | BBC/Acorn | 1980 | MOS 6502 | Home | 0xFF000908 | false |
| Spec48 | ZX Spectrum 48K | Sinclair | 1982 | Z80A | Home | 0xFF090000 | false |
| Spec128 | ZX Spectrum 128K | Sinclair | 1985 | Z80B | Home | 0xFF090000 | false |
| CPC464 | Amstrad CPC 464 | Amstrad | 1984 | Z80A | Home | 0xFF100802 | false |
| CPC664 | Amstrad CPC 664 | Amstrad | 1985 | Z80A | Home | 0xFF100802 | false |
| CPC6128 | Amstrad CPC 6128 | Amstrad | 1985 | Z80A | Home | 0xFF100802 | false |
| LC80 | LC 80 | Robotron | 1984 | U880 / Z80 | Home | 0xFF03050C | false |
| Z9001 | Robotron Z9001 | Robotron | 1984 | U880 / Z80 | Home | 0xFF03050C | false |
| KC87 | Robotron KC 87 | Robotron | 1987 | U880 / Z80 | Home | 0xFF03050C | false |
| KC852 | KC 85/2 | Robotron | 1984 | U880 / Z80 | Home | 0xFF03050C | false |
| KC853 | KC 85/3 | Robotron | 1986 | U880 / Z80 | Home | 0xFF03050C | false |
| KC854 | KC 85/4 | Robotron | 1987 | U880 / Z80 | Home | 0xFF03050C | false |
| Z101301 | Z1013.01 | Robotron | 1985 | U880 / Z80 | Home | 0xFF03050C | false |
| Z101316 | Z1013.16 | Robotron | 1985 | U880 / Z80 | Home | 0xFF03050C | false |
| Z101364 | Z1013.64 | Robotron | 1985 | U880 / Z80 | Home | 0xFF03050C | false |
| VZ200 | VTech VZ200 | VTech | 1982 | Z80 | Home | 0xFF0A0D05 | false |
| VZ300 | VTech VZ300 | VTech | 1984 | Z80 | Home | 0xFF0A0D05 | false |
| Oric1 | Oric-1 | Oric | 1983 | MOS 6502 | Home | 0xFF00050A | false |
| OricAtms | Oric Atmos | Oric | 1984 | MOS 6502 | Home | 0xFF00050A | false |
| CHIP8 | CHIP-8 / SCHIP / XO | Various | 1977 | Virtual | Other | 0xFF1A0407 | false |
| BombJack | Bomb Jack | Arcade | 1984 | Z80 | Arcade | 0xFF0C0D00 | true |
| PacMan | Pac-Man | Arcade | 1980 | Z80 | Arcade | 0xFF00070B | true |
| Pengo | Pengo | Arcade | 1982 | Z80 | Arcade | 0xFF060E00 | true |
| Asteroids | Asteroids | Arcade | 1979 | MOS 6502 | Arcade | 0xFF050300 | true |
| LunarLndr | Lunar Lander | Arcade | 1979 | Custom | Arcade | 0xFF050300 | true |

*Note: screenBg values are 0xAABBGGRR (ImGui `IM_COL32` layout). Derived from the `sc` field in the JSX.*

### 3.2 ROM Entry

```cpp
struct RomEntry {
    std::string name;
    std::string region;   // "PAL", "NTSC", "EUR", "USA", "JPN", "WORLD"
    int         year;
    std::string rev;      // version / revision string
    std::string fmt;      // file extension: ".d64", ".nes", ".tap", ".zip" …
    std::string size;     // human-readable, e.g. "170K"
    std::string tags;     // "[!]", "(crack)", "" …
};
```

### 3.3 Config Field

```cpp
struct ConfigField {
    std::string id;          // machine-readable key
    std::string label;       // display label (short, title-case)
    std::vector<std::string> options;
    std::string defaultValue;
};
using ConfigSchema = std::vector<ConfigField>;
```

### 3.4 App State

```cpp
enum class ActivePanel { Systems, Roms };
enum class SortDir { Asc, Desc };
enum class RomSortCol { Name, Region, Year, Rev, Fmt };

struct AppState {
    ActivePanel panel          = ActivePanel::Systems;
    std::string sysQuery;                    // system search text
    SystemType  typeFilter     = (all);      // add an "All" sentinel
    std::string makerFilter;                 // "" = all
    int         selSysIdx      = 0;          // index into filtered system list
    int         selRomIdx      = 0;          // index into filtered ROM list
    std::string romQuery;
    std::string romRegionFilter;             // "" = all
    std::string romFmtFilter;               // "" = all
    RomSortCol  sortCol        = RomSortCol::Name;
    SortDir     sortDir        = SortDir::Asc;
    std::unordered_map<std::string, std::unordered_map<std::string,std::string>> configs;
    std::unordered_set<std::string> favourites = {"C64","NES","Spec48"};
    bool        sysSearchActive = false;
    bool        romSearchActive = false;
};
```

---

## 4. Configuration Schemas

Map from `system id → ConfigSchema`. Variant systems fall back to a base system's schema:

```
BBCB+, BBCMstr, Atom  → BBC
CPC664, CPC6128       → CPC464
AppleIIe, AppleIIc,
APPLE1                → AppleII
C16, C116, PLUS4      → C64
FC                    → NES
Pengo, LunarLndr      → Asteroids  (only lives/ships field applies)
VZ300                 → VZ200
OricAtms              → Oric1
Z9001, KC87, KC852,
KC853, KC854,
Z101301-64, LC80      → CHIP8
```

### C64
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | PAL, NTSC | PAL |
| sid | SID | MOS 6581, MOS 8580, Dual 6581 | MOS 6581 |
| ram | RAM Exp. | None, 256K REU, 512K REU, GeoRAM | None |
| port1 | Port 1 | None, Joystick, Mouse 1351 | None |
| port2 | Port 2 | None, Joystick, Mouse 1351 | Joystick |
| drive | Drive | 1541 (fast), 1541 (accurate), 1571 | 1541 (fast) |

### VIC20
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | PAL, NTSC | PAL |
| ram | RAM | Unexpanded, 8K, 16K, 24K, 27.5K | Unexpanded |
| port | Joystick | None, Port 1 | None |
| ieee | Disk | None, 1540, 1541 | None |

### C128
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | PAL, NTSC | PAL |
| mode | Boot | C128, C64, CP/M | C128 |
| ram | RAM Exp. | 128K, 512K REU | 128K |
| sid | SID | MOS 8580, MOS 6581 | MOS 8580 |

### NES
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | NTSC, PAL, Dendy | NTSC |
| port2 | Port 2 | Gamepad, Zapper, None | Gamepad |
| tap | Multi-tap | None, Four Score, Satellite | None |

### Spec48
| id | label | options | default |
|----|-------|---------|---------|
| joy | Joystick | None, Kempston, Interface 2, Cursor | None |
| if1 | Interface 1 | Disabled, Enabled | Disabled |

### Spec128
| id | label | options | default |
|----|-------|---------|---------|
| joy | Joystick | None, Kempston, Interface 2, Cursor | None |
| mode | Boot Mode | 128K, 48K compat. | 128K |
| ay | AY Chip | Internal, None | Internal |

### A2600
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | NTSC, PAL, PAL-60, SECAM | NTSC |
| ctrl1 | Left | Joystick, Paddle, Driving, None | Joystick |
| ctrl2 | Right | Joystick, Paddle, Driving, None | Joystick |

### AppleII
| id | label | options | default |
|----|-------|---------|---------|
| ram | RAM | 48K, 64K (Lang Card), 128K (80-col) | 48K |
| joy | Joystick | None, Analog, Gamepad | None |
| slot6 | Slot 6 | Disk II, None | Disk II |

### BBC
| id | label | options | default |
|----|-------|---------|---------|
| rom | Sideways | DFS, ADFS, DFS + Watford | DFS |
| joy | Analogue | None, Joystick | None |
| tube | 2nd Proc. | None, 6502 (3 MHz), Z80, ARM | None |

### CPC464
| id | label | options | default |
|----|-------|---------|---------|
| joy | Joystick | None, Digital | None |
| mem | Mem. Exp. | None, Silicon Disc 256K | None |

### PacMan
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | Japan, World | Japan |
| lives | Lives | 1, 2, 3, 5 | 3 |
| speed | Difficulty | Easy, Normal, Hard | Normal |

### Asteroids
| id | label | options | default |
|----|-------|---------|---------|
| lives | Ships | 2, 3, 4, 5 | 3 |
| shield | Center dot | On, Off | Off |

### BombJack
| id | label | options | default |
|----|-------|---------|---------|
| lives | Lives | 2, 3, 5 | 3 |
| bonus | Bonus at | 10K, 20K, 30K | 10K |

### CHIP8
| id | label | options | default |
|----|-------|---------|---------|
| mode | Mode | CHIP-8, SCHIP, XO-CHIP | CHIP-8 |
| hz | Speed | 10 cyc/f, 30 cyc/f, 60 cyc/f, Turbo | 30 cyc/f |
| buzz | Buzzer | Enabled, Disabled | Enabled |

### VZ200
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | PAL, NTSC | PAL |
| joy | Joystick | None, Digital | None |

### Oric1
| id | label | options | default |
|----|-------|---------|---------|
| region | Region | PAL, NTSC | PAL |
| joy | Joystick | None, Kempston-style | None |

---

## 5. ROM Library

Below is the full seeded ROM dataset. In production these would come from TOSEC directory scanning. Missing entries produce an empty `RomEntry` list which shows the "Add TOSEC directory" empty state.

### C64 (41 entries)
Impossible Mission (USA, 1984, v1.0, .d64, 170K, [!]), Impossible Mission II (USA, 1988, v1.0, .d64, 170K), Maniac Mansion (USA, 1987, v1.0, .d64, 170K, [!]), Zak McKracken (USA, 1988, v1.0, .d64, 340K), The Last Ninja (EUR, 1987, v1.1, .d64, 170K, [!]), The Last Ninja 2 (EUR, 1988, v1.0, .d64, 170K), The Last Ninja 3 (EUR, 1991, v1.0, .d64, 170K), Turrican (EUR, 1990, v1.0, .d64, 170K, [!]), Turrican II (EUR, 1991, v1.0, .d64, 340K, [!]), Turrican II (EUR, 1991, crack, .d64, 170K, (crack)), IK+ (EUR, 1988, v1.0, .d64, 170K), International Karate (EUR, 1986, v1.0, .d64, 170K), Bubble Bobble (EUR, 1987, v1.0, .d64, 170K), Fort Apocalypse (USA, 1982, v1.0, .d64, 170K), Leaderboard Golf (USA, 1986, v1.0, .d64, 170K), Pitstop II (USA, 1984, v1.0, .d64, 170K), Winter Games (USA, 1985, v1.0, .d64, 170K), Summer Games (USA, 1984, v1.0, .d64, 170K), Elite (EUR, 1985, v1.0, .d64, 170K, [!]), Wizball (EUR, 1987, v1.0, .d64, 170K), Paradroid (EUR, 1985, v1.0, .d64, 170K, [!]), Uridium (EUR, 1986, v1.0, .d64, 170K), Katakis (EUR, 1988, v1.0, .d64, 170K), R-Type (EUR, 1988, v1.0, .d64, 170K), Great Giana Sisters (EUR, 1987, v1.0, .d64, 170K), Commando (EUR, 1985, v1.0, .d64, 170K), Green Beret (EUR, 1986, v1.0, .d64, 170K), Barbarian (EUR, 1987, v1.0, .d64, 170K), Barbarian II (EUR, 1988, v1.0, .d64, 170K), Ghostbusters (USA, 1984, v1.0, .d64, 170K), Jumpman (USA, 1983, v1.0, .d64, 170K), Blue Max (USA, 1983, v1.0, .d64, 170K), Monty on the Run (EUR, 1985, v1.0, .d64, 170K), Cauldron II (EUR, 1986, v1.0, .d64, 170K), Jet Set Willy (EUR, 1984, v1.0, .d64, 170K), Spy vs Spy (USA, 1984, v1.0, .d64, 170K), Archon (USA, 1984, v1.0, .d64, 170K), Arkanoid (EUR, 1987, v1.0, .d64, 170K), Flimbo's Quest (EUR, 1990, v1.0, .d64, 170K), Creatures (EUR, 1990, v1.0, .d64, 170K), Creatures II (EUR, 1992, v1.0, .d64, 170K)

### NES (20 entries)
Super Mario Bros. (NTSC, 1985, v1.0, .nes, 40K, [!]), Super Mario Bros. (PAL, 1987, PAL, .nes, 40K), Super Mario Bros. 2 (NTSC, 1988, v1.0, .nes, 128K), Super Mario Bros. 3 (NTSC, 1990, v1.0, .nes, 384K, [!]), Super Mario Bros. 3 (PAL, 1991, PAL, .nes, 384K), Castlevania (NTSC, 1987, v1.0, .nes, 128K, [!]), Castlevania II (NTSC, 1988, v1.0, .nes, 128K), Castlevania III (NTSC, 1990, v1.0, .nes, 256K), Mega Man 2 (NTSC, 1989, v1.0, .nes, 128K, [!]), Mega Man 3 (NTSC, 1990, v1.0, .nes, 256K), Contra (NTSC, 1988, v1.0, .nes, 128K, [!]), Metroid (NTSC, 1987, v1.0, .nes, 128K), The Legend of Zelda (NTSC, 1987, v1.0, .nes, 128K, [!]), Zelda II (NTSC, 1988, v1.0, .nes, 128K), Punch-Out!! (NTSC, 1987, v1.0, .nes, 128K), Ninja Gaiden (NTSC, 1989, v1.0, .nes, 128K), Battletoads (NTSC, 1991, v1.0, .nes, 256K), DuckTales (NTSC, 1989, v1.0, .nes, 64K), Bionic Commando (NTSC, 1988, v1.0, .nes, 128K), Jackal (NTSC, 1988, v1.0, .nes, 128K)

### Spec48 (12 entries)
Manic Miner (EUR, 1983, v1.0, .tap, 45K, [!]), Jet Set Willy (EUR, 1984, v1.0, .tap, 48K), R-Type (EUR, 1988, v1.0, .tap, 48K), Head Over Heels (EUR, 1987, v1.0, .tap, 48K, [!]), Chuckie Egg (EUR, 1983, v1.0, .tap, 20K), Saboteur (EUR, 1985, v1.0, .tap, 48K), Horace Goes Skiing (EUR, 1982, v1.0, .tap, 12K), Dizzy (EUR, 1987, v1.0, .tap, 48K), Dizzy II (EUR, 1988, v1.0, .tap, 48K), Knight Lore (EUR, 1984, v1.0, .tap, 48K, [!]), Atic Atac (EUR, 1983, v1.0, .tap, 48K), 3D Ant Attack (EUR, 1983, v1.0, .tap, 48K)

### CPC464 (9 entries)
Barbarian (EUR, 1987, v1.0, .dsk, 178K), Arkanoid (EUR, 1987, v1.0, .dsk, 178K), Ghostbusters (EUR, 1984, v1.0, .dsk, 178K), Green Beret (EUR, 1986, v1.0, .dsk, 178K), Roland on the Ropes (EUR, 1984, v1.0, .dsk, 178K), Commando (EUR, 1985, v1.0, .dsk, 178K), Dizzy (EUR, 1987, v1.0, .dsk, 178K), Head Over Heels (EUR, 1987, v1.0, .dsk, 178K), R-Type (EUR, 1988, v1.0, .dsk, 178K)

### A2600 (10 entries)
Adventure (USA, 1979, v1.0, .a26, 4K), Pitfall! (USA, 1982, v1.0, .a26, 4K, [!]), Kaboom! (USA, 1981, v1.0, .a26, 4K), River Raid (USA, 1982, v1.0, .a26, 4K, [!]), Pitfall II (USA, 1984, v1.0, .a26, 8K), Space Invaders (USA, 1980, v1.0, .a26, 4K), Missile Command (USA, 1981, v1.0, .a26, 4K), Centipede (USA, 1982, v1.0, .a26, 4K), Frogger (USA, 1982, v1.0, .a26, 4K), Enduro (USA, 1983, v1.0, .a26, 4K)

### AppleII (7 entries)
Karateka (USA, 1984, v1.0, .dsk, 140K), Prince of Persia (USA, 1990, v1.0, .dsk, 140K, [!]), Ultima IV (USA, 1985, v1.0, .dsk, 280K), Lode Runner (USA, 1983, v1.0, .dsk, 140K), Wizardry I (USA, 1981, v1.0, .dsk, 140K), Choplifter (USA, 1982, v1.0, .dsk, 140K), Castle Wolfenstein (USA, 1981, v1.0, .dsk, 140K)

### PacMan (3 sets)
pacman Japan rev1 (JPN, 1980, rev1, .zip, 20K, [!]), pacmanjp Japan alt (JPN, 1980, alt, .zip, 20K), pacmanf Fastrom hack (WORLD, 1981, hack, .zip, 20K)

### BombJack (3 sets)
bombjack World (WORLD, 1984, v1.0, .zip, 64K, [!]), bombjacka alternate (WORLD, 1984, alt, .zip, 64K), bombjackb bootleg (WORLD, 1984, boot, .zip, 64K)

### Pengo (3 sets)
pengo World rev2 (WORLD, 1982, rev2, .zip, 32K, [!]), pengo World rev1 (WORLD, 1982, rev1, .zip, 32K), pengo Japan (JPN, 1982, JPN, .zip, 32K)

### Asteroids (3 sets)
asteroid rev4 (USA, 1979, rev4, .zip, 12K, [!]), asteroida rev1 (USA, 1979, rev1, .zip, 12K), asterockv hack (WORLD, 1979, hack, .zip, 12K)

### Lunar Lander (2 sets)
llander rev2 (USA, 1979, rev2, .zip, 8K, [!]), llander1 rev1 (USA, 1979, rev1, .zip, 8K)

### CHIP-8 (6 entries)
PONG (WORLD, 1972, orig, .ch8, 256B), TETRIS (WORLD, 1984, v1.0, .ch8, 512B), BREAKOUT (WORLD, 1978, v1.0, .ch8, 256B), Space Invaders (WORLD, 1978, v1.0, .ch8, 512B), Cave Explorer (WORLD, 2018, v1.0, .ch8, 2K), Octo Adventure (WORLD, 2015, v1.0, .xo8, 4K)

### C128 (5 entries)
GEOS 2.0 64 mode (EUR, 1988, v2.0, .d64, 340K), GEOS 2.0 128 mode (EUR, 1988, v2.0, .d81, 800K), Ultima V (EUR, 1988, v1.0, .d64, 340K), The Bard's Tale (EUR, 1988, v1.0, .d64, 170K), Might & Magic II (EUR, 1989, v1.0, .d64, 340K)

All other systems have an empty ROM list (shows the "Add TOSEC directory" empty state).

---

## 6. Filtering and Sorting Logic

### System filter

Applied in order:

1. `typeFilter` — exact match on `SystemType` (skip if "All")
2. `makerFilter` — exact string match on `system.maker` (skip if "")
3. `sysQuery` — case-insensitive substring search across: `name`, `id`, `maker`, `cpu`, `type-string`, `year-string`

### ROM filter

Applied in order:

1. `romQuery` — case-insensitive substring search across: `name`, `region`, `year-string`, `rev`, `fmt`, `tags`
2. `romRegionFilter` — exact match on `region` (skip if "")
3. `romFmtFilter` — exact match on `fmt` (skip if "")

### ROM sort

Sort the post-filter result by the selected column:
- `Name` → `std::string` lexicographic
- `Year` → integer numeric
- `Region`, `Rev`, `Fmt` → `std::string` lexicographic

Direction toggles between Asc/Desc. Default: Name Asc.

---

## 7. Keyboard Navigation

Process `ImGui::IsKeyPressed` **only when no ImGui widget has keyboard focus** (i.e., `!ImGui::IsAnyItemActive()` or check that the active ID is not a text input). Text inputs consume keys themselves and must be handled separately.

### Global (no text input focused)

| Key | Action |
|-----|--------|
| `↑` | If panel == Systems: `selSysIdx--` (clamp 0). If panel == Roms: `selRomIdx--` (clamp 0). |
| `↓` | If panel == Systems: `selSysIdx++` (clamp filtSys.size-1). If panel == Roms: `selRomIdx++` (clamp filtRoms.size-1). |
| `Tab` | Toggle `panel` between Systems ↔ Roms. |
| `Enter` | If panel == Systems: switch to Roms panel, set `selRomIdx = 0`. If panel == Roms: trigger Launch for selected ROM. |
| `/` | Focus the search input of the active panel (call `ImGui::SetKeyboardFocusHere` before rendering the relevant `InputText`). |
| `f` / `F` | If panel == Roms: focus ROM search input. |
| `Esc` | If panel == Roms: switch panel to Systems. |
| `s` / `S` | If panel == Roms: switch panel to Systems. |

### Inside system search input (`InputText`)

| Key | Action |
|-----|--------|
| `Enter` | Unfocus input (`ImGui::SetKeyboardFocusHere(-1)` or let ImGui lose focus naturally). |
| `↓` | Same as Enter — return focus to system list. |
| `Esc` | Clear `sysQuery`, unfocus. |

### Inside ROM search input

| Key | Action |
|-----|--------|
| `Enter` | Unfocus input. |
| `↓` | Unfocus input. |
| `Esc` | Clear `romQuery`, unfocus. |

### Double-click

Double-clicking a system row: switch panel to Roms and set `selRomIdx = 0`.

### Auto-scroll

After changing `selSysIdx`, call `ImGui::SetScrollHereY()` on the selected item if it is outside the visible list region. Same for ROM list.

---

## 8. Component Details

### 8.1 Top Bar

Height: ~28 px. Content left to right:

- **Logo** `◈ CERMU` — monospace font, colour `#2ec4a0`, size 14, weight bold, letter-spacing 2
- Vertical separator (1 px wide, 14 px tall, colour `#141c28`)
- **Type filter buttons**: `[All] [Home] [Console] [Arcade] [Other]`
  - Active: bg `#0e1e30`, fg `#5aaee8`, border `#1a3448`
  - Inactive: bg transparent, fg `#263444`, border `#111c28`
  - On click: set `typeFilter`, reset `selSysIdx = 0`
- **Maker combo**: label `Maker` + `ImGui::Combo` with all unique maker names + "All"
  - On change: reset `selSysIdx = 0`
  - Border colour: `#1a3050` when active selection, else `#111826`
- Right-aligned: `N / 42 systems` in colour `#1a2438`, size 10

### 8.2 System List Panel (left, 248 px)

#### Search input
- Full-width minus 8 px padding
- Bg `#090c1c`, border changes to `#243a54` when focused
- Placeholder: "Name, CPU, year, type…"
- `✕` clear button appears when query non-empty

#### System rows
Each row: ~46 px tall, 1 px separator at bottom (`#090d18`).

Layout (left to right, all vertically centred):

1. **Mini CRT thumbnail** — 46×36 px (see section 8.5)
2. **Name + meta column** (flex fill):
   - Line 1: system name, 12 px, colour `#cce0f8` if selected else `#6a7e98`, bold if selected
   - Line 2: year (10 px, `#263040`) + maker badge + optional ROMSET badge
3. **Favourite star** (`★`, `#aa8820`) — shown only if in favourites set

**Selected row**: bg `#0c1c2e`, left border `2.5 px` solid in maker colour, text brightened.

**Maker badge**: bg = maker colour at 18% alpha, text = maker colour at full, size 9, rounded 2 px. Show only first word before `/` (so "BBC/Acorn" → "BBC").

**ROMSET badge**: bg `rgba(170,119,17,0.15)`, text `#aa7711`, size 9.

**Focus indicator**: left panel gets a 1 px outline (`#1a3252`) when `panel == Systems`.

**Double-click** on any row: switch to Roms panel.

### 8.3 Content Panel (right, flex)

The content panel is divided into four vertical zones:

#### Zone A — System header
Height: auto (~80 px typically). Background `#060810` with a radial gradient tinted in the maker colour (left edge, 9% opacity).

Layout:

- **Mini CRT** 70×55 px (left)
- **Name / meta column** (flex):
  - Row 1: system name (16 px, `#ddeeff`, bold), year (11 px, `#263040`), · separator, CPU (11 px monospace, `#263040`), maker badge, type badge, optional ROMSET badge
  - Row 2: **config strip** (see below)
- **Action buttons** (right edge, vertical stack):
  - `★` favourite toggle (13 px, active `#cc9922` else `#263040`)
  - `⚙` settings placeholder
  - `▶ Launch` button — bg = maker colour, fg black, bold

**Config strip**: a horizontal `FlowLayout`-style row of combo boxes, each wrapped in a pill-shaped container:

- Container bg: `#080c1c` (default value) or `#0a1426` (non-default value)
- Container border: `#111826` (default) or `#1a3050` (changed) — this is the visual indicator that a value has been changed from its default
- Label: uppercase, 9 px, `#263444`
- `ImGui::Combo` inside: 11 px, colour `#607898` (default) or `#9ac8ee` (changed)
- Each field corresponds to a `ConfigField` from the schema for the selected system

#### Zone B — ROM filter bar
Height: ~34 px. Background same as Zone A.

Left to right:

1. **ROM search input** (200 px wide): placeholder "Title, year, region, revision, format…", same style as system search
2. **Region combo**: label "REGION" + `ImGui::Combo` populated from unique regions in the current system's ROM list. Options: ["All"] + sorted unique regions. Border highlights when non-All.
3. **Format combo**: label "FORMAT" + `ImGui::Combo` populated from unique formats. Same highlighting.
4. Right-aligned: ROM count
   - When filtered: `N` (colour `#4a7aaa`) ` / M files` (colour `#1e2c40`)
   - When unfiltered: `M files` (colour `#1e2c40`)
   - For romset systems, use "sets" instead of "files"

**Focus indicator**: right panel gets 1 px outline (`#1a3252`) when `panel == Roms`.

#### Zone C — Column headers
Height: ~22 px. Non-scrolling. Background `#060810`.

Columns (left to right):

| Column | Width | Label | Sortable |
|--------|-------|-------|----------|
| Name | flex | NAME | yes |
| Region | 66 px | REGION | yes |
| Year | 50 px | YEAR | yes |
| Rev | 76 px | REV | yes |
| Format | 56 px | FMT | yes |
| Tags | 52 px | TAGS | no |

Click a header to sort by that column (second click reverses direction). Active column: text `#7aaccc`, icon `↑` or `↓`. Inactive column: text `#1e2c40`, icon `⇅` at low opacity. Headers are uppercase, 10 px, font-weight 500.

#### Zone D — ROM rows (scrollable)
Each row: ~26 px tall, 1 px bottom separator `#08090e`.

Selected: bg `#0a1c30`, left border `2 px #2a5888`.
Hover (non-selected): bg `#090c18`.

Cell layout:

- **Name**: flex fill, 12 px, colour `#c0d8f4` (selected) or `#6880a0`, ellipsis on overflow, 8 px right padding
- **Region**: 66 px, 11 px monospace, `#374c64`
- **Year**: 50 px, 11 px, `#374c64`
- **Rev**: 76 px, 11 px, `#374c64`, ellipsis
- **Format**: 56 px, 11 px monospace, `#4a6680`
- **Tags**: 52 px — tag badge component (see 8.6)

**Empty state** (no ROMs for system):

```
No library configured for this system
[+ Add TOSEC directory…]  ← button, bg #0c1828, border #1c2e44, text #4a7aaa
```

**Footer row** (after last ROM, when ROMs exist): `+ Add TOSEC directory…` text link in `#131e2e`.

**Double-click** a ROM row: trigger Launch.

### 8.4 Status Bar

Height: ~22 px. Background `#050710`, top border `#0d1420`.

Left to right: key hint pills, right-aligned: panel name + version.

**Pill format**: `[key]` label — where `[key]` is `kbd` style:

- Key bg: `#0a0d1c`, border `#182030`, text `#263848`, rounded 3, 9 px monospace
- Label: 10 px, `#1e2c40`

**Context-dependent hints**:

| Condition | Hints shown |
|-----------|-------------|
| panel == Systems, no search focused | ↑↓ Navigate, ↵ Open ROMs, Tab Switch panel, / Search |
| panel == Roms, no search focused | ↑↓ Navigate, ↵ Launch, Tab Switch panel, / Search, Esc Back to systems |
| System search focused | ↵ Confirm, Esc Clear |
| ROM search focused | ↵/↓ Jump to list, Esc Clear |

Hints for focused search override the normal hints; show them in accent teal `#2ec4a0`.

Right edge: `SYSTEMS · CERMU v0.1` or `ROMS · CERMU v0.1` in `#0e1828`, monospace.

### 8.5 Mini CRT Thumbnail

Rendered as a small texture (or immediate-mode ImDrawList calls) showing a fake terminal screen.

**Algorithm** (deterministic from system id hash):

```
hash = FNV-1a(system.id)
rng(seed, n) = |sin(seed + n * 1234567.891)|

screenBg  = system.screenBg
textColor = green (#1eee50)  if id contains "APPLE" or "PET" or "BBC" or "Atom"
          = amber (#ffaa33)  if id contains "A2600" or "LC80" or "Z9001" or "KC" or "Z10"
          = blue  (#5aaee8)  otherwise

rows = max(5, floor(height / 6))
for i in 0..rows:
    lineWidth  = max(7, floor(rng(hash, i*3+1) * (width - 16)))
    xIndent    = floor(rng(hash, i*3+2) * 8)
    y          = 4 + i * (height - 10) / rows
    draw rect at (3 + xIndent, y) size (lineWidth, 2) colour textColor opacity 0.7

draw small cursor rect: (3, height-6) size (5, 2.5) colour textColor

draw horizontal scanlines every 3px: 1px height, black, opacity 0.16
```

In the system header, use 70×55 px. In the list rows, use 46×36 px. Pre-render these as `ImTextureID` at startup for performance, keyed by `(id, width, height)`.

### 8.6 Tag Badge

```
tag == "[!]"         → bg #0e2418  text #2a7a3a
tag contains "crack" → bg #240e0e  text #7a2222
tag contains "trainer" → bg #0a110e  text #224a30
tag non-empty, other → bg #0e1828  text #2a3a50
tag empty            → render nothing
```

Size 9 px, bold, letter-spacing 0.3, padding 1×5, rounded 3.

### 8.7 Background gradient in system header

Use `ImDrawList::AddRectFilledMultiColor` or a manual fragment: draw the header background as a plain rect first, then overlay a second rect from the left edge fading to transparent (width ~55% of panel), where the left colour is `makerColour` at 9% alpha (0x17 in the A byte) and the right colour is fully transparent.

---

## 9. ImGui Implementation Notes

### Window setup

```cpp
ImGui::SetNextWindowPos({0, 0});
ImGui::SetNextWindowSize(io.DisplaySize);
ImGui::Begin("##cermu", nullptr,
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus);
```

Push `ImGuiStyleVar_WindowPadding, {0,0}` and `ImGuiStyleVar_ItemSpacing, {0,0}` for the outermost layout, then restore per-region.

### Panels via `ImGui::BeginChild`

```cpp
// Left panel
ImGui::BeginChild("##syspanel", {248, contentHeight}, false, ImGuiWindowFlags_NoScrollbar);
// ... render system list ...
ImGui::EndChild();

ImGui::SameLine();

// Right panel
ImGui::BeginChild("##rompanel", {0, contentHeight}, false, ImGuiWindowFlags_NoScrollbar);
// ... render content ...
ImGui::EndChild();
```

### Config combos

```cpp
// Render a config field pill
for (auto& field : schema) {
    auto& val = configs[sysId][field.id];
    if (val.empty()) val = field.defaultValue;
    bool changed = (val != field.defaultValue);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,    changed ? COL_CFG_BG_CHANGED  : COL_CFG_BG);
    ImGui::PushStyleColor(ImGuiCol_Border,     changed ? COL_CFG_BD_CHANGED  : COL_CFG_BD);
    ImGui::PushStyleColor(ImGuiCol_Text,       changed ? COL_CFG_TEXT_CHG    : COL_CFG_TEXT);

    int idx = indexOfInVector(field.options, val);
    if (ImGui::Combo(("##cfg_" + field.id).c_str(), &idx, /* items */, field.options.size()))
        val = field.options[idx];

    ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 5);
}
```

### Sortable column headers

```cpp
if (ImGui::Selectable("NAME", sortCol == RomSortCol::Name, 0, {colNameWidth, 20})) {
    if (sortCol == RomSortCol::Name) sortDir = flip(sortDir);
    else { sortCol = RomSortCol::Name; sortDir = SortDir::Asc; }
}
```

Alternatively use `ImGui::TableSetupColumn` + `ImGui::TableNextColumn` with `ImGuiTableFlags_Sortable`.

### ROM table

```cpp
ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
    ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH;
if (ImGui::BeginTable("##roms", 6, flags, {0, availHeight})) {
    ImGui::TableSetupScrollFreeze(0, 1); // freeze header row
    ImGui::TableSetupColumn("NAME",   ImGuiTableColumnFlags_DefaultSort, 0);
    ImGui::TableSetupColumn("REGION", ImGuiTableColumnFlags_NoSort, 66);
    // ... etc
    ImGui::TableHeadersRow();
    // render rows
    ImGui::EndTable();
}
```

### Keyboard focus for search

To programmatically focus a text input:

```cpp
static bool wantFocusSysSearch = false;
if (wantFocusSysSearch) {
    ImGui::SetKeyboardFocusHere();
    wantFocusSysSearch = false;
}
ImGui::InputText("##syssearch", sysQuery, ...);
```

Set `wantFocusSysSearch = true` one frame before to get the timing right.

### Selected item scroll

```cpp
if (selSysIdx != prevSelSysIdx) {
    ImGui::SetScrollHereY(0.5f); // call this inside the child window, on the selected item's row
}
```

### Outline for focused panel

```cpp
auto* dl = ImGui::GetWindowDrawList();
if (panel == ActivePanel::Systems) {
    auto p0 = ImGui::GetWindowPos();
    auto p1 = p0 + ImGui::GetWindowSize();
    dl->AddRect(p0, p1, IM_COL32(0x1a, 0x32, 0x52, 0xff), 0, 0, 1.0f);
}
```

---

## 10. Fonts

Use two typefaces:

| Role | Font | Size | Style |
|------|------|------|-------|
| UI default | Sora (or Inter, Segoe UI as fallback) | 12–16 px | Regular / Medium |
| Monospace (logo, kbd hints, region/format cols) | JetBrains Mono (or Consolas) | 9–13 px | Regular / Bold |

Load both in `ImFontAtlas`. Use `PushFont` / `PopFont` to switch. The logo `◈ CERMU` uses the monospace font at 14 px, bold.

---

## 11. Summary of Interactive Behaviours

| Element | Interaction | Result |
|---------|------------|--------|
| Type filter button | Click | Update `typeFilter`, reset `selSysIdx` |
| Maker combo | Change | Update `makerFilter`, reset `selSysIdx` |
| System row | Single click | Set `selSysIdx`, set panel to Systems |
| System row | Double click | Set `selSysIdx`, set panel to Roms, reset `selRomIdx` |
| System search | Type | Update `sysQuery`, reset `selSysIdx` |
| Favourite `★` | Click | Toggle `sysId` in `favourites` set |
| Config combo | Change | Update `configs[sysId][fieldId]` |
| `▶ Launch` | Click | Launch selected system (no ROM) |
| ROM search | Type | Update `romQuery`, reset `selRomIdx` |
| Region combo | Change | Update `romRegionFilter`, reset `selRomIdx` |
| Format combo | Change | Update `romFmtFilter`, reset `selRomIdx` |
| Column header | Click | Sort by column, toggle direction |
| ROM row | Single click | Set `selRomIdx`, set panel to Roms |
| ROM row | Double click | Launch selected ROM |
| `+ Add TOSEC dir…` | Click | Open file picker (not yet implemented) |

---

*End of specification.*
