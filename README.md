<h1 align="center">FPGA-Based Gaming Console with Hardware Controller</h1>

<p align="center">
  A Windows game console written in C, built around a swappable controller layer.<br>
  Play with the keyboard today, with an FPGA board as the physical controller next.
</p>

<p align="center">
  <img alt="Language" src="https://img.shields.io/badge/language-C-blue">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-lightgrey">
  <img alt="Dependencies" src="https://img.shields.io/badge/dependencies-none-brightgreen">
  <img alt="Status" src="https://img.shields.io/badge/status-in%20progress-orange">
</p>

---

## Overview

This is a college major project that shows how an FPGA development board can work as a physical controller for games running on a Windows PC.

The key idea is **separation of input from game logic**. The game never reads the keyboard directly. It asks a controller module "which buttons are pressed?" and gets back one byte. Today that byte comes from the keyboard. Later it will come from an FPGA board over a serial link, and the game code will not need to change.

## Project Status

| Component | Status |
|---|---|
| Snake game in C (console) | Done, running on Windows |
| Keyboard controller backend | Done, tested |
| Controller abstraction (`controller.h` / `controller.c`) | Done |
| Serial/UART backend (FPGA to PC) | Planned (Stage 2) |
| Verilog HDL (debounce, encoder, UART TX) | Planned (Stage 3) |
| Board pin constraints and wiring | Waiting for the exact board model (Stage 4) |
| Car Racing game | Planned (Stage 5) |

> **Note:** Nothing in this project has been tested on real FPGA hardware yet. Right now the game runs in keyboard simulation mode, which needs no board.

## Objectives

1. Build a Windows game in C that is playable from the keyboard.
2. Keep the input source separate from the game logic, so an FPGA board can replace the keyboard without changing the game.
3. Design the FPGA side in Verilog: button reading, debouncing, Boolean encoding, and a UART transmitter.
4. Define a simple communication protocol for button states.
5. Add a second game (Car Racing) using the same controller interface.

## System Architecture

```
 [Buttons] -> [FPGA: debounce -> encode -> UART TX] -> [USB-UART bridge] -> [PC: serial RX]
                                                                                 |
 [Keyboard] ----------------------------------------------------------> [controller.c]
                                                                                 |
                                                           snake.c (game logic + console rendering)
```

The game only calls `controller_poll()` and never knows whether the input came from the keyboard or the FPGA. Only the keyboard path exists today.

## Project Structure

```
fpga_console/
├── README.md
└── src/
    ├── controller.h    Controller interface and button bit masks
    ├── controller.c    Keyboard backend
    └── snake.c         Snake game and main loop
```

## Getting Started

### Requirements

- Windows 10 or 11
- MinGW-w64 GCC, installed with:
  ```
  winget install BrechtSanders.WinLibs.POSIX.UCRT
  ```
- VS Code (optional, any terminal works)
- No external libraries

Check the compiler with `gcc --version`.

### Build and Run

From the project folder (the one containing `src`), run these two commands one after the other:

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
| Select game (reserved) | Tab |
| Quit | Esc |

Keyboard input is read globally by Windows, so keep the game window focused during the demo.

## Controller Button Bit Mask

All button states are packed into one byte. The same layout will be used in the FPGA packet later.

| Bit | Mask | Button |
|---|---|---|
| 0 | `0x01` | UP |
| 1 | `0x02` | DOWN |
| 2 | `0x04` | LEFT |
| 3 | `0x08` | RIGHT |
| 4 | `0x10` | START |
| 5 | `0x20` | RESTART |
| 6 | `0x40` | SELECT |
| 7 | `0x80` | QUIT (keyboard only, Esc) |

Example: UP and RIGHT pressed together gives `0x01 | 0x08 = 0x09`.

## How the Code Works

### `src/controller.h` - the controller interface

This header is the contract between the game and any input device.

- **Button masks** (`BTN_UP`, `BTN_DOWN`, ...) use one bit per button, so any combination fits in a single `uint8_t`.
- **`ControllerMode`** lists the available input backends. It currently has only `CTRL_KEYBOARD`, and `CTRL_SERIAL` will be added with the FPGA link.
- **`controller_init(mode, port, baud)`** selects a backend and returns 1 on success or 0 on failure. The keyboard ignores `port` and `baud`. They are reserved for the serial backend (for example `"COM3"` and `115200`).
- **`controller_poll()`** returns the current button state as a bit mask (1 = pressed).
- **`controller_close()`** releases resources. It does nothing for the keyboard and will close the serial port later.

### `src/controller.c` - the keyboard backend

- Uses the Windows function `GetAsyncKeyState()` to check whether a key is held down.
- `controller_poll()` starts at 0 and ORs in one button bit for each key that is down. For example, either the Up arrow or `W` sets `BTN_UP`.
- Adding the FPGA later only needs a second branch in this file. `snake.c` stays unchanged.

### `src/snake.c` - the game

**Data**

- The board is 40 by 20 cells.
- The snake is an array of points, `body[]`. `body[0]` is the head and `len` is the length.
- `dir` is the direction for the next move. `moved` is the direction of the last completed move, used to block reversals.
- The game has three states: title, playing, and game over.

**Functions**

| Function | What it does |
|---|---|
| `on_snake(x, y)` | Returns 1 if a cell is occupied by the snake |
| `place_food()` | Picks random cells until it finds one that is free |
| `reset_game()` | Starts a length-3 snake in the middle heading right, clears the score, places food |
| `step()` | Advances the game by one move (details below) |
| `draw()` | Builds the whole screen in one buffer and writes it in a single call |
| `main()` | Sets up the console and controller, then runs the main loop |

**What `step()` does**

1. Computes the new head position from `dir` and records it in `moved`.
2. Ends the game if the head leaves the board.
3. Checks for self-collision. When the snake is not eating, the tail cell is skipped because the tail moves away on the same turn.
4. If food is eaten, the snake grows by one and the score rises by 10.
5. Shifts the body with `memmove()` and puts the new head at the front.
6. If the snake fills the whole board, the player wins.

**Main loop (every 10 ms)**

1. Read the button byte with `controller_poll()`.
2. Detect new presses with `pressed = b & ~prev`. A button counts only on the frame where it goes from released to held, so holding Enter does not restart the game repeatedly.
3. Handle the current state:
   - **Title:** Enter starts the game.
   - **Playing:** R restarts. A direction is accepted only if it is not the opposite of `moved`. The game steps when enough time has passed.
   - **Game over:** Enter or R restarts, and the high score is kept.
4. Redraw only when something changed.

**Difficulty:** the delay between moves is `140 - (len - 3) * 3` ms, with a minimum of 60 ms, so the snake speeds up as it grows.

**Timing:** the game uses `GetTickCount()`. The unsigned subtraction `now - last` stays correct even when the 32-bit timer wraps around.

**Rendering:** `draw()` moves the cursor to the top-left and rewrites the screen in place with one `WriteConsoleA()` call. This avoids the flicker caused by clearing the screen every frame.

## Testing (Keyboard Mode)

1. The title screen appears and Enter starts the game.
2. Arrow keys and WASD steer the snake, and the opposite direction does nothing.
3. Eating `*` adds 10 points and the snake speeds up.
4. Hitting a wall or the snake's own body shows GAME OVER. Enter or R restarts, and the high score stays.
5. Esc exits cleanly.

## Roadmap

- [x] **Stage 1:** Snake game and keyboard controller
- [ ] **Stage 2:** Windows serial port backend and packet protocol (start byte, button byte, checksum, timeout), plus a keyboard-driven fake FPGA mode to test packet parsing without hardware
- [ ] **Stage 3:** Verilog modules for the debouncer, button encoder, and UART transmitter, with a testbench
- [ ] **Stage 4:** Board-specific pin constraints and wiring, once the exact board model and its USB-UART capability are confirmed
- [ ] **Stage 5:** Car Racing game on the same controller interface
- [ ] **Stage 6:** Demo script, viva questions and answers, final documentation

## Current Limitations

- Console graphics only (ASCII characters).
- Keyboard input is read globally, so keys pressed in other windows also affect the game.
- No FPGA communication exists yet. Hardware mode needs the planned serial backend and a board with a supported USB-UART interface.
- Only Snake is implemented so far.