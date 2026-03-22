PLATFORM ?= qemuvirt

CC = aarch64-none-elf-gcc
AS = aarch64-none-elf-as
LD = aarch64-none-elf-ld
OBJCOPY = aarch64-none-elf-objcopy

CFLAGS = -mcpu=cortex-a72 -ffreestanding -nostdlib -O0 -g -Wall -Iinclude -Itests -Idispatcher
ASFLAGS = -mcpu=cortex-a72

ifeq ($(PLATFORM),qemuvirt)
CFLAGS += -DTARGET_QEMU
LINKER_SCRIPT = linker/linkerqemu.ld
OUTPUT = build/kernel.elf
else
CFLAGS += -DTARGET_RPI5
LINKER_SCRIPT = linker/linkerrpi5.ld
OUTPUT_ELF = build/kernel.elf
OUTPUT = build/kernel8.img
endif

OBJS = build/boot.o build/vector.o build/irq.o build/main.o \
	   build/uart0.o build/ipc.o build/ringbuf.o build/tests.o build/timer_tests.o \
	   build/mmu.o build/sched.o build/scheduler.o build/dispatcher.o

# to skip one line we need to have backslash \

all: $(OUTPUT)

build/boot.o: src/boot.S
	@mkdir -p build
	$(AS) $(ASFLAGS) -c $< -o $@

build/vector.o: src/vector.S
	@mkdir -p build
	$(AS) $(ASFLAGS) -c $< -o $@

# CC means compiler "code" and AC assembler "code"
build/mmu.o: src/mmu.S
	@mkdir -p build
	$(CC) $(ASFLAGS) -c $< -o $@

build/sched.o: src/sched.S
	@mkdir -p build
	$(CC) $(ASFLAGS) -c $< -o $@

build/main.o: src/main.c include/uart/uart0.h include/ipc/ipc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/uart0.o: include/uart/uart0.c include/uart/uart0.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/ipc.o: include/ipc/ipc.c include/ipc/ipc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/ringbuf.o: include/ringbuffer/ringbuf.c include/ringbuffer/ringbuf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/tests.o: tests/trivial/tests.c tests/trivial/tests.h  \
				include/uart/uart0.h include/ipc/ipc.h include/ringbuffer/ringbuf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/timer_tests.o: tests/interrupt/timer_tests.c tests/interrupt/timer_tests.h \
				include/uart/uart0.h include/ipc/ipc.h include/ringbuffer/ringbuf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/scheduler.o: include/scheduler/scheduler.c include/scheduler/scheduler.h \
				include/uart/uart0.h 
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/dispatcher.o: dispatcher/dispatcher.c dispatcher/dispatcher.h \
        	include/uart/uart0.h include/ipc/ipc.h include/ringbuffer/ringbuf.h \
			include/scheduler/scheduler.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/irq.o: include/interrupts/irq.c include/interrupts/irq.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(PLATFORM),qemuvirt)
$(OUTPUT): $(OBJS)
	$(LD) -T $(LINKER_SCRIPT) $(OBJS) -o $@
else
$(OUTPUT): $(OBJS)
	$(LD) -T $(LINKER_SCRIPT) $(OBJS) -o $(OUTPUT_ELF)
	$(OBJCOPY) -O binary $(OUTPUT_ELF) $@
endif

clean:
	rm -rf build

.PHONY: all clean
