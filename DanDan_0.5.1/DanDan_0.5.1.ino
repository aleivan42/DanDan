#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "sprites.h"
#include <Preferences.h>  // For saving data to flash

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button and buzzer pins
#define LEFT_BUTTON 1
#define MIDDLE_BUTTON 2
#define RIGHT_BUTTON 3
#define BUZZER 0

// Game constants
const unsigned long STAT_DECAY_INTERVAL = 10000; // 10 seconds
const unsigned long FRAME_RATE = 66; // ~15 FPS
const int MAX_STAT = 100;
const int MIN_STAT = 0;

// Game states
enum GameState { 
  MAIN_MENU, 
  PLAYING, 
  HANG_OUT, 
  SHOP_BUYING,
  TAX_FRAUD, 
  BATHE,
  STAT_SCREEN,
  SAVING,
  INVENTORY,
  FEEDING_SELECT,
  SHOP_CONFIRM,
  FEEDING_ACTION
};

// Status effects
enum StatusEffect {
  NONE,
  MANIC,    // All stats > 90
  HUNGRY,   // Hunger < 20
  SAD,      // Happiness < 20
  DIRTY     // Cleanliness < 20
};

// Food items
struct FoodItem {
  const char* name;
  const unsigned char* sprite;
  int cost;
  int hungerValue;
  int quantity;
};

#define FOOD_ITEMS_COUNT 3
FoodItem foodItems[FOOD_ITEMS_COUNT] = {
  {"Candy", candy_sprite, 15, 20, 0},
  {"Ice Cream", ice_cream_sprite, 25, 35, 0},
  {"Evil Person", evil_person_sprite, 50, 60, 0}
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

// Game variables
DanDan pet;
Preferences preferences;
GameState gameState = MAIN_MENU;
const char* menuItems[] = {"Feed", "Play", "Hang Out", "Shop", "Tax Fraud", "Bathe", "Status", "Inventory", "Save"};
const int menuLength = 9;
int currentMenuItem = 0;
unsigned long lastFrameTime = 0;
int gameScore = 0;
bool gameActive = false;
unsigned long lastGameTime = 0;
unsigned long saveStartTime = 0;

// Shop and inventory
int currentShopItem = 0;
int currentInventoryItem = 0;
int selectedFoodIndex = -1;
unsigned long actionStartTime = 0;
String actionMessage = "";

// Whack-a-Mole game variables
const int MOLE_POSITIONS[3] = {20, 54, 88}; // X positions for holes
const unsigned long GAME_DURATION = 30000;   // 30 seconds
const unsigned long COUNTDOWN_DURATION = 3000; // 3-second countdown
int molePosition = -1;                      // -1 = no mole
int moleType = 0;                           // 0 = good, 1 = bad
unsigned long moleAppearTime = 0;
unsigned long moleVisibleDuration = 1500;    // Start with 1.5s visibility
unsigned long nextMoleTime = 0;
int countdownState = 0;                      // 0=waiting, 1=counting, 2=go
unsigned long gameStartTime = 0;
int highScore = 0;
bool moleHit = false;

// Function prototypes
void decayStats(int cycles = 1);
void updateStatusEffects();
void handleInput(unsigned long currentTime);
void startFeeding();
void startPlaying();
void endMiniGame();
void hangOut();
void enterShop();
void commitTaxFraud();
void bathe();
void updateDisplay();
void drawMainMenu();
void drawMiniGame();
void drawStatusScreen();
void drawSavingScreen();
void drawActionScreen();
void drawShopScreen();
void drawShopBuyingScreen();
void drawInventoryScreen();
void drawFeedingSelectScreen();
void drawActionMessageScreen();
void playSound(int type);
void saveGame();
void autoSave();
void loadGame();
void handleMoleHit();
void showActionMessage(String message);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial to initialize (for debugging)

  // Initialize OLED with more robust checking
  bool displayOK = false;
  for (int i = 0; i < 3; i++) { // Try up to 3 times
    if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      displayOK = true;
      break;
    }
    delay(200); // Wait between attempts
  }

  if (!displayOK) {
    Serial.println(F("SSD1306 initialization failed!"));
    while(1); // Halt
  }

  // Clear display buffer and perform multiple refreshes
  for (int i = 0; i < 2; i++) {
    display.clearDisplay();
    display.display();
    delay(100);
  }

  // Initialize buttons and buzzer
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  // Initialize save system
  preferences.begin("dandan", false);
  loadGame();

  // After loadGame()
  highScore = preferences.getInt("highScore", 0);

  // Force complete redraw of initial state
  updateDisplay();
  display.display();
  
  // Additional verification step
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Starting...");
  display.display();
  delay(50);
  
  // Now draw the actual menu
  updateDisplay();
  display.display();
  delay(50); // Final stabilization delay
}

void loop() {
  unsigned long currentTime = millis();

  // Stat decay
  if (currentTime - pet.lastStatDecay >= STAT_DECAY_INTERVAL) {
    decayStats();
    updateStatusEffects();
    pet.lastStatDecay = currentTime;
  }

  // Handle input
  handleInput(currentTime);

  // Update display at consistent frame rate
  if (currentTime - lastFrameTime >= FRAME_RATE) {
    updateDisplay();
    lastFrameTime = currentTime;
  }

  // Handle action message timeout
  if (gameState == FEEDING_ACTION && currentTime - actionStartTime > 2000) {
    gameState = MAIN_MENU;
  }
  
  // Handle shop confirmation timeout
  if (gameState == SHOP_CONFIRM && currentTime - actionStartTime > 2000) {
    gameState = SHOP_BUYING;
  }

  // Minigame timing
  if (gameState == PLAYING && gameActive) {
    unsigned long elapsed = currentTime - gameStartTime;
    
    // Handle countdown
    if (countdownState == 1) {
      if (elapsed > COUNTDOWN_DURATION) {
        countdownState = 2; // Start game
        gameStartTime = currentTime; // Reset timer
        nextMoleTime = currentTime; // Spawn first mole immediately
        playSound(4); // "GO!" sound
      }
    }
    // Main game
    else if (countdownState == 2) {
      // Check game end
      if (elapsed >= GAME_DURATION) {
        endMiniGame();
        return;
      }
      
      // Spawn new mole if needed
      if (molePosition == -1 && currentTime >= nextMoleTime) {
        molePosition = random(0, 3); // Random hole
        moleType = random(0, 2);     // Random type
        moleAppearTime = currentTime;
        moleHit = false;
      }
      
      // Remove mole if time expired
      if (molePosition != -1 && !moleHit && 
          currentTime - moleAppearTime > moleVisibleDuration) {
        if (moleType == 0) { // Good mole escaped
          gameScore = max(0, gameScore - 1);
          playSound(6); // Bad sound
        }
        molePosition = -1;
        nextMoleTime = currentTime + random(500, 1000); // Next mole delay
        
        // Increase speed - fixed type casting here
        moleVisibleDuration = max((unsigned long)500, moleVisibleDuration - 50);
      }
    }
  }
  
  // Handle save completion
  if (gameState == SAVING && currentTime - saveStartTime > 1000) {
    gameState = MAIN_MENU;
  }
}

void saveGame() {
  preferences.putInt("hunger", pet.hunger);
  preferences.putInt("happiness", pet.happiness);
  preferences.putInt("cleanliness", pet.cleanliness);
  preferences.putInt("coins", pet.coins);
  preferences.putChar("status", (char)pet.status);
  
  // Save inventory
  for (int i = 0; i < FOOD_ITEMS_COUNT; i++) {
    String key = "itemQty" + String(i);
    preferences.putInt(key.c_str(), foodItems[i].quantity);
  }
}

void autoSave() {
  saveGame();
  preferences.putUInt("autoSaveTime", millis());
}

void loadGame() {
  if (preferences.isKey("coins")) {
    // Load all saved values
    pet.hunger = preferences.getInt("hunger", 80);
    pet.happiness = preferences.getInt("happiness", 80);
    pet.cleanliness = preferences.getInt("cleanliness", 80);
    pet.coins = preferences.getInt("coins", 50);
    pet.status = (StatusEffect)preferences.getChar("status", NONE);
    unsigned long savedDecayTime = preferences.getUInt("lastStatDecay", 0);
    
    // Calculate time-based decay
    unsigned long currentTime = millis();
    if (currentTime > savedDecayTime) {
      unsigned long timeSinceLastDecay = currentTime - savedDecayTime;
      if (timeSinceLastDecay > STAT_DECAY_INTERVAL) {
        int decayCycles = timeSinceLastDecay / STAT_DECAY_INTERVAL;
        decayStats(decayCycles);
      }
    }
    pet.lastStatDecay = currentTime;  // Reset decay timer
    
    // Load inventory
    for (int i = 0; i < FOOD_ITEMS_COUNT; i++) {
      String key = "itemQty" + String(i);
      foodItems[i].quantity = preferences.getInt(key.c_str(), 0);
    }
  } else {
    // New game - initialize properly
    pet.lastStatDecay = millis();
  }
}

void decayStats(int cycles) {
  for (int i = 0; i < cycles; i++) {
    pet.hunger = constrain(pet.hunger - 2, MIN_STAT, MAX_STAT);
    pet.happiness = constrain(pet.happiness - 1, MIN_STAT, MAX_STAT);
    pet.cleanliness = constrain(pet.cleanliness - 1, MIN_STAT, MAX_STAT);
  }
  updateStatusEffects();
}

void updateStatusEffects() {
  if (pet.hunger <= 20) pet.status = HUNGRY;
  else if (pet.happiness <= 20) pet.status = SAD;
  else if (pet.cleanliness <= 20) pet.status = DIRTY;
  else if (pet.hunger > 90 && pet.happiness > 90 && pet.cleanliness > 90) pet.status = MANIC;
  else pet.status = NONE;
}

void handleInput(unsigned long currentTime) {
  static unsigned long lastLeftPress = 0;
  static unsigned long lastMiddlePress = 0;
  static unsigned long lastRightPress = 0;

  // LEFT BUTTON - Navigate left/up
  if (digitalRead(LEFT_BUTTON) == LOW && currentTime - lastLeftPress > 200) {
    lastLeftPress = currentTime;
    playSound(0);
    
    if (gameState == MAIN_MENU) {
      currentMenuItem = (currentMenuItem - 1 + menuLength) % menuLength;
    }
    else if (gameState == PLAYING && gameActive) {
      if (countdownState == 2) { // Whack-a-Mole active
        if (molePosition == 0) { // Left position
          handleMoleHit();
        } else {
          gameScore = max(0, gameScore - 1);
          playSound(6); // Bad sound
        }
      } else {
        gameScore++;
      }
    }
    else if (gameState == SHOP_BUYING) {
      currentShopItem = (currentShopItem - 1 + FOOD_ITEMS_COUNT) % FOOD_ITEMS_COUNT;
    }
    else if (gameState == INVENTORY || gameState == FEEDING_SELECT) {
      currentInventoryItem = (currentInventoryItem - 1 + FOOD_ITEMS_COUNT) % FOOD_ITEMS_COUNT;
    }
  }

  // MIDDLE BUTTON - Select/confirm
  if (digitalRead(MIDDLE_BUTTON) == LOW && currentTime - lastMiddlePress > 200) {
    lastMiddlePress = currentTime;
    playSound(0);
    
    if (gameState == MAIN_MENU) {
      switch (currentMenuItem) {
        case 0: gameState = FEEDING_SELECT; break; // Go to food selection
        case 1: startPlaying(); break;
        case 2: hangOut(); break;
        case 3: gameState = SHOP_BUYING; break; // Go directly to shop items
        case 4: commitTaxFraud(); break;
        case 5: bathe(); break;
        case 6: gameState = STAT_SCREEN; break;
        case 7: gameState = INVENTORY; break; // View inventory
        case 8: saveGame(); gameState = SAVING; saveStartTime = millis(); break;
      }
    }
    else if (gameState == PLAYING && gameActive) {
      if (countdownState == 2) { // Whack-a-Mole active
        if (molePosition == 1) { // Middle position
          handleMoleHit();
        } else {
          gameScore = max(0, gameScore - 1);
          playSound(6); // Bad sound
        }
      } else {
        gameState = MAIN_MENU;
      }
    }
    else if (gameState == SHOP_BUYING) {
      if (pet.coins >= foodItems[currentShopItem].cost) {
        pet.coins -= foodItems[currentShopItem].cost;
        foodItems[currentShopItem].quantity++;
        showActionMessage("Bought " + String(foodItems[currentShopItem].name));
        autoSave();
      } else {
        playSound(6); // Error sound
      }
    }
    else if (gameState == FEEDING_SELECT) {
      // In food selection for feeding
      if (foodItems[currentInventoryItem].quantity > 0) {
        selectedFoodIndex = currentInventoryItem;
        showActionMessage("Danielo eats " + String(foodItems[selectedFoodIndex].name));
        startFeeding();
      } else {
        playSound(6); // Error sound
      }
    }
    else if (gameState == INVENTORY) {
      // Exit inventory when middle pressed
      gameState = MAIN_MENU;
    }
    else if (gameState == SHOP_CONFIRM || gameState == FEEDING_ACTION) {
      gameState = MAIN_MENU;
    }
    else {
      gameState = MAIN_MENU; // Exit other states
    }
  }

  // RIGHT BUTTON - Navigate right/down
  if (digitalRead(RIGHT_BUTTON) == LOW && currentTime - lastRightPress > 200) {
    lastRightPress = currentTime;
    playSound(0);
    
    if (gameState == MAIN_MENU) {
      currentMenuItem = (currentMenuItem + 1) % menuLength;
    }
    else if (gameState == PLAYING && gameActive) {
      if (countdownState == 2) { // Whack-a-Mole active
        if (molePosition == 2) { // Right position
          handleMoleHit();
        } else {
          gameScore = max(0, gameScore - 1);
          playSound(6); // Bad sound
        }
      } else {
        gameScore++;
      }
    }
    else if (gameState == SHOP_BUYING) {
      currentShopItem = (currentShopItem + 1) % FOOD_ITEMS_COUNT;
    }
    else if (gameState == INVENTORY || gameState == FEEDING_SELECT) {
      currentInventoryItem = (currentInventoryItem + 1) % FOOD_ITEMS_COUNT;
    }
    else if (gameState == SHOP_CONFIRM || gameState == FEEDING_ACTION) {
      gameState = MAIN_MENU;
    }
  }
}

void showActionMessage(String message) {
  actionMessage = message;
  actionStartTime = millis();
  if (gameState == SHOP_BUYING) {
    gameState = SHOP_CONFIRM;
  } else if (gameState == FEEDING_SELECT) {
    gameState = FEEDING_ACTION;
  }
}

void handleMoleHit() {
  moleHit = true;
  if (moleType == 0) { // Good mole
    gameScore += 2;
    playSound(5); // Good sound
  } else { // Bad mole
    gameScore = max(0, gameScore - 2);
    playSound(6); // Bad sound
  }
  molePosition = -1;
  nextMoleTime = millis() + random(300, 800);
}

void startFeeding() {
  if (selectedFoodIndex == -1) {
    return;
  }

  // Use the selected food item
  if (foodItems[selectedFoodIndex].quantity > 0) {
    foodItems[selectedFoodIndex].quantity--;
    
    int bonus = (pet.status == HUNGRY) ? 10 : 0;
    pet.hunger = constrain(
      pet.hunger + foodItems[selectedFoodIndex].hungerValue + bonus, 
      MIN_STAT, 
      MAX_STAT
    );
    
    playSound(2);
    autoSave();
    
    // Reset selection
    selectedFoodIndex = -1;
  }
}

void startPlaying() {
  gameState = PLAYING;
  gameActive = true;
  gameScore = 0;
  molePosition = -1;
  moleVisibleDuration = 1500;
  countdownState = 1; // Start countdown
  gameStartTime = millis();
  lastGameTime = millis(); // Reset game timer
  moleHit = false;
  playSound(3); // Start countdown sound
}

void endMiniGame() {
  gameActive = false;
  int bonus = (pet.status == MANIC) ? 20 : 0;
  pet.happiness = constrain(pet.happiness + (gameScore) + bonus, MIN_STAT, MAX_STAT);
  pet.coins += gameScore / 2;
  playSound(2);
  autoSave();
  if (gameScore > highScore) {
    highScore = gameScore;
    preferences.putInt("highScore", highScore);
  }
}

void hangOut() {
  gameState = HANG_OUT;
  int penalty = (pet.status == SAD) ? -10 : 0;
  pet.happiness = constrain(pet.happiness + 25 + penalty, MIN_STAT, MAX_STAT);
  playSound(1);
  autoSave();
}

void enterShop() {
  gameState = SHOP_BUYING;
  playSound(0);
}

void commitTaxFraud() {
  gameState = TAX_FRAUD;
  int bonus = (pet.status == MANIC) ? 50 : 0;
  int penalty = (pet.status == HUNGRY) ? -20 : 0;
  pet.coins += 30 + bonus + penalty;
  playSound(1);
  autoSave();
}

void bathe() {
  gameState = BATHE;
  int penalty = (pet.status == DIRTY) ? -5 : 0;
  pet.cleanliness = constrain(pet.cleanliness + 30 + penalty, MIN_STAT, MAX_STAT);
  playSound(2);
  autoSave();
}

void updateDisplay() {
  display.clearDisplay();
  
  // Draw status bar at top only for non-main-menu states
  if (gameState != MAIN_MENU && gameState != STAT_SCREEN && gameState != SAVING && 
      gameState != INVENTORY && gameState != FEEDING_SELECT) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("C:");
    display.print(pet.coins);
    
    // Draw status effect icon using smiley as placeholder
    display.drawBitmap(SCREEN_WIDTH - 16, 0, smiley, 8, 8, SSD1306_WHITE);
  }

  switch (gameState) {
    case MAIN_MENU:
      drawMainMenu();
      break;
      
    case PLAYING:
      drawMiniGame();
      break;
      
    case STAT_SCREEN:
      drawStatusScreen();
      break;
      
    case SAVING:
      drawSavingScreen();
      break;
      
    case SHOP_BUYING:
      drawShopBuyingScreen();
      break;
      
    case INVENTORY:
      drawInventoryScreen();
      break;
      
    case FEEDING_SELECT:
      drawFeedingSelectScreen();
      break;
      
    case SHOP_CONFIRM:
    case FEEDING_ACTION:
      drawActionMessageScreen();
      break;
      
    default:
      drawActionScreen();
      break;
  }
  
  display.display();
}

void drawMainMenu() {
  // Draw status icons and values at top
  display.setTextSize(1);
  
  // Calculate positions
  const int sectionWidth = SCREEN_WIDTH / 3;
  const int iconOffset = 2;
  const int textOffset = 10;  // 8px icon + 2px gap

  // Hunger (left)
  display.drawBitmap(sectionWidth * 0 + iconOffset, 0, hunger_icon, 8, 8, SSD1306_WHITE);
  display.setCursor(sectionWidth * 0 + textOffset, 0);
  display.print(pet.hunger);
  display.print("%");

  // Happiness (center)
  display.drawBitmap(sectionWidth * 1 + iconOffset, 0, happiness_icon, 8, 8, SSD1306_WHITE);
  display.setCursor(sectionWidth * 1 + textOffset, 0);
  display.print(pet.happiness);
  display.print("%");

  // Cleanliness (right)
  display.drawBitmap(sectionWidth * 2 + iconOffset, 0, cleanliness_icon, 8, 8, SSD1306_WHITE);
  display.setCursor(sectionWidth * 2 + textOffset, 0);
  display.print(pet.cleanliness);
  display.print("%");

  // Draw pet sprite centered
  display.drawBitmap(
    (SCREEN_WIDTH - 32) / 2,
    (SCREEN_HEIGHT - 32) / 2,
    Danielo_sprite, 
    32, 32, 
    SSD1306_WHITE
  );

  // Draw menu with arrows
  const char* currentItem = menuItems[currentMenuItem];
  int textWidth = strlen(currentItem) * 6; // 6px per char in text size 1
  
  // Left arrow
  display.drawBitmap(10, SCREEN_HEIGHT - 10, left_arrow, 8, 8, SSD1306_WHITE);
  
  // Menu text (centered)
  display.setCursor(SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT - 9);
  display.print(currentItem);
  
  // Right arrow
  display.drawBitmap(SCREEN_WIDTH - 18, SCREEN_HEIGHT - 10, right_arrow, 8, 8, SSD1306_WHITE);
}

void drawMiniGame() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Score: ");
  display.print(gameScore);
  
  if (countdownState == 1) {
    // Draw countdown
    int count = 3 - (millis() - gameStartTime) / 1000;
    display.setTextSize(3);
    display.setCursor(SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2 - 15);
    if (count > 0) {
      display.print(count);
    } else {
      display.print("GO!");
    }
  } 
  else if (countdownState == 2) {
    // Draw time remaining
    int timeLeft = (GAME_DURATION - (millis() - gameStartTime)) / 1000;
    display.setCursor(SCREEN_WIDTH - 30, 0);
    display.print(timeLeft);
    
    // Draw holes
    for (int i = 0; i < 3; i++) {
      display.drawCircle(MOLE_POSITIONS[i], 40, 15, SSD1306_WHITE);
    }
    
    // Draw mole if active
    if (molePosition != -1) {
      const unsigned char* sprite = moleType == 0 ? mole_good : mole_bad;
      display.drawBitmap(
        MOLE_POSITIONS[molePosition] - 8, 
        25, 
        sprite, 
        16, 16, SSD1306_WHITE
      );
    }
  }
  else { // Game over
    display.setCursor(20, 20);
    display.setTextSize(2);
    display.print("SCORE:");
    display.print(gameScore);
    
    if (gameScore > highScore) {
      highScore = gameScore;
      display.setCursor(10, 40);
      display.print("NEW HIGHSCORE!");
    }
    
    display.setTextSize(1);
    display.setCursor(30, 55);
    display.print("Press MIDDLE");
  }
}

void drawStatusScreen() {
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("Hunger: ");
  display.print(pet.hunger);
  display.print("%");
  
  display.setCursor(0, 25);
  display.print("Happiness: ");
  display.print(pet.happiness);
  display.print("%");
  
  display.setCursor(0, 40);
  display.print("Cleanliness: ");
  display.print(pet.cleanliness);
  display.print("%");
  
  display.setCursor(0, 55);
  display.print("Coins: ");
  display.print(pet.coins);
}

void drawSavingScreen() {
  display.setTextSize(2);
  display.setCursor(SCREEN_WIDTH/2 - 24, SCREEN_HEIGHT/2 - 8);
  display.print("SAVING");
}

void drawActionScreen() {
  display.setTextSize(2);
  display.setCursor(10, 20);
  
  switch (gameState) {
    case HANG_OUT:
      display.print("HANG OUT");
      break;
    case TAX_FRAUD:
      display.print("TAX FRAUD");
      break;
    case BATHE:
      display.print("BATHING");
      break;
    default:
      display.print("ACTION");
      break;
  }
  
  // Draw instruction at bottom
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("Press MIDDLE");
}

void drawShopBuyingScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SHOP - Coins: ");
  display.print(pet.coins);
  
  display.setCursor(0, 12);
  display.print("Item: ");
  display.print(foodItems[currentShopItem].name);
  
  display.setCursor(0, 24);
  display.print("Cost: ");
  display.print(foodItems[currentShopItem].cost);
  display.print(" Owned: ");
  display.print(foodItems[currentShopItem].quantity);
  
  // Draw item sprite
  display.drawBitmap(
    SCREEN_WIDTH/2 - 16, 
    30, 
    foodItems[currentShopItem].sprite, 
    32, 32, SSD1306_WHITE
  );
  
  // Draw navigation arrows
  display.drawBitmap(10, SCREEN_HEIGHT - 10, left_arrow, 8, 8, SSD1306_WHITE);
  display.drawBitmap(SCREEN_WIDTH - 18, SCREEN_HEIGHT - 10, right_arrow, 8, 8, SSD1306_WHITE);
}

void drawInventoryScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("INVENTORY");
  
  // Draw item sprite
  display.drawBitmap(
    SCREEN_WIDTH/2 - 16, 
    10, 
    foodItems[currentInventoryItem].sprite, 
    32, 32, SSD1306_WHITE
  );
  
  // Draw item info
  display.setCursor(0, 42);
  display.print(foodItems[currentInventoryItem].name);
  display.print(" x");
  display.print(foodItems[currentInventoryItem].quantity);
  
  // Draw description
  display.setCursor(0, 52);
  display.print("Restores ");
  display.print(foodItems[currentInventoryItem].hungerValue);
  display.print(" hunger");
}

void drawFeedingSelectScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SELECT FOOD");
  
  // Draw item sprite
  display.drawBitmap(
    SCREEN_WIDTH/2 - 16, 
    10, 
    foodItems[currentInventoryItem].sprite, 
    32, 32, SSD1306_WHITE
  );
  
  // Draw item info
  display.setCursor(0, 42);
  display.print(foodItems[currentInventoryItem].name);
  display.print(" x");
  display.print(foodItems[currentInventoryItem].quantity);
  
  // Draw description
  display.setCursor(0, 52);
  display.print("Restores ");
  display.print(foodItems[currentInventoryItem].hungerValue);
  display.print(" hunger");
}

void drawActionMessageScreen() {
  display.setTextSize(1);
  display.setCursor(0, 20);
  
  // Center the message
  int textWidth = actionMessage.length() * 6;
  int xPos = (SCREEN_WIDTH - textWidth) / 2;
  if (xPos < 0) xPos = 0;
  
  display.setCursor(xPos, 20);
  display.print(actionMessage);
  
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - 10);
  display.print("Press any button");
}

void playSound(int type) {
  switch (type) {
    case 0: // Click
      tone(BUZZER, 1000, 100);
      break;
    case 1: // Action success
      tone(BUZZER, 1500, 100);
      delay(100);
      tone(BUZZER, 2000, 100);
      break;
    case 2: // Stat increase
      tone(BUZZER, 2000, 50);
      delay(50);
      tone(BUZZER, 2500, 50);
      break;
    case 3: // Countdown beep
      tone(BUZZER, 800, 200);
      break;
    case 4: // Go sound
      tone(BUZZER, 1500, 300);
      break;
    case 5: // Good hit
      tone(BUZZER, 1200, 100);
      delay(50);
      tone(BUZZER, 1800, 150);
      break;
    case 6: // Bad hit/miss
      tone(BUZZER, 400, 300);
      break;
  }
}