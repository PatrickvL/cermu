# Static library + dynamic CRT (/MD) — avoids CRT mismatch while
# producing a self-contained archive.lib with no DLL dependency.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
