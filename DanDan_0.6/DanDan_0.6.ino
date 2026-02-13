#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "config.h"
#include "PetData.h"
#include "sprites.h"

// Prototipi
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

// Istanze Globali
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DanDan pet;
Preferences preferences;
GameState gameState = MAIN_MENU;

// Variabili di stato
const char* menuItems[] = {"Feed", "Play", "Hang Out", "Shop", "Tax Fraud", "Bathe", "Status", "Inventory", "Save"};
const int menuLength = 9;
int currentMenuItem = 0;
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

int currentShopItem = 0;
int currentInventoryItem = 0;
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
int countdownState = 0; // 0: None, 1: Countdown, 2: Running
unsigned long gameStartTime = 0;
int highScore = 0;
bool moleHit = false;

// --- TAX FRAUD GAME VARIABLES ---
int playerLane = 1; 
float obstacleX = 128;        
int obstacleLane = 1;         
float runSpeed = 2.0;         
const int LANE_Y[3] = {18, 32, 46}; 
unsigned long lastSpeedIncrease = 0;

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

  // 1. Stats Decay (Always runs)
  if (currentTime - pet.lastStatDecay >= STAT_DECAY_INTERVAL) {
    decayStats();
    updateStatusEffects();
    pet.lastStatDecay = currentTime;
  }

  // 2. Input Handling (Always runs)
  handleInput(currentTime);

  // 3. Frame-Based Logic (Movement and Timers)
  if (currentTime - lastFrameTime >= FRAME_RATE) {
    lastFrameTime = currentTime; // Update the timer

    // --- TAX FRAUD LOGIC ---
    if (gameState == TAX_FRAUD) {
      if (countdownState == 1) {
        if (currentTime - gameStartTime > COUNTDOWN_DURATION) {
          countdownState = 2;
          gameActive = true;
          playSound(4);
        }
      } else if (countdownState == 2 && gameActive) {
        obstacleX -= runSpeed; // Now only moves ~30-60 times per second!

        if (currentTime - lastSpeedIncrease > 3000) {
          runSpeed += 0.1; 
          lastSpeedIncrease = currentTime;
        }

        // Collision logic
        if (obstacleX < 30 && obstacleX > 10) {
          if (playerLane == obstacleLane) {
            gameActive = false;
            playSound(6); 
            pet.coins = max(0, pet.coins - 20);
            pet.happiness = max(0, pet.happiness - 10);
            autoSave();
            gameState = MAIN_MENU; 
          }
        }
        if (obstacleX < -20) {
          obstacleX = 130;
          obstacleLane = random(0, 3);
          gameScore++;
          playSound(5); 
        }
      }
    }

    // --- WHACK-A-MOLE LOGIC ---
    if (gameState == PLAYING && gameActive) {
      unsigned long elapsed = currentTime - gameStartTime;
      if (countdownState == 1) {
        if (elapsed > COUNTDOWN_DURATION) {
          countdownState = 2; 
          gameStartTime = currentTime; 
          nextMoleTime = currentTime; 
          playSound(4);
        }
      } else if (countdownState == 2) {
        if (elapsed >= GAME_DURATION) { 
          endMiniGame(); 
        } else {
          if (molePosition == -1 && currentTime >= nextMoleTime) {
            molePosition = random(0, 3); 
            moleType = random(0, 2); 
            moleAppearTime = currentTime; 
            moleHit = false;
          }
          if (molePosition != -1 && !moleHit && currentTime - moleAppearTime > moleVisibleDuration) {
            if (moleType == 0) { gameScore = max(0, gameScore - 1); playSound(6); }
            molePosition = -1; 
            nextMoleTime = currentTime + random(500, 1000);
          }
        }
      }
    }

    // 4. State Transitions (Timed Actions)
    if (gameState == FEEDING_ACTION && currentTime - actionStartTime > 2000) gameState = MAIN_MENU;
    if (gameState == SHOP_CONFIRM && currentTime - actionStartTime > 2000) gameState = SHOP_BUYING;
    if (gameState == SAVING && currentTime - saveStartTime > 1000) gameState = MAIN_MENU;

    // 5. Update the Screen
    updateDisplay();
  }
}

void handleInput(unsigned long currentTime) {
  static unsigned long lastLeftPress = 0;
  static unsigned long lastMiddlePress = 0;
  static unsigned long lastRightPress = 0;

  if (isTransitioning) return;

  // LEFT BUTTON
  if (digitalRead(LEFT_BUTTON) == LOW && currentTime - lastLeftPress > 200) {
    lastLeftPress = currentTime; playSound(0);
    if (gameState == MAIN_MENU) currentMenuItem = (currentMenuItem - 1 + menuLength) % menuLength;
    else if (gameState == TAX_FRAUD) playerLane = 0;
    else if (gameState == PLAYING && gameActive && countdownState == 2) {
      if (molePosition == 0) handleMoleHit(); else { gameScore = max(0, gameScore - 1); playSound(6); }
    }
    else if (gameState == SHOP_BUYING) currentShopItem = (currentShopItem - 1 + FOOD_ITEMS_COUNT) % FOOD_ITEMS_COUNT;
    else if (gameState == INVENTORY || gameState == FEEDING_SELECT || gameState == STAT_SCREEN) gameState = MAIN_MENU;
  }

  // MIDDLE BUTTON
  if (digitalRead(MIDDLE_BUTTON) == LOW && currentTime - lastMiddlePress > 200) {
    lastMiddlePress = currentTime; playSound(0);
    
    if (gameState == MAIN_MENU) {
      isTransitioning = true;
      gameScore = 0;
      switch (currentMenuItem) {
        case 0: targetState = FEEDING_SELECT; break;
        case 1: startPlaying(); isTransitioning = false; break;
        case 2: targetState = HANG_OUT; break;
        case 3: targetState = SHOP_BUYING; break;
        case 4: 
            targetState = TAX_FRAUD; 
            gameActive = false; 
            countdownState = 1; 
            gameStartTime = millis(); 
            break; 
        case 5: targetState = BATHE; break;
        case 6: targetState = STAT_SCREEN; break;
        case 7: targetState = INVENTORY; break;
        case 8: saveGame(); gameState = SAVING; saveStartTime = millis(); isTransitioning = false; break;
      }
    }
    else if (gameState == TAX_FRAUD) {
      playerLane = 1;
    }
    else if (gameState == PLAYING) {
      if (!gameActive) gameState = MAIN_MENU;
      else if (countdownState == 2 && molePosition == 1) handleMoleHit();
    }
    else if (gameState == SHOP_BUYING) {
      if (pet.coins >= foodItems[currentShopItem].cost) {
        pet.coins -= foodItems[currentShopItem].cost; foodItems[currentShopItem].quantity++;
        showActionMessage("Bought " + String(foodItems[currentShopItem].name)); autoSave();
      } else playSound(6);
    }
    else if (gameState == FEEDING_SELECT) {
      if (foodItems[currentInventoryItem].quantity > 0) {
        selectedFoodIndex = currentInventoryItem;
        showActionMessage("Danielo eats " + String(foodItems[selectedFoodIndex].name)); startFeeding();
      } else playSound(6);
    }
    else gameState = MAIN_MENU;
  }

  // RIGHT BUTTON
  if (digitalRead(RIGHT_BUTTON) == LOW && currentTime - lastRightPress > 200) {
    lastRightPress = currentTime; playSound(0);
    if (gameState == MAIN_MENU) currentMenuItem = (currentMenuItem + 1) % menuLength;
    else if (gameState == TAX_FRAUD) playerLane = 2;
    else if (gameState == PLAYING && gameActive && countdownState == 2) {
      if (molePosition == 2) handleMoleHit(); else { gameScore = max(0, gameScore - 1); playSound(6); }
    }
    else if (gameState == SHOP_BUYING) currentShopItem = (currentShopItem + 1) % FOOD_ITEMS_COUNT;
  }
}

void drawActionScreen() {
  if (gameState == TAX_FRAUD) {
    if (countdownState == 1) {
      int count = 3 - (millis() - gameStartTime) / 1000;
      display.setCursor(35, 15); display.print("TAX ESCAPE");
      display.setTextSize(3); display.setCursor(55, 30);
      if (count > 0) display.print(count); else display.print("GO!");
      display.setTextSize(1);
    } else {
      display.drawFastHLine(0, 28, 128, 1);
      display.drawFastHLine(0, 42, 128, 1);
      display.drawFastHLine(0, 56, 128, 1);
      display.drawBitmap(10, LANE_Y[playerLane]-8, Danielo_sprite_L, 32, 32, 1);
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

// Helper Functions
void saveGame() { preferences.putInt("hunger", pet.hunger); preferences.putInt("happiness", pet.happiness); preferences.putInt("cleanliness", pet.cleanliness); preferences.putInt("coins", pet.coins); preferences.putChar("status", (char)pet.status); for (int i = 0; i < FOOD_ITEMS_COUNT; i++) { String key = "itemQty" + String(i); preferences.putInt(key.c_str(), foodItems[i].quantity); } }
void autoSave() { saveGame(); }
void loadGame() { if (preferences.isKey("coins")) { pet.hunger = preferences.getInt("hunger", 80); pet.happiness = preferences.getInt("happiness", 80); pet.cleanliness = preferences.getInt("cleanliness", 80); pet.coins = preferences.getInt("coins", 50); pet.status = (StatusEffect)preferences.getChar("status", NONE); for (int i = 0; i < FOOD_ITEMS_COUNT; i++) { String key = "itemQty" + String(i); foodItems[i].quantity = preferences.getInt(key.c_str(), 0); } } pet.lastStatDecay = millis(); }
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

void updateDisplay() {
  display.clearDisplay();
  switch (gameState) {
    case MAIN_MENU: drawMainMenu(); break;
    case PLAYING: drawMiniGame(); break;
    case STAT_SCREEN: drawStatusScreen(); break;
    case SAVING: drawSavingScreen(); break;
    case SHOP_BUYING: drawShopBuyingScreen(); break;
    case INVENTORY: drawInventoryScreen(); break;
    case FEEDING_SELECT: drawFeedingSelectScreen(); break;
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
      isTransitioning = false;
      transitionOffset = 0;
      gameState = targetState; 
      
      // TAX FRAUD INIT
      if (gameState == TAX_FRAUD) {
        obstacleX = 130;
        obstacleLane = random(0,3);
        runSpeed = 2.0;
        gameScore = 0;
        countdownState = 1;
        gameStartTime = millis();
      }

      playScreenWipe = true;
      wipeProgress = 0; 
      return; 
    }
  }
  // Icons and Stats
  display.setTextColor(SSD1306_WHITE);
  const int secW = SCREEN_WIDTH / 3;
  display.drawBitmap(secW * 0 + 2, 0, hunger_icon, 8, 8, 1);
  display.setCursor(secW * 0 + 12, 0); display.print(pet.hunger);
  display.drawBitmap(secW * 1 + 2, 0, happiness_icon, 8, 8, 1);
  display.setCursor(secW * 1 + 12, 0); display.print(pet.happiness);
  display.drawBitmap(secW * 2 + 2, 0, cleanliness_icon, 8, 8, 1);
  display.setCursor(secW * 2 + 12, 0); display.print(pet.cleanliness);

  // Walking logic
  if (!isTransitioning && (now - stateChangeTime > stateDuration)) {
    stateChangeTime = now;
    petState = random(0, 3);
    stateDuration = random(2000, 5000); 
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
  display.drawBitmap((int)drawX, 16 + bounceY, selectedSprite, 32, 32, 1);
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

void drawActionMessageScreen() {
  int xPos = (SCREEN_WIDTH - (actionMessage.length() * 6)) / 2;
  display.setCursor(max(0, xPos), 20); display.print(actionMessage);
  display.setCursor(0, SCREEN_HEIGHT - 10); display.print("Press any button");
}

void playSound(int type) {
  switch (type) {
    case 0: tone(BUZZER, 1000, 100); break;                             // Nav click
    case 1: tone(BUZZER, 1500, 100); delay(100); tone(BUZZER, 2000, 100); break; // Happy/Success
    case 2: tone(BUZZER, 2000, 50); delay(50); tone(BUZZER, 2500, 50); break;   // Eating/Bathe
    case 3: tone(BUZZER, 800, 200); break;                              // Game Start
    case 4: tone(BUZZER, 1500, 300); break;                             // Go!
    case 5: tone(BUZZER, 1200, 100); delay(50); tone(BUZZER, 1800, 150); break; // Point score
    case 6: tone(BUZZER, 400, 300); break;                              // Fail/Error
  }
}