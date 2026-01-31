@echo off
REM ===============================================
REM Build for QEMU - Native Windows Version
REM ===============================================

echo.
echo [Building for QEMU...]
echo.

REM Set MSYS2 environment
set MSYSTEM=MINGW64
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

REM Navigate to project directory
cd D:\QEMU_Cortex-A72

REM Clean using make
echo Cleaning previous build...
make clean

REM Build for QEMU
echo Building kernel...
make PLATFORM=qemuvirt

REM Check if build succeeded
if exist "build\kernel.elf" (
    echo.
    echo ============================================
    echo BUILD SUCCESSFUL!
    echo ============================================
    echo Output: build\kernel.elf
    echo.
) else (
    echo.
    echo ============================================
    echo BUILD FAILED!
    echo ============================================
    echo Check error messages above.
    echo.
    pause
    exit /b 1
)

pause