# =====================================================================================================
#  uC/OS-II V2.93.01  -  NXP S32G274A (ARM Cortex-M7) real-hardware bring-up
#
#  Build model: ucos_ii.c is the OS_MASTER_FILE - it #includes os_core/flag/mbox/mem/mutex/q/sem/
#  task/time/tmr.c, so ONLY ucos_ii.c is compiled (never the individual os_*.c). os_dbg_r.c is
#  compiled separately. The port's GNU/os_dbg.c is deliberately NOT built (it would duplicate OSDebugEn).
#
#  Derived from the qemu_mps2_an500 sibling Makefile. Deltas for real silicon:
#    - semihost.o            -> s32g_linflexd.o   (real LINFlexD0 UART instead of ARM semihosting)
#    - mps2_an500.ld         -> s32g.ld           (unified RAM @ 0x34000000, no FLASH/RAM split)
#    - QEMU run/test targets -> T32 'load' target (no qemu-system-arm; load over JTAG via Lauterbach)
#  There is NO `make run` on real HW: the image is loaded and started by debug_t32/s32g274_m7.cmm.
# =====================================================================================================

# This demo lives INSIDE the port directory:
#   Ports/ARM-Cortex-M/ARMv7-M/S32G/
# so the port is the PARENT, and the kernel root is 4 levels up.
UCOS_ROOT := ../../../..
SRC       := $(UCOS_ROOT)/Source
PORT      := ..
PORT_GNU  := ../GNU

CROSS     := $(HOME)/opt/gcc-arm-none-eabi-9-2019-q4-major/bin/arm-none-eabi-
CC        := $(CROSS)gcc
SIZE      := $(CROSS)size
OBJCOPY   := $(CROSS)objcopy
READELF   := $(CROSS)readelf

BUILD     := build
TARGET    := $(BUILD)/ucos_s32g.elf

# Cortex-M7, software floating point (matches the qemu sibling and the uC/OS port's OS_CPU_ARM_FP_EN=0).
CPUFLAGS  := -mcpu=cortex-m7 -mthumb -mfloat-abi=soft
INCLUDES  := -I. -I$(SRC) -I$(PORT_GNU) -I$(PORT)
CFLAGS    := $(CPUFLAGS) $(INCLUDES) -O0 -g3 -ffunction-sections -fdata-sections \
             -ffreestanding -Wall -Wno-unused-function
ASFLAGS   := $(CPUFLAGS) $(INCLUDES) -g3
LDFLAGS   := $(CPUFLAGS) -nostartfiles -T s32g.ld -Wl,--gc-sections \
             -Wl,-Map,$(BUILD)/ucos_s32g.map --specs=nosys.specs

OBJ := \
  $(BUILD)/ucos_ii.o \
  $(BUILD)/os_dbg_r.o \
  $(BUILD)/os_cpu_c.o \
  $(BUILD)/os_cpu_a.o \
  $(BUILD)/app_hooks.o \
  $(BUILD)/startup.o \
  $(BUILD)/s32g_linflexd.o \
  $(BUILD)/test_app.o

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ucos_ii.o:       $(SRC)/ucos_ii.c        | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_dbg_r.o:      $(SRC)/os_dbg_r.c       | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_cpu_c.o:      $(PORT)/os_cpu_c.c      | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/os_cpu_a.o:      $(PORT_GNU)/os_cpu_a.S  | $(BUILD) ; $(CC) $(ASFLAGS) -c $< -o $@
$(BUILD)/%.o:             %.c                      | $(BUILD) ; $(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ) s32g.ld | $(BUILD)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	$(SIZE) $@

# Sanity check: confirm the image is linked at the S32G2 unified RAM base 0x34000000 and that the
# NXP boot script's symbol VTABLE resolves to that same address.
verify: $(TARGET)
	@echo "=== ELF entry + load address (expect 0x34000000 region) ==="
	@$(READELF) -h $(TARGET) | grep -E 'Entry point'
	@$(READELF) -l $(TARGET) | grep -E 'LOAD' | head -1
	@echo "=== VTABLE / Reset_Handler / g_pfnVectors symbols ==="
	@$(READELF) -s $(TARGET) | grep -E 'VTABLE|Reset_Handler|g_pfnVectors' || true

# Load + run on real hardware via Lauterbach TRACE32. Requires t32marm in PATH and a powered probe.
# The .cmm loads $(TARGET), programs MC_ME boot address from symbol VTABLE, and `go main`.
T32       ?= t32marm
CMM       := debug_t32/s32g274_m7.cmm
load: $(TARGET)
	@echo "=== Launching TRACE32 to load $(TARGET) on S32G274A M7 ==="
	$(T32) -s $(CMM)

clean:
	rm -rf $(BUILD)

.PHONY: all verify load clean
