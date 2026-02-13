/**
 * PETSCII Character Encoding Utilities — Implementation
 */

#include "petscii.h"

void petscii_trim_padding(char* str, int maxlen) {
    int end = maxlen - 1;
    while (end >= 0 && ((uint8_t)str[end] == 0xA0 || str[end] == ' ' || str[end] == '\0')) {
        str[end] = '\0';
        end--;
    }
}

char petscii_to_ascii(uint8_t petscii) {
    /* Basic PETSCII → ASCII mapping for printable range */
    if (petscii >= 0x41 && petscii <= 0x5A) return (char)(petscii + 0x20);  /* uppercase → lowercase */
    if (petscii >= 0xC1 && petscii <= 0xDA) return (char)(petscii - 0x80);  /* shifted → uppercase */
    if (petscii >= 0x20 && petscii <= 0x7E) return (char)petscii;           /* printable ASCII range */
    return '.';
}
