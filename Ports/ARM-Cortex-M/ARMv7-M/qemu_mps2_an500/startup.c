/*
*********************************************************************************************************
*                          Startup / Vector Table - QEMU mps2-an500 (Cortex-M7)
*
* - Vector table placed at 0x00000000 (ssram1). QEMU reads initial SP from [0] and PC from [1] at reset.
* - Reset_Handler copies .data, zeros .bss, then calls main().
* - PendSV (entry 14) and SysTick (entry 15) point at the uC/OS-II ARMv7-M port handlers, as the port
*   header mandates (see os_cpu_c.c Note: "MUST be placed on entry 15").
* - Fault handlers emit a semihosting message and exit, turning silent hangs into visible diagnostics.
*********************************************************************************************************
*/

#include <stdint.h>
#include "semihost.h"

extern uint32_t _sidata;        /* .data init values in FLASH (LMA)                                     */
extern uint32_t _sdata;         /* .data start in RAM (VMA)                                              */
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;        /* top of RAM = initial MSP                                              */

extern int  main(void);

/* uC/OS-II ARMv7-M port handlers (Ports/ARM-Cortex-M/ARMv7-M/GNU/os_cpu_a.S, os_cpu_c.c) */
extern void OS_CPU_PendSVHandler(void);
extern void OS_CPU_SysTickHandler(void);

void Reset_Handler(void);
void Default_Handler(void);

/* ---- Fault handlers: report and exit instead of hanging silently ---------------------------------- */
static void fault(const char *name)
{
    sh_write0("\n*** ");
    sh_write0(name);
    sh_write0(" ***\n");
    sh_exit(1);
}

void NMI_Handler(void)        { fault("NMI"); }
void HardFault_Handler(void)  { fault("HARDFAULT"); }
void MemManage_Handler(void)  { fault("MEMMANAGE FAULT"); }
void BusFault_Handler(void)   { fault("BUSFAULT"); }
void UsageFault_Handler(void) { fault("USAGEFAULT"); }

void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));

void Default_Handler(void)
{
    fault("UNEXPECTED EXCEPTION");
}

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {                          /* copy initialised data FLASH -> RAM               */
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; ) {             /* zero BSS                                          */
        *dst++ = 0u;
    }

    (void)main();

    for (;;) { }                                     /* main() should never return                       */
}

/* ---- Cortex-M vector table ------------------------------------------------------------------------ */
__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) =
{
    (void (*)(void))(&_estack),     /* 0  Initial Stack Pointer                                          */
    Reset_Handler,                  /* 1  Reset                                                          */
    NMI_Handler,                    /* 2  NMI                                                            */
    HardFault_Handler,              /* 3  HardFault                                                      */
    MemManage_Handler,              /* 4  MemManage                                                      */
    BusFault_Handler,               /* 5  BusFault                                                       */
    UsageFault_Handler,             /* 6  UsageFault                                                     */
    0, 0, 0, 0,                     /* 7-10 Reserved                                                     */
    SVC_Handler,                    /* 11 SVCall                                                         */
    DebugMon_Handler,               /* 12 Debug Monitor                                                  */
    0,                              /* 13 Reserved                                                       */
    OS_CPU_PendSVHandler,           /* 14 PendSV   -> uC/OS-II context switch                            */
    OS_CPU_SysTickHandler,          /* 15 SysTick  -> uC/OS-II tick                                      */
    /* External interrupts [16+] are not used by this bring-up demo. */
};
