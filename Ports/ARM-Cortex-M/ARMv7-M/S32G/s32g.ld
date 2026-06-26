/*
*********************************************************************************************************
*                       Linker script - NXP S32G274A (ARM Cortex-M7)
*
* Memory map taken from the FreeRTOS S32G2 GCC demo (linker/s32g2_image.ld) and the
* debug_t32/s32g274_m7.cmm load script, which load the image to a single unified SRAM window:
*
*   OsInternalRAM @ 0x34000000  (208 KiB)  -> vectors + code + rodata + data + bss + heap + stack
*
* Unlike the QEMU mps2-an500 port there is NO split FLASH(0x0)/RAM(0x20000000) layout: the whole image
* lives in one RAM block. The Lauterbach .cmm loads the ELF here with `data.load.elf` and then sets
* PC = Reset_Handler, so:
*   - .data VMA == LMA (both in RAM). The debugger loads .data in place; the startup .data copy loop is
*     therefore a harmless no-op (_sidata == _sdata). It is kept so the same ELF stays loadable by a
*     bootloader that copies a separate LMA image, without editing startup.c.
*   - The CPU does NOT auto-load MSP from vector[0] on a debugger PC-set, so startup.c sets MSP itself
*     and programs VTOR = &g_pfnVectors (mirrors the FreeRTOS startup.asm + cmm `register.set pc`).
*********************************************************************************************************
*/

ENTRY(Reset_Handler)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x34000000, LENGTH = 0x34000    /* 208 KiB, matches FreeRTOS OsInternalRAM      */
}

_estack = ORIGIN(RAM) + LENGTH(RAM);                     /* top of RAM = 0x34034000 = initial MSP        */

_Min_Heap_Size  = 0x1000;
_Min_Stack_Size = 0x2000;

SECTIONS
{
    .isr_vector :                                        /* vector table FIRST, at 0x34000000           */
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > RAM

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > RAM

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > RAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > RAM

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > RAM

    . = ALIGN(4);
    _sidata = LOADADDR(.data);                           /* == _sdata (VMA==LMA): copy loop is a no-op   */

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM                                              /* no AT>: LMA == VMA, all in RAM               */

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
