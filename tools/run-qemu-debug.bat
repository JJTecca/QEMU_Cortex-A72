@echo off

REM ===============================================
REM Build and Run QEMU in DEBUG MODE (with GDB)
REM ===============================================

REM Kill any existing QEMU processes first
echo Checking for stuck QEMU processes...
taskkill /F /IM qemu-system-aarch64.exe 2>nul
if %errorlevel% == 0 (
    echo [OK] Killed previous QEMU instance
    timeout /t 1 >nul
) else (
    echo [OK] No QEMU processes running
)

REM Set all tool paths
set "ARM_TOOLCHAIN=C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin"
set "MSYS2_ROOT=C:\msys64"
set "PATH=%ARM_TOOLCHAIN%;%MSYS2_ROOT%\mingw64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

REM Navigate to project root
cd D:\QEMU_Cortex-A72

echo.
echo ============================================
echo Cleaning previous build...
echo ============================================
make clean

echo.
echo ============================================
echo Building Multi-Core Kernel...
echo ============================================
make PLATFORM=qemuvirt

if not exist "build/kernel.elf" (
    echo.
    echo ============================================
    echo BUILD FAILED!
    echo ============================================
    pause
    exit /b 1
)

echo.
echo ============================================
echo Starting QEMU in DEBUG MODE
echo ============================================
echo.
echo QEMU is waiting for GDB connection on port 1234
echo.
echo In another terminal, run:
echo   aarch64-none-elf-gdb build/kernel.elf
echo   (gdb) target remote localhost:1234
echo   (gdb) break _start
echo   (gdb) continue
echo   (gdb) info registers
echo   (gdb) stepi
echo.
echo Press Ctrl-C to stop QEMU when done debugging
echo.

REM Debug mode: -S pauses at start, -s opens GDB server on port 1234
qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -m 2048M -nographic -kernel build/kernel.elf -serial mon:stdio -S -s

echo.
echo ============================================
echo QEMU terminated
echo ============================================

REM Final cleanup
taskkill /F /IM qemu-system-aarch64.exe 2>nul

pause