# DanDan

An ESP32-based virtual pet, built as a personal side project outside of coursework — inspired by classic Tamagotchi-style devices, with a few unexpected extras.

**Status:** work in progress (v0.7 beta)

## Hardware

- ESP32 microcontroller
- SSD1306 OLED display (128×64)
- 3 physical buttons (navigation/action) + buzzer for sound feedback
- Persistent storage via ESP32 `Preferences` (flash-backed key-value store) for save/load

## Features

- **Core pet simulation** — hunger, happiness, and cleanliness stats that decay over time, with status effects (hungry, sad, dirty, manic) triggered by neglect
- **Shop & inventory** — buy food and items with in-game currency
- **Wardrobe** — collectible hats, rendered as sprites on the pet
- **Mini-games:**
  - *Whack-a-Mole* — reflex-based scoring game with an escalating difficulty curve
  - *Tax Fraud* — a lane-based endless runner with increasing speed
  - *Dungeon* — a tile-based dungeon crawler rendered with a **custom raycasting engine** (camera plane + direction vector projection, in the style of Wolfenstein 3D), running in real time on the ESP32's limited hardware
- **Persistent save system** — pet state and high scores survive power-off via on-device flash storage
- **Custom sprite system** — hand-defined bitmap sprites for the pet, items, and hats

## Architecture

| File | Purpose |
|---|---|
| `DanDan_0.7_beta.ino` | Main game loop, state machine (menu, feeding, shop, mini-games, dungeon), display rendering, input handling |
| `PetData.h` / `PetData.cpp` | Pet data structures (stats, food items, hats) and their default values |
| `config.h` | Hardware pin mapping, display constants, game-wide constants, and enums for game states / status effects |
| `sprites.h` | Bitmap sprite definitions |

The game runs on a fixed-rate update loop (frame timing decoupled from stat decay, which runs on its own independent timer for consistency regardless of frame rate), with a state machine (`GameState` enum) driving navigation between menu, mini-games, and the dungeon mode.

## Notes

This is an active side project, built and iterated on for fun rather than for a course — current focus areas include balancing the mini-games and expanding the dungeon mode beyond a single level.
