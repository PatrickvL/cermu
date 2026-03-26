#pragma once

#include "chip/logic/pla.hpp"

// Forward declare C64System
class C64System;

// ============================================================================
// C64 PLA system-specific rendering callbacks
// ============================================================================
// The PLA chip (PLA906114) is system-independent.  These callbacks provide
// C64-specific debug/settings GUI (interactive banking tables, mode
// explorer) and are wired to PLA906114::set_system_context() during
// C64System::initialize().

#ifdef CERMU_HAS_GUI
void c64_pla_render_debug(void* ctx, PLA906114& pla);
void c64_pla_render_settings(void* ctx, PLA906114& pla);
#endif
