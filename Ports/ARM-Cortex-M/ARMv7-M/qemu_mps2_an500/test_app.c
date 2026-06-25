/*
*********************************************************************************************************
*                       uC/OS-II V2.93 bring-up test - QEMU mps2-an500 (Cortex-M7)
*
* Proves the port end-to-end:
*   (1) OSStart() launches the first task          -> OSStartHighRdy / initial PSP frame correct
*   (2) TaskA and TaskB output interleaves         -> PendSV context switch works
*   (3) OSTimeGet() advances                        -> SysTick ISR + OSTimeTick working
*   (4) Higher-priority TaskA preempts TaskB        -> priority scheduling works
*
* TaskA exits QEMU (semihosting) after a fixed number of iterations, so the run is self-terminating.
*********************************************************************************************************
*/

#include <ucos_ii.h>
#include "semihost.h"

#define  TASK_STK_SIZE        256u

#define  APP_START_PRIO         3u
#define  TASK_A_PRIO            4u
#define  TASK_B_PRIO            5u

static  OS_STK  AppStartStk[TASK_STK_SIZE];
static  OS_STK  TaskAStk[TASK_STK_SIZE];
static  OS_STK  TaskBStk[TASK_STK_SIZE];

static  void  AppStartTask(void *p_arg);
static  void  TaskA(void *p_arg);
static  void  TaskB(void *p_arg);


int main(void)
{
    OSInit();

    OSTaskCreate(AppStartTask,
                 (void *)0,
                 &AppStartStk[TASK_STK_SIZE - 1u],
                 APP_START_PRIO);

    OSStart();                                       /* never returns                                    */

    for (;;) { }
    return 0;
}


static void AppStartTask(void *p_arg)
{
    (void)p_arg;
                                                     /* SysTick MUST be started after OSStart()          */
    OS_CPU_SysTickInitFreq(CPU_FREQ_HZ);

    sh_write0("\r\n");
    sh_write0("=========================================================\r\n");
    sh_write0(" uC/OS-II V2.93.01  on  QEMU mps2-an500 (Cortex-M7, soft-fp)\r\n");
    sh_write0(" OSStart() OK -> startup task is running\r\n");
    sh_write0("=========================================================\r\n");

    OSTaskCreate(TaskA, (void *)0, &TaskAStk[TASK_STK_SIZE - 1u], TASK_A_PRIO);
    OSTaskCreate(TaskB, (void *)0, &TaskBStk[TASK_STK_SIZE - 1u], TASK_B_PRIO);

    OSTaskDel(OS_PRIO_SELF);                          /* startup work done                                */
}


static void TaskA(void *p_arg)
{
    INT32U  i;
    (void)p_arg;

    for (i = 0u; i < 6u; i++) {
        sh_write0("[A] iter=");
        sh_print_u32(i);
        sh_write0("  OSTime=");
        sh_print_u32(OSTimeGet());
        sh_write0("\r\n");
        OSTimeDly(2u);
    }

    sh_write0("\r\n*** uC/OS-II CONTEXT SWITCH + SYSTICK VERIFIED: TEST PASSED ***\r\n");
    sh_exit(0);
}


static void TaskB(void *p_arg)
{
    INT32U  i;
    (void)p_arg;

    for (i = 0u; ; i++) {
        sh_write0("    [B] iter=");
        sh_print_u32(i);
        sh_write0("  OSTime=");
        sh_print_u32(OSTimeGet());
        sh_write0("\r\n");
        OSTimeDly(3u);
    }
}
