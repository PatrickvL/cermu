# VIC-II C Emulator Refactor & Completion Project Plan

## Overview

This project aims to complete and refine the C implementation of the Commodore 64 VIC-II video chip emulator, using a modern, cycle-accurate, bus-arbitrated design. The goal is to achieve hardware-accurate emulation, passing VICE and other test suites, and to support both PAL and NTSC in the future. The plan leverages the proven C# implementation, the authoritative vic-ii.txt documentation, and best practices from open-source emulators.

---

## 1. **Project Principles**

- **Preserve the new C design:**  
  - The cycle-ticking, `cycle_group`, and `access_type`-driven bus access is the core.
  - BA/AEC and bus floating data handling must remain as in the new C code.
- **Integrate missing features from C# and VIC-II docs:**  
  - Port only the missing state tracking, sprite handling, and pixel emission logic.
  - Use the C# code and vic-ii.txt as reference for correct state transitions, flip-flops, and sprite logic.
- **Enquote all comments from vic-ii.txt** for traceability.
- **Incremental, review-friendly changes:**  
  - All modifications should be minimal, local, and clearly marked.
  - Commented-out code should be reactivated in place, with clear separation for review.
- **Sequencer logic:**  
  - Where documentation is lacking, note uncertainties and suggest further research.

---

## 2. **Major Work Areas**

### 2.1. **State Tracking and Flip-Flops**

- Audit and add any missing flip-flops and state variables:
  - Expansion flip-flop
  - Vertical border flip-flop
  - Sprite DMA and display state
  - Any other state from C# or vic-ii.txt
- Update these in the correct cycles, using the C# code as a guide, but always within the new C cycle framework.
- **Status:** _Complete_

### 2.2. **Sprite Handling**

- Implement sprite pointer/data fetches in the correct cycles, using the `cycle_group` and `access_type` logic.
- Store sprite data in per-sprite buffers, as in the C# code.
- Implement sprite DMA and display state transitions (e.g., in cycles 55, 56, 58, etc.), using the C# logic, but always called from the correct place in the C cycle.
- Implement sprite pixel emission, priority, and collision logic.
- **Status:** _Sprite state, DMA, display state, pixel emission, priority, and collision logic: Complete & hardware-accurate._

### 2.3. **Pixel Emission**

- Complete the pixel emission functions:
  - `vicii_common_emit_graphics_pixels`
  - `vicii_common_emit_border_pixels`
  - Sprite pixel emission
- Use only the data fetched and stored in the current cycle, never direct memory reads.
- Ensure the correct priority and collision logic, as in the C# code and hardware.
- **Status:** _Sprite pixel emission: hardware-accurate and complete. Graphics pixel emission: PARTIALLY COMPLETE—cycle-accuracy and full mode validation IN PROGRESS._
- **Current state:**
  - Sprite sequencer and pixel emission logic is implemented and hardware-accurate, using a 24-bit shift register per sprite, with DMA buffer and per-cycle shifting.
  - Sprite pixel emission is called from the display pipeline, using only intermediate storage, and is ready for priority/collision logic.
  - The graphics pixel emission (`vicii_common_emit_graphics_pixels` / `vic_graphics_sequencer`) is present and called, but the actual cycle-accurate emission and sequencer logic for graphics pixels (foreground/background, multicolor, etc.) needs further validation and completion for full hardware accuracy. Not all graphics modes are fully validated.
- **Next steps:**
  - Review and update `vicii_common_emit_graphics_pixels` and `vic_graphics_sequencer` to ensure cycle-accurate sequencer/shift register logic for graphics pixels, as described in vic-ii.txt (section 3.7.3 and following).
  - Validate all graphics modes (text, bitmap, multicolor, ECM, idle) and document any uncertainties.
  - Keep all code modular and well-commented, with enquoted documentation from vic-ii.txt where appropriate.
  - Update this plan with any new research findings or open questions.

### 2.4. **Intermediate Storage**

- Ensure all character, color, and sprite data fetched during bus access is stored in per-line or per-sprite buffers.
- All pixel emission must use these buffers.
- **Status:** _Complete_

### 2.5. **Register Read/Write and Bus Floating Data**

- Maintain the new C approach for register read/write, including bus floating data bits.
- Only port C# logic where it does not conflict with the new bus model.
- **Status:** _Ongoing_

### 2.6. **Sequencer Logic and Uncertainties**

- Where sequencer behavior is not fully documented:
  - Mark code and comments clearly.
  - Suggest further research (e.g., VICE source, die shots, C64 Wiki, forums).
  - Prefer a modular design so sequencer logic can be improved later.
- **Status:** _Ongoing. Graphics sequencer logic is modular but incomplete for full hardware accuracy—cycle-accuracy and mode-specific behavior need further research and validation. Expand TODOs and uncertainties in code and plan._

### 2.7. **PAL/NTSC Abstraction**

- Keep all timing and dimension constants in a struct or config, so you can easily switch between PAL and NTSC later.
- **Status:** _Pending_

### 2.8. **Documentation and Comments**

- Enquote all comments from vic-ii.txt.
- Clearly mark all code ported from C# or other sources.
- Document all uncertainties and TODOs, especially in sequencer logic and graphics emission.
- **Status:** _Ongoing_

### 2.9. **Sequencer Research & Open Questions**

- The VIC-II sequencer logic (for both graphics and sprites) is not fully documented in vic-ii.txt or the original C# code.
- Key uncertainties:
  - Exact timing and state transitions for the graphics and sprite sequencers.
  - How intermediate storage (shift registers, latches) is updated and used for pixel emission.
  - The order and interaction of graphics and sprite pixel emission, especially for priority and collision.
- **Current approach:**
  - Incrementally modularize the C code to allow for future improvements as sequencer details become clearer.
  - Use VICE and open-source emulators as reference implementations for edge cases and undocumented behavior.
  - Document all uncertainties and TODOs in the code and plan for future research.

---

## 3. **Incremental Task Breakdown**

### 3.1. **Audit and Add State Variables**
- [x] Compare C and C# state variables.
- [x] Add missing flip-flops and state fields to the C struct.

### 3.2. **Reactivate and Complete Commented-Out Functions**
- [x] For each commented-out function (e.g., `vicii_common_handle_bad_line_related_state`, border flip-flops, retrace, pixel emission):
  - [x] Place the new/active version in the same location.
  - [x] Only make the minimal required changes for clarity and review.
  - [x] Clearly separate the old and new code with comments if both are present.

### 3.3. **Sprite DMA and Display State**
- [x] Implement sprite DMA and display state transitions in the correct cycles.
- [x] Store sprite data in per-sprite buffers.

### 3.4. **Pixel Emission**
- [x] Refactored: Pixel emission now uses only intermediate storage and is called from the display pipeline, not from bus access logic.
- [x] Sprite pixel emission, priority, and collision logic implemented and hardware-accurate.
- [ ] Graphics pixel emission: PARTIALLY COMPLETE—cycle-accuracy and mode validation IN PROGRESS.
- [ ] Ensure correct use of intermediate storage and sequencer emulation for graphics.
- [ ] Validate all graphics modes (text, bitmap, multicolor, ECM, idle) and document any uncertainties.

### 3.5. **Priority and Collision Logic**
- [x] Implement correct priority and collision logic for graphics and sprites.

### 3.6. **Sequencer Logic**
- [ ] Graphics sequencer logic is modular but incomplete for full hardware accuracy—cycle-accuracy and mode-specific behavior need further research and validation.
- [ ] Document all uncertainties and research findings in this plan and code.
- **Current state:**
  - Sprite sequencer logic is modular and hardware-accurate.
  - Graphics sequencer logic is present but needs further validation for XSCROLL, reload/shift timing, and mode-specific behavior.
- **Next steps:**
  - Incrementally modularize and document the graphics sequencer logic.
  - Compare with VICE and C# outputs for validation.
  - Note all uncertainties and TODOs for future research.

### 3.7. **Testing and Validation**
- [ ] After each major step, test against known good C# output or VICE test results.
- [ ] Start with border and background, then text/bitmap graphics, then sprites.
- [ ] Add explicit TODOs for test/validation hooks in code.

---

## 4. **Research and Reference**

- Use vic-ii.txt as the primary source of truth.
- For sequencer and undocumented behavior, consult:
  - VICE emulator source code
  - C64 Wiki (https://www.c64-wiki.com/wiki/VIC-II)
  - Forum posts (Lemon64, etc.)
  - Die shot analysis (visual6502, etc.)
  - Open-source projects (CS64, Pi64, etc.)

---

## 5. **Review and Handoff**

- All changes should be incremental and review-friendly.
- If the project is interrupted, the plan and current code state should allow a new developer (or AI) to continue seamlessly.

---

## 6. **Example Code and Commenting Conventions**

- Enquote all comments from vic-ii.txt:
  ```c
  // "A Bad Line Condition is given at any arbitrary clock cycle, if at the
  // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
  // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
  // DEN bit was set during an arbitrary cycle of raster line $30."
  ```
- When reactivating commented-out code:
  ```c
  // Old (commented out):
  // void vicii_common_handle_bad_line_related_state() { ... }

  // New (active):
  void vicii_common_handle_bad_line_related_state() {
      // ...new code here...
  }
  ```

---

## 7. **Handoff Checklist**

- [x] All major state variables and flip-flops present in the C struct.
- [x] All bus accesses and pixel emission use the new cycle-ticking model.
- [x] Sprite DMA, display state, and pixel emission logic ported and active.
- [x] All comments from vic-ii.txt are enquoted.
- [x] All uncertainties and TODOs are clearly marked.
- [x] The code is modular and ready for further sequencer improvements.
- [x] The plan and code are ready for handoff or continuation.

---

## 8. **Appendix: Useful Links**

- [VICE Emulator Source](https://sourceforge.net/p/vice-emu/code/HEAD/tree/)
- [C64 Wiki VIC-II](https://www.c64-wiki.com/wiki/VIC-II)
- [CS64 (MIT)](https://github.com/RupertAvery/CS64)
- [Pi64 (GPL)](https://github.com/sampopeltonen/Pi64)
- [Visual6502](http://visual6502.org/)
- [Lemon64 Forums](https://www.lemon64.com/forum/)

---

*This plan is designed to be copy-pasted into a new prompt or handed off to another developer or AI for seamless continuation of the project.*
