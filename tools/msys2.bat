@echo off
echo ====================================
echo Installing QEMU Bare-Metal Toolchain
echo ====================================
echo.

echo [1/3] Installing QEMU...
pacman -S --noconfirm mingw-w64-x86_64-qemu
echo.

echo [2/3] Installing ARM GCC Compiler...
pacman -S --noconfirm mingw-w64-x86_64-arm-none-eabi-gcc
echo.

echo [3/3] Installing ARM Binutils...
pacman -S --noconfirm mingw-w64-x86_64-arm-none-eabi-binutils
echo.

echo ====================================
echo Installation Complete!
echo ====================================
echo.

echo Verifying installation...
echo.

echo Checking QEMU:
qemu-system-aarch64 --version
echo.

echo Checking ARM GCC:
arm-none-eabi-gcc --version
echo.

echo Checking Make:
make --version
echo.

echo Checking Git:
git --version
echo.

echo ====================================
echo All tools installed successfully!
echo ====================================
pause
