#include "systems/commodore/c64/c64_config.hpp"
#include <cstring>

void c64_config_t::init_defaults() {
    // System configuration defaults
    vicii_standard = VIC_PAL;     // Default to PAL timing
    
    // Test mode defaults - normal boot (like real C64)
    test_mode = C64_TEST_MODE_NORMAL;
    test_binary_config = NULL;
    
    // Initialize with no cartridge ROMs by default (saves memory)
    roml_present = false;
    romh_present = false;
}
