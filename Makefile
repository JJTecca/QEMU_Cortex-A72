PLATFORM ?= qemuvirt

CC = aarch64-none-elf-gcc
AS = aarch64-none-elf-as
LD = aarch64-none-elf-ld
OBJCOPY = aarch64-none-elf-objcopy

CFLAGS = -mcpu=cortex-a72 -ffreestanding -nostdlib -O2 -Wall
ASFLAGS = -mcpu=cortex-a72

ifeq ($(PLATFORM),qemuvirt)
    CFLAGS += -DPLATFORM_QEMUVIRT
    LINKER_SCRIPT = linker/linkerqemu.ld
    OUTPUT = build/kernel.elf
else
    LINKER_SCRIPT = linker/linkerrpi5.ld
    OUTPUT_ELF = build/kernel.elf
    OUTPUT = build/kernel8.img
endif

all: $(OUTPUT)

build/boot.o: src/boot.S
	@mkdir -p build
	$(AS) $(ASFLAGS) -c $< -o $@

build/main.o: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(PLATFORM),qemuvirt)
$(OUTPUT): build/boot.o build/main.o
	$(LD) -T $(LINKER_SCRIPT) build/boot.o build/main.o -o $@
else
$(OUTPUT): build/boot.o build/main.o
	$(LD) -T $(LINKER_SCRIPT) build/boot.o build/main.o -o $(OUTPUT_ELF)
	$(OBJCOPY) -O binary $(OUTPUT_ELF) $@
endif

clean:
	rm -rf build

.PHONY: all clean
