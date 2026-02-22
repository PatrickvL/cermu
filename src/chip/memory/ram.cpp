#include "ram.h"
#include <stdlib.h>

ram_s::~ram_s() {
    if (owns_memory && memory) {
        free(memory);
    }
}
