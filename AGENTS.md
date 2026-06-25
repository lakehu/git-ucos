# AGENTS.md — uC/OS-II Linux port (v2.52)

Runs the unmodified uC/OS-II v2.52 kernel as a normal Linux user process.
The hardware-specific layer (timer IRQ, context switch, cli/sti) is re-implemented
with POSIX **signals + ucontext** instead of assembly. No kernel module, no root.

## Build

    make            # builds EX1/EX2/EX3 + EX4 (FP) + EX5 -> <dir>/bin.exec
    make clean

- Compiler: `gcc -g -m32 -x c++`. The port is **32-bit i386 only** (see Pitfalls).
- On x86_64 Ubuntu 20 this links because multilib is present (libc6:i386 / gcc-multilib).
- Run an example: `./EX1_x86L/bin.exec` (Ctrl-C to quit).

## Layout

    Port/        the port (replaces the official Ix86L/BC45 OS_CPU_*.{C,ASM})
      os_cpu.h     data types, OS_CPU_SR=sigset_t, ENTER/EXIT_CRITICAL, OS_TASK_SW
      os_cpu_c.c   stack init, ctx switch, signal handlers, timer setup
      utils.{c,h}  console / helpers
    SOURCE/      stock uC/OS-II v2.52 kernel (unchanged, portable C)
    EX1..3,EX5/  apps; each has its own os_cfg.h + TEST.C + bin.exec
    EX4_x86L.FP/ floating-point app; gcc/SOURCE/ holds the Linux port,
                 BC45/ keeps the untouched Borland DOS original. Builds with -lm.
    Makefile     gcc -m32, one bin.exec per app dir; delegates to each EXn/Makefile

## Key functions (Port/os_cpu_c.c, os_cpu.h)

    OSTaskStkInit()        build task ucontext (getcontext+makecontext), return ptr as OSTCBStkPtr
    OSTaskStkInit_FPE_x86() [Port/utils.c] runs `fninit` to clear the x87 FPU; an FP task must
                           call it at entry (see FP pitfall). No-op for the non-FP examples.
    OSStartHighRdy()       start multitasking: setcontext() into first task, never returns
    OSCtxSw()              voluntary switch: setcontext(OSTCBHighRdy ucontext)
    OSIntCtxSw()           ISR-level switch; == OSCtxSw() (no separate ISR frame)
    OSCtxSwSigHandler()    SIGUSR1 handler: save uc into TCB, call OSCtxSw()
    OSTickISR()            tick body: OSIntEnter/OSTimeTick/OSIntExit/OSIntCtxSw
    OSTimeTickSigHandler() SIGALRM handler: save uc, run OSTickISR (the simulated timer ISR)
    linuxInit()            register SIGALRM+SIGUSR1 handlers (from OSInitHookBegin)
    linuxInitInt()         ualarm() arm periodic SIGALRM (call from first task)
    OS_TASK_SW()  [macro]  kill(getpid(),SIGUSR1) — software-INT trigger

## Core idea — signal as timer for schedule simulation

A bare-metal RTOS needs three CPU features the official x86 port gets from hardware.
This port simulates each with a userspace-legal POSIX mechanism. A **signal is the
userspace equivalent of an interrupt**: the kernel asynchronously preempts the thread,
saves its full register set, and vectors to a handler you registered — exactly an ISR.

| uC/OS-II needs        | Bare-metal x86 (official)        | Linux port (this repo)                         |
|-----------------------|----------------------------------|------------------------------------------------|
| Periodic tick         | 8259 PIC timer IRQ (~200 Hz)     | `ualarm()` → periodic **SIGALRM**              |
| Tick ISR              | `OSTickISR` in OS_CPU_A.ASM      | `OSTimeTickSigHandler` (SIGALRM handler)       |
| Save preempted regs   | `PUSHA/PUSH ES/DS` to SS:SP      | kernel hands `ucontext_t *uc` to the handler   |
| Context switch        | swap SS:SP, `POPA/IRET`          | `setcontext(uc)` (ucontext register restore)   |
| Trigger ctx switch    | software `INT`                   | `kill(getpid(), SIGUSR1)` → SIGUSR1 handler    |
| OS_ENTER_CRITICAL     | `CLI` (mask IRQ)                 | `sigprocmask` block SIGALRM+SIGUSR1            |
| OS_EXIT_CRITICAL      | `STI` / restore flags            | `sigprocmask` restore saved mask               |
| OSTCBStkPtr holds     | real SS:SP (regs PUSHA'd there)  | **pointer to a `ucontext_t`**                  |
| End-of-ISR housekeep  | EOI to PIC (0x20), DOS chain     | none — kernel handles signal delivery          |

### Tick flow (the simulated timer interrupt)
1. First task calls `linuxInitInt()` → `ualarm(1e6/OS_TICKS_PER_SEC, …)` arms periodic SIGALRM.
2. SIGALRM → `OSTimeTickSigHandler(signo, info, uc)` (registered with `SA_SIGINFO`).
   Kernel already saved the interrupted task's registers into `uc`.
3. Handler stores `OSTCBCur->OSTCBStkPtr = uc`, then runs the stock path:
   `OSTickISR` → `OSIntEnter` → `OSTimeTick` → `OSIntExit` → `OSIntCtxSw`.
4. If a higher-prio task is ready, `setcontext()` restores ITS saved ucontext = "iret into another task".

### Context-switch flow (voluntary / software interrupt)
- `OS_TASK_SW()` = `kill(getpid(), SIGUSR1)` → `OSCtxSwSigHandler` saves `uc` into the
  current TCB, then `OSCtxSw()` does `setcontext(OSTCBHighRdy->OSTCBStkPtr)`.
- `OSIntCtxSw()` == `OSCtxSw()` here: no separate ISR stack frame in userspace.

### Stack init
- `OSTaskStkInit()` uses `getcontext`+`makecontext` to build a runnable context whose
  entry is the task fn, copies that `ucontext_t` onto the task's stack memory, and
  returns a pointer to it. So `OSTCBStkPtr` is a **ucontext pointer**, not a CPU SP.

### Critical sections (cli/sti simulation)
- `OS_CPU_SR` is a `sigset_t`; `OS_CRITICAL_METHOD 3` saves/restores it in `cpu_sr`.
- ENTER blocks {SIGALRM,SIGUSR1}; while blocked the kernel queues SIGALRM —
  "interrupt pending but masked", same as the hardware case. EXIT restores the old mask.

## Differences vs official v2.52 (git-ucos/uCOS-II, Ix86L/BC45)

- SOURCE/ kernel is byte-for-byte stock; ALL differences live in Port/.
- Official port = `OS_CPU_A.ASM` (16-bit real mode, `.186`, LARGE model). This port = **no .asm**, pure C.
- `OS_STK`: official INT16U (16-bit) vs here INT32U (32-bit). `OS_STK_GROWTH=1` both.
- Tick: official drives the 8259 + chains DOS every 11th tick (`OSTickDOSCtr`, `INT 81H`, EOI 0x20);
  here `ualarm`/SIGALRM only — no PIC, no DOS, no EOI.
- Register save: official explicit `PUSHA/PUSH ES/DS`; here implicit via kernel-delivered ucontext.
- An FP variant exists upstream (Ix86L-FP, `OSTaskStkInit_FPE_x86`); ported here as EX4_x86L.FP
  (see FP pitfall for the two Linux-port-specific fixes it needed).

## Pitfalls

- **32-bit only.** Context code hard-codes i386: `gregs[REG_EIP]`, `REG_EBP`, and pointer
  math cast through `int` (`(INT32U*)((int)ptos - …)`). Native x86_64 would truncate
  pointers and has `REG_RIP`, not `REG_EIP`. Hence `-m32`; needs i386 multilib to build/run.
- **setcontext is not atomic on i386** (userspace register restore, not a syscall). If SIGALRM
  fires inside it the context is half-restored. `OSTimeTickSigHandler` guards by checking
  whether the interrupted EIP is inside `setcontext` (≈110 bytes) and aborting that tick.
  This is a race mitigation, not a fix (author \todo: use `sigreturn`).
- **fpregs==0 guard**: handler bails if `uc_mcontext.fpregs==0` (Linux i386 quirk mid-restore).
- **Built as C++ (`-x c++`), not C.** Files are lowercase `.c` but compiled as C++ because
  the i386 `REG_EIP`/`REG_EBP` macros are GNU-guarded: g++ auto-defines `_GNU_SOURCE`, gcc-as-C
  does not. Dropping `-x c++` breaks the build (`REG_EIP undeclared`) unless you add `-D_GNU_SOURCE`.
  Apps (`TEST.C`) and kernel must use the same frontend or C/C++ name mangling fails to link.
- Timer precision is best-effort (subject to Linux scheduling), fine for the demos.
- `linuxInit()` (handler setup) runs from `OSInitHookBegin`; `linuxInitInt()` (arm timer)
  MUST be called by the user from the first task, after OSInit — not before multitasking starts.
- **FP tasks need a clean x87 at entry (EX4).** `OSTaskStkInit()` snapshots the FPU via
  `getcontext()` while building each task's context, so every task inherits `main()`'s *dirty*
  x87 tag word (all 8 regs marked in-use). The first `cos()`/`sin()` then overflows the x87
  stack → **SIGFPE**. glibc `makecontext` does NOT carry a getcontext FPU snapshot on i386, so
  fixing it inside the port is not enough — the FP task must call `OSTaskStkInit_FPE_x86()`
  (`fninit`) as its first statement. Only EX4 hits this (only example doing FP in tasks).
- **DOS stacks are too small for the ucontext port (EX4).** The Linux port runs real host code
  (scheduler, `sprintf`, `sigprocmask`) on each task stack, but the DOS originals size idle/stat
  stacks at 512 `OS_STK` entries (2 KB) → **SIGSEGV** in the stat task. Size stacks from
  `OS_TASK_DEF_STK_SIZE` (2000), not the DOS literal.

## Verify after changes

    make clean && make
    file ./EX1_x86L/bin.exec        # expect: ELF 32-bit LSB, Intel 80386
    ./EX1_x86L/bin.exec             # expect Labrosse banner, rising #Task switch/sec
