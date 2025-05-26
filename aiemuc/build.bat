@echo off
echo Building CPU 6510 test...
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c cpu6510.c -o cpu6510.o
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c bus.c -o bus.o
gcc -Wall -Wextra -std=c99 -O2 -g -I. -c cpu_test.c -o cpu_test.o
gcc -Wall -Wextra -std=c99 -O2 -g cpu6510.o bus.o cpu_test.o -o cpu_test.exe
echo Build complete. Running test...
cpu_test.exe