#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    printf("AIEMU-C: C Implementation of 6510 CPU Emulator\n");
    printf("Version: 0.1.0\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    
    if (argc > 1) {
        printf("Arguments provided:\n");
        for (int i = 1; i < argc; i++) {
            printf("  [%d]: %s\n", i, argv[i]);
        }
    }
    
    printf("\nTODO: Implement MOS 6510 CPU emulation\n");
    
    return EXIT_SUCCESS;
}
