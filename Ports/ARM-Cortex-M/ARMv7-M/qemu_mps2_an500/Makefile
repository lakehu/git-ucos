# =====================================================================================================
#  uC/OS-II V2.93.01  -  QEMU mps2-an500 (ARM Cortex-M7) bring-up
#
#  Build model: ucos_ii.c is the OS_MASTER_FILE - it #includes os_core/flag/mbox/mem/mutex/q/sem/
#  task/time/tmr.c, so ONLY ucos_ii.c is compiled (never the individual os_*.c). os_dbg_r.c is
#  compiled separately. The port's GNU/os_dbg.c is deliberately NOT built (it would duplicate OSDebugEn).
# =====================================================================================================

# This demo now lives INSIDE the port directory:
#   Ports/ARM-Cortex-M/ARMv7-M/qemu_mps2_an500/
# so the port is the PARENT, and the kernel root is 4 levels up.
UCOS_ROOT := ../../../..
SRC       := $(UCOS_ROOT)/Source
PORT      := ..
PORT_GNU  := ../GNU

CROSS     := $(HOME)/opt/gcc-arm-none-eabi-9-2019-q4-major/bin/arm-none-eabi-
CC        := $(CROSS)gcc
SIZE      := $(CROSS)size

QEMU      := $(HOME)/usr/bin/qemu-system-arm
QEMUFLAGS := -M mps2-an500 -cpu cortex-m7 -nographic -monitor none -serial null \
             -semihosting-config enable=on,target=native

BUILD     := build
TARGET    := $(BUILD)/ucos_qemu.elf

CPUFLAGS  := -mcpu=cortex-m7 -mthumb -mfloat-abi=soft
INCLUDES  := -I. -I$(SRC) -I$(PORT_GNU) -I$(PORT)
CFLAGS    := $(CPUFLAGS) $(INCLUDES) -O0 -g3 -ffunction-sections -fdata-sections \
             -ffreestanding -Wall -Wno-unused-function
ASFLAGS   := $(CPUFLAGS) $(INCLUDES) -g3
LDFLAGS   := $(CPUFLAGS) -nostartfiles -T mps2_an500.ld -Wl,--gc-sections \
             -Wl,-Map,$(BUILD)/ucos_qemu.map --specs=nosys.specs

OBJ := \
  $(BUILD)/ucos_ii.o \
  $(BUILD)/os_dbg_r.o \
  $(BUILD)/os_cpu_c.o \
  $(BUILD)/os_cpu_a.o \
  $(BUILD)/app_hooks.o \
  $(BUILD)/startup.o \
  $(BUILD)/semihost.o \
  $(BUILD)/test_app.o

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ucos_ii.o:  $(SRC)/ucos_ii.c        | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_dbg_r.o: $(SRC)/os_dbg_r.c       | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_cpu_c.o: $(PORT)/os_cpu_c.c      | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_cpu_a.o: $(PORT_GNU)/os_cpu_a.S  | $(BUILD) ; $(CC) $(ASFLAGS) -c $< -o $@
$(BUILD)/%.o:        %.c                      | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ) mps2_an500.ld | $(BUILD)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	$(SIZE) $@

run: $(TARGET)
	$(QEMU) $(QEMUFLAGS) -kernel $(TARGET)

test: $(TARGET)
	@echo "=== Running uC/OS-II under QEMU (30s watchdog) ==="
	@timeout 30 $(QEMU) $(QEMUFLAGS) -kernel $(TARGET) 2>&1 | tee $(BUILD)/run.log; \
	 if grep -q "TEST PASSED" $(BUILD)/run.log; then echo "RESULT: PASS"; \
	 else echo "RESULT: FAIL"; exit 1; fi

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
