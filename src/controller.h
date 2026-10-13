#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>

/* Button bit mask - the same bits will be used in the FPGA packet later */
#define BTN_UP      0x01
#define BTN_DOWN    0x02
#define BTN_LEFT    0x04
#define BTN_RIGHT   0x08
#define BTN_START   0x10
#define BTN_RESTART 0x20
#define BTN_SELECT  0x40
#define BTN_QUIT    0x80   /* keyboard-only: Esc */

typedef enum {
    CTRL_KEYBOARD = 0      /* CTRL_SERIAL is added in Stage 2 */
} ControllerMode;

/* Returns 1 on success, 0 on failure. port/baud are unused by the keyboard backend. */
int     controller_init(ControllerMode mode, const char *port, unsigned baud);

/* Returns the current button state as a bit mask (1 = pressed). */
uint8_t controller_poll(void);

void    controller_close(void);

#endif