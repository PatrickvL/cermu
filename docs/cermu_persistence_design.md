# Persistence Model — Updated Design (2026-03-28)

## 1. Application Settings

- Layered config: system-wide (e.g., `/etc/cermu/settings.toml`), user override (e.g., `~/.config/cermu/settings.toml`).
- TOML format, human-editable, merged at load.
- Precedence: session > user > system.

## 2. Catalog Store

- Per-user SQLite DB: `~/.local/share/cermu/catalog.db` (Linux), `%APPDATA%\Cermu\catalog.db` (Windows).
- No system-wide catalog.

## 3. Emulated System State Persistence

- Per-system, per-instance state as a single file:  
  `~/.local/share/cermu/state/{system}_{instance}.state.toml` (Linux)  
  `%APPDATA%\Cermu\state\{system}_{instance}.state.toml` (Windows)  
  (i.e., always under the user data folder, not config)
- File is TOML (or similar), human/machine-readable, with embedded binary blobs (base64/hex) for large/binary fields.
- Top-level keys: `version`, `system`, `instance`, `timestamp`, `config`, `peripherals`, `roms`, `eeproms`, `dipswitches`, `snapshot`.
- Example:

```toml
version = 1
system = "c64"
instance = "default"
timestamp = "2026-03-28T14:22:00Z"

[config]
region = "PAL"
memory = "64K"
peripherals = ["joystick_port2", "datasette"]

[roms]
kernal = { filename = "kernal.rom", data = "<base64-encoded>" }
basic  = { filename = "basic.rom",  data = "<base64-encoded>" }

[eeproms]
rtc = "<base64-encoded>"

[dipswitches]
sw1 = true
sw2 = false

[snapshot]
data = "<base64-encoded>"
fields = ["cpu", "vic", "cia1", "cia2", ...]
```

- All metadata/config is human-editable; binary blobs are visible/replaceable.
- Versioning: `version` field at top; loader must check and migrate/validate.
- Atomic save (write temp, rename); consider `.bak` backup.

## 4. Extensibility & UI

- New fields can be added without breaking old files; unknown fields ignored.
- UI/CLI: list, create, rename, delete, switch instances; show metadata.
- Document file locations, naming, migration, and manual edit caveats in README.

## 5. Further Considerations

- All per-user; no cloud sync for now.
- Migration: document copy/restore in README.
- State file may be large (esp. with full snapshot); consider optional compression.
- Save/load must be robust to partial/corrupt writes.
- All fields not required for every system; loader must tolerate missing/extra fields.