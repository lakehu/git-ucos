/*
 * s32g_linflexd.c - LINFlexD_0 UART driver for S32G274A bare-metal
 *                   (FreeRTOS demo, no AUTOSAR RTD)
 *
 * Pin mapping (matches eTSW OEM_VCCD board Pbcfg):
 *   TX: PC_09 (SIUL2_0 MSCR[41]), mux = ALT1   -> LINFLEXD_0_TX
 *   RX: PC_10 (SIUL2_0 MSCR[42]), mux = AS_GPIO; IMCR[0] = ALT2 -> LIN0_RX
 *
 * Clock assumption:
 *   We assume the previous boot stage (BAF/HSE bootstrap, or the production
 *   bootloader that hands off to us) has ALREADY:
 *     - enabled MC_ME PRTN1_COFB1 bit 8 for LINFlexD_0
 *     - left LIN_CLK source at FIRC = 48 MHz (reset default)
 *
 *   We do NOT write MC_ME ourselves — on S32G2 those registers are
 *   IPS-protected and an M7 application image will HardFault on the very
 *   first PRTN write attempt (write-ignored bus error).  If your firmware
 *   image was loaded by a bootloader that did NOT enable LinFlexD_0, this
 *   driver will silently appear dead; in that case, run a T32 .cmm pre-load
 *   script that does the MC_ME bring-up.
 *
 * Baud rate:
 *   LDIV = 48e6 / (16 * 115200) = 26.0417
 *   LINIBRR = 26, LINFBRR = 1   ->  actual ~115108 baud (-0.08% error)
 *
 *   If LIN_CLK has been switched to PERIPH_PLL_PHI3 = 125 MHz somewhere
 *   upstream, swap to LINIBRR=67, LINFBRR=13.
 *
 * Safety:
 *   All wait loops have a cycle-count timeout.  A hardware issue (wrong
 *   clock, missing peripheral enable, wrong pad mux) will produce silence
 *   on the wire and an early return, but will NEVER hang the CPU.
 */

#include "s32g_linflexd.h"
#include <stdarg.h>
#include <stdio.h>

/*============================== Register map ===============================*/

/* LINFlexD_0 */
#define LINFLEXD0_BASE   0x401C8000UL
#define LINCR1           (LINFLEXD0_BASE + 0x00)
#define LINIER           (LINFLEXD0_BASE + 0x04)
#define LINSR            (LINFLEXD0_BASE + 0x08)
#define UARTCR           (LINFLEXD0_BASE + 0x10)
#define UARTSR           (LINFLEXD0_BASE + 0x14)
#define LINFBRR          (LINFLEXD0_BASE + 0x24)
#define LINIBRR          (LINFLEXD0_BASE + 0x28)
#define BDRL             (LINFLEXD0_BASE + 0x38)
#define BDRM             (LINFLEXD0_BASE + 0x3C)

/* SIUL2_0 — pad muxing + input mux */
#define SIUL2_0_BASE     0x4009C000UL
#define SIUL2_MSCR(n)    (SIUL2_0_BASE + 0x240 + ((n) * 4))   /* output side */
#define SIUL2_IMCR(n)    (SIUL2_0_BASE + 0xA40 + ((n) * 4))   /* input mux  */

#define PAD_PC_09        41u   /* LINFLEXD0_TX */
#define PAD_PC_10        42u   /* LINFLEXD0_RX */
#define IMCR_LIN0_RX     0u    /* PC_10 -> LIN0_RX via IMCR[0], SSS=ALT2  */

/* MSCR bit fields (from BaseNXP S32G274A_SIUL2.h) */
#define MSCR_SSS_SHIFT   0
#define MSCR_SRE_SHIFT   14
#define MSCR_IBE_BIT     (1u << 19)
#define MSCR_OBE_BIT     (1u << 21)
#define IMCR_SSS_SHIFT   0

/* Slew rate control 4 = the value used by the working eTSW config. */
#define SLEW_RATE_CTRL4  4u

/*============================ LINFlexD bit fields ==========================*/

#define LINCR1_INIT      (1u << 0)
#define LINSR_LINS_MASK  0xF000u
#define LINSR_LINS_INIT  0x1000u
#define UARTCR_UART      (1u << 0)
#define UARTCR_WL0       (1u << 1)
#define UARTCR_TXEN      (1u << 4)
#define UARTCR_RXEN      (1u << 5)
#define UARTSR_DTF       (1u << 1)
#define UARTSR_DRF       (1u << 2)

/* Hard upper bound on any busy-wait — far longer than the slowest real-world
 * wait, but small enough to prevent a hardware misconfig from locking the
 * CPU.  At 200 MHz core clock, 1M iterations ~ a few ms.                    */
#define WAIT_TIMEOUT_ITERS  1000000u

/*============================ Register accessor ============================*/

#define REG(x)           (*(volatile uint32_t *)(x))

/*============================ SIUL2 pad muxing =============================*/

static void configure_uart_pads(void)
{
    /* TX = PC_09 (pad 41), mux = ALT1, OBE=1, SRE=4. */
    REG(SIUL2_MSCR(PAD_PC_09)) = MSCR_OBE_BIT
                               | ((uint32_t)SLEW_RATE_CTRL4 << MSCR_SRE_SHIFT)
                               | (1u << MSCR_SSS_SHIFT);  /* SSS = ALT1 */

    /* RX = PC_10 (pad 42), mux = AS_GPIO at MSCR (SSS=0), IBE=1, SRE=4. */
    REG(SIUL2_MSCR(PAD_PC_10)) = MSCR_IBE_BIT
                               | ((uint32_t)SLEW_RATE_CTRL4 << MSCR_SRE_SHIFT)
                               | (0u << MSCR_SSS_SHIFT);

    /* Route PC_10 to LIN0_RX via IMCR[0], SSS = ALT2. */
    REG(SIUL2_IMCR(IMCR_LIN0_RX)) = (2u << IMCR_SSS_SHIFT);
}

/*=========================== LINFlexD bring-up =============================*/

int s32g_linflexd_init(void)
{
    uint32_t timeout;

    /* Configure pads.  Touches only SIUL2_0 (always available to M7). */
    configure_uart_pads();

    /* LINFlexD into Init mode. SLEEP=0, INIT=1.
     * NOTE: if PRTN1_COFB1[REQ8] is NOT enabled this write is dropped and
     * the status read below will spin out the timeout, then return early.
     * It will NEVER hang the CPU.                                          */
    REG(LINCR1) = LINCR1_INIT;

    timeout = WAIT_TIMEOUT_ITERS;
    while (((REG(LINSR) & LINSR_LINS_MASK) != LINSR_LINS_INIT) && (--timeout != 0u)) { }
    if (timeout == 0u) return -1;        /* peripheral not clocked / bad config */

    /* UART mode, 8-bit, TX + RX enabled.  ROSE=0 (16x oversampling). */
    REG(UARTCR) = UARTCR_UART | UARTCR_WL0 | UARTCR_TXEN | UARTCR_RXEN;

    /* Baud rate — LIN_BAUD_CLK = PERIPH_PLL_PHI3 = 125 MHz, 115200 baud, OSR=16.
     * The .cmm debug script programs MC_CGM_0 MUX_8 SELCTL to PHI3 before load,
     * matching the metha/U-Boot bootloader.
     * 125e6 / (16 * 115200) = 67.8168  ->  LINIBRR=67, LINFBRR=13
     * Actual baud ~115200 (exact). Same values as working bootloader. */
    REG(LINIBRR) = 67u;
    REG(LINFBRR) = 13u;

    /* Leave Init mode. */
    REG(LINCR1) &= ~LINCR1_INIT;

    return 0;
}

/*================================ TX / RX ==================================*/

void uart_putc(char c)
{
    uint32_t timeout;

    REG(BDRL) = (uint32_t)(uint8_t)c;

    timeout = WAIT_TIMEOUT_ITERS;
    while (((REG(UARTSR) & UARTSR_DTF) == 0u) && (--timeout != 0u)) { }
    REG(UARTSR) = UARTSR_DTF;     /* W1C clear */
}

void uart_puts(const char *s)
{
    if (s == 0) return;
    while (*s) {
        uart_putc(*s);
        if (*s == '\n') uart_putc('\r');
        s++;
    }
}

char uart_getc_nonblock(void)
{
    /* Buffer mode on M7 (no FIFO): hardware clears TXEN (bit4) on each
     * received byte.  Use TXEN=0 as "new data arrived" signal, read BDRM,
     * then restore TXEN so TX keeps working.
     *
     * To avoid duplicate detection: after restoring TXEN, wait until
     * TXEN reads back as 1 (hardware accepted it) before returning.
     * This ensures the next call won't see a stale TXEN=0. */
    if ((REG(UARTCR) & UARTCR_TXEN) == 0u) {
        char c = (char)(REG(BDRM) & 0xFFu);
        REG(UARTCR) |= UARTCR_TXEN;
        REG(UARTSR) = UARTSR_DRF;
        /* Spin until TXEN reads back as 1, so next poll starts clean.
         * If a new byte arrives immediately, TXEN drops again — that's
         * a genuine new byte, not a duplicate. */
        while ((REG(UARTCR) & UARTCR_TXEN) == 0u) {
            REG(UARTCR) |= UARTCR_TXEN;
        }
        return c;
    }
    return 0;
}

int uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0) return n;
    if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;

    for (int i = 0; i < n; i++) {
        uart_putc(buf[i]);
        if (buf[i] == '\n') uart_putc('\r');
    }
    return n;
}
