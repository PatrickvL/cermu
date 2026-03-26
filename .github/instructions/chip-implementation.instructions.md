---
applyTo: 'src/chip/**'
---
# Chip Implementation Standards

Every discrete IC modeled in cermu **must** follow these rules.
No exceptions — even trivial glue-logic chips (latches, muxes, buffers) are
first-class `ChipBase` citizens.

---

## Mandatory Checklist

| # | Requirement | Details |
|---|-------------|---------|
| 1 | **Derive from role-appropriate base** | `IoChipBase`, `InputChipBase`, `LogicChipBase`, `SoundChipBase`, `VideoChipBase`, `MemoryChipBase`, `CpuChipBase`. Create a new intermediate base if none fits. |
| 2 | **Provide `ChipInfo`** | Pass `ChipInfo{part_number, manufacturer, description}` to the base constructor. |
| 3 | **DECL register / MMIO map** | Define a `<PREFIX>_DECL(REG, FLD, CMP)` X-macro with `REG(...)` rows for every addressable register or enumerable output, and `FLD(...)` rows for sub-fields. Use `DECL_EXTRACT(PREFIX, DECL)` to generate `_REG_INFO[]` and `_DECL_ENTRIES`. |
| 4 | **Register debug fields** | Implement `register_debug_fields()` under `#ifdef CERMU_HAS_CHIP_DEBUG`, called in the constructor. Use the `debug_registry_` builder API (`.category()`, `.value()`, `.flag()`, `.bitfield()`, etc.). |
| 5 | **`ChipLayout`** | Override `create_chip_layout()` and `get_layout_pin_states()` in a `_gui.cpp` file under `#ifdef CERMU_HAS_GUI`. Use the appropriate DIP/SOIC/QFP factory function and `PIN_LR` / `PIN_QUAD` macros. |
| 6 | **Chip registry** | Add a `REGISTER_CHIP_TYPE("Name", Type)` entry in the category's `*_registry.cpp` file (create one if it doesn't exist). |
| 7 | **`reset()` override** | Every chip must implement `reset()` setting all outputs to their power-on state. |

---

## File Placement

- Header: `src/chip/<category>/<name>.hpp` — header-only when possible (no side effects).
- GUI: `src/chip/<category>/<name>_gui.cpp` — compiled only with `CERMU_HAS_GUI`.
- Registry: `src/chip/<category>/<category>_registry.cpp` — one per category.
- Cross-system rule: if a chip is used by more than one system, it lives in `src/chip/`, never under `src/systems/`.

---

## Naming & Style

- Class name: part number in PascalCase or datasheet convention (e.g., `LS259`, `CD4021`, `mos6529_t`).
- DECL prefix: uppercase abbreviated chip name (e.g., `LS259`, `CD4021`).
- Follow project-wide naming (see `coding-guidelines.instructions.md`).

---

## Template

```cpp
#pragma once
// <chip_name>.hpp — <Part Number> <Short Description>

#include "chip/<category>/<category>_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ── DECL register map ───────────────────────────────────────────────
#define CHIP_DECL(REG, FLD, CMP) \
    REG(0x00, REG_NAME, "Description") \
    FLD(REG_NAME, FIELD, 7_0, "Field description", Value, 0, 0)

DECL_EXTRACT(CHIP, CHIP_DECL)

class ChipType : public CategoryChipBase {
public:
    ChipType()
        : CategoryChipBase(ChipInfo{"Part", "Manufacturer", "Description"}) {
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void reset() override { /* power-on state */ }

    // ... chip-specific API ...

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    // ... state ...

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("Part — Section")
            .value("Name", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const ChipType*>(c)->member_;
            });
    }
#endif
};
```
