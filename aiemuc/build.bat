@echo off
echo Building CPU 6510 test...
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c mos6510.c -o mos6510.o
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c c64_bus.c -o c64_bus.o
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c cpu_test.c -o cpu_test.o
gcc -Wall -Wextra -std=c99 -O2 -g mos6510.o c64_bus.o cpu_test.o -o cpu_test.exe
echo Build complete. Running test...
cpu_test.exe