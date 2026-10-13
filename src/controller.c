#include <windows.h>
#include "controller.h"

#define KEY_DOWN(vk) ((GetAsyncKeyState(vk) & 0x8000) != 0)

static ControllerMode g_mode = CTRL_KEYBOARD;

int controller_init(ControllerMode mode, const char *port, unsigned baud)
{
    (void)port;
    (void)baud;
    if (mode != CTRL_KEYBOARD) {
        return 0;
    }
    g_mode = mode;
    return 1;
}

uint8_t controller_poll(void)
{
    uint8_t b = 0;

    if (g_mode == CTRL_KEYBOARD) {
        if (KEY_DOWN(VK_UP)    || KEY_DOWN('W')) b |= BTN_UP;
        if (KEY_DOWN(VK_DOWN)  || KEY_DOWN('S')) b |= BTN_DOWN;
        if (KEY_DOWN(VK_LEFT)  || KEY_DOWN('A')) b |= BTN_LEFT;
        if (KEY_DOWN(VK_RIGHT) || KEY_DOWN('D')) b |= BTN_RIGHT;
        if (KEY_DOWN(VK_RETURN))                 b |= BTN_START;
        if (KEY_DOWN('R'))                       b |= BTN_RESTART;
        if (KEY_DOWN(VK_TAB))                    b |= BTN_SELECT;
        if (KEY_DOWN(VK_ESCAPE))                 b |= BTN_QUIT;
    }
    return b;
}

void controller_close(void)
{
}