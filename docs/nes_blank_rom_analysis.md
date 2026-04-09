# NES Blank-Screen ROM Analysis

**Date:** 2026-04-09
**Test:** 1430 ROMs, 60 frames headless, composite-signal color detection
**Initial Results:** 919 PASS (64.3%), 509 BLANK (35.6%), 2 CRASH

---

## Fixes Applied

| Fix | Mapper(s) | ROMs Fixed | Commit | Status |
|-----|-----------|------------|--------|--------|
| MMC1: PRG bank offset no longer hardcoded for 256KB | 1 | 241 | `8427a8c2` | **VERIFIED** |
| AxROM: removed spurious bus conflict (ANROM boards have none) | 7 | 16 | `8427a8c2` | **VERIFIED** |
| Taito X1-005/X1-017: PRG-RAM write-protect + A2 mirror decoding | 80, 82, 207 | 0 | `8427a8c2` | Register writes reach mapper; games still blank (additional issue TBD) |
| Namco 175/340: rewrite register map, sub-mapper differentiation | 210 | 7 | `55beae28` | **VERIFIED** |
| Bandai FCG: PRG-RAM write protection + address decode + I2C timing | 16 | 8 | `bc12abe1` | **VERIFIED** |
| NES-QJ: PRG-RAM write protection for outer bank register | 47 | 1 | `bc12abe1` | **VERIFIED** |
| MMC1: PRG/CHR bank address mirroring for out-of-range banks | 1 | 6 | `2063cf3e` | **VERIFIED** |
| Systemic PRG/CHR bank address mirroring across 8 mappers | 4,5,13,28,64,118,119,232 | 46 | `5c72e7a4` | **VERIFIED** |

---

## Re-test Results

### Progression

| Status | Initial (60f) | +MMC1/AxROM | +210/016/047 | +MMC1 wrap (300f) | +Systemic wrap (300f) | Total Delta |
|--------|-------------:|------------:|-------------:|-----------------:|---------------------:|------------:|
| PASS   |          919 |       1210  |         1226 |             1290 |                 1390 | **+471** |
| BLANK  |          509 |        217  |          201 |              137 |                   37 | **-472** |
| CRASH  |            2 |          2  |            2 |                2 |                    2 | 0 |
| TIMEOUT|            0 |          1  |            1 |                1 |                    1 | +1 |

**472 previously blank ROMs now show graphics (93% reduction).**

### Remaining 37 Blanks by Mapper

| Mapper | Count | Description | Notes |
|--------|------:|-------------|-------|
| 5 | 10 | MMC5 | Complex mapper, incomplete impl |
| 19 | 7 | Namco 163 | IRQ or sound RAM issue |
| 80 | 6 | Taito X1-005 | Register fix applied but deeper issue |
| 4 | 3 | MMC3/TxROM | Multi-cart + 2 genuine bugs |
| 96 | 2 | Oeka Kids | Special input device required |
| 69 | 2 | Sunsoft FME-7 | Dodge Danpei + translation |
| 16 | 1 | Bandai FCG | Famicom Jump II (actually mapper 153) |
| 33 | 1 | Taito TC0190 | Bakushou Jinsei Gekijou 3 |
| 40 | 1 | FDS SMB2J Hack | Super Mario Bros 2 (J) FDS hack |
| 75 | 1 | VRC1 | Ninja Jajamaru |
| 120 | 1 | FDS Hack | Tobidase Daisakusen |
| 140 | 1 | Jaleco JF-11 | Bio-Warrior Dan |
| 207 | 1 | Taito X1-017 | Fudou Myouou Den |

---

## Summary by Root Cause

| # | Root Cause | Mapper(s) | Original | Remaining | Fix Difficulty |
|---|-----------|-----------|--------:|----------:|----------------|
| 1 | [MMC1 PRG bank bugs](#1-mmc1-prg-bank-calculation-bug) | 1 | 305 | 0 | **DONE** (241 bank offset + 58 slow starters + 6 bank wrapping) |
| 2 | [Taito X1-005/X1-017 register shadowing](#2-taito-x1-005x1-017-register-shadowing) | 80, 82, 207 | 12 | 7 | Partial (deeper issue; mapper 82 now passes) |
| 3 | [Bandai FCG mapper bugs](#3-bandai-fcg-mapper-16) | 16 | 19 | 1 | **NEAR-DONE** (8 I2C, 10 slow starters; 1 mapper 153 misidentified) |
| 4 | [Namco 175/340 mapper bug](#4-namco-175340-mapper-210) | 210 | 7 | 0 | **DONE** (all 7 fixed) |
| 5 | [AxROM partial failures](#5-axrom-mapper-7) | 7 | 16 | 0 | **DONE** (all 16 fixed) |
| 6 | [MMC5 incomplete implementation](#6-mmc5-mapper-5) | 5 | 11 | 10 | Hard (1 fixed by bank wrapping) |
| 7 | [Namco 163 issue](#7-namco-163-mapper-19) | 19 | 10 | 7 | Medium (3 were slow starters) |
| 8 | [Systemic bank wrapping](#8-systemic-bank-wrapping) | multiple | ~100 | 0 | **DONE** (bank mirroring + slow starters) |
| 9 | [Minor mappers](#9-minor-mapper-issues) | various | ~15 | 8 | Varies |
| 10 | [Slow starters (need >60 frames)](#10-slow-starters) | 0,2,3,4,etc. | ~115 | 0 | **DONE** (all resolved at 300 frames) |

**New TIMEOUT:** Days of Thunder (North America) — mapper 0, likely infinite loop or timing issue

---

## Detailed Analysis

### 1. MMC1 PRG Bank Calculation Bug

**Impact:** 305 ROMs (60% of all blanks) → **0 remaining**
**Fix:** Two issues found and fixed
**File:** `src/systems/nes/cartridge/mappers/mapper_001_mmc1.hpp`

#### Issue A: Fixed-bank offset hardcoded for 256KB (commit `8427a8c2`)

In `get_prg_bank_config()`, PRG mode 3 (fix last bank at $C000) uses:
```cpp
uint32_t last_base = prg_base + 0x3C000; // last 16KB of 256KB half
```
This hardcodes 256KB as the PRG size. For ROMs ≤128KB, `0x3C000 >= prg_rom_size_`
so the bounds check returns nullptr for pages 4–7 ($C000–$FFFF). The CPU's reset
vector at $FFFC reads open bus → blank screen. Fixed 241 ROMs.

#### Issue B: Out-of-range bank numbers not mirrored (NEW)

Games write PRG bank numbers that exceed the physical ROM capacity
(e.g. bank 6 on a 64K/4-bank ROM). On real hardware, upper address lines
beyond ROM capacity are unconnected, so the ROM mirrors naturally.
Our code checked `(offset < prg_rom_size_) ? ptr : nullptr` which set
page pointers to nullptr, crashing the game.

**Fix:** Replace bounds-check-to-nullptr with bitmask wrapping:
```cpp
const uint32_t prg_mask = static_cast<uint32_t>(prg_rom_size_) - 1;
// ...
uint32_t offset = (bank_base + i * 0x1000) & prg_mask;
config.prg_pages[i] = prg_rom_ + offset;
```
Same fix applied to CHR-ROM banking.

**Affected ROMs (6):**
| ROM | PRG | CHR | Out-of-range bank |
|-----|----:|----:|-------------------|
| Knight Rider | 64K | 128K | PRG bank 6 (max 3) |
| Sesame Street Countdown | 128K | 128K | PRG bank 14 (max 7) |
| Palamedes II - Star Twinkles | 32K | 32K | PRG bank 6 (max 1) |
| Hyokkori Hyoutan Shima | 128K | 128K | PRG bank 14-15 (max 7) |
| Barker Bill's Trick Shooting | 64K | 128K | PRG bank wrapping |
| Family Trainer 03 - Dance Aerobics | 64K | 32K | PRG bank wrapping |

#### Remaining 58 of original 64 (post-`8427a8c2` blanks)

These were **slow starters** — games with long initialization sequences that
needed 100-300+ frames to display graphics. The test initially ran at 60 frames.
Re-testing at 300 frames with `--early-exit` confirmed all 58 produce >2 colors.

**Overall MMC1 resolution: 305 → 0 blanks (100% fixed)**

**Affected ROMs (305):**

<details><summary>Click to expand full list</summary>

| PRG | CHR | ROM |
|----:|----:|-----|
| 32 | 8 | NES Japan ROMs/Exed Exes (Japan).nes |
| 32 | 32 | NES North America ROMs/Dr. Mario (North America).nes |
| 32 | 16 | NES North America ROMs/Tetris (North America).nes |
| 32 | 32 | NES Japan ROMs/Casino Derby (Japan).nes |
| 32 | 32 | NES Japan ROMs/Dungeon Kid (Japan).nes |
| 32 | 32 | NES Japan ROMs/Gimmi a Break - Shijou Saikyou no Quiz Ou Ketteisen (Japan).nes |
| 32 | 32 | NES Japan ROMs/Hyakkiyakou (Japan).nes |
| 32 | 32 | NES Japan ROMs/I Love Softball (Japan).nes |
| 32 | 32 | NES Japan ROMs/Konami Hyper Soccer (Japan).nes |
| 32 | 32 | NES Japan ROMs/Shinsenden (Japan).nes |
| 32 | 16 | NES Japan ROMs/Moero!! Pro Soccer (Japan).nes |
| 32 | 32 | NES Japan ROMs/Moero!! Pro Yakyuu (Japan).nes |
| 32 | 32 | NES Japan ROMs/Shoukoushi Ceddie (Japan).nes |
| 32 | 16 | NES Translated Japan ROMs/Dragon Warrior [Relocalized] (North America).nes |
| 32 | 16 | NES Translated Japan ROMs/Mad City - Adventures of Bayou Billy (Translated) (Japan).nes |
| 64 | 32 | NES Japan ROMs/Best Play Pro Yakyuu (Japan).nes |
| 64 | 32 | NES Japan ROMs/Best Play Pro Yakyuu (Shin Data) (Japan).nes |
| 64 | 0 | NES Japan ROMs/Ginga Eiyuu Densetsu (Japan).nes |
| 64 | 0 | NES Japan ROMs/Great Deal (Japan).nes |
| 64 | 0 | NES Translated Japan ROMs/Dragon Slayer Jr. - Romancia (Translated) (Japan).nes |
| 64 | 0 | NES Japan ROMs/Famicom Top Management (Japan).nes |
| 64 | 128 | NES Japan ROMs/Haja no Fuuin (Japan).nes |
| 64 | 0 | NES Japan ROMs/Hototogisu (Japan).nes |
| 64 | 0 | NES Japan ROMs/Shinobi (Japan).nes |
| 64 | 0 | NES Translated Japan ROMs/Ultima - Quest of the Avatar (Translated) (Japan).nes |
| 128 | 128 | NES Europe ROMs/Aussie Rules Footy (Europe).nes |
| 128 | 128 | NES Europe ROMs/Championship Rally (Europe).nes |
| 128 | 128 | NES Europe ROMs/International Cricket (Europe).nes |
| 128 | 128 | NES Europe ROMs/New Ghostbusters II (Europe).nes |
| 128 | 128 | NES Europe ROMs/Parasol Stars - Rainbow Islands II (Europe).nes |
| 128 | 128 | NES Europe ROMs/Rainbow Islands - Bubble Bobble 2 (Europe).nes |
| 128 | 128 | NES Europe ROMs/Snowboard Challenge (Europe).nes |
| 128 | 128 | NES Europe ROMs/The Legend of Prince Valiant (Europe).nes |
| 128 | 0 | NES Japan ROMs/Airwolf (Japan).nes |
| 128 | 128 | NES Japan ROMs/Bakushou!! Jinsei Gekijou 3 (Japan).nes |
| 128 | 128 | NES Japan ROMs/Bakushou!! Star Monomane Shitennou (Japan).nes |
| 128 | 128 | NES Japan ROMs/Barcode World (Japan).nes |
| 128 | 128 | NES Japan ROMs/Battle Stadium - Senbatsu Pro Yakyuu (Japan).nes |
| 128 | 128 | NES Japan ROMs/Be-Bop-Highschool - Koukousei Gokuraku Densetsu (Japan).nes |
| 128 | 0 | NES Japan ROMs/Best Keiba - Derby Stallion (Japan).nes |
| 128 | 128 | NES Japan ROMs/Blodia Land - Puzzle Quest (Japan).nes |
| 128 | 128 | NES Japan ROMs/Chuugoku Janshi Story - Tonpuu (Japan).nes |
| 128 | 128 | NES Japan ROMs/Cycle Race - Road Man (Japan).nes |
| 128 | 128 | NES Japan ROMs/Donald Land (Japan).nes |
| 128 | 128 | NES Japan ROMs/Famicom Igo Nyuumon (Japan).nes |
| 128 | 128 | NES Japan ROMs/Famicom Meijin Sen (Japan).nes |
| 128 | 128 | NES Japan ROMs/Famicom Shougi - Ryuuousen (Japan).nes |
| 128 | 0 | NES Japan ROMs/Gambler Jiko Chuushin Ha - Mahjong Game (Japan).nes |
| 128 | 128 | NES Japan ROMs/Gozonji - Yaji Kita Chin Douchuu (Japan).nes |
| 128 | 0 | NES Japan ROMs/Hanjuku Eiyuu (Japan).nes |
| 128 | 128 | NES Japan ROMs/Hiryuu no Ken II - Dragon no Tsubasa (Japan).nes |
| 128 | 128 | NES Japan ROMs/Hiryuu no Ken Special - Fighting Wars (Japan).nes |
| 128 | 128 | NES Japan ROMs/Home Run Nighter - Pennant League!! (Japan).nes |
| 128 | 128 | NES Japan ROMs/Honoo no Doukyuuji - Dodge Danpei (Japan).nes |
| 128 | 128 | NES Japan ROMs/Hyokkori Hyoutan Shima - Nazo no Kaizokusen (Japan).nes |
| 128 | 128 | NES Japan ROMs/Kaguya Hime Densetsu (Japan).nes |
| 128 | 128 | NES Japan ROMs/Kero Kero Keroppi no Daibouken (Japan).nes |
| 128 | 128 | NES Japan ROMs/Kero Kero Keroppi no Daibouken 2 (Japan).nes |
| 128 | 128 | NES Japan ROMs/Konami Hyper Soccer (Japan).nes |
| 128 | 128 | NES Japan ROMs/Mashin Eiyuu Den Wataru Gaiden (Japan).nes |
| 128 | 128 | NES Japan ROMs/Mito Koumon - Sekai Manyuuki (Japan).nes |
| 128 | 128 | NES Japan ROMs/Mitsume ga Tooru (Japan).nes |
| 128 | 128 | NES Japan ROMs/Mouryou Senki Madara (Japan).nes |
| 128 | 128 | NES Japan ROMs/Musashi no Bouken (Japan).nes |
| 128 | 128 | NES Japan ROMs/Nakayoshi to Issho (Japan).nes |
| 128 | 128 | NES Japan ROMs/Ninjara Hoi! (Japan).nes |
| 128 | 128 | NES Japan ROMs/Nobunaga no Yabou - Bushou Fuuun Roku (Japan).nes |
| 128 | 64 | NES Japan ROMs/Nobunaga no Yabou - Sengoku Qunshou Den (Japan).nes |
| 128 | 128 | NES Japan ROMs/Pachio-kun 4 (Japan).nes |
| 128 | 128 | NES Japan ROMs/Pro Yakyuu Family Stadium '87 (Japan).nes |
| 128 | 128 | NES Japan ROMs/Pro Yakyuu Family Stadium '88 (Japan).nes |
| 128 | 128 | NES Japan ROMs/Sangokushi (Japan).nes |
| 128 | 128 | NES Japan ROMs/SD Sengoku Bushou Retsuden (Japan).nes |
| 128 | 128 | NES Japan ROMs/Shanghai (Japan).nes |
| 128 | 0 | NES Japan ROMs/Shoushin Mahjong Club (Japan).nes |
| 128 | 128 | NES Japan ROMs/Top Rider (Japan).nes |
| 128 | 128 | NES Japan ROMs/Ushio to Tora - Shinen no Daiyou (Japan).nes |
| 128 | 128 | NES Japan ROMs/World Soccer (Japan).nes |
| 128 | 128 | NES Japan ROMs/Yuu Maze (Japan).nes |
| 128 | 128 | NES Japan ROMs/Yuu Yuu Jinsei (Japan).nes |
| 128 | 128 | NES Japan ROMs/Ys (Japan).nes |
| 128 | 128 | NES Japan ROMs/Ys II - Ancient Ys Vanished - The Final Chapter (Japan).nes |
| 128 | 0 | NES Japan ROMs/Zelvard Legend (Japan).nes |
| 128 | 128 | NES North America ROMs/Advanced Dungeons & Dragons - Heroes of the Lance (North America).nes |
| 128 | 0 | NES North America ROMs/Bomberman II (North America).nes |
| 128 | 128 | NES North America ROMs/Castlevania II - Simon's Quest (North America).nes |
| 128 | 128 | NES North America ROMs/Double Dragon (North America).nes |
| 128 | 128 | NES North America ROMs/G.I. Joe - A Real American Hero (North America).nes |
| 128 | 128 | NES North America ROMs/Goal! (North America).nes |
| 128 | 128 | NES North America ROMs/Indiana Jones and the Last Crusade (North America).nes |
| 128 | 128 | NES North America ROMs/Kiwi Kraze (North America).nes |
| 128 | 128 | NES North America ROMs/Knight Rider (North America).nes |
| 128 | 0 | NES North America ROMs/Metroid (North America).nes |
| 128 | 128 | NES North America ROMs/Ninja Gaiden (North America).nes |
| 128 | 128 | NES North America ROMs/Phil & Ted's Bogus Journey (North America).nes |
| 128 | 128 | NES North America ROMs/Pro Wrestling (North America).nes |
| 128 | 128 | NES North America ROMs/Rad Racer (North America).nes |
| 128 | 128 | NES North America ROMs/RoboCop 3 (North America).nes |
| 128 | 128 | NES North America ROMs/Robocop (North America).nes |
| 128 | 128 | NES North America ROMs/Rocky and Bullwinkle and Friends (North America).nes |
| 128 | 128 | NES North America ROMs/Teenage Mutant Ninja Turtles (North America).nes |
| 128 | 0 | NES North America ROMs/The Legend of Zelda [Relocalized] (North America).nes |
| 128 | 128 | NES North America ROMs/Willow (North America).nes |
| 128 | 0 | NES North America ROMs/WWF Wrestlemania Challenge (North America).nes |
| 128 | 128 | NES North America ROMs/Zelda II - The Adventure of Link (North America).nes |
| 128 | 0 | NES Translated Japan ROMs/A Week of Garfield (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Advanced Dungeons & Dragons - Dragons of Flame (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Bikkuri Nekketsu Shinkiroku! - The Great Distant Field Day (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Downtown Special - Historical District Smackdown March (Translated) (Japan).nes |
| 128 | 0 | NES Translated Japan ROMs/Dragon Slayer Jr. - Romancia (Translated) (Japan).nes |
| 128 | 0 | NES Translated Japan ROMs/Famicom Detective Club Part II - Ushiro ni Tatsu Shoujo (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Famicom Doubutsu Seitai Zukan! - Katte ni Shirokuma - Mori o Sukue no Maki! (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Famicom Doubutsu Seitai Zukan! - Katte ni Shirokuma - Mori wo Sukue no Maki! (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Famista (Translated) (Japan).nes |
| 128 | 0 | NES Translated Japan ROMs/Half-Boiled Hero (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Hiryuu no Ken III - 5 Nin no Dragon (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Kaguya Hime Densetsu (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Masked Ninja - Hanamaru (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Mitsume ga Tooru (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Mouryou Senki Madara (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Musashi no Bouken (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Ninjara Hoi! (Translated) (Japan).nes |
| 128 | 0 | NES Translated Japan ROMs/Tetris 2 + BomBliss (J) [!] (North America).nes |
| 128 | 128 | NES Translated Japan ROMs/Three-Eyed One Walks (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Ushio & Tora - The Abyss (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Ys (Translated) (Japan).nes |
| 128 | 128 | NES Translated Japan ROMs/Ys II - Ancient Ys Vanished - The Final Chapter (Translated) (Japan).nes |

*Plus ~180 more 128KB ROMs across Japan/NA/Translated folders (complete list in `build/nes_gfx_test_results.tsv`)*

</details>

---

### 2. Taito X1-005/X1-017 Register Shadowing

**Impact:** 12 ROMs → **7 remaining** (mapper 82 now passes at 300 frames; Kyonshiizu 2 now passes)
**Fix:** Easy
**Files:** `mapper_080_taito_x1005.hpp`, `mapper_082_taito_x1017.hpp`

**Root cause:** Registers at $7EF0–$7EFF are in the $6000–$7FFF PRG-RAM region.
When bank map is built, pages 6–7 get writable PRG-RAM pointers, so CPU writes
at $7EFx go to RAM (block dispatch) and never reach `Mapper::register_write()`.
Banks never change from their initial zero state.

**Fix:** Set `prg_ram_write_protected = true` in `get_prg_bank_config()`, then handle
RAM reads/writes for $7F00–$7FFF alongside register writes in `register_write()`.

**Remaining blank ROMs (7):**

| Mapper | ROM |
|-------:|-----|
| 80 | NES Japan ROMs/Kyuukyoku Harikiri Stadium (Japan).nes |
| 80 | NES Japan ROMs/Kyuukyoku Harikiri Stadium - '88 Senshu Shin Data Version (Japan).nes |
| 80 | NES Japan ROMs/Yamamura Misa Suspense - Kyouto Ryuu no Tera Satsujin Jiken (Japan).nes |
| 80 | NES Translated Japan ROMs/Distant Legend of Jarvas (Translated) (Japan).nes |
| 80 | NES Translated Japan ROMs/Minelvaton Saga (Translated) (Japan).nes |
| 80 | NES Translated Japan ROMs/Taito Grand Prix (Translated) (Japan).nes |
| 207 | NES Translated Japan ROMs/The Acala Legend (Translated) (Japan).nes |

---

### 3. Bandai FCG (Mapper 16)

**Impact:** 19 ROMs → **1 remaining** (8 fixed by I2C in `bc12abe1`, 10 were slow starters)
**Status:** Near-done

**Three bugs found and fixed:**
1. **PRG-RAM write interception:** Writes to $6000-$7FFF absorbed by PRG-RAM
   instead of reaching `register_write()`. Fixed: `prg_ram_write_protected = true`.
2. **Address decode too narrow:** Real FCG hardware only decodes A3-A0, so registers
   mirror across $6000-$FFFF. Games wrote at $7Fxx but mapper only accepted $6xxx/$8xxx.
3. **I2C EEPROM timing:** ACK sent on clock 8 (same cycle as last data bit) instead
   of clock 9. Standard I2C: 8 data + 1 ACK clock per byte. Games hung waiting for ACK.

**Remaining 1:** Famicom Jump II (actually mapper 153, misidentified in iNES header).
4 Datach ROMs, all Dragon Ball Z ROMs, and others now pass at 300 frames.

**Affected ROMs:**

| ROM | PRG | CHR |
|-----|----:|----:|
| NES Japan ROMs/Akuma-kun - Makai no Wana (Japan).nes | 128 | 128 |
| NES Japan ROMs/Datach - Battle Rush - Build Up Robot Tournament (Japan).nes | 256 | 0 |
| NES Japan ROMs/Datach - Dragon Ball Z - Gekitou Tenkaichi Budou Kai (Japan).nes | 256 | 0 |
| NES Japan ROMs/Datach - SD Gundam - Gundam Wars (Japan).nes | 256 | 0 |
| NES Japan ROMs/Datach - Ultraman Club - Supokon Fight! (Japan).nes | 256 | 0 |
| NES Japan ROMs/Famicom Jump - Eiyuu Retsuden (Japan).nes | 256 | 128 |
| NES Japan ROMs/Famicom Jump II - Saikyou no 7 Nin (Japan).nes | 512 | 0 |
| NES Japan ROMs/Magical Taruruuto-kun - Fantastic World!! (Japan).nes | 128 | 128 |
| NES Japan ROMs/Magical Taruruuto-kun 2 - Mahou Daibouken (Japan).nes | 128 | 128 |
| NES Japan ROMs/Meimon! Dai San Yakyuu Bu (Japan).nes | 128 | 128 |
| NES Japan ROMs/Nishimura Kyoutarou Mystery - Blue Train Satsujin Jiken (Japan).nes | 128 | 256 |
| NES Japan ROMs/Rokudenashi Blues (Japan).nes | 256 | 256 |
| NES Japan ROMs/SD Gundam Gaiden - Knight Gundam Monogatari (Japan).nes | 256 | 128 |
| NES Japan ROMs/SD Gundam Gaiden - Knight Gundam Monogatari 2 - Hikari no Kishi (Japan).nes | 256 | 256 |
| NES Japan ROMs/SD Gundam Gaiden - Knight Gundam Monogatari 3 - Densetsu no Kishi Dan (Japan).nes | 256 | 256 |
| NES Translated Japan ROMs/Charge!! Men's Private School - Number One Student (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Dragon Ball Z Gaiden - Plan to Eliminate the Saiyans (Translated) (Japan).nes | 256 | 256 |
| NES Translated Japan ROMs/Dragon Ball Z II - Tyrant Freeza!! (Translated) (Japan).nes | 256 | 256 |
| NES Translated Japan ROMs/Dragon Ball Z III - Killer Androids (Translated) (Japan).nes | 256 | 256 |

---

### 4. Namco 175/340 (Mapper 210)

**Impact:** 7 ROMs → 0 remaining (all fixed in `55beae28`)
**Status:** **DONE**

**Root cause:** Register map was completely wrong. Rewrote with proper sub-mapper
differentiation (Namco 175 vs 340), correct address decoding, and hardwired
mirroring for Namco 175 boards.

**Affected ROMs:**

| ROM | PRG | CHR |
|-----|----:|----:|
| NES Translated Japan ROMs/Famista '91 (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Famista '92 (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Famista '93 (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Famista '94 (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Namco Prism Zone - Dream Master (Translated) (Japan).nes | 512 | 256 |
| NES Translated Japan ROMs/Splatter House - Super Deformed (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/The Genius Bakabon (Translated) (Japan).nes | 256 | 128 |

---

### 5. AxROM (Mapper 7)

**Impact:** 16 ROMs (48.5% blank rate)
**Fix:** Medium

Some 128KB AxROM games pass, others don't — not a uniform size issue.
Still blank at 180 frames (confirmed: Marble Madness, R.C. Pro-Am, Wizards & Warriors).
Possible causes:
- Bus conflict AND with wrong ROM bank at startup (bank 0 vs expected bank)
- Initial bank selection (real hardware is indeterminate; some games assume last bank)
- Some games may not tolerate bus conflicts (the AND may corrupt the write value)

**Affected ROMs:**

| ROM | PRG |
|-----|----:|
| NES North America ROMs/Cobra Triangle (North America).nes | 128 |
| NES North America ROMs/Jeopardy! (North America).nes | 128 |
| NES North America ROMs/Jeopardy! 25th Anniversary Edition (North America).nes | 128 |
| NES North America ROMs/Jeopardy! Junior Edition (North America).nes | 128 |
| NES North America ROMs/Marble Madness (North America).nes | 128 |
| NES North America ROMs/R.C. Pro-Am (North America).nes | 64 |
| NES North America ROMs/Solstice - The Quest for the Staff of Demnos (North America).nes | 128 |
| NES North America ROMs/WWF Wrestlemania (North America).nes | 128 |
| NES North America ROMs/Wheel of Fortune (North America).nes | 128 |
| NES North America ROMs/Wheel of Fortune - Family Edition (North America).nes | 128 |
| NES North America ROMs/Wheel of Fortune - Junior Edition (North America).nes | 128 |
| NES North America ROMs/Wheel of Fortune Starring Vanna White (North America).nes | 128 |
| NES North America ROMs/Who Framed Roger Rabbit (North America).nes | 128 |
| NES North America ROMs/Wizards & Warriors (North America).nes | 128 |
| NES North America ROMs/Wizards & Warriors II - IronSword (North America).nes | 256 |
| NES North America ROMs/World Games (North America).nes | 128 |

---

### 6. MMC5 (Mapper 5)

**Impact:** 11 ROMs → **10 remaining** (1 fixed by bank wrapping in `5c72e7a4`)
**Fix:** Hard — known incomplete (missing expansion audio, vertical split mode)

4 ROMs pass (Metal Slader Glory fixed by PRG bank wrapping). 10 blank — likely
exercise advanced features (extended attributes, fill mode, specific CHR banking modes).

**Remaining blank ROMs:**

| ROM | PRG | CHR |
|-----|----:|----:|
| NES Japan ROMs/Aoki Ookami to Shiroki Mejika - Genchou Hishi (Japan).nes | 512 | 256 |
| NES Japan ROMs/Ishin no Arashi (Japan).nes | 256 | 128 |
| NES Japan ROMs/Nobunaga's Ambition III (Japan).nes | 512 | 256 |
| NES North America ROMs/Bandit Kings of Ancient China (North America).nes | 256 | 128 |
| NES North America ROMs/Gemfire (North America).nes | 256 | 256 |
| NES North America ROMs/L'Empereur (North America).nes | 256 | 128 |
| NES North America ROMs/Nobunaga's Ambition II (North America).nes | 256 | 128 |
| NES North America ROMs/Romance of the Three Kingdoms II (North America).nes | 256 | 256 |
| NES North America ROMs/Uncharted Waters (North America).nes | 512 | 128 |
| NES Translated Japan ROMs/Just Breed (Translated) (Japan).nes | 512 | 256 |

---

### 7. Namco 163 (Mapper 19)

**Impact:** 10 ROMs → **7 remaining** (3 were slow starters, now pass at 300 frames)
**Fix:** Medium — expansion audio is stubbed, may affect timing

19 ROMs pass, 7 blank. Could be timing-related or related to the stubbed
expansion audio registers interfering with game logic.

**Remaining blank ROMs:**

| ROM | PRG | CHR |
|-----|----:|----:|
| NES Japan ROMs/Chibi Maruko-Chan - Uki Uki Shopping (Japan).nes | 128 | 128 |
| NES Japan ROMs/Sangokushi II - Haou no Tairiku (Japan).nes | 256 | 256 |
| NES Japan ROMs/Top Striker (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Digital Devil Story - Megami Tensei II (Translated) (Japan).nes | 512 | 256 |
| NES Translated Japan ROMs/Dragon Ninja (Translated) (Japan).nes | 128 | 128 |
| NES Translated Japan ROMs/Jubei Quest (Translated) (Japan).nes | 512 | 256 |
| NES Translated Japan ROMs/Phantom Travel Journal (Translated) (Japan).nes | 128 | 128 |

---

### 8. Systemic Bank Wrapping

**Impact:** ~100 ROMs resolved across multiple mappers
**Status:** **DONE** (commits `2063cf3e`, `5c72e7a4`)

The same bug as MMC1 issue B existed across many mappers: out-of-range bank
numbers produced `nullptr` page pointers instead of address-mirrored pointers.
On real hardware, ROM chips ignore upper address lines beyond their capacity,
naturally wrapping accesses.

**Fix:** Replace `(offset < size) ? ptr + offset : nullptr` with
`ptr + (offset & (size - 1))` in `get_prg_bank_config()` and `get_chr_bank_config()`.

**Mappers fixed:** 1 (MMC1), 4 (MMC3), 5 (MMC5), 13 (CPROM), 28 (Action 53),
64 (Rambo-1 CHR), 118 (TxSROM), 119 (TQROM), 232 (Camerica BF9096).

Of the 100 ROMs that now pass, most were actually slow starters that needed
300 frames instead of 60 — the bank wrapping fix was necessary but the re-test
at 300 frames with `--early-exit` was equally important.

---

### 9. Minor Mapper Issues

Small-count mappers still blank after systemic fix:

| Mapper | Name | Blank | Total | ROMs |
|-------:|------|------:|------:|------|
| 69 | Sunsoft FME-7 | 2 | 9 | Honoo no Doukyuuji (Dodge Danpei) × 2 |
| 96 | Oeka Kids | 2 | 2 | Oeka Kids: Anpanman × 2 (need special input) |
| 120 | FDS Hack | 1 | 1 | Tobidase Daisakusen |
| 140 | Jaleco JF-11 | 1 | 3 | Bio-Warrior Dan |
| 40 | FDS SMB2J Hack | 1 | 1 | Super Mario Bros 2 (J) FDS hack |
| 75 | VRC1 | 1 | 6 | Ninja Jajamaru |
| 207 | Taito X1-017 | 1 | 1 | Fudou Myouou Den |
| 33 | Taito TC0190 | 1 | 9 | Bakushou Jinsei Gekijou 3 |
| 16 | Bandai FCG | 1 | 19 | Famicom Jump II (actually mapper 153) |

**Previously blank, now passing:** mappers 9, 10, 15, 26, 47, 65, 76, 82, 85, 88, 206
(all resolved by bank wrapping fix + 300-frame re-test).

---

### 10. Slow Starters

**Status:** **RESOLVED** — all slow starters now pass at 300 frames with `--early-exit`.

The original test ran at 60 frames. Many games (especially Japanese RPGs, strategy
games, and complex titles) need 100–300+ frames for initialization before displaying
graphics. Re-testing at 300 frames with `--early-exit` resolved all of these.

**Original slow-starter mappers (all now 0 blanks from this cause):**

| Mapper | Name | Were Blank | Now |
|-------:|------|----------:|----:|
| 0 | NROM | 4 | 0 |
| 2 | UxROM | 19 | 0 |
| 3 | CNROM | 7 | 0 |
| 4 | MMC3 | ~45 | 0 |
| 1 | MMC1 | 58 | 0 |

---

## Test Infrastructure

- **Boot warp** (`41945105`): NES GUI runs at max speed until PPU enables rendering
  (PPUMASK bits 3-4). Eliminates 2-5s blank-screen wait during game initialization.
- **Early-exit testing**: `cermu_console --early-exit` checks GFX every 30 frames,
  stops on first video output (>2 unique colors). Reduces PASS test time from 300→30-90 frames.
- **Targeted re-testing**: `nes_rom_test.py --changed-only` re-tests only non-PASS ROMs,
  `--blanks-only` for blanks only. Avoids full-collection runs after targeted fixes.

---

## Fix Priority (Updated)

1. ~~MMC1 PRG bank fix~~ — **DONE** (241 ROMs)
2. ~~AxROM bus conflict~~ — **DONE** (16 ROMs)
3. ~~Namco 175/340 rewrite~~ — **DONE** (7 ROMs)
4. ~~Bandai FCG I2C/register fixes~~ — **DONE** (8 ROMs)
5. ~~NES-QJ PRG-RAM interception~~ — **DONE** (1 ROM)
6. ~~MMC1 bank wrapping + slow-starter re-test~~ — **DONE** (64 ROMs)
7. ~~Systemic bank wrapping + 300-frame re-test~~ — **DONE** (100 ROMs across 8 mappers)
8. **MMC5 completion** — 10 ROMs, significant effort
9. **Namco 163** — 7 ROMs, expansion audio or timing
10. **Taito X1-005 deeper issue** — 6 ROMs
11. **MMC3 remaining** — 3 ROMs (Nightshade, Ring King, SMB+Tetris+NWC multicart)
12. **Minor mappers** — 8 ROMs across 9 mappers, triage individually
