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
const unsigned long FRAME_RATE = 66;
const int MAX_STAT = 100;
const int MIN_STAT = 0;

// Enums
enum GameState { 
  MAIN_MENU, PLAYING, HANG_OUT, SHOP_BUYING, TAX_FRAUD, 
  BATHE, STAT_SCREEN, SAVING, INVENTORY, FEEDING_SELECT, 
  SHOP_CONFIRM, FEEDING_ACTION 
};

enum StatusEffect { NONE, MANIC, HUNGRY, SAD, DIRTY };

#endif