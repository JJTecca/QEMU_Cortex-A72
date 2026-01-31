# Raspberry Pi 5 Bare Metal Development - Windows Toolchain Setup

Complete guide to setting up the development environment for Raspberry Pi 5 bare-metal programming on Windows using MSYS2.

---

## 📋 Required Tools Overview

| Tool | Purpose | Installation Method |
|------|---------|-------------------|
| **MSYS2** | Unix-like environment, Make, QEMU | Chocolatey / Manual |
| **ARM GNU Toolchain** | AArch64 cross-compiler | Manual installer |
| **QEMU** | ARM64 emulator for testing | MSYS2 package |
| **Make** | Build automation | MSYS2 package |

---

## 🚀 Quick Installation Guide

### Step 1: Install Chocolatey (Package Manager)

**Run PowerShell as Administrator** and execute:

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
```

**Verify installation:**
```powershell
choco --version
```

---

### Step 2: Install MSYS2

**Using Chocolatey (Recommended):**

```powershell
choco install msys2 -y
```

Default installation path: `C:\msys64`

**Manual Download:**
- Download: https://www.msys2.org/
- Run installer and follow prompts
- Install to: `C:\msys64` (recommended)

---

### Step 3: Install QEMU and Make in MSYS2

**Open MSYS2 MINGW64 terminal** (search "MSYS2 MINGW64" in Start Menu)

```bash
# Update MSYS2 package database
pacman -Syu

# If terminal closes, reopen MSYS2 MINGW64 and run:
pacman -Su

# Install QEMU for ARM64 emulation
pacman -S mingw-w64-x86_64-qemu

# Install Make build tool
pacman -S make

# Verify installations
qemu-system-aarch64 --version
make --version
```

**Expected output:**
```
QEMU emulator version 10.x.x
GNU Make 4.4.1
```

---

### Step 4: Install ARM GNU Toolchain (AArch64 Cross-Compiler)

**Download from ARM Developer:**

🔗 **Download Link:** https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

**What to download:**
- **Target:** AArch64 bare-metal target (aarch64-none-elf)
- **Host:** Windows (mingw-w64-i686) hosted cross toolchains
- **Version:** 13.2.Rel1 or newer
- **File:** `arm-gnu-toolchain-13.2.rel1-mingw-w64-i686-aarch64-none-elf.exe`

**Direct download (if link still valid):**
```
https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-mingw-w64-i686-aarch64-none-elf.exe
```

**Installation steps:**
1. Run the `.exe` installer
2. Install to: `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\`
3. ⚠️ **Check the box:** "Add path to environment variable" during installation
4. Click Install and wait for completion

---

## 🔧 Environment Variables Configuration

### Method 1: Add to Windows PATH (System-wide)

**If installer didn't add PATH automatically:**

1. Press `Windows Key + R`
2. Type: `sysdm.cpl` and press Enter
3. Click **"Advanced"** tab → **"Environment Variables"**
4. Under **"User variables"**, select **Path** → Click **"Edit"**
5. Click **"New"** and add these paths:

```
C:\msys64\mingw64\bin
C:\msys64\usr\bin
C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin
```

6. Click **OK** on all dialogs
7. **Close and reopen** all Command Prompt/PowerShell/MSYS2 terminals

---

### Method 2: Add to MSYS2 Only (MSYS2-specific)

**In MSYS2 MINGW64 terminal:**

```bash
# Add ARM toolchain to MSYS2 profile
echo 'export PATH="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-aarch64-none-elf/bin:$PATH"' >> ~/.bashrc

# Reload configuration
source ~/.bashrc

# Verify
aarch64-none-elf-gcc --version
```

---

### Method 3: Use Batch Files (No PATH Configuration Needed)

**Create `build.bat` in your project root:**

```batch
@echo off
echo ============================================
echo Building for QEMU ARM64
echo ============================================
echo.

REM Set all tool paths
set "ARM_TOOLCHAIN=C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin"
set "MSYS2_ROOT=C:\msys64"
set "PATH=%ARM_TOOLCHAIN%;%MSYS2_ROOT%\mingw64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

cd /d "%~dp0"

echo Cleaning...
make clean

echo Building...
make PLATFORM=qemuvirt

if exist "build\kernel.elf" (
    echo.
    echo ========================================
    echo BUILD SUCCESS!
    echo ========================================
    echo Output: build\kernel.elf
    echo.
) else (
    echo.
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
)

pause
```

**Create `run.bat` in your project root:**

```batch
@echo off

set "ARM_TOOLCHAIN=C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin"
set "MSYS2_ROOT=C:\msys64"
set "PATH=%ARM_TOOLCHAIN%;%MSYS2_ROOT%\mingw64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

cd /d "%~dp0"

if not exist "build\kernel.elf" (
    echo Building first...
    make PLATFORM=qemuvirt
)

if not exist "build\kernel.elf" (
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ============================================
echo Starting QEMU ARM64 Emulator
echo ============================================
echo Target: Cortex-A72 (Raspberry Pi 5)
echo Cores: 4, RAM: 2GB
echo.
echo Exit: Ctrl+A then X
echo.

qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -m 2048M -nographic -kernel build\kernel.elf -serial mon:stdio

pause
```

---

## ✅ Verification

### Test All Tools

**Open a NEW Command Prompt or PowerShell:**

```powershell
# Test Make
make --version

# Test QEMU
qemu-system-aarch64 --version

# Test ARM compiler
aarch64-none-elf-gcc --version

# Test ARM assembler
aarch64-none-elf-as --version

# Test ARM linker
aarch64-none-elf-ld --version

# Test objcopy
aarch64-none-elf-objcopy --version
```

**Expected output:**
```
GNU Make 4.4.1
QEMU emulator version 10.x.x
aarch64-none-elf-gcc (Arm GNU Toolchain 13.2.Rel1) ...
GNU assembler ...
GNU ld ...
GNU objcopy ...
```

---

## 📦 Tool Versions (As of January 2026)

| Tool | Version | Notes |
|------|---------|-------|
| MSYS2 | Latest | Rolling release |
| QEMU | 10.1.3+ | ARM64 emulation support |
| Make | 4.4.1+ | GNU Make |
| ARM Toolchain | 13.2.Rel1+ | AArch64 bare-metal target |

---

## 🔥 Common Issues & Solutions

### Issue 1: `make: command not found`

**Solution:**
```bash
# In MSYS2 MINGW64 terminal
pacman -S make
```

### Issue 2: `aarch64-none-elf-gcc: command not found`

**Solutions:**
1. Verify ARM toolchain is installed at: `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin\`
2. Add to PATH (see Environment Variables section)
3. Use batch files (Method 3)

### Issue 3: `refreshenv` doesn't work

**Solution:**
- **Close ALL terminals** (Command Prompt, PowerShell, MSYS2)
- Open a **brand new** terminal
- Windows only loads PATH at process start

### Issue 4: MSYS2 terminal doesn't see Windows PATH

**Solution:**
- MSYS2 has its own environment
- Use Method 2 (add to `~/.bashrc`) or Method 3 (batch files)

---

## 🎯 Quick Start Workflow

### Option A: Using Batch Files (Easiest)

1. Create `build.bat` and `run.bat` in project root
2. **Double-click `run.bat`** to build and run in QEMU
3. Exit QEMU: `Ctrl+A` then `X`

### Option B: Using MSYS2 Terminal

```bash
# Navigate to project
cd /c/path/to/your/project

# Build for QEMU
make PLATFORM=qemuvirt

# Run in QEMU
qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -m 2048M -nographic -kernel build/kernel.elf -serial mon:stdio

# Exit QEMU: Ctrl+A then X
```

### Option C: Using Windows Terminal

```cmd
cd C:\path\to\your\project
make PLATFORM=qemuvirt
qemu-system-aarch64 -M virt -cpu cortex-a72 -smp 4 -m 2048M -nographic -kernel build\kernel.elf -serial mon:stdio
```

---

## 📂 Installation Directory Summary

| Component | Default Path |
|-----------|--------------|
| MSYS2 | `C:\msys64` |
| MSYS2 binaries | `C:\msys64\mingw64\bin`, `C:\msys64\usr\bin` |
| ARM Toolchain | `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf` |
| ARM binaries | `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin` |
| Chocolatey | `C:\ProgramData\chocolatey` |

---

## 🔗 Official Download Links

| Tool | Link |
|------|------|
| **MSYS2** | https://www.msys2.org/ |
| **Chocolatey** | https://chocolatey.org/install |
| **ARM GNU Toolchain** | https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |
| **QEMU Documentation** | https://www.qemu.org/docs/master/ |

---

## 📚 Additional Resources

- **MSYS2 Documentation:** https://www.msys2.org/docs/what-is-msys2/
- **ARM Cortex-A Series Guide:** https://developer.arm.com/Processors/Cortex-A72
- **QEMU ARM System Emulation:** https://www.qemu.org/docs/master/system/target-arm.html
- **Raspberry Pi 5 BCM2712 Documentation:** https://datasheets.raspberrypi.com/

---

## ✨ Success Checklist

- [ ] Chocolatey installed and working (`choco --version`)
- [ ] MSYS2 installed at `C:\msys64`
- [ ] QEMU installed and accessible (`qemu-system-aarch64 --version`)
- [ ] Make installed and accessible (`make --version`)
- [ ] ARM toolchain installed
- [ ] `aarch64-none-elf-gcc --version` works
- [ ] PATH configured (System/MSYS2/Batch files)
- [ ] Can build project successfully
- [ ] Can run kernel in QEMU

---

**Setup complete! You're ready for Raspberry Pi 5 bare-metal development! 🎉**
