# FPGA-Based Gaming Console with Hardware Controller

A college major project that demonstrates how an FPGA development board can act as a physical game controller for games running on a Windows PC.

## Project Status

| Component | Status |
|---|---|
| Snake game in C (console) | Done, running on Windows |
| Keyboard controller backend | Done, tested |
| Controller abstraction (`controller.h/.c`) | Done |
| Serial/UART backend (FPGA to PC) | Not yet built (Stage 2) |
| Verilog HDL (debounce, encoder, UART TX) | Not yet built (Stage 3) |
| Board-specific pins and constraints | Waiting for the exact board model (Stage 4) |
| Car Racing game | Not yet built (Stage 5) |

Nothing in this project has been tested on real FPGA hardware yet. Right now the games run in **keyboard simulation mode**, which needs no board.

## Objectives

1. Build a Windows game in C that is playable from a keyboard.
2. Separate the input source from the game logic, so the same game can later be driven by an FPGA board without changing the game code.
3. Design the FPGA side (button reading, debouncing, Boolean encoding, UART transmitter) in Verilog.
4. Define a simple communication protocol for button states.
5. Add a second game (Car Racing) using the same controller interface.

## System Architecture

```
 [Buttons] -> [FPGA: debounce -> encode -> UART TX] -> [USB-UART bridge] -> [PC: serial RX]
                                                                               |
 [Keyboard] -------------------------------------------------------------> [controller.c]
                                                                               |
                                                            snake.c (game logic + console rendering)
```

The game only calls `controller_poll()`. It never knows whether the input came from the keyboard or the FPGA. Only the keyboard path exists today.

## Folder Structure

```
fpga_console/
  README.md
  src/
    controller.h
    controller.c
    snake.c
```

## Requirements

- Windows 10 or 11
- MinGW-w64 GCC (install with `winget install BrechtSanders.WinLibs.POSIX.UCRT`)
- VS Code (optional, any terminal works)
- No external libraries are used

Check the compiler with `gcc --version`.

## Build and Run

From the project folder (the one containing `src`), run the two commands separately:

```
gcc -Wall -Wextra -O2 -o snake.exe src/snake.c src/controller.c
```
```
.\snake.exe
```

Use a console window of at least 50 columns by 28 rows. Maximize it if the board looks cut off.

## Controls (Keyboard Mode)

| Action | Keys |
|---|---|
| Move up | Up arrow or W |
| Move down | Down arrow or S |
| Move left | Left arrow or A |
| Move right | Right arrow or D |
| Start | Enter |
| Restart | R |
| Select game (reserved for later) | Tab |
| Quit | Esc |

Keyboard input is read globally by Windows, so keep the game window focused during the demo.

## Controller Button Bit Mask

All button states are packed into one byte. The same bit layout will be used in the FPGA packet later.

| Bit | Mask | Button |
|---|---|---|
| 0 | 0x01 | UP |
| 1 | 0x02 | DOWN |
| 2 | 0x04 | LEFT |
| 3 | 0x08 | RIGHT |
| 4 | 0x10 | START |
| 5 | 0x20 | RESTART |
| 6 | 0x40 | SELECT |
| 7 | 0x80 | QUIT (keyboard only, Esc) |

Example: UP and RIGHT pressed together gives `0x01 | 0x08 = 0x09`.

## Code Explanation, File by File

### 1. `src/controller.h`: the controller interface

This header is the contract between the game and any input device.

- **Button masks** (`BTN_UP`, `BTN_DOWN`, ...): one bit per button, so any combination of buttons fits in a single `uint8_t`.
- **`ControllerMode`**: an enum listing the available input backends. It currently has only `CTRL_KEYBOARD`. A `CTRL_SERIAL` entry will be added when the FPGA link is built.
- **`controller_init(mode, port, baud)`**: selects and initializes a backend. Returns 1 on success and 0 on failure. `port` and `baud` are ignored by the keyboard backend and are reserved for the serial backend (for example `"COM3"` and `115200`).
- **`controller_poll()`**: returns the current state of all buttons as a bit mask (1 = pressed).
- **`controller_close()`**: releases resources. It does nothing for the keyboard and will close the serial port later.

### 2. `src/controller.c`: the keyboard backend

- Uses the Windows function `GetAsyncKeyState()` to check whether a key is currently held down. The macro `KEY_DOWN(vk)` checks the high bit of its result.
- `controller_init()` stores the chosen mode and rejects any mode that is not supported.
- `controller_poll()` starts with `b = 0`, then ORs in a button bit for each key that is down. For example, either the Up arrow or `W` sets `BTN_UP`.
- Because the game only sees the byte returned by `controller_poll()`, replacing the keyboard with the FPGA later only requires adding a second branch in this file. `snake.c` stays unchanged.

### 3. `src/snake.c`: the game

**Constants and data**

- The board is `W = 40` by `H = 20` cells. `MAX_LEN = W * H` is the largest possible snake.
- The snake is an array of points, `body[]`, where `body[0]` is the head and `len` is the current length.
- `dir` is the direction that will be used on the next move. `moved` is the direction of the last completed move, used to block reversals.
- `food`, `score`, `high` (high score), and `won` hold the rest of the game state.
- The game has three states: `ST_TITLE`, `ST_PLAY`, and `ST_OVER`.

**Functions**

- **`on_snake(x, y)`**: returns 1 if a cell is occupied by the snake. Used when placing food.
- **`place_food()`**: picks random cells until it finds one not covered by the snake.
- **`reset_game()`**: sets the snake to length 3 in the middle of the board heading right, clears the score, and places food.
- **`step()`**: advances the game by one move:
  1. Computes the new head position from `dir`, and records it in `moved`.
  2. Returns 0 (game over) if the head leaves the board.
  3. Checks for self-collision. If the snake is not eating, the tail cell is skipped, because the tail moves away on that same turn.
  4. If food is eaten, the length grows by one and the score increases by 10.
  5. Shifts the whole body by one cell with `memmove()` and puts the new head at the front.
  6. If the snake fills the entire board, the player wins.
- **`draw()`**: builds the whole screen in one text buffer (title, score, borders, grid, status line) and writes it with a single `WriteConsoleA()` call after moving the cursor to the top-left. Redrawing in place this way avoids the flicker caused by clearing the screen every frame.
- **`main()`**: sets up the console, seeds the random number generator, and initializes the controller in keyboard mode. It then runs the main loop.

**Main loop, once every 10 ms**

1. Reads the button byte with `controller_poll()`.
2. Detects new presses with `pressed = b & ~prev`. A button counts as "pressed" only on the frame where it goes from released to held, so holding Enter does not restart the game repeatedly.
3. Handles the current state:
   - **Title:** Enter starts the game.
   - **Playing:** R restarts. A direction is accepted only if it is not the opposite of `moved`, which prevents the snake from reversing into itself. The game steps when enough time has passed.
   - **Game over:** Enter or R restarts. The high score is kept.
4. Redraws only when something changed.

**Speed and difficulty:** the time between moves is `140 - (len - 3) * 3` milliseconds, with a minimum of 60 ms. The snake gets faster each time it grows.

**Timing:** the game uses `GetTickCount()`. The unsigned subtraction `now - last` stays correct even when the 32-bit timer wraps around.

## Testing Keyboard Mode

1. The title screen appears, and Enter starts the game.
2. Arrow keys and WASD steer the snake, and pressing the opposite direction does nothing.
3. Eating `*` adds 10 points and the snake speeds up.
4. Hitting a wall or the snake's own body shows GAME OVER. Enter or R restarts, and the high score stays.
5. Esc exits cleanly.

## Planned Stages

- **Stage 2:** Windows serial port backend and a packet protocol (start byte, button byte, checksum, timeout), plus a keyboard-driven fake "FPGA" mode to test packet parsing without hardware.
- **Stage 3:** Verilog modules for the debouncer, button encoder, and UART transmitter, with a testbench.
- **Stage 4:** Board-specific pin constraints and wiring, once the exact board model and its USB-UART capability are confirmed.
- **Stage 5:** Car Racing game using the same controller interface.
- **Stage 6:** Demo script, viva questions and answers, and final documentation.

## Limitations (Current)

- Console graphics only (ASCII characters).
- Keyboard input is read globally, so keys pressed in other windows also affect the game.
- No FPGA communication exists yet. Hardware mode requires the planned serial backend and a board with a supported USB-UART interface.
- Only Snake is implemented.#   f p g a _ c o n s o l e  
 