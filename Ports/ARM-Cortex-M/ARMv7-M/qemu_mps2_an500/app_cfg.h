/*
*********************************************************************************************************
*                                      APPLICATION CONFIGURATION
*
*                          uC/OS-II V2.93 - QEMU mps2-an500 (Cortex-M7) port
*
* Filename : app_cfg.h
* Note     : Based on Cfg/Template/app_cfg.h. The ONLY additions required by the ARMv7-M GNU port are
*            CPU_CFG_NVIC_PRIO_BITS and CPU_CFG_KA_IPL_BOUNDARY (see Ports/.../GNU/os_cpu.h Note #1).
*            Without uC/CPU present, the port mandates these be #define'd here.
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
*                               4 bits (16 levels). QEMU's mps2-an500 M7 also tolerates 4. Under-claiming
*                               bits is always safe; over-claiming silently breaks BASEPRI masking.
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
                                                    /* SysTick clock for QEMU mps2 (FPGA-class 25 MHz).  */
                                                    /* Exact value is non-critical under QEMU - it only  */
                                                    /* scales wall-clock tick rate, not correctness.     */
#define  CPU_FREQ_HZ                       25000000uL


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
