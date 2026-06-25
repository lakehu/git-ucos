# AGENTS.md — uC/OS-II Linux port (v2.52)

Runs the unmodified uC/OS-II v2.52 kernel as a normal Linux user process.
The hardware-specific layer (timer IRQ, context switch, cli/sti) is re-implemented
with POSIX **signals + ucontext** instead of assembly. No kernel module, no root.

Refer to https://github.com/ShenChen1/uCOS-II-linux-port.git

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
- **setcontext is not atomic on i386** — see "Signal-handler races" section below
  for full root-cause analysis. Short form: userspace register restore, not a
  syscall; SIGALRM landing inside its ~110-byte window leaves the context
  half-restored. Mitigated by `OSTimeTickSigHandler` aborting any tick whose
  interrupted EIP falls inside `setcontext`. Not a fix (author \todo: use `sigreturn`).
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

## Signal-handler races (Port/os_cpu_c.c)

Symptom: intermittent `*** stack smashing detected ***` (SIGABRT) or SIGSEGV,
most commonly in EX4 (FP), with backtrace
`__stack_chk_fail → sigprocmask → OS_TaskIdle:922`. Pre-fix crash rate on EX4
was ~20 % per 2 s run; post-fix ~5 % (residual race 4).

Root cause is **four** independent issues sharing one symptom — the signal-handling
machinery silently corrupts the interrupted task's stack or the saved register state.
Reproducer: `~/.hermes/skills/embedded/ucos-linux-port-debugging/scripts/stress.sh`.

### Race 1 — saved-ucontext aliasing (FIXED)

Original:
```c
OSTCBCur->OSTCBStkPtr = (OS_STK *)uc;   // uc points INTO the task's own stack
```
The kernel writes the signal frame (ucontext+siginfo+xsave, ~700 B) onto the
preempted task's stack and hands the handler a pointer `uc` into that area.
Storing `uc` in the TCB means "the saved register set" and "the live task
stack" alias each other. The very next signal delivery (or any deep stack
use by the task) overwrites the saved state.

**Fix**: per-priority TCB-owned slot, decoupled from any task stack:
```c
static ucontext_t g_saved_uc[OS_LOWEST_PRIO + 1];
memcpy(&g_saved_uc[OSTCBCur->OSTCBPrio], uc, sizeof(ucontext_t));
OSTCBCur->OSTCBStkPtr = (OS_STK *)&g_saved_uc[OSTCBCur->OSTCBPrio];
```
Applied to both `OSCtxSwSigHandler` and `OSTimeTickSigHandler`. Cost ≈ 23 KB
for 64 priorities.

### Race 2 — empty sa_mask, SIGALRM ⇄ SIGUSR1 nest (FIXED)

Original `linuxInit()` did `sigemptyset(&mask); act.sa_mask = mask;` for both
handlers. SIGUSR1 could land inside the SIGALRM handler (or vice versa);
the inner handler then writes its own ucontext frame onto the task stack on
top of the outer handler's frame. Saw this directly in cores as two stacked
handler frames inside `OSTaskStatStk` before the crash.

**Fix**: each handler masks BOTH signals while it runs:
```c
sigemptyset(&mask);
sigaddset(&mask, SIGALRM);
sigaddset(&mask, SIGUSR1);
act.sa_mask = mask;
```

### Race 3 — kernel writes signal frames on task stack (FIXED)

Even with the aliasing fixed and signals serialized, **the kernel still writes
the ~700 B signal frame onto the task's own stack** on every delivery, because
no alternate stack was installed. When that write lands on top of an SSP canary
that libc placed just before the task got preempted (e.g. inside `sigprocmask`
called from `OS_EXIT_CRITICAL`), the canary changes value → `__stack_chk_fail`
the moment libc returns. This is the dominant cause of the
`OS_TaskIdle:922 → sigprocmask` crash.

The original `linuxInit()` had `// | SA_ONSTACK` commented out — intent was
there but `sigaltstack()` was never wired up.

**Fix**: install a persistent alternate stack and add `SA_ONSTACK`:
```c
static char altstack[SIGSTKSZ];
stack_t ss = { .ss_sp = altstack, .ss_size = SIGSTKSZ, .ss_flags = 0 };
sigaltstack(&ss, NULL);
act.sa_flags = SA_SIGINFO | SA_ONSTACK;
```
All handler-side stack usage (and the kernel's signal-frame push) now goes to
`altstack[]`, off any task stack.

### Race 4 — setcontext non-atomicity (NOT FIXED, residual ~5 %)

The port author flagged this directly in three places in `Port/os_cpu_c.c`:

1. `OSCtxSw()` doxygen (~lines 141-143):
   ```
   \todo setcontext is a user space implementation of calling sigprocmask and then
   restoring registers; should try (ugly) hack using sigreturn which is a real syscall
   ```
2. `OSTimeTickSigHandler()` doxygen (~lines 206-213):
   ```
   \todo this handler maybe executed during a restore (i.e. setcontext()) call.
   This is a problem with the setcontext() user space implementation in Linux i386.
   Most SysV and Linux RISC systems don't have this. The tests below are an
   indication that this situatiuon has occured; the SIGALRM is then aborted...
   \todo If this is not fixed by adding a Linux syscall for get/setcontext, a user
   level implementation with an 'clock interrupt' lock should be used
   ```
3. The shipped mitigation (~lines 221-225) — an EIP-range guard that aborts SIGALRM
   if the interrupted PC is inside `setcontext`'s ~110-byte window:
   ```c
   if (eip >= (uint)setcontext && eip < (uint)setcontext + 110) return;
   ```

Mechanism: glibc i386 `setcontext` is a userspace sequence —
`sigprocmask(restore_mask)` then a register restore — **not** a syscall.
A SIGALRM landing inside that sequence (especially after the mask restore
but before the register restore completes) sees a partially-restored CPU
state. The 110-byte guard catches *most* but not all PC positions in the
window (it does not cover the `sigprocmask` call inside `setcontext`
itself, nor cases where the PC has already returned).

**Proper fix** is the author's plan: replace `setcontext(uc)` with a
hand-rolled wrapper that builds a kernel-style signal frame and invokes the
real `sigreturn` syscall — atomic mask+register restore. That is a port
rewrite, not a small patch, so it is left unfixed; residual ~5 % crash rate
on EX4 stress is accepted.

**See also: branch `ucos2.86_2007_linux_example`** (Philip Mitchell, 2007 —
pthread/signal port of uC/OS-II v2.86). What Mitchell did in 2007 IS the
architectural "use real syscalls" answer the 2.52 author's `\todo` was
pointing at — just via **pthreads/futex** rather than via raw `sigreturn`.
Each task becomes a real Linux thread; the context switch is
`pthread_cond_signal(next) + pthread_cond_wait(self)` under a mutex,
which is atomic via the kernel FUTEX backing the condvar. No `ucontext`,
no `setcontext`, no register-restore window — races 1, 3, and 4 simply
do not exist there.

The cost: every task becomes a real Linux thread (kernel TCB, ~8 MB
default pthread stack, `OS_TASK_DEF_STK_SIZE` becomes meaningless), plus
new failure modes specific to that approach — signal-delivery-thread is
undefined under pthreads, and `OSTickISR` runs `pthread_mutex_lock` /
`pthread_cond_*` from a signal handler (not async-signal-safe per POSIX,
glibc-NPTL works in practice). Different bug class, not strictly fewer
bugs. Not backported here under the "smallest change" policy.

### Detection / verification

Apport silently throttles core dumps under high crash rates, so **count
crashes from the run logs**, not from new cores:
```bash
grep -l 'stack smashing' /tmp/ucos_stress/*.log | wc -l
```
On a fresh build, three sanity bars:
- EX1/EX2/EX3 must run 2 s clean (rc=124 from `timeout`).
- EX4 default load: expect ≤ 2 crashes per 40 × 2 s runs (race 4 residual).
- EX5 default load: 0 crashes per 20 × 2 s runs.

Anything worse means a regression in races 1-3 was reintroduced.
