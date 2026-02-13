#ifndef PETDATA_H
#define PETDATA_H

#include "Config.h"
#include "sprites.h"

struct FoodItem {
  const char* name;
  const unsigned char* sprite;
  int cost;
  int hungerValue;
  int quantity;
};

struct DanDan {
  char name[6] = "Dan";
  int hunger = 80;
  int happiness = 80;
  int cleanliness = 80;
  int coins = 50;
  StatusEffect status = NONE;
  unsigned long lastStatDecay = 0;
};

#define FOOD_ITEMS_COUNT 3
extern FoodItem foodItems[FOOD_ITEMS_COUNT];

#endif