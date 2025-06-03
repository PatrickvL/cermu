@echo off
echo Building CPU 6510 test with MSVC...
cl /nologo /W3 /O2 /I. /c c64_bus.c
cl /nologo /W3 /O2 /I. /c mos6510.c
cl /nologo /W3 /O2 /I. /c vic.c
cl /nologo /W3 /O2 /I. /c cia.c
cl /nologo /W3 /O2 /I. /c sid.c
cl /nologo /W3 /O2 /I. /c c64.c
cl /nologo /W3 /O2 /I. /c cpu_test.c
cl /nologo /W3 /O2 c64_bus.obj mos6510.obj vic.obj cia.obj sid.obj c64.obj cpu_test.obj /Fe:cpu_test.exe
echo Build complete. Running test...
cpu_test.exe