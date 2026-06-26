/*
*********************************************************************************************************
*                                      APPLICATION CONFIGURATION
*
*                          uC/OS-II V2.93 - NXP S32G274A (Cortex-M7) real-hardware port
*
* Filename : app_cfg.h
* Note     : Based on Cfg/Template/app_cfg.h. The ONLY additions required by the ARMv7-M GNU port are
*            CPU_CFG_NVIC_PRIO_BITS and CPU_CFG_KA_IPL_BOUNDARY (see Ports/.../GNU/os_cpu.h Note #1).
*            Without uC/CPU present, the port mandates these be #define'd here.
*
*            Derived from the qemu_mps2_an500 port. The only application-visible change for real silicon
*            is CPU_FREQ_HZ (see below): the SysTick input clock is now the S32G2 M7 core clock, not the
*            QEMU FPGA clock.
*********************************************************************************************************
*/

#ifndef  _APP_CFG_H_
#define  _APP_CFG_H_

#include  <stdarg.h>
#include  <stdio.h>


/*
*********************************************************************************************************
*                                  ARMv7-M PORT REQUIRED DEFINES
*
* (1) CPU_CFG_NVIC_PRIO_BITS  : number of implemented NVIC priority bits. The S32G2 Cortex-M7 implements
*                               4 bits (16 levels). Under-claiming bits is always safe; over-claiming
*                               silently breaks BASEPRI masking.
*
* (2) CPU_CFG_KA_IPL_BOUNDARY : kernel-aware interrupt priority boundary. With value 4 and 4 prio bits,
*                               BASEPRI boundary = 4 << (8-4) = 0x40. Priorities 4..15 are kernel aware,
*                               0..3 are non-kernel aware. SysTick is set to this level; PendSV is lowest.
*********************************************************************************************************
*/

#define  CPU_CFG_NVIC_PRIO_BITS            4u
#define  CPU_CFG_KA_IPL_BOUNDARY           4u


/*
*********************************************************************************************************
*                                       APPLICATION DEFINES
*********************************************************************************************************
*/
                                                    /* SysTick is clocked from the M7 CORE clock         */
                                                    /* (os_cpu_c.c sets SYST_CSR.CLKSOURCE=1 = core clk).*/
                                                    /* Value taken authoritatively from the FreeRTOS     */
                                                    /* S32G2 demo FreeRTOSConfig.h: configCPU_CLOCK_HZ = */
                                                    /* 24000000UL, run under the SAME NXP T32 boot script*/
                                                    /* we reuse here, so the tick timing matches 1:1.    */
                                                    /* This 24 MHz is the CORE-clock figure - it is NOT  */
                                                    /* FIRC (48 MHz) and NOT the LIN baud clock (125 MHz, */
                                                    /* see s32g_linflexd.c); do not conflate them.       */
                                                    /* We do not switch the core PLL in this bring-up, so */
                                                    /* if a boot stage changes the core clock only the   */
                                                    /* wall-clock tick RATE scales - tick correctness is */
                                                    /* unaffected. To confirm, compare OSTimeGet() deltas*/
                                                    /* against a stopwatch, or read the core PLL in T32. */
#define  CPU_FREQ_HZ                       24000000uL


/*
*********************************************************************************************************
*                                           TASK PRIORITIES
*********************************************************************************************************
*/

#define  APP_CFG_STARTUP_TASK_PRIO          3u

#define  OS_TASK_TMR_PRIO                  (OS_LOWEST_PRIO - 2u)


/*
*********************************************************************************************************
*                                          TASK STACK SIZES
*********************************************************************************************************
*/

#define  APP_CFG_STARTUP_TASK_STK_SIZE    256u


/*
*********************************************************************************************************
*                                     TRACE / DEBUG CONFIGURATION
*********************************************************************************************************
*/

#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                    0u
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                   1u
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                    2u
#endif

#define  APP_TRACE_LEVEL                   TRACE_LEVEL_OFF
#define  APP_TRACE                         printf

#define  APP_TRACE_INFO(x)    ((APP_TRACE_LEVEL >= TRACE_LEVEL_INFO)  ? (void)(APP_TRACE x) : (void)0)
#define  APP_TRACE_DBG(x)     ((APP_TRACE_LEVEL >= TRACE_LEVEL_DBG)   ? (void)(APP_TRACE x) : (void)0)


#endif                                              /* End of module include.                            */
