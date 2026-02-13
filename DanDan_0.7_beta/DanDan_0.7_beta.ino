#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "config.h"
#include "PetData.h"
#include "sprites.h"

// Prototyping
void decayStats(int cycles = 1);
void updateStatusEffects();
void saveGame();
void loadGame();
void handleInput(unsigned long currentTime);
void updateDisplay();
void playSound(int type);
void startFeeding();
void startPlaying();
void endMiniGame();
void showActionMessage(String message);
void handleMoleHit();
void hangOut();
void commitTaxFraud();
void bathe();
void autoSave();
void drawActionScreen();
void initDungeon();
void drawDungeon();

// Global Instances
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DanDan pet;
Preferences preferences;
GameState gameState = MAIN_MENU;

// Menu Items
const char* menuItems[] = {"Feed", "Play", "Hang Out", "Shop", "Tax Fraud", "Bathe", "Wardrobe", "Status", "Inventory", "Save", "Dungeon"};
const int menuLength = 11;
int currentMenuItem = 0;

// Timing & Game Variables
unsigned long lastFrameTime = 0;
int gameScore = 0;
bool gameActive = false;
unsigned long saveStartTime = 0;

// Walking & State
float petX = 48.0;
float walkDir = 0.2;
int petState = 0; 
unsigned long stateChangeTime = 0;
int stateDuration = 2000;

GameState targetState = MAIN_MENU;
float transitionOffset = 0;
bool isTransitioning = false;
bool playScreenWipe = false;
int wipeProgress = 0;

// Interactions
int currentShopItem = 0;
int currentInventoryItem = 0;
int currentHatSelection = 0; // For Wardrobe navigation
int selectedFoodIndex = -1;
unsigned long actionStartTime = 0;
String actionMessage = "";

// Whack-a-Mole
const int MOLE_POSITIONS[3] = {20, 54, 88};
const unsigned long GAME_DURATION = 30000;
const unsigned long COUNTDOWN_DURATION = 3000;
int molePosition = -1;
int moleType = 0;
unsigned long moleAppearTime = 0;
unsigned long moleVisibleDuration = 1500;
unsigned long nextMoleTime = 0;
int countdownState = 0; 
unsigned long gameStartTime = 0;
int highScore = 0;
bool moleHit = false;

// Tax Fraud
int playerLane = 1; 
float obstacleX = 128;        
int obstacleLane = 1;         
float runSpeed = 2.0;         
const int LANE_Y[3] = {18, 32, 46}; 
unsigned long lastSpeedIncrease = 0;

// --- DUNGEON VARIABLES ---
uint8_t worldMap[MAP_WIDTH][MAP_HEIGHT];
float posX = 2, posY = 2;      // Player Position
float dirX = -1, dirY = 0;     // Direction Vector
float planeX = 0, planeY = 0.66; // Camera Plane (FOV)
unsigned long dungeonStartTime = 0;
const int DUNGEON_TIME_LIMIT = 20; // Seconds
bool dungeonWin = false;
int dungeonLevel = 1; 
const int DUNGEON_MAX_SIZE = 16; // Adjust based on your worldMap size

void setup() {
  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(35, 28);
  display.print("DANDAN OS");
  display.display();
  delay(1000);

  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  preferences.begin("dandan", false);
  loadGame();
  highScore = preferences.getInt("highScore", 0);
  updateDisplay();
}

void loop() {
  unsigned long currentTime = millis();

  // 1. Stats Decay (Remains independent of frame rate for accuracy)
  if (currentTime - pet.lastStatDecay >= STAT_DECAY_INTERVAL) {
    decayStats();
    updateStatusEffects();
    pet.lastStatDecay = currentTime;
  }

  // 2. Logic & Timers (Synchronized to FRAME_RATE)
  if (currentTime - lastFrameTime >= FRAME_RATE) {
    lastFrameTime = currentTime;

    // --- THE FIX: INPUT NOW ONLY HAPPENS ONCE PER FRAME ---
    handleInput(currentTime);

    // --- TAX FRAUD LOGIC ---
    if (gameState == TAX_FRAUD) {
      if (countdownState == 1) {
        if (currentTime - gameStartTime > COUNTDOWN_DURATION) {
          countdownState = 2; gameActive = true; playSound(4);
        }
      } else if (countdownState == 2 && gameActive) {
        obstacleX -= runSpeed;
        if (currentTime - lastSpeedIncrease > 3000) { runSpeed += 0.1; lastSpeedIncrease = currentTime; }

        // Collision logic
        if (obstacleX < 28 && obstacleX > 12) { 
          if (playerLane == obstacleLane) {
            gameActive = false; 
            playSound(6); 
            pet.coins = max(0, pet.coins - 20);
            pet.happiness = max(0, pet.happiness - 10);
            autoSave(); 
            gameState = MAIN_MENU; 
            isTransitioning = false; 
            return; 
          }
        }
        
        if (obstacleX < -20) {
          obstacleX = 130; 
          obstacleLane = random(0, 3);
          gameScore++; 
          pet.coins += 5; 
          pet.happiness += 2;
          playSound(5); 
          autoSave(); 
        }
      }
    }

    // --- WHACK-A-MOLE LOGIC ---
    if (gameState == PLAYING && gameActive) {
      unsigned long elapsed = currentTime - gameStartTime;
      if (countdownState == 1) {
        if (elapsed > COUNTDOWN_DURATION) { countdownState = 2; gameStartTime = currentTime; nextMoleTime = currentTime; playSound(4); }
      } else if (countdownState == 2) {
        if (elapsed >= GAME_DURATION) endMiniGame(); 
        else {
          if (molePosition == -1 && currentTime >= nextMoleTime) {
            molePosition = random(0, 3); moleType = random(0, 2); moleAppearTime = currentTime; moleHit = false;
          }
          if (molePosition != -1 && !moleHit && currentTime - moleAppearTime > moleVisibleDuration) {
            if (moleType == 0) { gameScore = max(0, gameScore - 1); playSound(6); }
            molePosition = -1; nextMoleTime = currentTime + random(500, 1000);
          }
        }
      }
    }

    // State Transitions
    if (gameState == FEEDING_ACTION && currentTime - actionStartTime > 2000) gameState = MAIN_MENU;
    if (gameState == SHOP_CONFIRM && currentTime - actionStartTime > 2000) gameState = SHOP_BUYING;
    if (gameState == SAVING && currentTime - saveStartTime > 1000) gameState = MAIN_MENU;

    updateDisplay();
  }
}


void handleInput(unsigned long currentTime) {
  static unsigned long lastLeftPress = 0;
  static unsigned long lastMiddlePress = 0;
  static unsigned long lastRightPress = 0;

  if (isTransitioning) return;

  // Movement speed constants for the Dungeon
  const float moveSpeed = 0.3;
  const float rotSpeed = 0.1;

  // --- LEFT BUTTON ---
  if (digitalRead(LEFT_BUTTON) == LOW) {
    if (currentTime - lastLeftPress > 150) { // Standard debounce for menus
      if (gameState == MAIN_MENU) { currentMenuItem = (currentMenuItem - 1 + menuLength) % menuLength; playSound(0); }
      else if (gameState == TAX_FRAUD) { playerLane = 0; playSound(0); }
      else if (gameState == WARDROBE) { currentHatSelection = (currentHatSelection - 1 + HAT_COUNT) % HAT_COUNT; playSound(0); }
      else if (gameState == SHOP_BUYING) { currentShopItem = (currentShopItem - 1 + FOOD_ITEMS_COUNT) % FOOD_ITEMS_COUNT; playSound(0); }
      else if (gameState == FEEDING_SELECT) { currentInventoryItem = (currentInventoryItem - 1 + FOOD_ITEMS_COUNT) % FOOD_ITEMS_COUNT; playSound(0); }
      else if (gameState == PLAYING && gameActive && countdownState == 2) {
          if (molePosition == 0) handleMoleHit(); else { gameScore = max(0, gameScore - 1); playSound(6); }
      }
      lastLeftPress = currentTime;
    }

    // CONTINUOUS INPUT for Dungeon Rotation (Left)
    if (gameState == DUNGEON) {
      float oldDirX = dirX;
      dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
      dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
      float oldPlaneX = planeX;
      planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
      planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
  }

  // --- MIDDLE BUTTON ---
  if (digitalRead(MIDDLE_BUTTON) == LOW) {
    if (currentTime - lastMiddlePress > 200) {
      if (gameState == MAIN_MENU) {
        isTransitioning = true; gameScore = 0;
        switch (currentMenuItem) {
          case 0: targetState = FEEDING_SELECT; break;
          case 1: startPlaying(); isTransitioning = false; break;
          case 2: hangOut(); isTransitioning = false; break;
          case 3: targetState = SHOP_BUYING; break;
          case 4: targetState = TAX_FRAUD; gameActive = false; countdownState = 1; gameStartTime = millis(); break; 
          case 5: bathe(); isTransitioning = false; break;
          case 6: targetState = WARDROBE; currentHatSelection = pet.currentHat; break;
          case 7: targetState = STAT_SCREEN; break;
          case 8: targetState = INVENTORY; break;
          case 9: saveGame(); gameState = SAVING; saveStartTime = millis(); isTransitioning = false; break;
          case 10:
            initDungeon(); // Generates the walls and goal
            targetState = DUNGEON; 
            break;
        }
        playSound(0);
      } 
      else if (gameState == WARDROBE) {
        pet.currentHat = currentHatSelection;
        showActionMessage("Equipped " + String(hatList[currentHatSelection].name));
        autoSave();
        gameState = MAIN_MENU;
        playSound(0);
      }
      else if (gameState == TAX_FRAUD) { playerLane = 1; playSound(0); }
      else if (gameState == SHOP_BUYING) {
        if (pet.coins >= foodItems[currentShopItem].cost) {
          pet.coins -= foodItems[currentShopItem].cost; 
          foodItems[currentShopItem].quantity++;
          showActionMessage("Bought " + String(foodItems[currentShopItem].name)); 
          autoSave();
          gameState = MAIN_MENU;
        } else { playSound(6); gameState = MAIN_MENU; }
      }
      else if (gameState == FEEDING_SELECT) {
        if (foodItems[currentInventoryItem].quantity > 0) {
          selectedFoodIndex = currentInventoryItem;
          showActionMessage("Danielo eats " + String(foodItems[selectedFoodIndex].name)); 
          startFeeding();
        } else { playSound(6); }
      }
      else if (gameState == PLAYING) {
        if (!gameActive) gameState = MAIN_MENU;
        else if (countdownState == 2 && molePosition == 1) handleMoleHit();
        playSound(0);
      }
      else if (gameState != DUNGEON) { // Generic back to menu
         gameState = (gameState == SHOP_CONFIRM) ? SHOP_BUYING : MAIN_MENU;
         playSound(0);
      }
      lastMiddlePress = currentTime;
    }

    // CONTINUOUS INPUT for Dungeon Movement (Forward)
    if (gameState == DUNGEON) {
      // 1. Calculate next positions
      int nextX = (int)(posX + dirX * moveSpeed);
      int nextY = (int)(posY + dirY * moveSpeed);

      // 2. Allow movement if the tile ISN'T a wall (allows Empty AND Goal tiles)
      if(worldMap[nextX][(int)posY] != TILE_WALL) posX += dirX * moveSpeed;
      if(worldMap[(int)posX][nextY] != TILE_WALL) posY += dirY * moveSpeed;
      
      if(worldMap[(int)posX][(int)posY] == TILE_GOAL) {
    playSound(1);
    pet.happiness = constrain(pet.happiness + 20, 0, 100);
    pet.coins += (10 * dungeonLevel); // More coins for harder levels!
    
    dungeonLevel++; // <--- LEVEL UP!
    
    showActionMessage("LEVEL " + String(dungeonLevel-1) + " CLEAR!");
    gameState = MAIN_MENU; 
    autoSave();
      }
    }
  }

  // --- RIGHT BUTTON ---
  if (digitalRead(RIGHT_BUTTON) == LOW) {
    if (currentTime - lastRightPress > 150) {
      if (gameState == MAIN_MENU) { currentMenuItem = (currentMenuItem + 1) % menuLength; playSound(0); }
      else if (gameState == TAX_FRAUD) { playerLane = 2; playSound(0); }
      else if (gameState == WARDROBE) { currentHatSelection = (currentHatSelection + 1) % HAT_COUNT; playSound(0); }
      else if (gameState == SHOP_BUYING) { currentShopItem = (currentShopItem + 1) % FOOD_ITEMS_COUNT; playSound(0); }
      else if (gameState == FEEDING_SELECT) { currentInventoryItem = (currentInventoryItem + 1) % FOOD_ITEMS_COUNT; playSound(0); }
      else if (gameState == PLAYING && gameActive && countdownState == 2) {
        if (molePosition == 2) handleMoleHit(); else { gameScore = max(0, gameScore - 1); playSound(6); }
      }
      lastRightPress = currentTime;
    }

    // CONTINUOUS INPUT for Dungeon Rotation (Right)
    if (gameState == DUNGEON) {
      float oldDirX = dirX;
      dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
      dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
      float oldPlaneX = planeX;
      planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
      planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }
  }
}
// Helpers
void saveGame() { 
  preferences.putInt("hunger", pet.hunger); 
  preferences.putInt("happiness", pet.happiness); 
  preferences.putInt("cleanliness", pet.cleanliness); 
  preferences.putInt("coins", pet.coins); 
  preferences.putInt("currentHat", pet.currentHat); // Save Hat
  preferences.putChar("status", (char)pet.status); 
  for (int i = 0; i < FOOD_ITEMS_COUNT; i++) { String key = "itemQty" + String(i); preferences.putInt(key.c_str(), foodItems[i].quantity); } 
}
void autoSave() { saveGame(); }
void loadGame() { 
  if (preferences.isKey("coins")) { 
    pet.hunger = preferences.getInt("hunger", 80); 
    pet.happiness = preferences.getInt("happiness", 80); 
    pet.cleanliness = preferences.getInt("cleanliness", 80); 
    pet.coins = preferences.getInt("coins", 50); 
    pet.currentHat = preferences.getInt("currentHat", 0); // Load Hat
    pet.status = (StatusEffect)preferences.getChar("status", NONE); 
    for (int i = 0; i < FOOD_ITEMS_COUNT; i++) { String key = "itemQty" + String(i); foodItems[i].quantity = preferences.getInt(key.c_str(), 0); } 
  } 
  pet.lastStatDecay = millis(); 
}
void decayStats(int cycles) { for (int i = 0; i < cycles; i++) { pet.hunger = constrain(pet.hunger - 2, 0, 100); pet.happiness = constrain(pet.happiness - 1, 0, 100); pet.cleanliness = constrain(pet.cleanliness - 1, 0, 100); } updateStatusEffects(); }
void updateStatusEffects() { if (pet.hunger <= 20) pet.status = HUNGRY; else if (pet.happiness <= 20) pet.status = SAD; else if (pet.cleanliness <= 20) pet.status = DIRTY; else if (pet.hunger > 90 && pet.happiness > 90 && pet.cleanliness > 90) pet.status = MANIC; else pet.status = NONE; }
void showActionMessage(String message) { actionMessage = message; actionStartTime = millis(); if (gameState == SHOP_BUYING) gameState = SHOP_CONFIRM; else if (gameState == FEEDING_SELECT) gameState = FEEDING_ACTION; }
void handleMoleHit() { moleHit = true; if (moleType == 0) { gameScore += 2; playSound(5); } else { gameScore = max(0, gameScore - 2); playSound(6); } molePosition = -1; nextMoleTime = millis() + random(300, 800); }
void drawScreenWipe() { display.fillRect(wipeProgress, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK); wipeProgress += 10; if (wipeProgress >= SCREEN_WIDTH) { playScreenWipe = false; wipeProgress = 0; } }
void startFeeding() { if (selectedFoodIndex != -1 && foodItems[selectedFoodIndex].quantity > 0) { foodItems[selectedFoodIndex].quantity--; pet.hunger = constrain(pet.hunger + foodItems[selectedFoodIndex].hungerValue, 0, 100); playSound(2); autoSave(); selectedFoodIndex = -1; } }
void startPlaying() { gameState = PLAYING; gameActive = true; gameScore = 0; molePosition = -1; moleVisibleDuration = 1500; countdownState = 1; gameStartTime = millis(); playSound(3); }
void endMiniGame() { gameActive = false; pet.happiness = constrain(pet.happiness + gameScore, 0, 100); pet.coins += gameScore / 2; playSound(2); autoSave(); if (gameScore > highScore) { highScore = gameScore; preferences.putInt("highScore", highScore); } }
void hangOut() { gameState = HANG_OUT; pet.happiness = constrain(pet.happiness + 25, 0, 100); playSound(1); autoSave(); }
void bathe() { gameState = BATHE; pet.cleanliness = constrain(pet.cleanliness + 30, 0, 100); playSound(2); autoSave(); }

// --- DRAWING FUNCTIONS ---

void updateDisplay() {
  display.clearDisplay();
  switch (gameState) {
    case MAIN_MENU: drawMainMenu(); break;
    case DUNGEON: drawDungeon(); break;
    case PLAYING: drawMiniGame(); break;
    case STAT_SCREEN: drawStatusScreen(); break;
    case SAVING: drawSavingScreen(); break;
    case SHOP_BUYING: drawShopBuyingScreen(); break;
    case INVENTORY: drawInventoryScreen(); break;
    case FEEDING_SELECT: drawFeedingSelectScreen(); break;
    case WARDROBE: drawWardrobeScreen(); break;
    case SHOP_CONFIRM: 
    case FEEDING_ACTION: drawActionMessageScreen(); break;
    default: drawActionScreen(); break;
  }
  if (playScreenWipe) drawScreenWipe();
  display.display();
}

void drawMainMenu() {
  unsigned long now = millis();
  if (isTransitioning) {
    transitionOffset += 7.0;
    if (transitionOffset >= SCREEN_WIDTH) {
      isTransitioning = false; transitionOffset = 0; gameState = targetState; 
      if (gameState == TAX_FRAUD) { obstacleX = 130; obstacleLane = random(0,3); runSpeed = 2.0; gameScore = 0; countdownState = 1; gameStartTime = millis(); }
      playScreenWipe = true; wipeProgress = 0; return; 
    }
  }

  // Icons
  display.setTextColor(SSD1306_WHITE);
  const int secW = SCREEN_WIDTH / 3;
  display.drawBitmap(secW * 0 + 2, 0, hunger_icon, 8, 8, 1);
  display.setCursor(secW * 0 + 12, 0); display.print(pet.hunger);
  display.drawBitmap(secW * 1 + 2, 0, happiness_icon, 8, 8, 1);
  display.setCursor(secW * 1 + 12, 0); display.print(pet.happiness);
  display.drawBitmap(secW * 2 + 2, 0, cleanliness_icon, 8, 8, 1);
  display.setCursor(secW * 2 + 12, 0); display.print(pet.cleanliness);

  // Walking & Logic
  if (!isTransitioning && (now - stateChangeTime > stateDuration)) {
    stateChangeTime = now; petState = random(0, 3); stateDuration = random(2000, 5000); 
    if (petState == 2) walkDir *= -1; 
  }
  if (!isTransitioning && petState == 0) { 
    petX += walkDir;
    if (petX <= 5 || petX >= 91) { walkDir *= -1; petState = 1; stateDuration = 1000; }
  }
  
  const unsigned char* selectedSprite;
  bool isBlinking = (now % 4000 < 150);
  if (isTransitioning || walkDir > 0) selectedSprite = isBlinking ? Danielo_blink_L : Danielo_sprite_L;
  else selectedSprite = isBlinking ? Danielo_blink : Danielo_sprite;

  int bounceY = (isTransitioning || petState == 0) ? ((now % 600 < 300) ? 2 : 0) : ((sin(now / 400.0) > 0) ? 1 : 0);
  float drawX = petX + transitionOffset;
  
// 1. Draw Danielo
  display.drawBitmap((int)drawX, 16 + bounceY, selectedSprite, 32, 32, 1);
  
 // 2. Draw Equipped Hat with Flipping and Offsets
if (pet.currentHat > 0 && pet.currentHat < HAT_COUNT) {
    const unsigned char* hatSprite;
    int xOffset = 0; // Start at 8 to center (32 - 16) / 2
    int yOffset = -3; // Your "perfect" y-offset

    // Based on your code: walkDir > 0 means Danielo is facing LEFT (_L sprites)
    if (walkDir > 0) {
        // Facing Left: Use flipped hat
        hatSprite = (pet.currentHat == 1) ? wizard_hat_flipped : hatList[pet.currentHat].sprite;
        xOffset = 13; // Nudge slightly left to match head tilt
    } else {
        // Facing Right: Use standard hat
        hatSprite = hatList[pet.currentHat].sprite;
        xOffset = 3; // Nudge slightly right to match head tilt
    }

    // CRITICAL: You must add xOffset to drawX here!
    display.drawBitmap((int)drawX + xOffset, 16 + bounceY + yOffset, hatSprite, 16, 16, 1);
}

    

  display.drawFastHLine(0, 49, 128, 1); 

  if (!isTransitioning) {
    const char* currentItem = menuItems[currentMenuItem];
    int pulse = sin(now / 150.0) * 3; 
    display.drawBitmap(10 - pulse, SCREEN_HEIGHT - 10, left_arrow, 8, 8, 1);
    display.drawBitmap(SCREEN_WIDTH - 18 + pulse, SCREEN_HEIGHT - 10, right_arrow, 8, 8, 1);
    int tW = strlen(currentItem) * 6;
    display.setCursor((SCREEN_WIDTH - tW) / 2, SCREEN_HEIGHT - 9);
    if (currentMenuItem == 4) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
    display.print(currentItem);
    display.setTextColor(SSD1306_WHITE);
  }
}

void drawWardrobeScreen() {
  // 1. Right Side: The Wardrobe Background Element
  // Positioned at x=64 to fill the right half of the 128px screen
  display.drawBitmap(64, 0, image_wardrobe_bits, 64, 64, SSD1306_WHITE);

  // 2. Left Side: Header & Context
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WARDROBE");

  // 3. Left Side: Selected Hat Sprite
  // Centered in the left 64-pixel block (x=32 is the center of the left half)
  int hatX = 32 - 8; // (Center of 64px) - (Half of 16px sprite)
  int hatY = 18;

  if (hatList[currentHatSelection].sprite != nullptr) {
      display.drawBitmap(hatX, hatY, hatList[currentHatSelection].sprite, 16, 16, SSD1306_WHITE);
  } else {
      // "No Hat" placeholder
      display.drawRect(hatX, hatY, 16, 16, SSD1306_WHITE);
      display.drawLine(hatX, hatY, hatX + 16, hatY + 16, SSD1306_WHITE);
  }

  // 4. Navigation Arrows (Matching the Shop's style/sprites)
  display.drawBitmap(5, 23, image_arrow_left_bits, 7, 5, SSD1306_WHITE);
  display.drawBitmap(52, 23, image_arrow_right_bits, 7, 5, SSD1306_WHITE);

  // 5. Left Side: Name Tag
  String name = hatList[currentHatSelection].name;
  int tW = name.length() * 6;
  // Centering text in the 0-64 range
  display.setCursor(max(0, (int)(32 - (tW / 2))), 38); 
  display.print(name);

  // 6. Horizontal Divider (Matching the Shop)
  display.drawFastHLine(0, 50, 60, SSD1306_WHITE);

  // 7. Equipped Status
  if (currentHatSelection == pet.currentHat) {
      display.setCursor(5, 54);
      display.print("[ EQUIPPED ]");
  } else {
      display.setCursor(12, 54);
      display.print("MIDDLE: Wear");
  }
}
void drawMiniGame() {
  display.setTextSize(1); display.setCursor(0, 0); display.print("Score: "); display.print(gameScore);
  if (countdownState == 1) {
    int count = 3 - (millis() - gameStartTime) / 1000;
    display.setTextSize(3); display.setCursor(SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2 - 15);
    if (count > 0) display.print(count); else display.print("GO!");
    display.setTextSize(1);
  } else if (countdownState == 2) {
    int timeLeft = (GAME_DURATION - (millis() - gameStartTime)) / 1000;
    display.setCursor(SCREEN_WIDTH - 30, 0); display.print(timeLeft);
    for (int i = 0; i < 3; i++) display.drawCircle(MOLE_POSITIONS[i], 40, 15, SSD1306_WHITE);
    if (molePosition != -1) {
      const unsigned char* sprite = moleType == 0 ? mole_good : mole_bad;
      display.drawBitmap(MOLE_POSITIONS[molePosition] - 8, 25, sprite, 16, 16, SSD1306_WHITE);
    }
  } else {
    display.setCursor(20, 20); display.setTextSize(2); display.print("SCORE:"); display.print(gameScore);
    display.setTextSize(1); display.setCursor(30, 55); display.print("Press MIDDLE");
  }
}

void drawStatusScreen() {
  display.setCursor(0, 10); display.print("Hunger: "); display.print(pet.hunger); display.print("%");
  display.setCursor(0, 25); display.print("Happiness: "); display.print(pet.happiness); display.print("%");
  display.setCursor(0, 40); display.print("Cleanliness: "); display.print(pet.cleanliness); display.print("%");
  display.setCursor(0, 55); display.print("Coins: "); display.print(pet.coins);
  display.setCursor(0, 65); display.print("Press Any Button");
}

void drawSavingScreen() { display.setTextSize(2); display.setCursor(SCREEN_WIDTH/2 - 24, SCREEN_HEIGHT/2 - 8); display.print("SAVING"); }

void drawShopBuyingScreen() {
  display.drawBitmap(64, 8, image_store_bits, 64, 64, SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(95, 2); display.print("$:"); display.print(pet.coins);
  display.drawBitmap(17, 4, foodItems[currentShopItem].sprite, 32, 32, SSD1306_WHITE);
  display.drawBitmap(5, 18, image_arrow_left_bits, 7, 5, SSD1306_WHITE);
  display.drawBitmap(54, 18, image_arrow_right_bits, 7, 5, SSD1306_WHITE);
  String name = foodItems[currentShopItem].name;
  display.setCursor(max(0, (int)(33 - ((name.length() * 6) / 2))), 38); display.print(name);
  display.drawFastHLine(0, 52, 60, SSD1306_WHITE); 
  String price = "$ " + String(foodItems[currentShopItem].cost);
  display.setCursor(max(0, (int)(33 - ((price.length() * 6) / 2))), 55); display.print(price);
}

void drawInventoryScreen() {
  display.setCursor(0, 0); display.print("INVENTORY");
  display.drawBitmap(SCREEN_WIDTH/2 - 16, 10, foodItems[currentInventoryItem].sprite, 32, 32, SSD1306_WHITE);
  display.setCursor(0, 42); display.print(foodItems[currentInventoryItem].name); display.print(" x"); display.print(foodItems[currentInventoryItem].quantity);
  display.setCursor(0, 52); display.print("Restores "); display.print(foodItems[currentInventoryItem].hungerValue); display.print(" hunger");
}

void drawFeedingSelectScreen() {
  display.setCursor(0, 0); display.print("SELECT FOOD");
  display.drawBitmap(SCREEN_WIDTH/2 - 16, 10, foodItems[currentInventoryItem].sprite, 32, 32, SSD1306_WHITE);
  display.setCursor(0, 42); display.print(foodItems[currentInventoryItem].name); display.print(" x"); display.print(foodItems[currentInventoryItem].quantity);
  display.setCursor(0, 52); display.print("Restores "); display.print(foodItems[currentInventoryItem].hungerValue); display.print(" hunger");
}

void drawActionScreen() {
  if (gameState == TAX_FRAUD) {
    if (countdownState == 1) {
      int count = 3 - (millis() - gameStartTime) / 1000;
      display.setCursor(35, 15); display.print("TAX FRAUD");
      display.setTextSize(3); display.setCursor(55, 30);
      if (count > 0) display.print(count); else display.print("GO!");
      display.setTextSize(1);
    } else {
      display.drawFastHLine(0, 28, 128, 1);
      display.drawFastHLine(0, 42, 128, 1);
      display.drawFastHLine(0, 56, 128, 1);
      // Draw Danielo in Tax Fraud (Add Hat here too!)
      display.drawBitmap(10, LANE_Y[playerLane]-8, Danielo_sprite_L, 32, 32, 1);
       if (pet.currentHat > 0) display.drawBitmap(10, LANE_Y[playerLane]-8 - 6, hatList[pet.currentHat].sprite, 16, 16, 1);
       
      display.fillRect((int)obstacleX, LANE_Y[obstacleLane]+4, 12, 12, 1);
      display.setCursor((int)obstacleX + 3, LANE_Y[obstacleLane]+6);
      display.setTextColor(0); display.print("!"); display.setTextColor(1);
      display.setCursor(0, 0); display.print("Dodged: "); display.print(gameScore);
    }
  } else {
    display.setTextSize(2); display.setCursor(10, 20);
    if(gameState == HANG_OUT) display.print("HANG OUT");
    else if(gameState == BATHE) display.print("BATHING");
    display.setTextSize(1); display.setCursor(25, 55); display.print("Press MIDDLE");
  }
}

void drawActionMessageScreen() {
  int xPos = (SCREEN_WIDTH - (actionMessage.length() * 6)) / 2;
  display.setCursor(max(0, xPos), 20); display.print(actionMessage);
  display.setCursor(0, SCREEN_HEIGHT - 10); display.print("Press any button");
}

void playSound(int type) {
  switch (type) {
    case 0: tone(BUZZER, 1000, 100); break;                                 // Nav click
    case 1: tone(BUZZER, 1500, 100); delay(100); tone(BUZZER, 2000, 100); break; // Happy/Success
    case 2: tone(BUZZER, 2000, 50); delay(50); tone(BUZZER, 2500, 50); break;   // Eating/Bathe
    case 3: tone(BUZZER, 800, 200); break;                                  // Game Start
    case 4: tone(BUZZER, 1500, 300); break;                                 // Go!
    case 5: tone(BUZZER, 1200, 100); delay(50); tone(BUZZER, 1800, 150); break; // Point score
    case 6: tone(BUZZER, 400, 300); break;                                  // Fail/Error
  }
}

void initDungeon() {
  // 1. Calculate Dynamic Size based on level
  // Starts at 6x6 and expands, but never exceeds the array bounds (MAP_WIDTH/HEIGHT)
  int activeWidth = constrain(6 + dungeonLevel, 6, MAP_WIDTH);
  int activeHeight = constrain(6 + dungeonLevel, 6, MAP_HEIGHT);

  // 2. Fill with Walls
  for (int x = 0; x < MAP_WIDTH; x++) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
      // If outside the current "level size", fill with solid walls
      if (x >= activeWidth || y >= activeHeight) {
        worldMap[x][y] = TILE_WALL;
        continue;
      }

      // Borders for the current active area
      if (x == 0 || x == activeWidth - 1 || y == 0 || y == activeHeight - 1) {
        worldMap[x][y] = TILE_WALL;
      } else {
        // Wall density increases slightly as you level up (capped at 35%)
        int wallChance = constrain(15 + dungeonLevel, 15, 35);
        worldMap[x][y] = (random(0, 100) < wallChance) ? TILE_WALL : TILE_EMPTY;
      }
    }
  }

  // 3. Place Player (ensure empty spot at start)
  posX = 1.5; posY = 1.5;
  worldMap[1][1] = TILE_EMPTY;
  worldMap[2][1] = TILE_EMPTY; 

  // 4. Place Ice Cream Goal (Near the far corner of the current active area)
  int goalX = activeWidth - 2;
  int goalY = activeHeight - 2;
  worldMap[goalX][goalY] = TILE_GOAL;
  
  // Force a clear path around the goal
  worldMap[goalX-1][goalY] = TILE_EMPTY;
  worldMap[goalX][goalY-1] = TILE_EMPTY;

  // Reset Vectors (Facing into the maze)
  dirX = 1; dirY = 0;
  planeX = 0; planeY = 0.66;
  
  dungeonStartTime = millis();
  dungeonWin = false;
}

void drawDungeon() {
  // 1. Timer Logic
  int timeLeft = 20 - (millis() - dungeonStartTime) / 1000;
  if (timeLeft <= 0) {
    dungeonLevel = 1; 
    gameState = MAIN_MENU; 
    playSound(6); 
    showActionMessage("TIME'S UP! LVL 1"); 
    return;
  }

  // 2. Horizon Line (Grounding the 3D space)
  display.drawFastHLine(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SSD1306_WHITE);

  // 3. RAYCASTING LOOP
  for (int x = 0; x < SCREEN_WIDTH; x += 2) { 
    float cameraX = 2 * x / (float)SCREEN_WIDTH - 1; 
    float rayDirX = dirX + planeX * cameraX;
    float rayDirY = dirY + planeY * cameraX;

    int mapX = int(posX); 
    int mapY = int(posY);
    
    float deltaDistX = (rayDirX == 0) ? 1e30 : std::abs(1 / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : std::abs(1 / rayDirY);
    float sideDistX, sideDistY, perpWallDist;
    int stepX, stepY, hit = 0, side; 

    if (rayDirX < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaDistX; }
    else { stepX = 1; sideDistX = (mapX + 1.0 - posX) * deltaDistX; }
    if (rayDirY < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaDistY; }
    else { stepY = 1; sideDistY = (mapY + 1.0 - posY) * deltaDistY; }

    while (hit == 0) {
      if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
      else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
      if (worldMap[mapX][mapY] > 0) hit = 1;
    }

    perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
    int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
    int drawStart = max(0, -lineHeight / 2 + SCREEN_HEIGHT / 2);
    int drawEnd = min(SCREEN_HEIGHT - 1, lineHeight / 2 + SCREEN_HEIGHT / 2);

    // --- TEXTURE SELECTION ---
    double wallX = (side == 0) ? (posY + perpWallDist * rayDirY) : (posX + perpWallDist * rayDirX);
    wallX -= floor(wallX);
    
    const uint8_t* texPtr;
    int texSize = 16; 

    if (worldMap[mapX][mapY] == TILE_GOAL) {
        texPtr = ice_cream_sprite;
        texSize = 32;
    } else {
        // Randomize wall texture based on map position
        if ((mapX + mapY) % 3 == 0) {
            texPtr = wall_cracked; 
        } else {
            texPtr = wall_texture; 
        }
    }

    int texX = int(wallX * (double)texSize);
    if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0)) texX = texSize - texX - 1;

    float step = 1.0 * texSize / lineHeight;
    float texPos = (drawStart - SCREEN_HEIGHT / 2 + lineHeight / 2) * step;

    // --- VERTICAL RENDERING LOOP ---
    for (int y = drawStart; y < drawEnd; y++) {
      int texY = (int)texPos & (texSize - 1);
      texPos += step;
      
      bool shouldDraw = true;

      // A. DISTANCE DITHERING (Fog effect for distant walls)
      if (perpWallDist > 4.5 && (x + y) % 2 == 0) {
          shouldDraw = false; 
      }

      // B. DIRECTIONAL SHADING (Artificial shadows for E/W walls)
      // This creates depth by making side-facing walls "dimmer"
      if (side == 1 && (y % 2 == 0)) {
          shouldDraw = false;
      }

      if (shouldDraw) {
        int byteIndex = (texX + texY * texSize) / 8;
        uint8_t byteVal = pgm_read_byte(&texPtr[byteIndex]);
        
        if (byteVal & (0x80 >> (texX % 8))) {
          display.drawPixel(x, y, SSD1306_WHITE);
          display.drawPixel(x+1, y, SSD1306_WHITE);
        }
      }
    }
  }

  // 4. UI OVERLAY (Level & Timer)
  display.fillRect(0, 0, 48, 12, SSD1306_BLACK);
  display.drawRect(0, 0, 48, 12, SSD1306_WHITE);
  display.setCursor(4, 2); 
  display.print("L"); display.print(dungeonLevel);
  display.print(" "); display.print(timeLeft); display.print("s");
  // 5. RADAR MINIMAP (Zoomed in on Player)
  int radarSize = 5;      // How many tiles to show around the player (5x5)
  int radarScale = 4;     // Size of each tile (4px makes it much more readable)
  int rxOffset = SCREEN_WIDTH - (radarSize * radarScale) - 4;
  int ryOffset = 4;

  // Draw Radar Frame
  display.drawRect(rxOffset - 2, ryOffset - 2, (radarSize * radarScale) + 4, (radarSize * radarScale) + 4, SSD1306_WHITE);
  display.fillRect(rxOffset, ryOffset, radarSize * radarScale, radarSize * radarScale, SSD1306_BLACK);

  for (int dy = -2; dy <= 2; dy++) {
    for (int dx = -2; dx <= 2; dx++) {
      int mx = int(posX) + dx;
      int my = int(posY) + dy;

      // Check bounds so we don't crash
      if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
        int tile = worldMap[mx][my];
        int screenX = rxOffset + (dx + 2) * radarScale;
        int screenY = ryOffset + (dy + 2) * radarScale;

        if (tile == TILE_WALL) {
          display.fillRect(screenX, screenY, radarScale - 1, radarScale - 1, SSD1306_WHITE);
        } else if (tile == TILE_GOAL) {
          if ((millis() / 200) % 2 == 0) {
            display.drawRect(screenX, screenY, radarScale - 1, radarScale - 1, SSD1306_WHITE);
          }
        }
      }
    }
  }

  // Player icon in the dead center of the radar
  display.drawRect(rxOffset + (2 * radarScale) + 1, ryOffset + (2 * radarScale) + 1, 2, 2, SSD1306_WHITE);
}

