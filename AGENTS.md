# AGENTS.md

## Agent Behavioral Rules

### Meta-Rules (HIGHEST PRIORITY)
1. **Verify first** — Before any action, check compliance with all rules below. Prioritize rule adherence over task convenience.
2. **Extend on command** — When given explicit behavioral instructions, add them to this document immediately and maintain rule hierarchy.

### Project Constraints
- **No small test/debug programs.** Always use consolidated/unified test runners. Treat all tasks holistically — consider interconnections.
- **Consolidation mandate.** When multiple files serve similar purposes, consolidate immediately. Delete originals only after verified consolidation. Preserve all functionality. Maintain working state throughout.
- **Generalize for reuse.** Any component, utility, or abstraction that has use (or clear potential use) beyond a single call-site or context must be built generically. This applies to all code — not just emulated systems, but utilities, data structures, algorithms, UI helpers, etc. Spend the effort to parameterize and decouple it from context-specific details. Place shared artifacts per the Cross-System Sharing Rule in the coding guidelines (`src/chip/`, `src/ports/`, `src/devices/`, `src/utils/`).
- **Preserve comments.** Do not remove or rewrite comments during refactoring unless they are factually wrong or refer to deleted code. Comments represent the author's intent and context — keep them intact.

### Version Control
- **Scoped commits only.** When committing, include only the files that were changed for the current task. Never bundle unrelated changes into a single commit.
- **Hands off unrelated changes.** Do not stage or commit files modified by other sessions or the user. Those are not your responsibility.

---
*Last Updated: 2026-03-04*