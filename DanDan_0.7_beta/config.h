#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware pins
#define LEFT_BUTTON 1
#define MIDDLE_BUTTON 2
#define RIGHT_BUTTON 3
#define BUZZER 0

// Display constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Game constants
const unsigned long STAT_DECAY_INTERVAL = 10000;
const unsigned long FRAME_RATE = 33; // Lower frame rate slightly for Raycast calc
const int MAX_STAT = 100;
const int MIN_STAT = 0;

// Enums
enum GameState { 
  MAIN_MENU, PLAYING, HANG_OUT, SHOP_BUYING, TAX_FRAUD, 
  BATHE, STAT_SCREEN, SAVING, INVENTORY, FEEDING_SELECT, 
  SHOP_CONFIRM, FEEDING_ACTION, WARDROBE, DUNGEON // <--- Added DUNGEON
};

enum StatusEffect { NONE, MANIC, HUNGRY, SAD, DIRTY };

// Dungeon Constants
#define MAP_WIDTH 12
#define MAP_HEIGHT 12
#define TILE_EMPTY 0
#define TILE_WALL 1
#define TILE_GOAL 2 // The Ice Cream

#endif