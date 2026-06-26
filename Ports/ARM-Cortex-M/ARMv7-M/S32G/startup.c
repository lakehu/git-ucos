/*
*********************************************************************************************************
*                          Startup / Vector Table - NXP S32G274A (Cortex-M7)
*
* Derived from the qemu_mps2_an500 startup.c. Differences forced by real silicon (see README.md):
*
* - The image lives in RAM at 0x34000000 (not at address 0). The Lauterbach .cmm loads the ELF and does
*   `register.set pc Reset_Handler`; it does NOT make the M7 auto-load MSP from vector[0]. So Reset_Handler
*   is `naked` and sets MSP itself, then programs VTOR = &g_pfnVectors before any exception can fire.
*   This mirrors the FreeRTOS S32G2 startup.asm (msr msp / str VTABLE -> VTOR).
* - Fault handlers print over the real LINFlexD0 UART (not semihosting) and then spin, so a fault is a
*   visible message on the wire + a catchable busy-loop for T32, instead of a silent hang.
* - PendSV (entry 14) and SysTick (entry 15) still point at the uC/OS-II ARMv7-M port handlers, exactly
*   as the port header mandates. The tick source is the Cortex-M core SysTick (same core block as QEMU);
*   only its input clock differs (S32G2 FIRC 24 MHz, set in app_cfg.h CPU_FREQ_HZ).
*********************************************************************************************************
*/

#include <stdint.h>
#include "s32g_linflexd.h"

extern uint32_t _sidata;        /* .data load address (LMA). On S32G2 == _sdata (VMA), see s32g.ld      */
extern uint32_t _sdata;         /* .data start (VMA) in RAM                                             */
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;        /* top of RAM = initial MSP                                             */

extern int  main(void);

/* uC/OS-II ARMv7-M port handlers (Ports/ARM-Cortex-M/ARMv7-M/GNU/os_cpu_a.S, os_cpu_c.c) */
extern void OS_CPU_PendSVHandler(void);
extern void OS_CPU_SysTickHandler(void);

void Reset_Handler(void);
void Reset_Handler_C(void);
void Default_Handler(void);

/* ---- Fault handlers: report over UART and spin instead of hanging silently ------------------------ */
/* uart_puts is safe even if the UART is not yet initialised: every wait loop in the driver is bounded  */
/* by WAIT_TIMEOUT_ITERS, so a fault before s32g_linflexd_init() prints nothing but never hangs.        */
static void fault(const char *name)
{
    uart_puts("\n*** ");
    uart_puts(name);
    uart_puts(" ***\n");
    for (;;) { }                                     /* T32 catches the spin; PC shows which fault       */
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

/* ---- Reset entry ---------------------------------------------------------------------------------- */
/* naked: set MSP + VTOR before the C environment exists. The M7 may arrive here with an arbitrary MSP  */
/* (debugger PC-set / BAF handoff), so we cannot let the C prologue touch the stack first.              */
__attribute__((naked)) void Reset_Handler(void)
{
    __asm volatile(
        "cpsid i                 \n"   /* mask IRQs until the kernel is ready                            */
        "ldr   r0, =_estack      \n"
        "msr   msp, r0           \n"   /* initial MSP = top of RAM (not auto-loaded on a debugger start) */
        "ldr   r0, =0xE000ED08   \n"   /* SCB->VTOR                                                      */
        "ldr   r1, =g_pfnVectors \n"
        "str   r1, [r0]          \n"   /* relocate vector table to our image @ 0x34000000               */
        "dsb                     \n"
        "isb                     \n"
        "bl    Reset_Handler_C   \n"
        "b     .                 \n"
        ".ltorg                  \n"   /* dump the literal pool here, in PC range of the ldr= above      */
    );
}

void Reset_Handler_C(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {                          /* .data copy: no-op on S32G2 (VMA==LMA), kept so   */
        *dst++ = *src++;                             /* the ELF also loads via a copy-from-LMA boot path */
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
    (void (*)(void))(&_estack),     /* 0  Initial Stack Pointer (used on a true reset; we also set MSP)  */
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

/* ---- VTABLE alias for the NXP T32 boot script ----------------------------------------------------- */
/* debug_t32/s32g274_m7.cmm programs the M7 boot-address register MC_ME.PRTN0_CORE0_ADDR from the ELF  */
/* symbol `VTABLE` (EnableCM7_0: "Data.Set eaxi:0x4008814C %Long VTABLE"), exactly as the FreeRTOS     */
/* S32G2 vector.asm exported it (".globl VTABLE"). Our table keeps the qemu_mps2_an500 sibling's name  */
/* g_pfnVectors; we ALIAS VTABLE onto it (rename-not-remove: both symbols resolve to 0x34000000) so    */
/* the UNMODIFIED NXP boot sequence finds the vector base. The naked Reset_Handler above independently  */
/* programs VTOR = g_pfnVectors, so the table is correct whether entered via the cmm or a true reset.   */
extern void (* const VTABLE[])(void) __attribute__((alias("g_pfnVectors")));
