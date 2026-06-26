/*
*********************************************************************************************************
*                       uC/OS-II V2.93 bring-up test - NXP S32G274A (Cortex-M7)
*
* Proves the port end-to-end on real silicon:
*   (1) OSStart() launches the first task          -> OSStartHighRdy / initial PSP frame correct
*   (2) TaskA and TaskB output interleaves         -> PendSV context switch works
*   (3) OSTimeGet() advances                        -> SysTick ISR + OSTimeTick working
*   (4) Higher-priority TaskA preempts TaskB        -> priority scheduling works
*
* Real-hardware differences vs the qemu_mps2_an500 test_app.c:
*   - Output goes over LINFlexD0 UART (115200 8N1) instead of ARM semihosting.
*   - There is no semihosting sh_exit() to end the run. Instead TaskA sets a `volatile g_TestResult`
*     to 0x5A on PASS (mirrors the FreeRTOS S32G2 demo's `testResult = 0x5A`). A T32 script (or a human
*     watching the UART) reads g_TestResult to confirm PASS. TaskA then idles instead of exiting.
*********************************************************************************************************
*/

#include <ucos_ii.h>
#include "s32g_linflexd.h"

#define  TASK_STK_SIZE        256u

#define  APP_START_PRIO         3u
#define  TASK_A_PRIO            4u
#define  TASK_B_PRIO            5u

#define  TEST_RESULT_RUNNING  0x33u
#define  TEST_RESULT_PASS     0x5Au

/* Watched by debug_t32 scripts / T32 Var.View. volatile + used so it survives -O and lands in .data. */
volatile INT8U  g_TestResult = TEST_RESULT_RUNNING;

/* Per-task iteration counters. Promoted from the TaskA/TaskB local 'i' to file scope so they have a
 * fixed (static) address and can be watched in TRACE32. A local 'i' lives on the task stack and has no
 * stable address, so it cannot be put in a persistent T32 Var.Watch. volatile keeps them in memory. */
volatile INT32U g_TaskA_iter = 0u;
volatile INT32U g_TaskB_iter = 0u;

static  OS_STK  AppStartStk[TASK_STK_SIZE];
static  OS_STK  TaskAStk[TASK_STK_SIZE];
static  OS_STK  TaskBStk[TASK_STK_SIZE];

static  void  AppStartTask(void *p_arg);
static  void  TaskA(void *p_arg);
static  void  TaskB(void *p_arg);


/* Minimal unsigned-int -> UART helper (keeps test self-contained, no printf float pull-in). */
static void uart_print_u32(INT32U v)
{
    char     buf[11];
    int      i = 10;

    buf[i] = '\0';
    if (v == 0u) {
        uart_putc('0');
        return;
    }
    while (v != 0u && i > 0) {
        buf[--i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    uart_puts(&buf[i]);
}


int main(void)
{
    INT8U  err;

    s32g_linflexd_init();                            /* bring up the console BEFORE the scheduler        */

    uart_puts("\r\n");
    uart_puts("=========================================================\r\n");
    uart_puts(" uC/OS-II V2.93.01  on  NXP S32G274A (Cortex-M7)\r\n");
    uart_puts(" UART: LINFLEXD0 @ 115200 8N1\r\n");
    uart_puts(" Starting kernel...\r\n");
    uart_puts("=========================================================\r\n");

    OSInit();

    OSTaskCreate(AppStartTask,
                 (void *)0,
                 &AppStartStk[TASK_STK_SIZE - 1u],
                 APP_START_PRIO);
    OSTaskNameSet(APP_START_PRIO, (INT8U *)"AppStartTask", &err);  /* name for TRACE32 uC/OS-II awareness */

    OSStart();                                       /* never returns                                    */

    for (;;) { }
    return 0;
}


static void AppStartTask(void *p_arg)
{
    INT8U  err;

    (void)p_arg;
                                                     /* SysTick MUST be started after OSStart()          */
    OS_CPU_SysTickInitFreq(CPU_FREQ_HZ);

    uart_puts(" OSStart() OK -> startup task is running\r\n");

    OSTaskCreate(TaskA, (void *)0, &TaskAStk[TASK_STK_SIZE - 1u], TASK_A_PRIO);
    OSTaskCreate(TaskB, (void *)0, &TaskBStk[TASK_STK_SIZE - 1u], TASK_B_PRIO);
    OSTaskNameSet(TASK_A_PRIO, (INT8U *)"TaskA", &err);  /* names for TRACE32 uC/OS-II awareness */
    OSTaskNameSet(TASK_B_PRIO, (INT8U *)"TaskB", &err);

    OSTaskDel(OS_PRIO_SELF);                          /* startup work done                                */
}


static void TaskA(void *p_arg)
{
    (void)p_arg;

    for (g_TaskA_iter = 0u; g_TaskA_iter < 6u; g_TaskA_iter++) {
        uart_puts("[A] iter=");
        uart_print_u32(g_TaskA_iter);
        uart_puts("  OSTime=");
        uart_print_u32(OSTimeGet());
        uart_puts("\r\n");
        OSTimeDly(2u);
    }

    g_TestResult = TEST_RESULT_PASS;                 /* T32 / human watches this for 0x5A = PASS         */
    uart_puts("\r\n*** uC/OS-II CONTEXT SWITCH + SYSTICK VERIFIED: TEST PASSED (g_TestResult=0x5A) ***\r\n");

    for (;;) {                                        /* no semihosting exit on real HW: idle forever     */
        OSTimeDly(1000u);
    }
}


static void TaskB(void *p_arg)
{
    (void)p_arg;

    for (g_TaskB_iter = 0u; ; g_TaskB_iter++) {
        uart_puts("    [B] iter=");
        uart_print_u32(g_TaskB_iter);
        uart_puts("  OSTime=");
        uart_print_u32(OSTimeGet());
        uart_puts("\r\n");
        OSTimeDly(3u);
    }
}
