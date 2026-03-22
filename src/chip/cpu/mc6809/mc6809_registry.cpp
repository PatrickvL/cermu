#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/cpu/mc6809/motorola_mc6809.hpp"
#include "chip/cpu/mc6809/motorola_mc6809e.hpp"
#include "chip/cpu/mc6809/hitachi_hd6309.hpp"

REGISTER_CHIP_TYPE("MC6809",  MotorolaMC6809)
REGISTER_CHIP_TYPE("MC6809E", MotorolaMC6809E)
REGISTER_CHIP_TYPE("HD6309",  HitachiHD6309)
