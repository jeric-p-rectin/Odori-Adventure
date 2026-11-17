
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================================
// ODORI ADVENTURE V2
// Fixed attack + Energy Star Projectiles
// Arduino Uno | 128x64 OLED | 5 Buttons
// ======================================

#define SW 128
#define SH 64
#define OLED_ADDR 0x3C

#define LEFT_BTN 2
#define RIGHT_BTN 3
#define JUMP_BTN 4
#define ATTACK_BTN 5
#define SPECIAL_BTN 6

#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK

Adafruit_SSD1306 display(SW, SH, &Wire, -1);

// GAME STATES
enum GameState {
  MENU,
  PLAY,
  GAME_OVER,
  VICTORY
};

GameState gameState = MENU;

// WORLD
const int GROUND = 56;
const int WORLD_WIDTH = 640;

const int HERO_W = 14;
const int HERO_H = 16;

int playerX = 8;
int playerY = GROUND - HERO_H;
int cameraX = 0;

int velocityY = 0;

bool grounded = true;
bool facingRight = true;

bool previousJump = false;
bool previousAttack = false;
bool previousSpecial = false;

// SCORE AND LEVEL
int lives = 3;
int score = 0;
int bestScore = 0;
int level = 1;

unsigned long frames = 0;
unsigned long lastFrame = 0;

const unsigned long FRAME_MS = 42;

// ABILITIES
byte attackFlash = 0;
byte attackCooldown = 0;
byte shieldTimer = 0;
byte shieldCooldown = 0;
byte invulnerableTimer = 0;

// WORLD OBJECTS
struct Platform {
  int x;
  byte y;
  byte width;
};

struct Star {
  int x;
  int y;
  bool collected;
};

struct Enemy {
  int x;
  int minX;
  int maxX;
  int direction;
  bool alive;
};

struct EnergyStar {
  int x;
  int y;
  int direction;
  bool active;
};

Platform platforms[5];
Star stars[8];
Enemy enemies[4];

// Two energy stars can exist at once
EnergyStar shots[2];

// ======================================
// DRAW ODORI
// ======================================

void drawOdori(int x, int y) {

  // Cute ears
  display.fillTriangle(
    x + 1, y + 6,
    x + 3, y,
    x + 6, y + 6,
    WHITE
  );

  display.fillTriangle(
    x + 8, y + 6,
    x + 11, y,
    x + 13, y + 6,
    WHITE
  );

  // Rounded body
  display.fillRoundRect(
    x, y + 4, 14, 11, 5, WHITE
  );

  // Blinking eyes
  if (frames % 90 < 4) {

    display.drawFastHLine(
      x + 3, y + 9, 3, BLACK
    );

    display.drawFastHLine(
      x + 9, y + 9, 3, BLACK
    );

  } else {

    display.fillRect(
      x + 4, y + 8, 2, 3, BLACK
    );

    display.fillRect(
      x + 10, y + 8, 2, 3, BLACK
    );
  }

  // Smile
  display.drawPixel(x + 7, y + 12, BLACK);
  display.drawPixel(x + 8, y + 12, BLACK);

  // Animated feet
  if (!grounded) {

    display.drawFastHLine(
      x + 2, y + 15, 4, WHITE
    );

    display.drawFastHLine(
      x + 9, y + 15, 4, WHITE
    );

  } else if ((frames / 4) % 2 == 0) {

    display.fillRect(
      x + 1, y + 14, 4, 2, WHITE
    );

    display.fillRect(
      x + 10, y + 14, 4, 1, WHITE
    );

  } else {

    display.fillRect(
      x + 1, y + 14, 4, 1, WHITE
    );

    display.fillRect(
      x + 10, y + 14, 4, 2, WHITE
    );
  }

  // Attack flash
  if (attackFlash > 0) {

    if (facingRight) {

      display.drawLine(
        x + 15, y + 5,
        x + 21, y + 2,
        WHITE
      );

      display.drawLine(
        x + 15, y + 11,
        x + 21, y + 14,
        WHITE
      );

    } else {

      display.drawLine(
        x - 2, y + 5,
        x - 8, y + 2,
        WHITE
      );

      display.drawLine(
        x - 2, y + 11,
        x - 8, y + 14,
        WHITE
      );
    }
  }

  // Bubble shield
  if (shieldTimer > 0) {

    display.drawCircle(
      x + 7, y + 8, 12, WHITE
    );

    display.drawPixel(
      x - 3, y + 2, WHITE
    );
  }
}

// ======================================
// DRAW ENEMIES
// ======================================

void drawEnemy(int x, int y) {

  display.fillRoundRect(
    x, y + 3, 12, 8, 3, WHITE
  );

  // Little horns
  display.fillTriangle(
    x + 1, y + 4,
    x + 3, y,
    x + 5, y + 4,
    WHITE
  );

  display.fillTriangle(
    x + 7, y + 4,
    x + 9, y,
    x + 11, y + 4,
    WHITE
  );

  // Eyes
  display.drawPixel(x + 3, y + 6, BLACK);
  display.drawPixel(x + 8, y + 6, BLACK);

  // Feet
  display.fillRect(x + 1, y + 10, 3, 1, WHITE);
  display.fillRect(x + 8, y + 10, 3, 1, WHITE);
}

// ======================================
// DRAW STARS AND HEARTS
// ======================================

void drawStar(int x, int y) {

  display.drawFastVLine(
    x + 3, y, 7, WHITE
  );

  display.drawFastHLine(
    x, y + 3, 7, WHITE
  );

  display.drawPixel(x + 1, y + 1, WHITE);
  display.drawPixel(x + 5, y + 1, WHITE);
  display.drawPixel(x + 1, y + 5, WHITE);
  display.drawPixel(x + 5, y + 5, WHITE);
}

void drawHeart(int x, int y) {

  display.fillCircle(x + 2, y + 2, 2, WHITE);
  display.fillCircle(x + 6, y + 2, 2, WHITE);

  display.fillTriangle(
    x, y + 3,
    x + 8, y + 3,
    x + 4, y + 8,
    WHITE
  );
}

// ======================================
// ENERGY STAR ATTACK
// ======================================

void fireEnergyStar() {

  if (attackCooldown > 0) {
    return;
  }

  for (int i = 0; i < 2; i++) {

    if (!shots[i].active) {

      shots[i].active = true;

      shots[i].direction =
        facingRight ? 1 : -1;

      if (facingRight) {
        shots[i].x = playerX + HERO_W;
      } else {
        shots[i].x = playerX - 5;
      }

      shots[i].y = playerY + 8;

      attackFlash = 6;
      attackCooldown = 7;

      break;
    }
  }
}

void drawEnergyStars() {

  for (int i = 0; i < 2; i++) {

    if (!shots[i].active) {
      continue;
    }

    int x = shots[i].x - cameraX;
    int y = shots[i].y;

    // Bright pixel star
    display.drawFastHLine(
      x, y + 2, 5, WHITE
    );

    display.drawFastVLine(
      x + 2, y, 5, WHITE
    );

    display.drawPixel(x, y, WHITE);
    display.drawPixel(x + 4, y, WHITE);
    display.drawPixel(x, y + 4, WHITE);
    display.drawPixel(x + 4, y + 4, WHITE);
  }
}

void updateEnergyStars() {

  for (int i = 0; i < 2; i++) {

    if (!shots[i].active) {
      continue;
    }

    // Travel through the world
    shots[i].x += shots[i].direction * 5;

    // Stars gradually descend toward the ground
    if (shots[i].y < 48) {
      shots[i].y++;
    }

    // Remove after leaving the visible screen
    if (
      shots[i].x < cameraX - 10 ||
      shots[i].x > cameraX + SW + 10
    ) {
      shots[i].active = false;
      continue;
    }

    // Detect enemy hit
    for (int j = 0; j < 4; j++) {

      if (!enemies[j].alive) {
        continue;
      }

      bool hitX =
        shots[i].x < enemies[j].x + 12 &&
        shots[i].x + 5 > enemies[j].x;

      bool hitY =
        shots[i].y < 56 &&
        shots[i].y + 5 > 45;

      if (hitX && hitY) {

        enemies[j].alive = false;
        shots[i].active = false;

        score += 15;
        break;
      }
    }
  }
}

// ======================================
// BACKGROUND
// ======================================

void drawCloud(int x, int y) {

  display.drawCircle(x + 4, y + 4, 3, WHITE);
  display.drawCircle(x + 10, y + 2, 4, WHITE);

  display.drawFastHLine(
    x + 2, y + 7, 15, WHITE
  );
}

void drawBackground() {

  int cloud1 =
    140 - ((frames / 3) % 170);

  int cloud2 =
    160 - ((frames / 5 + 60) % 190);

  drawCloud(cloud1, 17);
  drawCloud(cloud2, 27);

  display.drawFastHLine(
    0, GROUND, 128, WHITE
  );

  // Moving grass
  for (int x = -16; x < 144; x += 20) {

    int gx = x - (cameraX % 20);

    display.drawFastHLine(
      gx, 61, 7, WHITE
    );

    display.drawPixel(
      gx + 11, 59, WHITE
    );
  }
}

// ======================================
// CREATE LEVEL
// ======================================

void createLevel() {

  playerX = 8;
  playerY = GROUND - HERO_H;

  velocityY = 0;
  grounded = true;
  facingRight = true;

  cameraX = 0;
  frames = 0;

  attackFlash = 0;
  attackCooldown = 0;
  shieldTimer = 0;
  shieldCooldown = 0;
  invulnerableTimer = 0;

  // Clear projectiles
  for (int i = 0; i < 2; i++) {
    shots[i].active = false;
  }

  int heights[5] = {
    42, 36, 41, 35, 40
  };

  for (int i = 0; i < 5; i++) {

    platforms[i].x = 75 + i * 101;

    platforms[i].y =
      heights[i] - ((level - 1) * (i % 2));

    platforms[i].width = 43;
  }

  // Stars on platforms
  for (int i = 0; i < 5; i++) {

    stars[i].x =
      platforms[i].x + 19;

    stars[i].y =
      platforms[i].y - 10;

    stars[i].collected = false;
  }

  // Ground stars
  stars[5].x = 42;
  stars[5].y = 47;

  stars[6].x = 339;
  stars[6].y = 47;

  stars[7].x = 572;
  stars[7].y = 47;

  for (int i = 5; i < 8; i++) {
    stars[i].collected = false;
  }

  // Enemies
  for (int i = 0; i < 4; i++) {

    enemies[i].x = 145 + i * 130;

    enemies[i].minX =
      enemies[i].x - 15;

    enemies[i].maxX =
      enemies[i].x + 15;

    enemies[i].direction = -1;
    enemies[i].alive = true;
  }
}

// ======================================
// START GAME
// ======================================

void startGame() {

  score = 0;
  lives = 3;
  level = 1;

  createLevel();

  gameState = PLAY;
  lastFrame = millis();
}

// ======================================
// MENU
// ======================================

void drawMenu() {

  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(34, 0);
  display.print(F("ODORI"));

  display.setTextSize(1);
  display.setCursor(34, 18);
  display.print(F("ADVENTURE V2"));

  drawOdori(56, 25);

  display.setCursor(1, 44);
  display.print(F("D2/3 MOVE  D4 JUMP"));

  display.setCursor(1, 53);
  display.print(F("D5 SHOOT D6 SHIELD"));

  display.display();
}

// ======================================
// END SCREEN
// ======================================

void drawEndScreen() {

  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);

  if (gameState == VICTORY) {

    display.setCursor(16, 4);
    display.print(F("YOU WIN!"));

  } else {

    display.setCursor(10, 4);
    display.print(F("GAME OVER"));
  }

  display.setTextSize(1);

  display.setCursor(30, 30);
  display.print(F("SCORE: "));
  display.print(score);

  display.setCursor(30, 41);
  display.print(F("BEST:  "));
  display.print(bestScore);

  display.setCursor(21, 56);
  display.print(F("D4 TO RESTART"));

  display.display();
}

// ======================================
// DRAW GAME WORLD
// ======================================

void drawWorld() {

  display.clearDisplay();

  drawBackground();

  // Platforms
  for (int i = 0; i < 5; i++) {

    int x = platforms[i].x - cameraX;

    if (x > -50 && x < 130) {

      display.fillRoundRect(
        x,
        platforms[i].y,
        platforms[i].width,
        3, 1, WHITE
      );

      display.drawPixel(
        x + 6,
        platforms[i].y + 5,
        WHITE
      );

      display.drawPixel(
        x + 30,
        platforms[i].y + 5,
        WHITE
      );
    }
  }

  // Collectible stars
  for (int i = 0; i < 8; i++) {

    if (!stars[i].collected) {

      int x = stars[i].x - cameraX;

      if (x >= -7 && x < 128) {
        drawStar(x, stars[i].y);
      }
    }
  }

  // Enemies
  for (int i = 0; i < 4; i++) {

    if (enemies[i].alive) {

      int x = enemies[i].x - cameraX;

      if (x > -15 && x < 128) {
        drawEnemy(x, 45);
      }
    }
  }

  // Energy star projectiles
  drawEnergyStars();

  // Finish flag
  int flagX = 605 - cameraX;

  if (flagX > -20 && flagX < 128) {

    display.drawFastVLine(
      flagX, 26, 30, WHITE
    );

    display.fillTriangle(
      flagX + 1, 27,
      flagX + 19, 33,
      flagX + 1, 39,
      WHITE
    );
  }

  // Draw Odori
  if (
    invulnerableTimer == 0 ||
    (frames / 3) % 2 == 0
  ) {

    drawOdori(
      playerX - cameraX,
      playerY
    );
  }

  // HUD background
  display.fillRect(
    0, 0, 128, 12, BLACK
  );

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 2);
  display.print(F("L"));
  display.print(level);

  // Hearts
  for (int i = 0; i < lives; i++) {
    drawHeart(17 + i * 11, 2);
  }

  // Shield ready indicator
  display.setCursor(65, 2);

  if (shieldCooldown == 0) {
    display.print(F("B"));
  } else {
    display.print(F("-"));
  }

  // Score
  display.setCursor(83, 2);
  display.print(score);

  display.drawFastHLine(
    0, 12, 128, WHITE
  );

  display.display();
}

// ======================================
// PLAYER MOVEMENT AND PHYSICS
// ======================================

void updatePlayer(
  bool left,
  bool right,
  bool jump
) {

  if (left && !right) {

    playerX -= 2;
    facingRight = false;
  }

  if (right && !left) {

    playerX += 2;
    facingRight = true;
  }

  playerX = constrain(
    playerX, 0,
    WORLD_WIDTH - HERO_W
  );

  if (jump && grounded) {

    velocityY = -8;
    grounded = false;
  }

  int oldBottom = playerY + HERO_H;

  // Gravity
  velocityY++;

  if (velocityY > 5) {
    velocityY = 5;
  }

  playerY += velocityY;
  grounded = false;

  // Platform landing
  if (velocityY >= 0) {

    for (int i = 0; i < 5; i++) {

      Platform &p = platforms[i];

      bool overlapX =
        playerX + HERO_W - 2 > p.x &&
        playerX + 2 < p.x + p.width;

      bool crossedTop =
        oldBottom <= p.y &&
        playerY + HERO_H >= p.y;

      if (overlapX && crossedTop) {

        playerY = p.y - HERO_H;

        velocityY = 0;
        grounded = true;
        break;
      }
    }
  }

  // Ground collision
  if (playerY + HERO_H >= GROUND) {

    playerY = GROUND - HERO_H;
    velocityY = 0;
    grounded = true;
  }

  // Camera tracking
  cameraX = playerX - 35;

  cameraX = constrain(
    cameraX, 0,
    WORLD_WIDTH - SW
  );
}

// ======================================
// COLLECT STARS
// ======================================

void collectStars() {

  for (int i = 0; i < 8; i++) {

    if (stars[i].collected) {
      continue;
    }

    bool overlapX =
      playerX + HERO_W > stars[i].x &&
      playerX < stars[i].x + 7;

    bool overlapY =
      playerY + HERO_H > stars[i].y &&
      playerY < stars[i].y + 7;

    if (overlapX && overlapY) {

      stars[i].collected = true;
      score += 10;
    }
  }
}

// ======================================
// ENEMY MOVEMENT
// ======================================

void moveEnemies() {

  for (int i = 0; i < 4; i++) {

    Enemy &e = enemies[i];

    if (!e.alive) {
      continue;
    }

    int movementInterval = 4 - level;

    if (frames % movementInterval == 0) {

      e.x += e.direction;

      if (e.x <= e.minX) {
        e.x = e.minX;
        e.direction = 1;
      }

      if (e.x >= e.maxX) {
        e.x = e.maxX;
        e.direction = -1;
      }
    }
  }
}

// ======================================
// ENEMY COLLISION
// ======================================

void checkEnemyCollisions() {

  for (int i = 0; i < 4; i++) {

    Enemy &e = enemies[i];

    if (!e.alive) {
      continue;
    }

    bool overlapX =
      playerX + HERO_W - 3 > e.x + 2 &&
      playerX + 3 < e.x + 10;

    bool overlapY =
      playerY + HERO_H - 2 > 48 &&
      playerY + 3 < 55;

    if (overlapX && overlapY) {

      // Shield defeats enemy
      if (shieldTimer > 0) {

        e.alive = false;
        score += 15;
        continue;
      }

      // Player takes damage
      if (invulnerableTimer == 0) {

        lives--;
        invulnerableTimer = 36;

        playerX -= 12;

        if (playerX < 0) {
          playerX = 0;
        }

        if (lives <= 0) {

          if (score > bestScore) {
            bestScore = score;
          }

          gameState = GAME_OVER;
          return;
        }
      }
    }
  }
}

// ======================================
// MAIN GAME UPDATE
// ======================================

void updateGame(
  bool left,
  bool right,
  bool jumpPressed,
  bool attackPressed,
  bool specialPressed
) {

  frames++;

  // Ability timers
  if (attackFlash > 0) {
    attackFlash--;
  }

  if (attackCooldown > 0) {
    attackCooldown--;
  }

  if (shieldTimer > 0) {
    shieldTimer--;
  }

  if (shieldCooldown > 0) {
    shieldCooldown--;
  }

  if (invulnerableTimer > 0) {
    invulnerableTimer--;
  }

  // Move player first
  updatePlayer(
    left,
    right,
    jumpPressed
  );

  // D5 - SHOOT ENERGY STAR
  if (attackPressed) {
    fireEnergyStar();
  }

  // D6 - BUBBLE SHIELD
  if (
    specialPressed &&
    shieldCooldown == 0
  ) {

    shieldTimer = 50;
    shieldCooldown = 240;
  }

  // Update world
  collectStars();
  moveEnemies();

  // Projectile collision before enemy touch
  updateEnergyStars();
  checkEnemyCollisions();

  if (gameState == GAME_OVER) {
    drawEndScreen();
    return;
  }

  // Reach finish flag
  if (playerX >= 600) {

    if (level < 3) {

      level++;

      if (lives < 3) {
        lives++;
      }

      createLevel();

    } else {

      if (score > bestScore) {
        bestScore = score;
      }

      gameState = VICTORY;
      drawEndScreen();
      return;
    }
  }

  drawWorld();
}

// ======================================
// ARDUINO SETUP
// ======================================

void setup() {

  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(JUMP_BTN, INPUT_PULLUP);
  pinMode(ATTACK_BTN, INPUT_PULLUP);
  pinMode(SPECIAL_BTN, INPUT_PULLUP);

  Wire.begin();

  if (!display.begin(
    SSD1306_SWITCHCAPVCC,
    OLED_ADDR
  )) {

    while (true) {
      delay(100);
    }
  }

  randomSeed(analogRead(A0));

  display.clearDisplay();
  display.display();

  drawMenu();
}

// ======================================
// MAIN LOOP
// ======================================

void loop() {

  if (millis() - lastFrame < FRAME_MS) {
    return;
  }

  lastFrame = millis();

  bool left =
    digitalRead(LEFT_BTN) == LOW;

  bool right =
    digitalRead(RIGHT_BTN) == LOW;

  bool jumpHeld =
    digitalRead(JUMP_BTN) == LOW;

  bool attackHeld =
    digitalRead(ATTACK_BTN) == LOW;

  bool specialHeld =
    digitalRead(SPECIAL_BTN) == LOW;

  // Button press detection
  bool jumpPressed =
    jumpHeld && !previousJump;

  bool attackPressed =
    attackHeld && !previousAttack;

  bool specialPressed =
    specialHeld && !previousSpecial;

  previousJump = jumpHeld;
  previousAttack = attackHeld;
  previousSpecial = specialHeld;

  // Start / restart
  if (gameState != PLAY) {

    if (jumpPressed) {
      startGame();
    }

    return;
  }

  updateGame(
    left,
    right,
    jumpPressed,
    attackPressed,
    specialPressed
  );
}
