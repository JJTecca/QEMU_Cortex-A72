# GPIO and Peripheral Driver Development Guide

## Document Information
- **Project**: RPi5-Industrial-Gateway
- **Purpose**: GPIO and peripheral driver implementation reference
- **Target Platforms**: Raspberry Pi 5 (RP1), QEMU virt (development)
- **Last Updated**: January 2026

---

## 1. GPIO Controller (IOBANK0)

### Base Addresses
- **IOBANK0** (GPIO Control): `0x400d0000`
- **PADSBANK0** (Pad Configuration): `0x400f0000`
- **SYSRIO0** (Registered I/O): `0x400e0000`

### GPIO Pin Count and Organization
- **Total Pins**: 28 user-accessible GPIO pins
- **Bank**: IOBANK0 (user-facing)
- **Reserved Banks**: IOBANK1, IOBANK2 (internal use only)

---

## 2. GPIO Register Map (IOBANK0)

### Register Layout

| Offset | Register | Description |
|--------|----------|-------------|
| `0x000` | `GPIO0_STATUS` | GPIO 0 status and event flags |
| `0x004` | `GPIO0_CTRL` | GPIO 0 control (FUNCSEL, overrides, interrupt masks) |
| `0x008` | `GPIO1_STATUS` | GPIO 1 status and event flags |
| `0x00c` | `GPIO1_CTRL` | GPIO 1 control |
| ... | ... | Pattern continues for GPIO 0-27 |
| `0x0d8` | `GPIO27_STATUS` | GPIO 27 status and event flags |
| `0x0dc` | `GPIO27_CTRL` | GPIO 27 control |
| `0x100` | `INTR` | Raw interrupts for all GPIO |
| `0x104` | `PROC0_INTE` | Interrupt enable for processor 0 |
| `0x108` | `PROC0_INTF` | Interrupt force for processor 0 |
| `0x10c` | `PROC0_INTS` | Interrupt status for processor 0 |
| `0x110` | `PROC1_INTE` | Interrupt enable for processor 1 |
| `0x114` | `PROC1_INTF` | Interrupt force for processor 1 |
| `0x118` | `PROC1_INTS` | Interrupt status for processor 1 |
| `0x11c` | `PCIE_INTE` | Interrupt enable for PCIe host |
| `0x120` | `PCIE_INTF` | Interrupt force for PCIe |
| `0x124` | `PCIE_INTS` | Interrupt status for PCIe |

---

## 3. GPIO Function Selection

### Function Multiplexing
Each GPIO pin supports **9 configurable functions** via the `FUNCSEL` field in `GPIOn_CTRL` register.

### GPIO Function Table (Selected Examples)

| GPIO | F0 (Primary) | F1 | F2 | F3 | F4 | F5 |
|------|--------------|----|----|----|----|-----|
| 0 | SPI0_SIO3 | DPI_PCLK | UART1_TX | I2C0_SDA | SYSRIO0 | PROCRIO0 |
| 1 | SPI0_SIO2 | DPI_DE | UART1_RX | I2C0_SCL | SYSRIO1 | PROCRIO1 |
| 14 | PWM0_2 | DPI_D10 | UART4_CTS | I2C3_SDA | UART0_TX | SYSRIO14 |
| 15 | PWM0_3 | DPI_D11 | UART4_RTS | I2C3_SCL | UART0_RX | SYSRIO15 |
| 22 | SDIO0_CLK | DPI_D18 | I2S0_SDI1 | I2C3_SDA | I2S1_SDI1 | SYSRIO22 |
| 23 | SDIO0_CMD | DPI_D19 | I2S0_SDO1 | I2C3_SCL | I2S1_SDO1 | SYSRIO23 |

**Note**: See complete function table in `rp1_peripherals.pdf` for all 28 GPIO pins.

---

## 4. GPIO Control Register (GPIOn_CTRL)

### Bit Field Breakdown

| Bits | Field | Description |
|------|-------|-------------|
| `31:30` | `IRQOVER` | Interrupt override (0=normal, 1=invert, 2=force low, 3=force high) |
| `29` | Reserved | - |
| `28` | `IRQRESET` | Reset interrupt edge detector (self-clearing) |
| `27` | `IRQMASK_DBLEVEL_HIGH` | Mask debounced level high interrupt |
| `26` | `IRQMASK_DBLEVEL_LOW` | Mask debounced level low interrupt |
| `25` | `IRQMASK_FEDGE_HIGH` | Mask filtered edge high interrupt |
| `24` | `IRQMASK_FEDGE_LOW` | Mask filtered edge low interrupt |
| `23` | `IRQMASK_LEVEL_HIGH` | Mask level high interrupt |
| `22` | `IRQMASK_LEVEL_LOW` | Mask level low interrupt |
| `21` | `IRQMASK_EDGE_HIGH` | Mask edge high interrupt |
| `20` | `IRQMASK_EDGE_LOW` | Mask edge low interrupt |
| `19:18` | Reserved | - |
| `17:16` | `INOVER` | Input override (0=normal, 1=invert, 2=force low, 3=force high) |
| `15:14` | `OEOVER` | Output enable override |
| `13:12` | `OUTOVER` | Output override |
| `11:5` | `FM` | Filter/debounce time constant M |
| `4:0` | `FUNCSEL` | Function select (0x00-0x08 for F0-F8, 0x1f=NULL) |

---

## 5. Atomic Register Access

### Overview
RP1 peripherals with atomic access support **modify-without-read** operations, avoiding PCIe round-trip latency (~1μs).

### Access Methods
Each peripheral with atomic support allocates **4 kB address space**:

| Address Offset | Access Type | Description |
|----------------|-------------|-------------|
| `0x0000` | Normal | Standard read/write access |
| `0x1000` | XOR | Atomic XOR on write, peek read (no side effects) |
| `0x2000` | SET | Atomic bitmask set on write, normal read |
| `0x3000` | CLEAR | Atomic bitmask clear on write, normal read |

### Example: GPIO Atomic Access

```c
// GPIO0_CTRL normal address
#define GPIO0_CTRL      (0x400d0004)

// Atomic aliases
#define GPIO0_CTRL_XOR  (0x400d1004)  // XOR toggle
#define GPIO0_CTRL_SET  (0x400d2004)  // Atomic set
#define GPIO0_CTRL_CLR  (0x400d3004)  // Atomic clear

// Example: Enable edge high interrupt without read-modify-write
*(volatile uint32_t *)GPIO0_CTRL_SET = (1 << 21);  // Set IRQMASK_EDGE_HIGH

// Example: Disable edge low interrupt
*(volatile uint32_t *)GPIO0_CTRL_CLR = (1 << 20);  // Clear IRQMASK_EDGE_LOW
```

---

## 6. Pad Configuration (PADSBANK0)

### Base Address
- **PADSBANK0**: `0x400f0000`

### Register Layout

| Offset | Register | Description |
|--------|----------|-------------|
| `0x00` | `VOLTAGE_SELECT` | Bank voltage selection (0=3.3V, 1=1.8V) |
| `0x04` | `GPIO0` | Pad control for GPIO 0 |
| `0x08` | `GPIO1` | Pad control for GPIO 1 |
| ... | ... | Pattern continues for GPIO 0-27 |
| `0x70` | `GPIO27` | Pad control for GPIO 27 |

### Pad Control Register Bit Fields

| Bits | Field | Description |
|------|-------|-------------|
| `31:8` | Reserved | - |
| `7` | `OD` | Output disable (overrides peripheral output enable) |
| `6` | `IE` | Input enable |
| `5:4` | `DRIVE` | Drive strength (0=2mA, 1=4mA, 2=8mA, 3=12mA) |
| `3` | `PUE` | Pull-up enable |
| `2` | `PDE` | Pull-down enable |
| `1` | `SCHMITT` | Enable Schmitt trigger |
| `0` | `SLEWFAST` | Slew rate control (1=fast, 0=slow) |

### Typical Pad Configuration

```c
// Configure GPIO 14 for UART TX (output, no pulls, fast slew)
#define PADS_GPIO14 (0x400f0040)

uint32_t pad_config = 0
    | (0 << 7)  // OD: Output not disabled
    | (1 << 6)  // IE: Input enabled (for UART echo)
    | (2 << 4)  // DRIVE: 8mA drive strength
    | (0 << 3)  // PUE: Pull-up disabled
    | (0 << 2)  // PDE: Pull-down disabled
    | (1 << 1)  // SCHMITT: Schmitt trigger enabled
    | (1 << 0); // SLEWFAST: Fast slew rate

*(volatile uint32_t *)PADS_GPIO14 = pad_config;
```

---

## 7. GPIO Interrupt Handling

### Interrupt Sources (8 per GPIO pin)
1. **Level High**: GPIO is logical 1
2. **Level Low**: GPIO is logical 0
3. **Debounced Level High**: GPIO logical 1 for debounce time
4. **Debounced Level Low**: GPIO logical 0 for debounce time
5. **Edge High (Rising)**: GPIO transitioned 0→1
6. **Edge Low (Falling)**: GPIO transitioned 1→0
7. **Filtered Edge High**: Rising edge after filter time
8. **Filtered Edge Low**: Falling edge after filter time

### Interrupt Configuration Steps

```c
// Example: Configure GPIO 17 for rising edge interrupt

#define GPIO17_CTRL     (0x400d0048)
#define GPIO17_CTRL_SET (0x400d2048)
#define PCIE_INTE       (0x400d011c)
#define PCIE_INTS       (0x400d0124)

// Step 1: Configure function (e.g., GPIO input)
*(volatile uint32_t *)GPIO17_CTRL = 0x1f;  // FUNCSEL=NULL (GPIO input mode)

// Step 2: Enable edge high interrupt mask (atomic set)
*(volatile uint32_t *)GPIO17_CTRL_SET = (1 << 21);  // IRQMASK_EDGE_HIGH

// Step 3: Enable GPIO17 interrupt for PCIe host
*(volatile uint32_t *)PCIE_INTE |= (1 << 17);

// Step 4: In interrupt handler, check and clear
uint32_t int_status = *(volatile uint32_t *)PCIE_INTS;
if (int_status & (1 << 17)) {
    // GPIO17 interrupt occurred
    // Clear edge interrupt by writing 1 to IRQRESET
    *(volatile uint32_t *)GPIO17_CTRL_SET = (1 << 28);
}
```

---

## 8. UART Driver (PL011)

### UART Instances
- **Count**: 6 UART controllers (uart0-uart5)
- **Standard**: ARM PrimeCell UART (PL011) Revision r1p5

### Base Addresses

| Instance | Base Address | Description |
|----------|--------------|-------------|
| uart0 | `0x40030000` | UART 0 (typically GPIO 14/15) |
| uart1 | `0x40034000` | UART 1 |
| uart2 | `0x40038000` | UART 2 |
| uart3 | `0x4003c000` | UART 3 |
| uart4 | `0x40040000` | UART 4 |
| uart5 | `0x40044000` | UART 5 |

### UART Features
- **TX FIFO**: 32×8 bits
- **RX FIFO**: 32×12 bits
- **Baud Rate**: Programmable (common: 115200)
- **Data Bits**: 5, 6, 7, or 8
- **Stop Bits**: 1 or 2
- **Parity**: Odd, Even, or None
- **Flow Control**: Hardware RTS/CTS support

### Key UART Registers

| Offset | Register | Description |
|--------|----------|-------------|
| `0x00` | `UARTDR` | Data register (read RX, write TX) |
| `0x04` | `UARTRSR` | Receive status / error clearance |
| `0x18` | `UARTFR` | Flag register (TXFF, RXFF, BUSY, etc.) |
| `0x24` | `UARTIBRD` | Integer baud rate divisor |
| `0x28` | `UARTFBRD` | Fractional baud rate divisor |
| `0x2c` | `UARTLCR_H` | Line control (word size, stop bits, parity) |
| `0x30` | `UARTCR` | Control register (TX/RX enable, loopback) |
| `0x34` | `UARTIFLS` | Interrupt FIFO level select |
| `0x38` | `UARTIMSC` | Interrupt mask |
| `0x3c` | `UARTRIS` | Raw interrupt status |
| `0x40` | `UARTMIS` | Masked interrupt status |
| `0x44` | `UARTICR` | Interrupt clear register |

### UART Initialization Example

```c
#define UART0_BASE   (0x40030000)
#define UARTDR       (UART0_BASE + 0x00)
#define UARTFR       (UART0_BASE + 0x18)
#define UARTIBRD     (UART0_BASE + 0x24)
#define UARTFBRD     (UART0_BASE + 0x28)
#define UARTLCR_H    (UART0_BASE + 0x2c)
#define UARTCR       (UART0_BASE + 0x30)

void uart_init(uint32_t baud_rate) {
    // Disable UART
    *(volatile uint32_t *)UARTCR = 0;

    // Set baud rate (assuming 48 MHz UART clock)
    // IBRD = UART_CLK / (16 * baud_rate)
    // FBRD = ((UART_CLK % (16 * baud_rate)) * 64) / (16 * baud_rate)
    uint32_t uart_clk = 48000000;
    uint32_t divisor = uart_clk / (16 * baud_rate);
    uint32_t remainder = uart_clk % (16 * baud_rate);
    uint32_t fraction = (remainder * 64) / (16 * baud_rate);

    *(volatile uint32_t *)UARTIBRD = divisor;
    *(volatile uint32_t *)UARTFBRD = fraction;

    // Line control: 8 data bits, no parity, 1 stop bit, FIFOs enabled
    *(volatile uint32_t *)UARTLCR_H = (3 << 5) | (1 << 4);  // WLEN=8, FEN=1

    // Enable UART, TX, and RX
    *(volatile uint32_t *)UARTCR = (1 << 0) | (1 << 8) | (1 << 9);  // UARTEN=1, TXE=1, RXE=1
}

void uart_putc(char c) {
    // Wait until TX FIFO not full
    while (*(volatile uint32_t *)UARTFR & (1 << 5));  // TXFF
    *(volatile uint32_t *)UARTDR = c;
}

char uart_getc(void) {
    // Wait until RX FIFO not empty
    while (*(volatile uint32_t *)UARTFR & (1 << 4));  // RXFE
    return *(volatile uint32_t *)UARTDR & 0xff;
}
```

---

## 9. SPI Driver (Synopsys DWapb_ssi)

### SPI Instances
- **Count**: 8 SPI controllers (spi0-spi7) + spi8 (reserved)
- **Standard**: Synopsys DWapb_ssi IP v4.02a

### Configuration Summary

| Instance | Mode | Chip Selects | Max I/O Width |
|----------|------|--------------|---------------|
| spi0 | Master | 4 | Quad-SPI |
| spi1 | Master | 3 | Dual-SPI |
| spi2 | Master | 2 | Dual-SPI |
| spi3 | Master | 2 | Dual-SPI |
| spi4 | Slave | 1 | Single |
| spi5 | Master | 2 | Dual-SPI |
| spi6 | Master | 3 | Dual-SPI |
| spi7 | Slave | 1 | Single |
| spi8 | Master | 2 | Dual-SPI |

### Base Addresses

| Instance | Base Address |
|----------|--------------|
| spi0 | `0x40050000` |
| spi1 | `0x40054000` |
| spi2 | `0x40058000` |
| spi3 | `0x4005c000` |
| spi4 | `0x40060000` |
| spi5 | `0x40064000` |
| spi6 | `0x40068000` |
| spi7 | `0x4006c000` |
| spi8 | `0x4004c000` |

### Key SPI Registers

| Offset | Register | Description |
|--------|----------|-------------|
| `0x00` | `CTRLR0` | SPI control register 0 |
| `0x04` | `CTRLR1` | SPI control register 1 |
| `0x08` | `SSIENR` | SSI enable |
| `0x10` | `SER` | Slave enable |
| `0x14` | `BAUDR` | Baud rate |
| `0x18` | `TXFTLR` | TX FIFO threshold level |
| `0x1c` | `RXFTLR` | RX FIFO threshold level |
| `0x20` | `TXFLR` | TX FIFO level |
| `0x24` | `RXFLR` | RX FIFO level |
| `0x28` | `SR` | Status register |
| `0x2c` | `IMR` | Interrupt mask |
| `0x30` | `ISR` | Interrupt status |
| `0x60` | `DR0` | Data register 0 (read RX, write TX) |

---

## 10. I2C Driver (Synopsys DWapb_i2c)

### I2C Instances
- **Count**: 7 I2C controllers (i2c0-i2c6)
- **Standard**: Synopsys DWapb_i2c v2.02

### Base Addresses

| Instance | Base Address |
|----------|--------------|
| i2c0 | `0x40070000` |
| i2c1 | `0x40074000` |
| i2c2 | `0x40078000` |
| i2c3 | `0x4007c000` |
| i2c4 | `0x40080000` |
| i2c5 | `0x40084000` |
| i2c6 | `0x40088000` |

### I2C Features
- **Modes**: Standard (100 kHz), Fast (400 kHz), Fast+ (1 MHz)
- **Address**: 7-bit addressing in master mode
- **TX FIFO**: 32 entries
- **RX FIFO**: 32 entries
- **DMA**: Supported (i2c0-i2c5 only, i2c6 no DMA)

### Key I2C Registers

| Offset | Register | Description |
|--------|----------|-------------|
| `0x00` | `CON` | I2C control |
| `0x04` | `TAR` | Target address |
| `0x08` | `SAR` | Slave address |
| `0x10` | `DATA_CMD` | Data buffer and command register |
| `0x14` | `SS_SCL_HCNT` | Standard speed SCL high count |
| `0x18` | `SS_SCL_LCNT` | Standard speed SCL low count |
| `0x1c` | `FS_SCL_HCNT` | Fast speed SCL high count |
| `0x20` | `FS_SCL_LCNT` | Fast speed SCL low count |
| `0x2c` | `INTR_STAT` | Interrupt status |
| `0x30` | `INTR_MASK` | Interrupt mask |
| `0x70` | `STATUS` | I2C status |
| `0x48` | `RXFLR` | RX FIFO level |
| `0x4c` | `TXFLR` | TX FIFO level |

---

## 11. Best Practices

### GPIO Configuration
1. **Always configure pad settings** (drive strength, slew rate, pulls) before enabling function
2. **Use atomic register access** to avoid PCIe latency in critical paths
3. **Enable Schmitt trigger** for noisy input signals
4. **Debounce GPIO interrupts** in software or use hardware debounce filter

### UART Usage
1. **Configure GPIO pins** (function select + pad control) before UART initialization
2. **Wait for TX FIFO** before writing to avoid data loss
3. **Handle UART errors** (framing, parity, overrun) in interrupt handler
4. **Use DMA** for high-throughput UART transfers (>115200 baud)

### SPI/I2C Usage
1. **Configure correct clock polarity and phase** (CPOL/CPHA for SPI)
2. **Set appropriate baud rate** based on peripheral requirements
3. **Check FIFO levels** before read/write operations
4. **Enable pull-ups** for I2C (typically 2.2kΩ to 4.7kΩ external resistors)

### PCIe Latency Optimization
1. **Disable ASPM** for time-critical GPIO polling (<10μs intervals)
2. **Use atomic register access** (SET/CLEAR/XOR) to avoid read-modify-write
3. **Batch PCIe writes** using posted transactions for better throughput
4. **Use memory barriers** to enforce write ordering

---

## 12. Common Pitfalls

### GPIO
- **Forgetting to configure pads**: Function select alone is insufficient; pad control required
- **Not handling interrupt clearing**: Edge interrupts must be explicitly cleared via `IRQRESET`
- **Mixing pull-up and pull-down**: Enable only one at a time

### UART
- **Wrong baud rate calculation**: Ensure correct UART clock frequency (typically 48 MHz)
- **TX FIFO overflow**: Always check `TXFF` flag before writing
- **Missing line control setup**: Data bits, stop bits, parity must match peripheral

### SPI/I2C
- **Incorrect clock speed**: Too fast can cause data corruption; too slow wastes time
- **Missing pull-ups for I2C**: I2C requires external pull-up resistors (2.2kΩ-4.7kΩ)
- **Wrong SPI mode**: CPOL/CPHA must match slave device

---

**Document Version**: 1.0  
**Generated**: January 2026  
**Target**: Bachelor Thesis - GPIO and Peripheral Driver Development
