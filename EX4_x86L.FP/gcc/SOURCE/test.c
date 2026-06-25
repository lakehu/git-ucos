/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                          (c) Copyright 1992-2002, Jean J. Labrosse, Weston, FL
*                                           All Rights Reserved
*
*                                               EXAMPLE #4
*                              (Linux/POSIX port  -  Floating-Point example)
*
* Notes on the Linux port (vs. the original Borland DOS version):
*   - main() returns int (was void).
*   - DOS console API (conio/pc.h) replaced by the port's VT100 helpers in utils.h / utils.c
*     (PC_DispStr, PC_DispClrScr, PC_GetKey, ...).
*   - DOS interrupt-vector setup (PC_VectSet(uCOS,...), PC_VectSet(0x08,...)) is replaced by the
*     Linux signal/timer setup: OSInitHookBegin() -> linuxInit(), and PC_VectSet()/PC_SetTickRate()
*     start the ualarm-based tick (see utils.c). Those calls are kept for source-shape fidelity.
*   - Floating point: the Linux port switches context with get/setcontext(), which save & restore the
*     x87/FPU state automatically. The separate Ix86L-FP assembly port (OSFPSave/OSFPRestore) is
*     therefore NOT required. OS_TASK_OPT_SAVE_FP is preserved in the OSTaskCreateExt() calls for
*     fidelity but is effectively a no-op on Linux.
*   - The Borland pseudo-variable _8087 (FPU type) does not exist on Linux; the FPU-type banner line
*     is reported as a hosted FPU.
*********************************************************************************************************
*/

#include "includes.h"
#include "utils.h"
#include <math.h>

/*$PAGE*/
/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE              8*1024       /* Size of each task's stacks (# of WORDs)            */
#define  N_TASKS                        10       /* Number of identical tasks                          */

/*
*********************************************************************************************************
*                                               VARIABLES
*********************************************************************************************************
*/

OS_STK        TaskStk[N_TASKS][TASK_STK_SIZE];        /* Tasks stacks                                  */
OS_STK        TaskStartStk[TASK_STK_SIZE];
INT8U         TaskData[N_TASKS];                      /* Parameters to pass to each task               */

/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

        void  Task(void *data);                       /* Function prototypes of tasks                  */
        void  TaskStart(void *data);                  /* Function prototypes of Startup task           */
static  void  TaskStartCreateTasks(void);
static  void  TaskStartDispInit(void);
static  void  TaskStartDisp(void);

/*$PAGE*/
/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/

int  main (void)
{
    PC_DispClrScr();                                       /* Clear the screen                         */

    OSInit();                                              /* Initialize uC/OS-II                      */

    PC_DOSSaveReturn();                                    /* Save environment to return to DOS        */
    //PC_VectSet(uCOS, OSCtxSw);                           /* Install uC/OS-II's context switch vector */

    OSTaskCreateExt(TaskStart,
                    (void *)0,
                    &TaskStartStk[TASK_STK_SIZE - 1],
                    0,                                     /* Task priority = 0                        */
                    0,
                    &TaskStartStk[0],
                    TASK_STK_SIZE,
                    (void *)0,
                    OS_TASK_OPT_SAVE_FP);                  /* (no-op on Linux ucontext port)           */
    OSStart();                                             /* Start multitasking                       */

    return 0;                                              /* not reached                              */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                              STARTUP TASK
*********************************************************************************************************
*/
void  TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr;
#endif
    INT16S     key;


    pdata = pdata;                                         /* Prevent compiler warning                 */

    TaskStartDispInit();                                   /* Initialize the display                   */

    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, (void *)OSTickISR);                   /* Install uC/OS-II's clock tick ISR        */
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    OS_EXIT_CRITICAL();

    OSStatInit();                                          /* Initialize uC/OS-II's statistics         */

    TaskStartCreateTasks();                                /* Create all the application tasks         */

    for (;;) {
        TaskStartDisp();

        if (PC_GetKey(&key) == TRUE) {                     /* See if key has been pressed              */
            if (key == 0x1B) {                             /* Yes, see if it's the ESCAPE key          */
                PC_DOSReturn();                            /*      Return to DOS                       */
            }
        }

        OSCtxSwCtr = 0;
        OSTimeDlyHMSM(0, 0, 1, 0);                         /* Wait one second                          */
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDispInit (void)
{
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
    PC_DispStr( 0,  0, "                         uC/OS-II, The Real-Time Kernel                         ", COLOR_WHITE , COLOR_RED );
    PC_DispStr( 0,  1, "                                Jean J. Labrosse                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  2, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  3, "                                    EXAMPLE #4                                  ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  4, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  5, "TaskPrio      Angle   cos(Angle)   sin(Angle)                                   ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  6, "--------      -----   ----------   ----------                                   ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  7, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  8, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0,  9, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 10, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 11, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 12, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 13, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 14, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 15, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 16, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 17, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 18, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 19, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 20, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 21, "                                                                                ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 22, "#Tasks          :        CPU Usage:     %                                       ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 23, "#Task switch/sec:                                                               ", COLOR_BLACK , COLOR_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                            <-PRESS 'Ctrl C' TO QUIT->                          ", COLOR_BLACK , COLOR_LIGHT_GRAY );
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           UPDATE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDisp (void)
{
    char   s[80];


    sprintf(s, "%5d", OSTaskCtr);                                  /* Display #tasks running               */
    PC_DispStr(18, 22, s, COLOR_YELLOW , COLOR_BLUE);

#if OS_TASK_STAT_EN > 0
    sprintf(s, "%3d", OSCPUUsage);                                 /* Display CPU usage in %               */
    PC_DispStr(36, 22, s, COLOR_YELLOW , COLOR_BLUE);
#endif

    sprintf(s, "%5d", OSCtxSwCtr);                                 /* Display #context switches per second */
    PC_DispStr(18, 23, s, COLOR_YELLOW , COLOR_BLUE);

    sprintf(s, "V%1d.%02d", OSVersion() / 100, OSVersion() % 100); /* Display uC/OS-II's version number    */
    PC_DispStr(75, 24, s, COLOR_YELLOW , COLOR_BLUE);

    PC_DispStr(71, 22, "  FPU   ", COLOR_YELLOW , COLOR_BLUE);     /* FPU present (hosted x87)             */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                             CREATE TASKS
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    INT8U  i;
    INT8U  prio;


    for (i = 0; i < N_TASKS; i++) {                        /* Create N_TASKS identical tasks           */
        prio        = i + 1;
        TaskData[i] = prio;
        OSTaskCreateExt(Task,
                        (void *)&TaskData[i],
                        &TaskStk[i][TASK_STK_SIZE - 1],
                        prio,
                        0,
                        &TaskStk[i][0],
                        TASK_STK_SIZE,
                        (void *)0,
                        OS_TASK_OPT_SAVE_FP);              /* (no-op on Linux ucontext port)           */
    }
}

/*
*********************************************************************************************************
*                                                  TASKS
*********************************************************************************************************
*/

void  Task (void *pdata)
{
    OSTaskStkInit_FPE_x86();              /* give this FP task a clean x87 FPU stack (porter hook) */
    FP32   x;
    FP32   y;
    FP32   angle;
    FP32   radians;
    char   s[81];
    INT8U  ypos;


    ypos  = *(INT8U *)pdata + 7;
    angle = (FP32)(*(INT8U *)pdata) * (FP32)36.0;
    for (;;) {
        radians = (FP32)2.0 * (FP32)3.141592 * angle / (FP32)360.0;
        x       = cos(radians);
        y       = sin(radians);
        sprintf(s, "   %2d       %8.3f  %8.3f     %8.3f", *(INT8U *)pdata, angle, x, y);
        PC_DispStr(0, ypos, s, COLOR_BLACK , COLOR_LIGHT_GRAY);
        if (angle >= (FP32)360.0) {
            angle  =   (FP32)0.0;
        } else {
            angle +=   (FP32)0.01;
        }
        OSTimeDly(1);
    }
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                                 HOOKS
*
*  The OS_CPU hooks live in the application file for this port (same as EX1/EX2/EX3/EX5).
*  OSInitHookBegin() installs the Linux signal handlers via linuxInit().
*********************************************************************************************************
*/

#if OS_CPU_HOOKS_EN > 0
void OSTaskCreateHook (OS_TCB *ptcb)
{
    ptcb = ptcb;
}
#endif

#if OS_CPU_HOOKS_EN > 0
void OSTaskDelHook (OS_TCB *ptcb)
{
    ptcb = ptcb;       /* Prevent compiler warning */
}
#endif

#if (OS_CPU_HOOKS_EN > 0) && (OS_TASK_SW_HOOK_EN > 0)
void OSTaskSwHook (void)
{
}
#endif

#if (OS_TASK_STAT_HOOK_EN > 0)
void OSTaskStatHook (void)
{
}
#endif

#if (OS_CPU_HOOKS_EN > 0) && (OS_TIME_TICK_HOOK_EN > 0)
void OSTimeTickHook (void)
{
}
#endif

#if OS_CPU_HOOKS_EN > 0 && OS_VERSION > 203
void OSInitHookBegin (void)
{
    linuxInit(); ///< installs the syscall handler
}
#endif

#if OS_CPU_HOOKS_EN > 0 && OS_VERSION > 203
void OSInitHookEnd (void)
{
}
#endif

#if OS_CPU_HOOKS_EN > 0 && OS_VERSION >= 251
void OSTaskIdleHook (void)
{
}
#endif

#if OS_CPU_HOOKS_EN > 0 && OS_VERSION > 203
void OSTCBInitHook (OS_TCB *ptcb)
{
    ptcb = ptcb;
}
#endif