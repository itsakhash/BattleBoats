# BattleBoats

A two-player, Battleship-style game built for the STM32 NUCLEO-F411RE. Two Nucleo boards communicate over UART using a custom checksummed message protocol, negotiate turn order through a cryptographic commitment scheme, and play a full game via an event-driven finite-state-machine agent.

The project includes both a fully automated AI agent and a human-playable agent with physical button/switch controls and live OLED feedback.

## Features

- **Custom communication protocol** — an NMEA-style, XOR-checksummed message format (`$TYPE,data*XX\r\n`) with a character-by-character decoder state machine that detects and recovers from malformed or corrupted messages
- **Fair turn-order negotiation** — a coin-flip commitment scheme lets two agents agree on who attacks first without either side being able to cheat, using a one-way hash to commit to a secret value before revealing it
- **Event-driven agent state machine** — models the full game lifecycle (negotiation → attacking/defending → victory/defeat) as a single, testable state machine driven entirely by discrete events
- **AI agent** — automatically places ships and makes random, non-repeating guesses
- **Human-playable agent (extra credit)** — manual ship placement with a live footprint preview, a physical switch for choosing ship orientation, and cursor-based target selection during gameplay, with software debouncing on all button input
- **Extensive test coverage** — dedicated unit test suites for each module, run directly on hardware

## Architecture

The project is split into four independent modules, each with its own header-defined interface:

| Module | Responsibility |
|---|---|
| `Message` | Encodes/decodes the wire protocol; computes and validates checksums |
| `Field` | Tracks each player's board, ship placement, and hit/miss/sink logic |
| `Negotiation` | Implements the coin-flip commitment scheme for fair turn order |
| `Agent` / `HumanAgent` | The top-level state machine tying the other modules together |

A separate `Lab10_main.c` handles the hardware event loop — polling buttons, driving the transmission service, and dispatching events into the active agent.

## Hardware

- 2x STM32 NUCLEO-F411RE development boards
- UCSC ECE13 I/O shield (buttons, switches, OLED display)
- 3 wires to connect two boards' UART pins (TX↔RX crossed, plus shared ground) for board-to-board play

## Repository Structure

    .
    ├── Common/          # Shared course libraries (BOARD, Buttons, Leds, Oled, etc.)
    └── Lab10/
        ├── src/         # All module implementations and test harnesses
        ├── include/     # Header files defining each module's interface
        ├── objs/        # Precompiled reference objects for incremental development
        └── platformio.ini

`Lab10` depends on `Common` via a relative path, so both folders must remain siblings for the project to build.

## Building and Running

This project uses [PlatformIO](https://platformio.org/). From inside `Lab10/`:

```
pio run --environment Lab10 --target upload
```
Builds and flashes the AI vs. AI game.

```
pio run --environment Lab10_ec --target upload
```
Builds and flashes the human-playable version.

Additional environments exist for running each module's test suite independently (e.g. `MessageTest`, `FieldTest`, `AgentTest`, `NegotiationTest`) — see `platformio.ini` for the full list.

## Testing

Every core module has a dedicated test harness that runs on real hardware and reports results over serial:

```
pio run --environment <TestEnvironmentName> --target upload
pio device monitor -b 115200
```

Tests are written against each module's documented header behavior rather than any single implementation's internal details, so the same test files verify multiple implementations of the same interface.

## Playing a Game

1. Flash the desired firmware to both boards and wire them together over UART
2. On **one** board only, press the Start button to send a challenge — the other board should respond automatically once it receives it (do **not** press Start on both boards, or neither will accept the other's challenge)
3. Follow the on-screen prompts to place ships and take turns
