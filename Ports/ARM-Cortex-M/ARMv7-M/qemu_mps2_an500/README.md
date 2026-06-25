# uC/OS-II V2.93.01 on QEMU mps2-an500 (ARM Cortex-M7)

Self-contained bring-up of the uC/OS-II ARMv7-M GNU port on QEMU's mps2-an500
machine (Cortex-M7), soft-float, output via ARM semihosting. Self-terminating.

This directory lives inside the port it exercises:
`Ports/ARM-Cortex-M/ARMv7-M/qemu_mps2_an500/`. It adds NOTHING to the upstream
tree except itself. The kernel under ../../../../Source and the port in its
parent dir (..) + ../GNU are used unmodified.

---------------------------------------------------------------------------
QUICK START
---------------------------------------------------------------------------
    make            # build build/ucos_qemu.elf
    make run        # run under QEMU (Ctrl-A X to quit if it didn't self-exit)
    make test       # run with a 30s watchdog, grep for "TEST PASSED"
    make clean

Toolchain : ~/opt/gcc-arm-none-eabi-9-2019-q4-major
QEMU      : ~/usr/bin/qemu-system-arm  (7.1.0), machine mps2-an500

---------------------------------------------------------------------------
WHY THESE CHOICES (the non-obvious decisions)
---------------------------------------------------------------------------
1. BUILD MODEL - master file.
   ucos_ii.c is the OS_MASTER_FILE: it #includes os_core/flag/mbox/mem/mutex/
   q/sem/task/time/tmr.c. So ONLY ucos_ii.c is compiled; the individual os_*.c
   are NOT in the object list. Compiling them too = duplicate symbols.

2. DUAL os_dbg - pick ONE.
   Both ../Source/os_dbg_r.c AND ../Ports/.../GNU/os_dbg.c define OSDebugEn and
   the full debug-constant table. Building both = multiple-definition link
   errors. We build Source/os_dbg_r.c and deliberately exclude the port's
   os_dbg.c. (os_dbg.c is an alternative provided for toolchains that lack a
   separate Source/ copy.)

3. NO uC/CPU / uC/LIB modules.
   This port references only native types (INT32U, OS_STK...) plus two macros:
   CPU_CFG_NVIC_PRIO_BITS and CPU_CFG_KA_IPL_BOUNDARY. We define them in
   app_cfg.h, so the heavyweight uC/CPU and uC/LIB modules are not needed.

4. SOFT-FLOAT first.
   -mfloat-abi=soft sets __SOFTFP__, which the port keys on (OS_CPU_ARM_FP_EN
   becomes 0). The initial task stack frame then uses EXEC_RETURN 0xFFFFFFFD
   (no FPU state). Validate the scheduler before adding the FPU variable.
   To go hard-float later: -mfloat-abi=hard -mfpu=fpv5-d16, and enable the FPU
   (CPACR CP10/CP11) in startup before OSStart().

5. MEMORY MAP - read from the live model, never guessed.
   `qemu-system-arm -M mps2-an500` 'info mtree':
       ssram1   @ 0x00000000  4 MiB  -> code + vectors (reset reads SP/PC here)
       ssram2/3 @ 0x20000000  4 MiB  -> data/bss/heap/stack
   A wrong base address is the #1 cause of a silent QEMU hang. mps2_an500.ld
   encodes exactly this.

6. VECTOR TABLE.
   startup.c places PendSV on entry 14 and SysTick on entry 15, pointing at the
   port's OS_CPU_PendSVHandler / OS_CPU_SysTickHandler - mandatory for this port.
   Fault handlers emit a semihosting string and SYS_EXIT, so a fault is a
   visible message, not a silent lockup.

7. NVIC PRIORITY BITS = 4.
   The S32G2 Cortex-M7 implements 4 priority bits; QEMU tolerates 4. Under-
   claiming bits is always safe; over-claiming silently breaks BASEPRI masking.

---------------------------------------------------------------------------
PORTABILITY ACROSS QEMU MACHINES (why this stays named ...an500)
---------------------------------------------------------------------------
Tested the UNMODIFIED build/ucos_qemu.elf on several QEMU 7.1.0 machines to
check whether this demo could be generalised to a board-agnostic "QEMU" name.

   machine          core (as run)        result
   --------------   ------------------   ----------------------------------
   mps2-an500       Cortex-M7 (default)  PASS   (baseline)
   mps2-an385       Cortex-M3 (default)  PASS
   mps2-an386       Cortex-M4 (default)  PASS
   lm3s6965evb      Cortex-M3            HANG   (watchdog timeout, no output)
   netduinoplus2    Cortex-M4 (STM32F4)  FAULT  "Lockup: can't escalate to
                                                 HardFault" at reset
   mps2-an385/386   -cpu cortex-m7       REJECT "This board can only be used
                                                 with cortex-m3/m4-arm-cpu"

ROOT CAUSE / WHAT THIS MEANS
 - mps2-an500 is the ONLY Cortex-M7 machine in QEMU 7.1.0. There is no other
   "QEMU M7 board" to retarget to.
 - What actually makes the ELF portable is the MPS2 MEMORY MAP (code @
   0x00000000, RAM @ 0x20000000), NOT the M7 core. AN385/AN386/AN500 are
   AN3/AN4/AN5 FPGA images of the SAME MPS2 motherboard, so they share that
   map. The ARMv7-M port uses only the common-denominator features (soft-
   float, BASEPRI, PendSV/SysTick) that M3/M4/M7 all implement -> it runs
   unmodified across the whole MPS2-AN family.
 - Boards with a DIFFERENT map die at reset: STM32F4 (netduinoplus2) puts
   flash at 0x08000000 and Stellaris (lm3s6965evb) has a different layout, so
   the CPU reads a garbage SP/PC from 0x0 -> immediate lockup / silent hang.
 - QEMU pins each machine to one fixed core; you cannot force -cpu cortex-m7
   onto an385/an386 (it is rejected before reset).

CONCLUSION: the directory keeps the name qemu_mps2_an500. A bare "QEMU" name
would be wrong -- the binary is NOT board-agnostic, only MPS2-map-agnostic.
an500 is the precise board the linker script targets, the only M7 machine
available, and the closest analog to the real S32G2 M7 target. (If family
portability ever needs advertising, qemu_mps2 is the only defensible
generalisation -- but it is a net loss of precision vs. the verified AN500/M7
run and is not adopted here.)

---------------------------------------------------------------------------
HOW THE OUTPUT PROVES CORRECTNESS (it is the timing, not the "PASSED" string)
---------------------------------------------------------------------------
   TaskA OSTimeDly(2) -> runs at OSTime = 0,2,4,6,8,10   (every 2 ticks)
   TaskB OSTimeDly(3) -> runs at OSTime = 0,3,6,9         (every 3 ticks)

   - OSTime advancing            => SysTick ISR + OSTimeTick working
   - delays land on exact ticks  => tick wheel + OSTimeDly correct
   - both tasks resume blocked   => PendSV saves/restores full context
   - at t=6 A prints before B    => priority scheduling (A=4 > B=5) correct
   - A exits cleanly at t=10     => semihosting SYS_EXIT works

---------------------------------------------------------------------------
FILES
---------------------------------------------------------------------------
   --- copied verbatim from the upstream template (NOT written by us) ---
   os_cfg.h        copy of Cfg/Template/os_cfg.h (kernel feature switches)
   app_hooks.c     copy of Cfg/Template/app_hooks.c (App_*Hook stubs)

   --- adapted from the template (template base + our additions) ---
   app_cfg.h       Cfg/Template/app_cfg.h + the two CPU_CFG_* macros the port
                   requires (NVIC_PRIO_BITS, KA_IPL_BOUNDARY) + CPU_FREQ_HZ +
                   larger task stacks

   --- written by us from scratch (no template exists) ---
   startup.c       vector table + Reset_Handler (.data/.bss init) + fault traps
   mps2_an500.ld   linker script (memory map above)
   semihost.c/.h   ARM semihosting (BKPT 0xAB): write0, print_u32, exit
   test_app.c      OSInit/OSTaskCreate/OSStart + TaskA/TaskB demo
   Makefile        master-file build model; all/run/test/clean
