#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "controller.h"

#define W 40
#define H 20
#define MAX_LEN (W * H)

typedef enum { ST_TITLE, ST_PLAY, ST_OVER } State;
typedef enum { D_UP, D_DOWN, D_LEFT, D_RIGHT } Dir;
typedef struct { int x, y; } Pt;

static Pt   body[MAX_LEN];
static int  len;
static Dir  dir;      /* direction used for the next move */
static Dir  moved;    /* direction of the last completed move (reversal check) */
static Pt   food;
static int  score = 0, high = 0;
static int  won = 0;

static int on_snake(int x, int y)
{
    for (int i = 0; i < len; i++)
        if (body[i].x == x && body[i].y == y) return 1;
    return 0;
}

static void place_food(void)
{
    do {
        food.x = rand() % W;
        food.y = rand() % H;
    } while (on_snake(food.x, food.y));
}

static void reset_game(void)
{
    len = 3;
    for (int i = 0; i < len; i++) {
        body[i].x = W / 2 - i;
        body[i].y = H / 2;
    }
    dir = moved = D_RIGHT;
    score = 0;
    won = 0;
    place_food();
}

/* Returns 1 if the game continues, 0 if it is over. */
static int step(void)
{
    Pt n = body[0];
    switch (dir) {
        case D_UP:    n.y--; break;
        case D_DOWN:  n.y++; break;
        case D_LEFT:  n.x--; break;
        case D_RIGHT: n.x++; break;
    }
    moved = dir;

    if (n.x < 0 || n.x >= W || n.y < 0 || n.y >= H) return 0;   /* wall */

    int eat = (n.x == food.x && n.y == food.y);

    /* If not eating, the tail leaves its cell this turn, so skip it. */
    int check = eat ? len : len - 1;
    for (int i = 0; i < check; i++)
        if (body[i].x == n.x && body[i].y == n.y) return 0;     /* self */

    if (eat) len++;
    memmove(body + 1, body, (size_t)(len - 1) * sizeof(Pt));
    body[0] = n;

    if (eat) {
        score += 10;
        if (len >= MAX_LEN) { won = 1; return 0; }
        place_food();
    }
    return 1;
}

static void draw(HANDLE out, State st)
{
    static char grid[H][W];
    static char buf[4096];
    int p = 0;
    DWORD written;

    memset(grid, ' ', sizeof(grid));
    grid[food.y][food.x] = '*';
    for (int i = len - 1; i >= 0; i--)
        grid[body[i].y][body[i].x] = (i == 0) ? 'O' : 'o';

    p += sprintf(buf + p, " FPGA GAMING CONSOLE - SNAKE%*s\n", 20, "");
    p += sprintf(buf + p, " Score: %-5d  High: %-5d%*s\n", score, high, 12, "");

    buf[p++] = '#';
    for (int x = 0; x < W; x++) buf[p++] = '#';
    buf[p++] = '#'; buf[p++] = '\n';

    for (int y = 0; y < H; y++) {
        buf[p++] = '#';
        for (int x = 0; x < W; x++) buf[p++] = grid[y][x];
        buf[p++] = '#'; buf[p++] = '\n';
    }

    buf[p++] = '#';
    for (int x = 0; x < W; x++) buf[p++] = '#';
    buf[p++] = '#'; buf[p++] = '\n';

    if (st == ST_TITLE)
        p += sprintf(buf + p, " Press ENTER to start. Arrows/WASD move. Esc quits.   \n");
    else if (st == ST_OVER)
        p += sprintf(buf + p, " %s Press ENTER or R to restart. Esc quits.     \n",
                     won ? "YOU WIN!" : "GAME OVER.");
    else
        p += sprintf(buf + p, " Arrows/WASD move. R restarts. Esc quits.            \n");

    COORD home = {0, 0};
    SetConsoleCursorPosition(out, home);
    WriteConsoleA(out, buf, (DWORD)p, &written, NULL);
}

int main(void)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(out, &ci);
    system("cls");
    srand((unsigned)time(NULL));

    if (!controller_init(CTRL_KEYBOARD, NULL, 0)) {
        printf("Controller init failed.\n");
        return 1;
    }

    State st = ST_TITLE;
    uint8_t prev = 0;
    int dirty = 1;
    DWORD last = GetTickCount();

    reset_game();

    for (;;) {
        uint8_t b = controller_poll();
        uint8_t pressed = (uint8_t)(b & ~prev);
        prev = b;
        DWORD now = GetTickCount();

        if (b & BTN_QUIT) break;

        if (st == ST_TITLE) {
            if (pressed & BTN_START) {
                reset_game(); st = ST_PLAY; last = now; dirty = 1;
            }
        } else if (st == ST_PLAY) {
            if (pressed & BTN_RESTART) {
                reset_game(); last = now; dirty = 1;
            }
            /* Accept a direction only if it is not the reverse of the last move */
            if ((b & BTN_UP)    && moved != D_DOWN)  dir = D_UP;
            if ((b & BTN_DOWN)  && moved != D_UP)    dir = D_DOWN;
            if ((b & BTN_LEFT)  && moved != D_RIGHT) dir = D_LEFT;
            if ((b & BTN_RIGHT) && moved != D_LEFT)  dir = D_RIGHT;

            int interval = 140 - (len - 3) * 3;
            if (interval < 60) interval = 60;

            if ((DWORD)(now - last) >= (DWORD)interval) {
                last = now;
                if (!step()) {
                    if (score > high) high = score;
                    st = ST_OVER;
                }
                dirty = 1;
            }
        } else { /* ST_OVER */
            if (pressed & (BTN_START | BTN_RESTART)) {
                reset_game(); st = ST_PLAY; last = now; dirty = 1;
            }
        }

        if (dirty) {
            draw(out, st);
            dirty = 0;
        }
        Sleep(10);
    }

    controller_close();
    CONSOLE_CURSOR_INFO show = {1, TRUE};
    SetConsoleCursorInfo(out, &show);
    system("cls");
    return 0;
}