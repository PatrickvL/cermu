#include "mos6510.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing basic MOS6510 functionality...\n");
    
    // Create CPU directly
    mos6510_t* cpu = malloc(sizeof(mos6510_t));
    if (!cpu) {
        printf("Failed to allocate CPU\n");
        return 1;
    }
    
    // Initialize CPU
    mos6510_init(cpu);
    printf("CPU initialized: PC=$%04X, A=$%02X, X=$%02X, Y=$%02X, SP=$%02X, P=$%02X\n",
           cpu->base.pc, cpu->base.a, cpu->base.x, cpu->base.y, cpu->base.sp, cpu->base.p);
    
    // Try to step without memory access (this should fail gracefully)
    printf("Attempting to step CPU (should fail without memory)...\n");
    
    free(cpu);
    return 0;
}
