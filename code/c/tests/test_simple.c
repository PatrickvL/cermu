#include "mos6510_test_harness.h"

int main() {
    test_harness_t* harness = test_harness_create();
    if (harness) {
        test_harness_destroy(harness);
    }
    return 0;
}
