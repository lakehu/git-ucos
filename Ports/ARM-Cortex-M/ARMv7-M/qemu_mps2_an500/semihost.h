/*
*********************************************************************************************************
*                              ARM Semihosting helpers (self-contained)
*
* Uses the ARM angel semihosting SVC interface via "BKPT 0xAB" (Thumb). Works under QEMU when started
* with -semihosting-config enable=on,target=native. No newlib syscall plumbing required.
*********************************************************************************************************
*/

#ifndef  SEMIHOST_H
#define  SEMIHOST_H

#include <stdint.h>

void  sh_write0   (const char *s);     /* SYS_WRITE0 (0x04): write NUL-terminated string to host       */
void  sh_print_u32(uint32_t v);        /* write an unsigned decimal integer                            */
void  sh_exit     (int code);          /* SYS_EXIT  : terminate QEMU with the given exit code          */

#endif
