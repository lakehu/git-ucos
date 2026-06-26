/*
 * s32g_linflexd.h - LINFlexD UART driver for S32G274A bare-metal
 * Adapted from /home/uie75906/data/HPC/car_common/uart/s32g_linflexd.h
 *
 * - Bare-metal: no AUTOSAR/RTD dependencies
 * - Configured for FIRC 24 MHz LIN_CLK, 115200 8N1
 * - Polled TX only, polled RX
 */

#ifndef S32G_LINFLEXD_H_
#define S32G_LINFLEXD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize LINFlexD_0 as UART, 115200 8N1, FIRC 24 MHz LIN_CLK source.
 * Also configures SIUL2 pad muxing for PC8 (TX) / PC9 (RX) and enables
 * the LIN_CLK source select in MC_CGM_0.
 * Returns 0 on success. */
int s32g_linflexd_init(void);

/* Blocking single character TX. */
void uart_putc(char c);

/* Blocking NUL-terminated string TX. Adds CR after LF automatically. */
void uart_puts(const char *s);

/* Non-blocking RX. Returns received char or 0 if no data. */
char uart_getc_nonblock(void);

/* Blocking printf-style output (uses vsnprintf into a 256-byte stack buf). */
int uart_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* S32G_LINFLEXD_H_ */
