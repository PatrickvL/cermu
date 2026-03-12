// =============================================================================
// Centralized STB single-file library implementations
// =============================================================================
// STB libraries are header-only; exactly ONE translation unit must define
// the _IMPLEMENTATION macro so that function bodies are emitted.  All other
// files that need stb_image or stb_image_write simply #include the header
// without the macro.
// =============================================================================

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
