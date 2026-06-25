/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*                         (c) Copyright 1992-2002, Jean J. Labrosse, Weston, FL
*                                          All Rights Reserved
*
*
*                                       80x86/80x88 Specific code
*                                          LARGE MEMORY MODEL
*
*                                          Borland C/C++ V4.51
*
* File         : OS_CPU_C.C
* By           : Jean J. Labrosse
*********************************************************************************************************
*/

#define  OS_CPU_GLOBALS
#include "includes.h"

#include <ucontext.h>   // for context switching
#include <string.h>
#include <unistd.h>   // used for syscalls:
                      // ualarm in Threads/unix/TMClock.c
              // kill   in Topsy/unix/SyscallMsg.c
              // getpid in Topsy/unix/SyscallMsg.c
#include <signal.h>   // used in Threads/unix/TMHal.c and
                      // Topsy/unix/SyscallMsg.c
#include <setjmp.h>   // for contextswitches in Threads/unix/TMHal.c
#include <sys/select.h>

#include <stdio.h>    // if we want to use printf...


/*--------------------------------------------------------------------------
 * Per-task saved-context slots (Linux port aliasing fix).
 *
 * Bug being fixed: the original handlers did
 *     OSTCBCur->OSTCBStkPtr = (OS_STK *)uc;
 * where `uc` is a pointer the kernel placed onto the *task's own stack*
 * when delivering the signal. The task kept running on that stack, so a
 * later signal delivery (or nested handler) would overwrite the live
 * ucontext frame -> *** stack smashing detected *** / SIGSEGV.
 *
 * Fix: keep one ucontext_t per priority outside the task's stack, and
 * memcpy(*uc) into it before stashing the pointer in the TCB. The
 * task's stack is no longer used as backing storage for the saved
 * register set.
 *
 * Size: OS_LOWEST_PRIO+1 slots (uC/OS-II indexes TCBs by priority).
 * sizeof(ucontext_t) on i386 glibc ~= 364 bytes -> ~23 KB for 64 prios.
 *--------------------------------------------------------------------------*/
static ucontext_t g_saved_uc[OS_LOWEST_PRIO + 1];


/**                                 INITIALIZE A TASK'S STACK
 *
 * This function is called by either OSTaskCreate() or OSTaskCreateExt() to
 * initialize the stack frame of the task being created. This function is highly
 * processor specific.
 * \arg task    is a pointer to the task code
 * \arg pdata   is a pointer to a user supplied data area that will be passed to the task
 *              when the task first executes.
 * \arg ptos   is a pointer to the top of stack. It is assumed that 'ptos' points to the
 *              highest valid address on the stack.
 * \arg opt specifies options that can be used to alter the behavior of OSTaskStkInit().
 *               \see uCOS_II.H for OS_TASK_OPT_???.
 *
 * \return Always returns the location of the new top-of-stack' once the processor
 * registers have been placed on the stack in the proper order.
 *
 * Note(s)    : Interrupts are enabled when your task starts executing. You can change
 * this by setting this SREG to 0x00 instead. In this case, interrupts would be disabled
 * upon task startup. The application code would be responsible for enabling interrupts
 * at the beginning of the task code. You will need to modify OSTaskIdle() and
 * OSTaskStat() so that they enable interrupts. Failure to do this will make your system
 * crash!
 */
OS_STK* OSTaskStkInit (void (*task)(void* pd), void* pdata, OS_STK* ptos, INT16U opt)
{
    INT32U*    stk;
    ucontext_t uc;
    INT32U     sigsize = 20 + sizeof(uc);

    opt = opt;            /* 'opt' is not used, prevent warning  */

    getcontext(&uc);

    stk = (INT32U*)((int)ptos - sigsize);
    uc.uc_link = NULL; ///< no successor
    uc.uc_mcontext.gregs[REG_EBP] = (int)stk;
    uc.uc_stack.ss_sp = (void*)(((int)stk) - (OS_TASK_DEF_STK_SIZE) + sigsize); ///< base address
    uc.uc_stack.ss_size = OS_TASK_DEF_STK_SIZE - sigsize;

    makecontext(&uc, (void (*)())task, 1, pdata);
    memcpy(stk, &uc, sizeof(uc));

    return ((OS_STK *)stk);
}


/** A new thread is started
 */
void OSStartHighRdy(void)
{
#if (OS_CPU_HOOKS_EN > 0) && (OS_TASK_SW_HOOK_EN > 0)
    OSTaskSwHook();
#endif
    OSRunning = TRUE;

    // real work goes here...
    {
    ucontext_t* ucp;

    ucp = (ucontext_t*)(OSTCBHighRdy->OSTCBStkPtr);
    setcontext(ucp);
    // not reached
    }
}

/** called by OSIntExit()
 *
 * typically, after signal is handled, a potentially new thread is picked and
 * resumed
 */
void OSIntCtxSw(void)
{
    // on Linux this is the same as OSCtxSw; no special treats for hw/sw ints
    OSCtxSw();
}

/**
 * This procedure is called whenever a thread sleeps voluntarily, or, a syscall is made and the
 * the thread is not the highest priority thread anymore after rescheduling.
 *
 * This procedure is called via software interrupt (on Linux synthetic targets: signal and
 * handler).
 *
 * \todo setcontext is a user space implementation of calling sigprocmask and then restoring
 * registers; should try (ugly) hack using sigreturn which is a real syscall
 */
void OSCtxSw(void)
{
    ucontext_t* uc = (ucontext_t*)OSTCBHighRdy->OSTCBStkPtr;

    // at this point, registers are already saved on stack
#if (OS_CPU_HOOKS_EN > 0) && (OS_TASK_SW_HOOK_EN > 0)
    OSTaskSwHook();
#endif

    OSTCBCur = OSTCBHighRdy;
    OSPrioCur = OSPrioHighRdy;

    //if (uc->uc_mcontext.fpregs == 0) {
    //    fprintf(stderr, "ctx sw: uc->uc_mcontext.fpregs == 0\n");
    //    uc->uc_mcontext.fpregs = (fpregset_t)0xbffff6cc;
    //}
    setcontext(uc);
}


/**
 * \arg signo Signal number
 * \arg info Signal info
 * \arg uc User context
 */
void OSCtxSwSigHandler(int signo, siginfo_t* info, /*struct ucontext* */ void* uc)
{
    /** Linux specific module variable of current signal context, i.e. interrupted thread
     */
    /* Fix: copy *uc into a TCB-owned slot instead of aliasing the task's
     * own stack (see g_saved_uc[] note at top of file). */
    INT8U prio = OSTCBCur->OSTCBPrio;
    memcpy(&g_saved_uc[prio], uc, sizeof(ucontext_t));
    OSTCBCur->OSTCBStkPtr = (OS_STK *)&g_saved_uc[prio];

    OSCtxSw();
}


/** \todo maybe wrong for linux port?
 * adjust sp  if nesting == 1 ???? maybe if there is a seperate interrupt stack
 * linux user space port does not have a special isr stack frame layout
 */
void OSTickISR(void)
{
    ///< register context already saved in sig handler
    OSIntEnter();
    if (OSIntNesting == 1) {
        //OSTCBCur->OSTCBStkPtr = $SP;
        //asm("mov %%esp, %0" : "=g"(OSTCBCur->OSTCBStkPtr) : );
    }
    OSTimeTick(); ///< adjusts counters and ready bitmaps
    OSIntExit();  ///< reschedules if necessary
    OSIntCtxSw(); ///< restore context and jump to highest prio task
}

/**
 * Periodic signal handler that adjusts timers
 * On Linux this is not very precise but for most applications sufficient
 *
 * Pick a value between 10 and 1000 Hz for the timer
 *
 * \todo this handler maybe executed during a restore (i.e. setcontext())
 * call. This is a problem with the setcontext() user space implementation in
 * Linux i386. Most SysV and Linux RISC systems don't have this. The tests
 * below are an indication that this situatiuon has occured; the SIGALRM
 * is then aborted...
 *
 * \todo If this is not fixed by adding a Linux syscall for get/setcontext, a user level
 * implementation with an 'clock interrupt' lock should be used
 *
 * \arg signo Signal number
 * \arg info Signal info
 * \arg uc User context
 */
void OSTimeTickSigHandler(int signo, siginfo_t* info, /*struct ucontext* */ void* uc)
{
    if ((((ucontext_t*)uc)->uc_mcontext.gregs[REG_EIP] >= (unsigned int)setcontext) &&
        (((ucontext_t*)uc)->uc_mcontext.gregs[REG_EIP] < ((unsigned int)setcontext + 110))) {
        //fprintf(stderr, "sig timer: thread interrupted in setcontext\n");
        return;
    }
    if (((ucontext_t*)uc)->uc_mcontext.fpregs == 0) {
        //fprintf(stderr, "sig timer: uc->uc_mcontext.fpregs == 0\n");
        return;
    }
    /* Fix: copy *uc into a TCB-owned slot instead of aliasing the task's
     * own stack (see g_saved_uc[] note at top of file). */
    {
        INT8U prio = OSTCBCur->OSTCBPrio;
        memcpy(&g_saved_uc[prio], uc, sizeof(ucontext_t));
        OSTCBCur->OSTCBStkPtr = (OS_STK *)&g_saved_uc[prio];
    }
    OSTickISR();
    // restore context
}

/**
 * Kick the periodic clock; this must be done \e after microC/OS-II is initialised,
 * typically at the end of main()
 *
 * Either SIGALRM or SIGVTALRM can be used
 *
 * \attention This must be called by the user in the first task
 */
void linuxInitInt()
{
    ualarm(1000000/OS_TICKS_PER_SEC, 1000000/OS_TICKS_PER_SEC); ///< periodic mode

    // alternative
    //setitimer(ITIMER_VIRTUAL, 1000000/OS_TICKS_PER_SEC); ///< SIGVTALRM  time spent by the
                                  ///< process in User Mode
}

/**
 * Setup of Linux specific stuff such as stacks, signals, handlers, mask etc.
 *
 * This is called inside the HAL by OSInitHookBegin()
 */
void linuxInit()
{
    struct sigaction act;
    sigset_t mask;

    /* Fix part 2: install an alternate signal stack so the kernel does NOT
     * push ucontext/siginfo/xsave frames onto the *task's own stack* on
     * signal delivery. Without this, even with the aliasing fix, the
     * kernel's signal-frame write can clobber SSP canaries that libc
     * functions (e.g. sigprocmask called from OS_EXIT_CRITICAL) placed on
     * the task's stack just before being interrupted -> *** stack smashing
     * detected *** in OS_TaskIdle. With SIGSTKSZ alternate stack +
     * SA_ONSTACK, all handler-side stack usage moves off-task. */
    {
        static char altstack[SIGSTKSZ];   /* lives forever; not on any task stack */
        stack_t ss;
        ss.ss_sp = altstack;
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
        sigaltstack(&ss, NULL);
    }

    /* Fix part 1 (mask): each handler must mask BOTH SIGALRM and SIGUSR1
     * while it runs, otherwise the other signal can nest into it and the
     * inner handler overwrites the outer handler's ucontext frame on the
     * task's stack (saw this as OSCtxSwSigHandler frame at
     * OSTaskStatStk+3532 with OSTimeTickSigHandler frame at +396 below it
     * -> SIGSEGV). */
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR1);
    act.sa_sigaction = OSTimeTickSigHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    act.sa_mask = mask;
    sigaction(SIGALRM, &act, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR1);
    act.sa_sigaction = OSCtxSwSigHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    act.sa_mask = mask;
    sigaction(SIGUSR1, &act, NULL);
}
