# uC/OS-II V2.93.01 — NXP S32G274A (ARM Cortex-M7) real-hardware port

This directory is the **real-silicon** bring-up of the ARMv7-M uC/OS-II port on the
NXP **S32G274A** Cortex-M7 core (S32G2 family). It is cloned in structure from the
sibling `../qemu_mps2_an500` port, with everything QEMU/semihosting-specific replaced
by real S32G2 hardware drivers and a Lauterbach TRACE32 boot/debug flow.

The CPU port core (`../os_cpu.h`, `../os_cpu_c.c`, `../GNU/os_cpu_a.S`) is shared and
**unchanged** — only the board/demo layer differs.


## Files in this directory

    Makefile              GCC build; targets: all / verify / load / clean
    s32g.ld               linker script — unified RAM @ 0x34000000 (no FLASH/RAM split)
    startup.c             vector table (g_pfnVectors) + Reset_Handler; VTABLE alias for the cmm
    s32g_linflexd.c/.h    LINFlexD0 UART driver, 115200 8N1, BUFFER mode (verbatim from FreeRTOS demo)
    os_cfg.h              uC/OS-II config (OS_TICKS_PER_SEC=100, OS_TASK_NAME_EN=1, OS_DEBUG_EN=1)
    app_cfg.h             CPU_FREQ_HZ = 24 MHz (core clock that feeds SysTick)
    app_hooks.c           uC/OS-II application hooks (verbatim from qemu sibling)
    test_app.c            bring-up test: main + AppStartTask + TaskA/TaskB
    debug_t32/
      s32g274_m7.cmm      NXP boot script: clock/PLL init, load ELF, set boot addr, go main
      dump_linclk.cmm     helper: dump LIN clock tree registers
      dump_linflexd0.cmm  helper: dump LINFlexD0 registers
    build/                build output (ucos_s32g.elf, .map, *.o) — created by `make`


## What this port replaces vs the qemu_mps2_an500 sibling

| Concern        | qemu_mps2_an500            | S32G (this port)                                  |
|----------------|----------------------------|---------------------------------------------------|
| Console output | ARM semihosting (semihost) | LINFlexD0 UART @ 115200 8N1 (s32g_linflexd.c)     |
| Linker map     | mps2_an500.ld (FLASH+RAM)  | s32g.ld — unified RAM @ 0x34000000                 |
| Run / load     | `qemu-system-arm` run/test | TRACE32 `load` target over JTAG (debug_t32/*.cmm) |
| End-of-test    | semihost sh_exit()         | g_TestResult = 0x5A (watched over UART / in T32)  |

There is **no `make run`** on real hardware: the image is loaded and started by
`debug_t32/s32g274_m7.cmm` on a Lauterbach probe.


## Clock / tick facts (do not confuse these three clocks)

- **Core clock = 24 MHz** → `CPU_FREQ_HZ` in app_cfg.h. This is the value passed to
  `OS_CPU_SysTickInitFreq(CPU_FREQ_HZ)`. Sourced from the FreeRTOS S32G2 demo's
  `configCPU_CLOCK_HZ = 24000000UL`. SysTick uses `CLKSOURCE=1` (full core clock,
  not core/8), so the reload value is `CPU_FREQ_HZ / OS_TICKS_PER_SEC`.
- **FIRC = 48 MHz** — the free-running internal RC oscillator. NOT the SysTick source.
- **LIN baud clock = PERIPH_PLL_PHI3 = 125 MHz** — feeds the LINFlexD0 baud divider
  (LINIBRR=67 / LINFBRR=13 → 115200). NOT the core clock.

Tick rate: `OS_TICKS_PER_SEC = 100` → 10 ms tick (`OSTimeDly(N)` = N×10 ms).


## Build

    make                # builds build/ucos_s32g.elf
    make verify         # prints entry/load address + VTABLE/Reset_Handler symbols
    make clean

Toolchain (hard-coded in Makefile): `~/opt/gcc-arm-none-eabi-9-2019-q4-major`.
CPU flags: `-mcpu=cortex-m7 -mthumb -mfloat-abi=soft` (soft float matches the port's
`OS_CPU_ARM_FP_EN = 0`).

**Build model:** `ucos_ii.c` is the OS_MASTER_FILE — it `#include`s all the individual
`os_*.c` kernel files, so ONLY `ucos_ii.c` is compiled (never os_core.c/os_task.c/… on
their own). `os_dbg_r.c` (the reduced debug table) is compiled separately. The port's
`GNU/os_dbg.c` is deliberately **NOT** built — it would duplicate `OSDebugEn`.

Verified output (entry/load unchanged at the S32G2 RAM base):

    text=11192  data=4  bss=21564
    Entry point address: 0x34002429
    LOAD  0x34000000 ... RWE          # image linked at unified-RAM base 0x34000000


## Load + run on hardware (Lauterbach TRACE32)

    make load                         # runs: t32marm -s debug_t32/s32g274_m7.cmm

### PREREQUISITE: run on a VIRGIN board (no boot software on QSPI/eMMC)

This cmm assumes a **bare / virgin S32G2** — it does the FULL bring-up itself
(reset M7, init FXOSC + core PLL + periph PLL, set the M7 boot address, load the
ELF into RAM, `go main`). It does **NOT** coexist with a board that is already
booting its own image from flash.

If the QSPI/eMMC still holds a BootROM-launched image (an existing M-core app,
or U-Boot/ATF that brings up the Cortex-A53 cluster), that software runs out of
reset and will fight this script — re-clocking PLLs, re-claiming LINFlexD0,
parking/holding cores, or trapping the M7 — giving intermittent or wrong-looking
results that are NOT a bug in the uC/OS port.

**Recommendation: erase QSPI/eMMC completely before running this test**, so the
M7 comes up clean and the cmm is the only thing touching clocks, the boot
address, and the UART. After uC/OS is validated you can re-flash your normal
boot image. (This matches how the FreeRTOS S32G2 demo cmm is intended to run:
debugger-driven bring-up from reset, not on top of a running boot stack.)

The cmm script:
1. resets/initializes the M7 core and the S32G2 clock tree (FXOSC, core PLL, periph PLL),
2. `Data.Load.Elf ..\build\ucos_s32g.elf /GLOBTYPES`,
3. programs the M7 boot address from symbol **VTABLE** (an alias onto `g_pfnVectors`,
   added in startup.c so the unmodified NXP script resolves it),
4. `Register.Set PC Reset_Handler`,
5. loads the uC/OS-II awareness + menu (see below),
6. `go main`.

Expected UART output (115200 8N1 on LINFlexD0):

    =========================================================
     uC/OS-II V2.93.01  on  NXP S32G274A (Cortex-M7)
     UART: LINFLEXD0 @ 115200 8N1
     Starting kernel...
    =========================================================
     OSStart() OK -> startup task is running
    [A] iter=0  OSTime=...
        [B] iter=0  OSTime=...
     ...
    *** uC/OS-II CONTEXT SWITCH + SYSTICK VERIFIED: TEST PASSED (g_TestResult=0x5A) ***

PASS criterion: `g_TestResult` reaches `0x5A` (TaskA ran all iterations, preemption +
SysTick proven). Watch it over UART or in T32 (`Var.View g_TestResult`).


## TRACE32 uC/OS-II awareness (task names)

The cmm loads the kernel awareness so the OS menu / task list works:

    TASK.CONFIG    ~~/demo/arm/kernel/ucos-ii/ucos.t32    ; uC/OS-II Awareness
    MENU.ReProgram ~~/demo/arm/kernel/ucos-ii/ucos.men    ; uC/OS-II menu

**Task names require OSTaskNameSet() — OSTaskCreate() does NOT set a name.**
The awareness reads each task's name from `OS_TCB.OSTCBTaskName`. `OSTaskCreate()`
initializes that field to the literal `"?"` (os_task.c); the *only* function that writes
a real name is `OSTaskNameSet()`. With `OS_TASK_NAME_EN = 1` (os_cfg.h) the field exists
and the reduced debug table publishes `OSTaskNameEn = 1` to T32 — which is why the menu
populates — but tasks show `"?"` until named. test_app.c therefore names each task right
after creating it:

    OSTaskCreate(AppStartTask, ...);
    OSTaskNameSet(APP_START_PRIO, (INT8U *)"AppStartTask", &err);
    ...
    OSTaskCreate(TaskA, ...);  OSTaskCreate(TaskB, ...);
    OSTaskNameSet(TASK_A_PRIO, (INT8U *)"TaskA", &err);
    OSTaskNameSet(TASK_B_PRIO, (INT8U *)"TaskB", &err);

`OSTaskNameSet()` does not require `OSRunning`, only that the task already exists in
`OSTCBPrioTbl[prio]` and that it is not called from an ISR.


## Known leftover from the FreeRTOS demo (not yet changed)

`debug_t32/s32g274_m7.cmm` line ~344 still has:

    v.w ui32_ms_cnt task_1 task_2

`ui32_ms_cnt`, `task_1`, `task_2` are **FreeRTOS demo symbols** that do not exist in the
uC/OS-II ELF, so this `Var.Watch` line cannot resolve them. It is left untouched on
purpose (smallest-change / preserve-NXP-script-intent). If you want a clean watch window,
replace it with uC/OS symbols, e.g.:

    v.w g_TestResult OSTime OSCtxSwCtr

(or simply delete the line). Decide before committing.


## Provenance / attribution

- `s32g_linflexd.c` / `.h` and all three `debug_t32/*.cmm` scripts are **verbatim** from
  the NXP FreeRTOS S32G2 demo (`/DATA2/uie75906/RTOS/FreeRTOS/FreeRTOS/Demo/CORTEX_M7_S32G2_GCC`),
  `diff`-clean against upstream, except the single repath in s32g274_m7.cmm line 23
  (`..\Output\image.elf` → `..\build\ucos_s32g.elf`) which is annotated inline.
- `app_hooks.c`, `os_cfg.h` are cloned from the `qemu_mps2_an500` sibling.
- `startup.c`, `s32g.ld`, `app_cfg.h`, `test_app.c`, `Makefile` are authored for this port.
- The shared CPU port (`../os_cpu*.{h,c}`, `../GNU/os_cpu_a.S`) is unchanged uC/OS-II V2.93.01.
