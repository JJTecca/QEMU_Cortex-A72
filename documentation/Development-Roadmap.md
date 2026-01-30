# Development Roadmap and Project Timeline

## Document Information
- **Project**: RPi5-Industrial-Gateway Bare Metal Development
- **Developer**: Solo Developer
- **Timeline**: 12-16 weeks
- **Target**: Bachelor Thesis Completion
- **Last Updated**: January 2026

---

## Overview

This roadmap outlines a **structured 16-week development plan** for building a production-grade bare-metal real-time protocol gateway on Raspberry Pi 5. The project demonstrates advanced embedded systems expertise suitable for bachelor thesis and potential startup commercialization.

---

## Phase 1: Core Infrastructure (Weeks 1-4)

### Week 1: Environment Setup and Firmware Acquisition

**Objectives:**
- [ ] Install cross-compilation toolchain
- [ ] Setup QEMU ARM64 emulation
- [ ] Obtain Pi5 firmware files
- [ ] Create basic project structure

**Tasks:**

1. **Install Development Tools (Linux/Ubuntu 22.04)**
   ```bash
   sudo apt update
   sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
   sudo apt install qemu-system-arm qemu-efi-aarch64
   sudo apt install gdb-multiarch make git
   ```

2. **Download Raspberry Pi 5 Firmware**
   ```bash
   git clone --depth=1 https://github.com/raspberrypi/firmware.git
   cd firmware/boot
   # Copy start.elf (or start5.elf for Pi5) and fixup.dat
   ```

3. **Create Project Directory Structure**
   ```
   rpi5-industrial-gateway/
   ├── boot/
   │   ├── config.txt
   │   ├── start.elf
   │   └── fixup.dat
   ├── src/
   ├── include/
   ├── linker/
   ├── scripts/
   ├── build/
   └── README.md
   ```

4. **Create Initial config.txt**
   ```ini
   # Enable 64-bit mode
   arm_64bit=1

   # Enable UART on GPIO 14/15
   enable_uart=1
   uart_2712=on

   # Kernel image name
   kernel=kernel8.img

   # Minimal GPU memory
   gpu_mem=16

   # Disable auto-detection
   camera_auto_detect=0
   display_auto_detect=0
   ```

**Deliverables:**
- Working cross-compilation environment
- Pi5 firmware files obtained
- Project structure created
- config.txt configured

---

### Week 2: Boot Assembly and UART "Hello World"

**Objectives:**
- [ ] Write `boot.S` startup assembly
- [ ] Implement basic `main.c`
- [ ] Create `linker.ld` memory layout
- [ ] Achieve UART output

**Tasks:**

1. **Write boot.S (AArch64 Startup)**
   - Stack pointer initialization (per-core stacks)
   - Clear .bss section
   - Initialize .data section
   - Jump to `main()`
   - Optional: Secondary core spin-up via mailbox

2. **Implement main.c**
   ```c
   void main(void) {
       uart_init(115200);
       uart_puts("Hello from Raspberry Pi 5 bare metal!\n");
       while (1) {
           // Main loop
       }
   }
   ```

3. **Create linker.ld**
   - Define memory regions (RAM @ 0x80000 for Pi5)
   - Section layout: .text, .rodata, .data, .bss
   - Stack allocation (4 kB per core × 4 cores)

4. **Implement Basic UART Driver**
   - Initialize UART0 @ 0x40030000
   - Implement `uart_putc()` and `uart_puts()`
   - Configure baud rate (115200)

**Deliverables:**
- `boot.S` with proper initialization
- `main.c` with UART "Hello World"
- `linker.ld` memory layout
- Functional UART output on GPIO 14/15

---

### Week 3: Timer and Delay Functions

**Objectives:**
- [ ] Implement timer driver
- [ ] Add delay functions (`delay_us`, `delay_ms`)
- [ ] Verify timing accuracy

**Tasks:**

1. **ARMv8 Generic Timer Access**
   - Read `CNTPCT_EL0` (Physical Counter)
   - Read `CNTFRQ_EL0` (Counter Frequency)
   - Implement microsecond-precision delays

2. **Timer Driver Implementation**
   ```c
   uint64_t timer_get_ticks(void);
   uint64_t timer_get_freq(void);
   void delay_us(uint32_t microseconds);
   void delay_ms(uint32_t milliseconds);
   ```

3. **RP1 Timer Peripheral (Optional)**
   - General-purpose timer @ 0x400ac000
   - Compare timer functionality

**Deliverables:**
- `timer.c` and `timer.h`
- Accurate microsecond delays
- Timing verification (e.g., LED blink at 1 Hz)

---

### Week 4: Build System and QEMU Testing

**Objectives:**
- [ ] Create Makefile
- [ ] Test on QEMU virt machine
- [ ] Verify UART output in QEMU
- [ ] Setup GDB debugging

**Tasks:**

1. **Create Makefile**
   ```makefile
   PLATFORM ?= qemu_virt  # or rpi5
   CC = aarch64-linux-gnu-gcc
   LD = aarch64-linux-gnu-ld
   OBJCOPY = aarch64-linux-gnu-objcopy

   all: build/kernel8.img

   build/kernel.elf: ...
   build/kernel8.img: ...
   ```

2. **QEMU Testing Script**
   ```bash
   #!/bin/bash
   qemu-system-aarch64 \
     -M virt \
     -cpu cortex-a72 \
     -smp 4 \
     -m 2048M \
     -nographic \
     -kernel build/kernel.elf \
     -serial mon:stdio \
     -s -S  # GDB server on :1234
   ```

3. **GDB Debugging**
   ```bash
   aarch64-linux-gnu-gdb build/kernel.elf
   (gdb) target remote localhost:1234
   (gdb) break main
   (gdb) continue
   ```

**Deliverables:**
- Complete Makefile
- QEMU testing scripts
- GDB debugging setup
- Verified UART output in QEMU

---

## Phase 2: Peripheral Driver Development (Weeks 5-8)

### Week 5: GPIO Driver and Pad Configuration

**Objectives:**
- [ ] Implement GPIO driver
- [ ] Configure GPIO pads
- [ ] Test GPIO output (LED blink)
- [ ] Test GPIO input (button read)

**Tasks:**

1. **GPIO Driver Implementation**
   - Function select (`FUNCSEL`)
   - Atomic register access (SET/CLEAR/XOR)
   - Input/output enable
   - Pull-up/pull-down configuration

2. **Pad Configuration**
   - Drive strength (2/4/8/12 mA)
   - Slew rate control
   - Schmitt trigger enable
   - Voltage select (3.3V default)

3. **Testing**
   - Blink LED on GPIO pin
   - Read button state
   - Toggle GPIO at known frequency (verify with logic analyzer)

**Deliverables:**
- `gpio.c` and `gpio.h`
- Working GPIO input/output
- LED blink demonstration

---

### Week 6: GPIO Interrupts

**Objectives:**
- [ ] Implement GPIO interrupt handling
- [ ] Configure GICv3 interrupt controller
- [ ] Test edge and level interrupts
- [ ] Measure interrupt latency

**Tasks:**

1. **GICv3 Configuration**
   - Enable distributor and CPU interface
   - Configure interrupt priorities
   - Route interrupts to CPU0

2. **GPIO Interrupt Setup**
   - Configure interrupt masks (edge/level)
   - Clear interrupt flags
   - Implement interrupt service routine (ISR)

3. **Latency Measurement**
   - Measure GPIO interrupt response time
   - Compare vs. Linux-based implementation
   - Target: <50μs latency

**Deliverables:**
- GPIO interrupt handler
- GICv3 driver
- Interrupt latency measurements

---

### Week 7: SPI Driver

**Objectives:**
- [ ] Implement SPI master driver (SPI0)
- [ ] Configure SPI mode, clock, chip select
- [ ] Test with SPI loopback or external device

**Tasks:**

1. **SPI Driver Implementation**
   - Initialize SPI0 @ 0x40050000
   - Configure clock polarity/phase (CPOL/CPHA)
   - Implement transfer functions
   - Handle FIFO management

2. **Testing**
   - Loopback test (MISO connected to MOSI)
   - External SPI device (e.g., SPI flash, sensor)
   - Measure throughput

**Deliverables:**
- `spi.c` and `spi.h`
- SPI loopback test passing
- Throughput benchmark

---

### Week 8: I2C Driver

**Objectives:**
- [ ] Implement I2C master driver (I2C0)
- [ ] Configure clock speed (100 kHz / 400 kHz)
- [ ] Test with I2C device

**Tasks:**

1. **I2C Driver Implementation**
   - Initialize I2C0 @ 0x40070000
   - Configure standard/fast mode
   - Implement read/write functions
   - Handle ACK/NACK conditions

2. **Testing**
   - I2C device detection (address scan)
   - Read/write to I2C sensor (e.g., temperature, accelerometer)
   - Verify timing with logic analyzer

**Deliverables:**
- `i2c.c` and `i2c.h`
- I2C device communication working
- Speed verification (100 kHz / 400 kHz)

---

## Phase 3: Integration and Multi-Core (Weeks 9-12)

### Week 9: Multi-Core Bring-Up

**Objectives:**
- [ ] Release secondary cores (CPU1-CPU3)
- [ ] Assign tasks to each core
- [ ] Implement inter-core communication

**Tasks:**

1. **Secondary Core Initialization**
   - Mailbox-based wake-up (write entry point to mailbox address)
   - Per-core stack allocation
   - Core identification (`MPIDR_EL1` register)

2. **Task Distribution**
   - **Core 0**: Modbus RTU RX handler
   - **Core 1**: Protocol parser
   - **Core 2**: MQTT publisher
   - **Core 3**: System watchdog

3. **Inter-Core Communication**
   - Shared memory buffers
   - Lock-free ring buffers
   - Mailbox signaling

**Deliverables:**
- All 4 cores operational
- Per-core task assignment
- Basic inter-core messaging

---

### Week 10: Lock-Free Data Structures

**Objectives:**
- [ ] Implement lock-free ring buffer
- [ ] Test producer-consumer pattern
- [ ] Measure throughput

**Tasks:**

1. **Ring Buffer Implementation**
   - Atomic head/tail pointers
   - Power-of-2 size for fast modulo
   - Memory barriers for ordering

2. **Testing**
   - Core 0 produces data → Core 1 consumes
   - Stress test at high rate (10 kHz)
   - Verify no data loss

**Deliverables:**
- `ringbuf.c` and `ringbuf.h`
- Lock-free data structures working
- Throughput benchmark

---

### Week 11: Protocol Implementation (Modbus RTU)

**Objectives:**
- [ ] Implement Modbus RTU parser
- [ ] CRC16-ANSI validation
- [ ] Frame detection (3.5 char silence)

**Tasks:**

1. **Modbus RTU Parser**
   - Function codes: 0x03 (Read Holding), 0x04 (Read Input)
   - CRC16-ANSI calculation
   - Frame timeout detection

2. **Integration with UART**
   - UART RX interrupt triggers parser
   - DMA-based receive (optional)
   - Error handling

**Deliverables:**
- `modbus_rtu.c` and `modbus_rtu.h`
- Modbus frame parsing working
- CRC validation

---

### Week 12: MQTT Integration

**Objectives:**
- [ ] Implement MQTT 3.1.1 message formatter
- [ ] JSON payload generation
- [ ] Publish messages via UART/Ethernet

**Tasks:**

1. **MQTT Message Formatting**
   - CONNECT packet
   - PUBLISH packet
   - JSON payload (sensor data)

2. **Integration**
   - Core 2 publishes MQTT messages
   - Retry logic with exponential backoff
   - Status monitoring

**Deliverables:**
- `mqtt.c` and `mqtt.h`
- MQTT message publishing working
- JSON payload generation

---

## Phase 4: Performance Optimization and Polish (Weeks 13-16)

### Week 13: Performance Benchmarking

**Objectives:**
- [ ] Measure GPIO latency/jitter
- [ ] Benchmark I2C/SPI throughput
- [ ] Compare vs. Linux baseline

**Tasks:**

1. **Latency Measurements**
   - GPIO interrupt latency
   - UART RX interrupt latency
   - Modbus poll cycle time

2. **Throughput Benchmarks**
   - SPI max throughput (MB/s)
   - I2C max throughput (kbps)
   - UART max baud rate

**Deliverables:**
- Performance benchmark results
- Comparison vs. Linux
- Latency histograms

---

### Week 14: Atomic Register Optimization

**Objectives:**
- [ ] Implement atomic GPIO access
- [ ] Optimize critical paths
- [ ] Reduce PCIe round-trip latency

**Tasks:**

1. **Atomic Register Access**
   - Use SET/CLEAR/XOR offsets (0x2000/0x3000/0x1000)
   - Avoid read-modify-write cycles
   - Measure latency improvement

2. **Critical Path Optimization**
   - Profile with GDB/timers
   - Optimize hot loops
   - Reduce interrupt handler overhead

**Deliverables:**
- Atomic register access implemented
- Latency reduction measured
- Optimized critical paths

---

### Week 15: Comprehensive Documentation

**Objectives:**
- [ ] Write thesis documentation
- [ ] Document all register offsets
- [ ] Create API reference
- [ ] Write user guide

**Tasks:**

1. **Thesis Documentation**
   - Introduction and motivation
   - System architecture
   - Implementation details
   - Performance evaluation
   - Conclusion and future work

2. **API Reference**
   - Document all functions
   - Register map reference
   - Code examples

3. **User Guide**
   - Build instructions
   - Deployment guide
   - Troubleshooting

**Deliverables:**
- Complete thesis documentation
- API reference manual
- User guide

---

### Week 16: Professional Demo and Final Polish

**Objectives:**
- [ ] Prepare professional demo
- [ ] Final testing and bug fixes
- [ ] Create presentation materials

**Tasks:**

1. **Demo Application**
   - Real-time sensor data logging
   - Modbus → MQTT protocol gateway
   - LED status indicators
   - UART debug output

2. **Testing**
   - End-to-end system test
   - Stress testing (24-hour runtime)
   - Error handling verification

3. **Presentation**
   - Demo video recording
   - Slide deck creation
   - Live demonstration preparation

**Deliverables:**
- Working professional demo
- Final code release
- Presentation materials

---

## Milestones and Checkpoints

### Milestone 1 (Week 4): Core Infrastructure Complete
- [ ] UART "Hello World" working
- [ ] QEMU testing operational
- [ ] Makefile complete

### Milestone 2 (Week 8): Peripheral Drivers Complete
- [ ] GPIO working (input/output/interrupts)
- [ ] SPI driver functional
- [ ] I2C driver functional

### Milestone 3 (Week 12): Multi-Core Integration Complete
- [ ] All 4 cores operational
- [ ] Modbus RTU parsing working
- [ ] MQTT publishing functional

### Milestone 4 (Week 16): Project Complete
- [ ] Performance targets met
- [ ] Documentation complete
- [ ] Professional demo ready

---

## Risk Management

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| PCIe latency too high | Medium | High | Use atomic register access, disable ASPM |
| GPIO interrupt jitter | Low | Medium | Optimize ISR, use hardware debounce |
| Multi-core synchronization bugs | Medium | High | Extensive testing, lock-free algorithms |
| UART overrun at high baud | Low | Medium | Use DMA, larger FIFO thresholds |

### Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Hardware availability delay | Low | Medium | Use QEMU for development |
| Debugging time underestimated | High | Medium | Allocate buffer weeks 13-16 |
| Documentation takes longer | Medium | Low | Start documentation early (week 8+) |

---

## Success Criteria

### Technical Criteria
- [ ] **Boot time**: <2 seconds to operational
- [ ] **Interrupt latency**: <100μs (target <50μs)
- [ ] **Jitter**: <10μs (99th percentile)
- [ ] **Modbus poll rate**: 10 kHz (100μs cycle)
- [ ] **RAM usage**: <16 MB
- [ ] **Code quality**: Zero TODOs, full implementation

### Academic Criteria
- [ ] **Thesis**: Complete, well-structured documentation
- [ ] **Demonstration**: Professional live demo
- [ ] **Code**: Clean, commented, version-controlled (Git)
- [ ] **Reproducibility**: Clear build and deployment instructions

### Startup Criteria (Optional)
- [ ] **Scalability**: Code supports multiple industrial protocols
- [ ] **Reliability**: 24-hour stress test without crashes
- [ ] **Market fit**: Solves real industrial gateway latency problem

---

## Tools and Resources

### Development Tools
- **Cross-compiler**: `aarch64-linux-gnu-gcc`
- **Debugger**: `gdb-multiarch`
- **Emulator**: `qemu-system-aarch64`
- **Version Control**: Git
- **Documentation**: Markdown, LaTeX

### Hardware
- **Raspberry Pi 5** (4GB or 8GB RAM)
- **microSD card** (16GB minimum, Class 10)
- **USB-UART adapter** (3.3V logic, FTDI recommended)
- **Logic analyzer** (optional, for signal verification)
- **Oscilloscope** (optional, for timing measurements)

### Reference Materials
- RP1 Peripherals Datasheet
- ARM Cortex-A76 Technical Reference Manual
- ARM GICv3 Architecture Specification
- PL011 UART Technical Reference Manual

---

**Document Version**: 1.0  
**Generated**: January 2026  
**Target**: Bachelor Thesis Project Execution
