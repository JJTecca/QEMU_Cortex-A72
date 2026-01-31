@echo off
REM ===============================================
REM Build and Run in QEMU - Native Windows
REM ===============================================

set MSYSTEM=MINGW64
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

REM Navigate to your project directory
cd D:\QEMU_Cortex-A72

echo Building kernel...
make PLATFORM=qemuvirt

if not exist "build\kernel.elf" (
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ============================================
echo STARTING QEMU...
echo ============================================
echo Press Ctrl+A then X to exit QEMU (see if it works)
echo.

qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -m 2048M -nographic -kernel build\kernel.elf -serial mon:stdio

echo.
echo QEMU terminated.
echo.
pause
