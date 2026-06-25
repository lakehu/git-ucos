/*
*********************************************************************************************************
*                              ARM Semihosting helpers (self-contained)
*********************************************************************************************************
*/

#include "semihost.h"

/* Issue a semihosting call: R0 = operation, R1 = parameter (block or value). Returns R0. */
static int sh_call(int op, void *arg)
{
    register int   r0 asm("r0") = op;
    register void *r1 asm("r1") = arg;
    asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

void sh_write0(const char *s)
{
    sh_call(0x04, (void *)s);                       /* SYS_WRITE0                                        */
}

void sh_exit(int code)
{
    uint32_t block[2];
    block[0] = 0x20026u;                            /* ADP_Stopped_ApplicationExit                       */
    block[1] = (uint32_t)code;
    sh_call(0x20, block);                           /* SYS_EXIT_EXTENDED: QEMU exits with 'code'         */
    sh_call(0x18, (void *)0x20026u);                /* Fallback plain SYS_EXIT (exit 0)                  */
    for (;;) { }
}

void sh_print_u32(uint32_t v)
{
    char buf[12];
    int  i = (int)sizeof(buf) - 1;

    buf[i--] = '\0';
    if (v == 0u) {
        buf[i--] = '0';
    } else {
        while (v != 0u && i >= 0) {
            buf[i--] = (char)('0' + (v % 10u));
            v /= 10u;
        }
    }
    sh_write0(&buf[i + 1]);
}
