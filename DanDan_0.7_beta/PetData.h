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

struct Hat {
  const char* name;
  const unsigned char* sprite;
  int cost;
};

struct DanDan {
  char name[6] = "Dan";
  int hunger = 80;
  int happiness = 80;
  int cleanliness = 80;
  int coins = 50;
  StatusEffect status = NONE;
  unsigned long lastStatDecay = 0;
  int currentHat = 0; // 0 = No Hat, 1 = Wizard
};

#define FOOD_ITEMS_COUNT 3
extern FoodItem foodItems[FOOD_ITEMS_COUNT];

// HAT LIST DEFINITION
#define HAT_COUNT 2
static const Hat hatList[HAT_COUNT] = {
  {"None", nullptr, 0},
  {"Wizard", wizard_hat, 0} 
};

#endif