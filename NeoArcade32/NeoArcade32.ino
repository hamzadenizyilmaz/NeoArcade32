#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>
#include <string.h>

/*
  TFT:
    CS   -> GPIO17
    RST  -> GPIO18
    DC   -> GPIO19
    BL   -> GPIO20
    SCK  -> GPIO21
    MOSI -> GPIO22

  Joy1:
    VRX  -> GPIO0
    VRY  -> GPIO1
    SW   -> GPIO2

  Joy2 (opsiyonel):
    VRX  -> GPIO3
    VRY  -> GPIO4
    SW   -> GPIO5

  Buttons:
    BTN1 -> GPIO6
    BTN2 -> GPIO7
    BTN3 -> GPIO9
    BTN4 -> GPIO10

  Buzzer:
    BUZZER -> GPIO11

  RGB:
    R -> GPIO14
    G -> GPIO15
    B -> GPIO16
*/

// ========================= Donanim =========================
#define TFT_CS    17
#define TFT_RST   18
#define TFT_DC    19
#define TFT_BL    20
#define TFT_SCK   21
#define TFT_MOSI  22

#define J1_VRX    0
#define J1_VRY    1
#define J1_SW     2

#define USE_SECOND_JOYSTICK true
#define J2_VRX    3
#define J2_VRY    4
#define J2_SW     5

#define BTN1      6
#define BTN2      7
#define BTN3      9
#define BTN4      10

#define BUZZER_PIN 11

#define LED_R     14
#define LED_G     15
#define LED_B     16

#define RGB_COMMON_ANODE false

// ========================= Ekran =========================
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ========================= Parlak renk paleti =========================
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_RED_NEON    0xF800
#define C_GREEN_NEON  0x07E0
#define C_BLUE_NEON   0x03FF
#define C_YELLOW_NEON 0xFFE0
#define C_PINK_NEON   0xF81F
#define C_ORANGE_NEON 0xFD20
#define C_PURPLE      0x801F
#define C_SKY         0x867F
#define C_MINT        0x7FFA
#define C_PANEL_DARK  0x18C3
#define C_PANEL_DEEP  0x10A2
#define C_STEEL       0x2945
#define C_TRACK       0x39E7
#define C_GRASS       0x2585
#define C_BG_DARK     0x0841

// ========================= Tema =========================
struct Theme {
  uint16_t bg;
  uint16_t panel;
  uint16_t panel2;
  uint16_t text;
  uint16_t accent;
  uint16_t accent2;
  uint16_t accent3;
  uint16_t good;
  uint16_t bad;
  uint16_t glow;
};

Theme themes[] = {
  {0x0841, 0x18C3, 0x2124, C_WHITE,      C_BLUE_NEON,   C_YELLOW_NEON, C_PINK_NEON,   C_GREEN_NEON, C_RED_NEON,    C_MINT},
  {0x0000, 0x1082, 0x18E3, C_WHITE,      C_GREEN_NEON,  C_SKY,         C_YELLOW_NEON, C_GREEN_NEON, C_RED_NEON,    C_SKY},
  {0x1042, 0x30E4, 0x49A6, C_WHITE,      C_ORANGE_NEON, C_YELLOW_NEON, C_PINK_NEON,   C_MINT,       C_RED_NEON,    C_ORANGE_NEON},
  {0x080F, 0x1818, 0x281A, C_WHITE,      C_PINK_NEON,   C_BLUE_NEON,   C_YELLOW_NEON, C_GREEN_NEON, C_RED_NEON,    C_PINK_NEON}
};
const int THEME_COUNT = sizeof(themes) / sizeof(themes[0]);
int gThemeIndex = 0;
Theme &UI() { return themes[gThemeIndex]; }

// ========================= Ayarlar =========================
int gDifficulty = 1;           // 0 kolay 1 normal 2 zor
bool gBacklightOn = true;
bool gSoundOn = true;
int gDeadZone = 420;

// ========================= Ses =========================
static const int BUZZER_CH = 0;

// ESP32 core uyumluluk katmani
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #define LEDC_ATTACH_OK 1
#else
  #define LEDC_ATTACH_OK 0
#endif

void initBuzzer() {
#if LEDC_ATTACH_OK
  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWriteTone(BUZZER_PIN, 0);
#else
  ledcSetup(BUZZER_CH, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
  ledcWriteTone(BUZZER_CH, 0);
#endif
}

void playTone(int freq, int ms) {
  if (!gSoundOn) return;
#if LEDC_ATTACH_OK
  ledcWriteTone(BUZZER_PIN, freq);
  delay(ms);
  ledcWriteTone(BUZZER_PIN, 0);
#else
  ledcWriteTone(BUZZER_CH, freq);
  delay(ms);
  ledcWriteTone(BUZZER_CH, 0);
#endif
}

void stopTone() {
#if LEDC_ATTACH_OK
  ledcWriteTone(BUZZER_PIN, 0);
#else
  ledcWriteTone(BUZZER_CH, 0);
#endif
}

// ========================= RGB =========================
void writeLedPin(int pin, bool on) {
  if (RGB_COMMON_ANODE) digitalWrite(pin, on ? LOW : HIGH);
  else digitalWrite(pin, on ? HIGH : LOW);
}

void setRGB(bool r, bool g, bool b) {
  writeLedPin(LED_R, r);
  writeLedPin(LED_G, g);
  writeLedPin(LED_B, b);
}

void rgbOff() { setRGB(false, false, false); }

void setBacklight(bool on) {
  gBacklightOn = on;
  digitalWrite(TFT_BL, on ? HIGH : LOW);
}

// ========================= Yardimci =========================
unsigned long nowMs() { return millis(); }

template <typename T>
T clampValue(T v, T lo, T hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int mapSignedToSpeed(int axis, int maxSpeed) {
  if (axis < 0) return -maxSpeed;
  if (axis > 0) return maxSpeed;
  return 0;
}

// ========================= Input =========================
struct Button {
  int pin;
  bool current;
  bool previous;
};

Button btn1{BTN1, false, false};
Button btn2{BTN2, false, false};
Button btn3{BTN3, false, false};
Button btn4{BTN4, false, false};
Button joy1Sw{J1_SW, false, false};
#if USE_SECOND_JOYSTICK
Button joy2Sw{J2_SW, false, false};
#endif

void updateButton(Button &b) {
  b.previous = b.current;
  b.current = (digitalRead(b.pin) == LOW);
}

bool justPressed(const Button &b) { return b.current && !b.previous; }

struct Joystick {
  int pinX;
  int pinY;
  int centerX;
  int centerY;
  int deadZone;
  int rawX;
  int rawY;
  int axisX;
  int axisY;
};

Joystick joy1{J1_VRX, J1_VRY, 2048, 2048, 420, 2048, 2048, 0, 0};
#if USE_SECOND_JOYSTICK
Joystick joy2{J2_VRX, J2_VRY, 2048, 2048, 420, 2048, 2048, 0, 0};
#endif

// Arduino .ino otomatik prototype hatalarini engellemek icin
void updateButton(Button &b);
bool justPressed(const Button &b);
void updateJoystick(Joystick &j);
int readAvg(int pin, int samples);
int axisStateFromRaw(int raw, int center, int deadZone);
void calibrateJoysticks();
void updateInputs();

int readAvg(int pin, int samples = 4) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(1);
  }
  return (int)(total / samples);
}

int axisStateFromRaw(int raw, int center, int deadZone) {
  int d = raw - center;
  if (d > deadZone) return 1;
  if (d < -deadZone) return -1;
  return 0;
}

void updateJoystick(Joystick &j) {
  j.rawX = readAvg(j.pinX, 3);
  j.rawY = readAvg(j.pinY, 3);
  j.axisX = axisStateFromRaw(j.rawX, j.centerX, j.deadZone);
  j.axisY = axisStateFromRaw(j.rawY, j.centerY, j.deadZone);
}

void calibrateJoysticks() {
  joy1.centerX = readAvg(joy1.pinX, 14);
  joy1.centerY = readAvg(joy1.pinY, 14);
  joy1.deadZone = gDeadZone;
#if USE_SECOND_JOYSTICK
  joy2.centerX = readAvg(joy2.pinX, 14);
  joy2.centerY = readAvg(joy2.pinY, 14);
  joy2.deadZone = gDeadZone;
#endif
}

void updateInputs() {
  updateButton(btn1);
  updateButton(btn2);
  updateButton(btn3);
  updateButton(btn4);
  updateButton(joy1Sw);
#if USE_SECOND_JOYSTICK
  updateButton(joy2Sw);
#endif
  updateJoystick(joy1);
#if USE_SECOND_JOYSTICK
  updateJoystick(joy2);
#endif
}

// ========================= UI =========================
void clearScreen(uint16_t color) { tft.fillScreen(color); }

void centerText(const char *text, int y, int size, uint16_t color) {
  int len = strlen(text);
  int w = len * 6 * size;
  int x = (SCREEN_W - w) / 2;
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

void fillHeader(const char *title, uint16_t color) {
  tft.fillRect(0, 0, SCREEN_W, 30, color);
  tft.drawFastHLine(0, 30, SCREEN_W, UI().glow);
  tft.setTextColor(C_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print(title);
}

void fillFooter(const char *text) {
  tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 20, UI().panel2);
  tft.setTextColor(UI().text);
  tft.setTextSize(1);
  tft.setCursor(6, SCREEN_H - 14);
  tft.print(text);
}

void drawCard(int x, int y, int w, int h, uint16_t fillColor, uint16_t borderColor) {
  tft.fillRoundRect(x, y, w, h, 8, fillColor);
  tft.drawRoundRect(x, y, w, h, 8, borderColor);
}

void drawStatusTopRight(const char *label, int value) {
  tft.fillRect(212, 34, 104, 18, UI().bg);
  tft.setTextColor(UI().text);
  tft.setTextSize(2);
  tft.setCursor(214, 35);
  tft.print(label);
  tft.print(value);
}

void drawButtonLegend(const char *b1, const char *b2, const char *b3, const char *b4) {
  tft.setTextColor(UI().text);
  tft.setTextSize(1);
  tft.fillRect(0, 220, 320, 20, UI().panel2);
  tft.setCursor(4, 226);   tft.print("B1:"); tft.print(b1);
  tft.setCursor(82, 226);  tft.print("B2:"); tft.print(b2);
  tft.setCursor(160, 226); tft.print("B3:"); tft.print(b3);
  tft.setCursor(238, 226); tft.print("B4:"); tft.print(b4);
}

// ========================= Sayfalar =========================
enum Page {
  PAGE_BOOT,
  PAGE_HOME,
  PAGE_SINGLE,
  PAGE_MULTI,
  PAGE_SETTINGS,
  PAGE_DODGE,
  PAGE_SNAKE,
  PAGE_BREAKOUT,
  PAGE_FLAPPY,
  PAGE_RUNNER,
  PAGE_RACER,
  PAGE_PONG,
  PAGE_REFLEX,
  PAGE_ARENA,
  PAGE_GAMEOVER
};

Page gPage = PAGE_BOOT;
Page gLastGame = PAGE_DODGE;
int gLastScore = 0;
char gGameOverTitle[32] = "GAME OVER";

// ========================= Menuler =========================
int homeIndex = 0;
int singleIndex = 0;
int multiIndex = 0;
int settingsIndex = 0;

const char *homeItems[] = {"TEK KISILIK", "2 KISILIK", "AYARLAR"};
const char *singleItems[] = {"DODGE", "SNAKE", "BREAKOUT", "FLAPPY BIRD", "RUNNER", "CAR RACER"};
const char *multiItems[] = {"PONG DUEL", "REFLEX DUEL", "ARENA DUEL"};
const char *settingsItems[] = {"Ses", "Tema", "Zorluk", "Arka Isik", "Dead Zone", "Kalibrasyon", "RGB Test"};

void gotoGameOver(const char *title, int score = 0) {
  strncpy(gGameOverTitle, title, sizeof(gGameOverTitle) - 1);
  gGameOverTitle[sizeof(gGameOverTitle) - 1] = '\0';
  gLastScore = score;
  gPage = PAGE_GAMEOVER;
  setRGB(true, false, false);
}

// ========================= Splash / Boot =========================
void bootLoadingScreen() {
  clearScreen(C_BLACK);
  for (int i = 0; i < 160; i += 8) {
    tft.drawCircle(160, 120, i, (i % 16 == 0) ? UI().accent : UI().accent3);
  }

  centerText("RUBY ARCADE", 34, 3, UI().accent2);
  centerText("ESP32-C6 ULTRA", 68, 3, UI().accent);
  centerText("TFT POWER BOOT", 102, 2, UI().glow);

  drawCard(44, 150, 232, 26, UI().panel, UI().accent2);
  for (int i = 0; i <= 100; i += 10) {
    int w = (228 * i) / 100;
    tft.fillRect(46, 152, w, 22, (i < 40) ? UI().accent : ((i < 80) ? UI().accent2 : UI().accent3));
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.fillRect(128, 182, 80, 16, C_BLACK);
    tft.setCursor(132, 182);
    tft.print(i);
    tft.print(" %");
    if (i < 40) setRGB(false, false, true);
    else if (i < 80) setRGB(false, true, true);
    else setRGB(true, true, true);
    delay(45);
  }

  centerText("Sistem hazirleniyor...", 206, 1, UI().text);
}

// ========================= Menu cizimi =========================
void drawMenuPage(const char *title, const char **items, int count, int selected, uint16_t headerColor, const char *footer) {
  clearScreen(UI().bg);
  fillHeader(title, headerColor);

  for (int i = 0; i < count; i++) {
    int y = 42 + i * 30;
    uint16_t bg = (i == selected) ? UI().accent2 : UI().panel;
    uint16_t tc = (i == selected) ? C_BLACK : UI().text;
    drawCard(24, y, 272, 24, bg, headerColor);
    tft.setTextColor(tc);
    tft.setTextSize(2);
    tft.setCursor(38, y + 5);
    tft.print(items[i]);
  }

  fillFooter(footer);
}

void drawHome() {
  drawMenuPage("ANA SAYFA", homeItems, 3, homeIndex, UI().accent, "JOY/B3/B4 gezin  B1 gir  B2 geri");
  setRGB(false, true, false);
}

void drawSingleMenu() {
  drawMenuPage("TEK KISILIK", singleItems, 6, singleIndex, UI().accent, "B3 yukari  B4 asagi  B1 sec  B2 geri");
  setRGB(false, true, true);
}

void drawMultiMenu() {
  drawMenuPage("2 KISILIK", multiItems, 3, multiIndex, C_ORANGE_NEON, "B3/B4 menu  B1 sec  B2 geri");
  setRGB(true, true, false);
}

void drawSettings() {
  clearScreen(UI().bg);
  fillHeader("AYARLAR", UI().accent3);

  for (int i = 0; i < 7; i++) {
    int y = 38 + i * 25;
    uint16_t bg = (i == settingsIndex) ? UI().accent3 : UI().panel;
    uint16_t tc = (i == settingsIndex) ? C_BLACK : UI().text;
    drawCard(12, y, 296, 22, bg, UI().accent3);
    tft.setTextColor(tc);
    tft.setTextSize(1);
    tft.setCursor(20, y + 7);
    tft.print(settingsItems[i]);

    tft.setCursor(188, y + 7);
    if (i == 0) tft.print(gSoundOn ? "Acik" : "Kapali");
    if (i == 1) tft.print(gThemeIndex);
    if (i == 2) {
      if (gDifficulty == 0) tft.print("Kolay");
      else if (gDifficulty == 1) tft.print("Normal");
      else tft.print("Zor");
    }
    if (i == 3) tft.print(gBacklightOn ? "Acik" : "Kapali");
    if (i == 4) tft.print(gDeadZone);
    if (i == 5) tft.print("Calistir");
    if (i == 6) tft.print("Calistir");
  }
  drawButtonLegend("Uygula", "Geri", "Yukari", "Asagi");
  setRGB(true, false, true);
}

void runRgbTest() {
  setRGB(true, false, false); delay(110);
  setRGB(false, true, false); delay(110);
  setRGB(false, false, true); delay(110);
  setRGB(true, true, false); delay(110);
  setRGB(false, true, true); delay(110);
  setRGB(true, false, true); delay(110);
  setRGB(true, true, true); delay(110);
  rgbOff();
}

void moveMenuIndex(int &index, int maxCount, int joyY) {
  static unsigned long lastMove = 0;
  if (justPressed(btn3)) {
    index--;
    if (index < 0) index = maxCount - 1;
    playTone(1800, 14);
    return;
  }
  if (justPressed(btn4)) {
    index++;
    if (index >= maxCount) index = 0;
    playTone(1800, 14);
    return;
  }
  if (joyY != 0 && nowMs() - lastMove > 160) {
    lastMove = nowMs();
    if (joyY < 0) index = (index + maxCount - 1) % maxCount;
    else index = (index + 1) % maxCount;
    playTone(1800, 14);
  }
}

void updateHome() {
  int old = homeIndex;
  moveMenuIndex(homeIndex, 3, joy1.axisY);
  if (old != homeIndex) drawHome();

  if (justPressed(btn1) || justPressed(joy1Sw)) {
    if (homeIndex == 0) { gPage = PAGE_SINGLE; drawSingleMenu(); }
    else if (homeIndex == 1) { gPage = PAGE_MULTI; drawMultiMenu(); }
    else { gPage = PAGE_SETTINGS; drawSettings(); }
    playTone(2400, 25);
  }
}

void updateSingleMenu() {
  int old = singleIndex;
  moveMenuIndex(singleIndex, 6, joy1.axisY);
  if (old != singleIndex) drawSingleMenu();

  if (justPressed(btn2)) {
    gPage = PAGE_HOME;
    drawHome();
    return;
  }

  if (justPressed(btn1) || justPressed(joy1Sw)) {
    if (singleIndex == 0) gPage = PAGE_DODGE;
    else if (singleIndex == 1) gPage = PAGE_SNAKE;
    else if (singleIndex == 2) gPage = PAGE_BREAKOUT;
    else if (singleIndex == 3) gPage = PAGE_FLAPPY;
    else if (singleIndex == 4) gPage = PAGE_RUNNER;
    else gPage = PAGE_RACER;
    playTone(2400, 25);
  }
}

void updateMultiMenu() {
  int old = multiIndex;
  moveMenuIndex(multiIndex, 3, joy1.axisY);
  if (old != multiIndex) drawMultiMenu();

  if (justPressed(btn2)) {
    gPage = PAGE_HOME;
    drawHome();
    return;
  }

  if (justPressed(btn1) || justPressed(joy1Sw)) {
    if (multiIndex == 0) gPage = PAGE_PONG;
    else if (multiIndex == 1) gPage = PAGE_REFLEX;
    else gPage = PAGE_ARENA;
    playTone(2400, 25);
  }
}

void updateSettings() {
  int old = settingsIndex;
  moveMenuIndex(settingsIndex, 7, joy1.axisY);
  if (old != settingsIndex) drawSettings();

  if (justPressed(btn2)) {
    gPage = PAGE_HOME;
    drawHome();
    return;
  }

  if (justPressed(btn1) || justPressed(joy1Sw)) {
    if (settingsIndex == 0) gSoundOn = !gSoundOn;
    else if (settingsIndex == 1) gThemeIndex = (gThemeIndex + 1) % THEME_COUNT;
    else if (settingsIndex == 2) gDifficulty = (gDifficulty + 1) % 3;
    else if (settingsIndex == 3) setBacklight(!gBacklightOn);
    else if (settingsIndex == 4) {
      gDeadZone += 40;
      if (gDeadZone > 700) gDeadZone = 260;
      joy1.deadZone = gDeadZone;
#if USE_SECOND_JOYSTICK
      joy2.deadZone = gDeadZone;
#endif
    }
    else if (settingsIndex == 5) {
      clearScreen(UI().bg);
      fillHeader("KALIBRASYON", UI().accent3);
      centerText("Joystickleri birak", 90, 2, UI().text);
      centerText("ve bekle...", 120, 2, UI().text);
      delay(700);
      calibrateJoysticks();
      playTone(2500, 35);
    }
    else if (settingsIndex == 6) {
      runRgbTest();
      playTone(2500, 30);
    }
    drawSettings();
  }
}

// ========================= Oyun: Dodge =========================
struct DodgeState {
  int playerX, playerY, playerW, playerH;
  int enemyX[4], enemyY[4], enemyS[4], enemySpeed[4];
  int score;
  unsigned long lastTick;
  bool initialized;
} dodge;

bool dodgeCollide(int ex, int ey, int es) {
  return !(dodge.playerX + dodge.playerW < ex || dodge.playerX > ex + es || dodge.playerY + dodge.playerH < ey || dodge.playerY > ey + es);
}

void initDodge() {
  gLastGame = PAGE_DODGE;
  dodge.playerW = 26;
  dodge.playerH = 16;
  dodge.playerX = 147;
  dodge.playerY = 194;
  dodge.score = 0;
  dodge.lastTick = 0;
  int base = 2 + gDifficulty;
  for (int i = 0; i < 4; i++) {
    dodge.enemyX[i] = random(10, SCREEN_W - 24);
    dodge.enemyY[i] = -random(30, 180 * (i + 1));
    dodge.enemyS[i] = 12 + i * 2;
    dodge.enemySpeed[i] = base + i;
  }
  dodge.initialized = true;
  clearScreen(UI().bg);
  fillHeader("DODGE", UI().accent);
  drawButtonLegend("-", "Cik", "Sola", "Saga");
  setRGB(false, false, true);
}

void drawDodgePlayer() {
  tft.fillRoundRect(dodge.playerX, dodge.playerY, dodge.playerW, dodge.playerH, 4, UI().accent);
  tft.fillRect(dodge.playerX + 6, dodge.playerY - 4, 14, 4, UI().accent2);
}

void drawDodge() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  for (int y = 40; y < 216; y += 18) {
    tft.fillRect(106, y, 4, 10, UI().panel2);
    tft.fillRect(212, y, 4, 10, UI().panel2);
  }
  drawDodgePlayer();
  for (int i = 0; i < 4; i++) tft.fillRect(dodge.enemyX[i], dodge.enemyY[i], dodge.enemyS[i], dodge.enemyS[i], UI().bad);
  drawStatusTopRight("SKOR:", dodge.score);
}

void updateDodge() {
  if (!dodge.initialized) initDodge();
  if (justPressed(btn2)) { dodge.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }
  if (nowMs() - dodge.lastTick < 24) return;
  dodge.lastTick = nowMs();

  int moveX = mapSignedToSpeed(joy1.axisX, 6);
  if (btn3.current) moveX -= 2;
  if (btn4.current) moveX += 2;
  dodge.playerX = clampValue(dodge.playerX + moveX, 4, SCREEN_W - dodge.playerW - 4);

  for (int i = 0; i < 4; i++) {
    dodge.enemyY[i] += dodge.enemySpeed[i];
    if (dodge.enemyY[i] > 220) {
      dodge.enemyY[i] = -random(18, 90);
      dodge.enemyX[i] = random(8, SCREEN_W - dodge.enemyS[i] - 8);
      dodge.score++;
      if ((dodge.score % 10) == 0 && dodge.enemySpeed[i] < 12 + gDifficulty * 2) dodge.enemySpeed[i]++;
    }
    if (dodgeCollide(dodge.enemyX[i], dodge.enemyY[i], dodge.enemyS[i])) {
      playTone(650, 70);
      dodge.initialized = false;
      gotoGameOver("DODGE BITTI", dodge.score);
      return;
    }
  }
  drawDodge();
}

// ========================= Oyun: Snake =========================
struct SnakeState {
  static const int MAX_SEG = 160;
  static const int CELL = 10;
  static const int GRID_W = 24;
  static const int GRID_H = 17;
  int x[MAX_SEG];
  int y[MAX_SEG];
  int len;
  int dir;
  int foodX;
  int foodY;
  int score;
  unsigned long lastTick;
  bool initialized;
} snake;

void snakeSpawnFood() {
  bool ok = false;
  while (!ok) {
    ok = true;
    snake.foodX = random(0, SnakeState::GRID_W);
    snake.foodY = random(0, SnakeState::GRID_H);
    for (int i = 0; i < snake.len; i++) {
      if (snake.x[i] == snake.foodX && snake.y[i] == snake.foodY) ok = false;
    }
  }
}

void initSnake() {
  gLastGame = PAGE_SNAKE;
  snake.len = 4;
  snake.dir = 0;
  snake.score = 0;
  snake.lastTick = 0;
  for (int i = 0; i < snake.len; i++) {
    snake.x[i] = 5 - i;
    snake.y[i] = 6;
  }
  snakeSpawnFood();
  snake.initialized = true;
  clearScreen(UI().bg);
  fillHeader("SNAKE", UI().good);
  drawButtonLegend("-", "Cik", "Sol", "Sag");
  setRGB(false, true, true);
}

void snakeSetDirectionFromInput() {
  if (btn3.current && snake.dir != 0) snake.dir = 2;
  if (btn4.current && snake.dir != 2) snake.dir = 0;
  if (joy1.axisX != 0 && joy1.axisY == 0) {
    if (joy1.axisX > 0 && snake.dir != 2) snake.dir = 0;
    if (joy1.axisX < 0 && snake.dir != 0) snake.dir = 2;
  } else if (joy1.axisY != 0 && joy1.axisX == 0) {
    if (joy1.axisY > 0 && snake.dir != 3) snake.dir = 1;
    if (joy1.axisY < 0 && snake.dir != 1) snake.dir = 3;
  }
}

void drawSnakeCell(int gx, int gy, uint16_t c) {
  int ox = 38 + gx * SnakeState::CELL;
  int oy = 38 + gy * SnakeState::CELL;
  tft.fillRect(ox, oy, SnakeState::CELL - 1, SnakeState::CELL - 1, c);
}

void drawSnake() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  tft.drawRect(36, 36, SnakeState::GRID_W * SnakeState::CELL + 4, SnakeState::GRID_H * SnakeState::CELL + 4, UI().panel2);
  drawSnakeCell(snake.foodX, snake.foodY, UI().bad);
  for (int i = 0; i < snake.len; i++) drawSnakeCell(snake.x[i], snake.y[i], (i == 0) ? UI().accent2 : UI().good);
  drawStatusTopRight("SKOR:", snake.score);
}

void updateSnake() {
  if (!snake.initialized) initSnake();
  if (justPressed(btn2)) { snake.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }
  snakeSetDirectionFromInput();

  int stepMs = (gDifficulty == 0) ? 180 : (gDifficulty == 1 ? 130 : 95);
  if (nowMs() - snake.lastTick < (unsigned long)stepMs) return;
  snake.lastTick = nowMs();

  for (int i = snake.len; i > 0; i--) {
    snake.x[i] = snake.x[i - 1];
    snake.y[i] = snake.y[i - 1];
  }

  if (snake.dir == 0) snake.x[0]++;
  else if (snake.dir == 1) snake.y[0]++;
  else if (snake.dir == 2) snake.x[0]--;
  else snake.y[0]--;

  if (snake.x[0] < 0 || snake.x[0] >= SnakeState::GRID_W || snake.y[0] < 0 || snake.y[0] >= SnakeState::GRID_H) {
    snake.initialized = false;
    gotoGameOver("SNAKE BITTI", snake.score);
    return;
  }

  for (int i = 1; i < snake.len; i++) {
    if (snake.x[0] == snake.x[i] && snake.y[0] == snake.y[i]) {
      snake.initialized = false;
      gotoGameOver("SNAKE BITTI", snake.score);
      return;
    }
  }

  if (snake.x[0] == snake.foodX && snake.y[0] == snake.foodY) {
    snake.len = clampValue(snake.len + 1, 0, SnakeState::MAX_SEG - 1);
    snake.score++;
    playTone(2500, 20);
    snakeSpawnFood();
  }
  drawSnake();
}

// ========================= Oyun: Breakout =========================
struct BreakoutState {
  bool bricks[5][9];
  int padX;
  int ballX;
  int ballY;
  int vX;
  int vY;
  int score;
  unsigned long lastTick;
  bool initialized;
} breakout;

void initBreakout() {
  gLastGame = PAGE_BREAKOUT;
  for (int r = 0; r < 5; r++) for (int c = 0; c < 9; c++) breakout.bricks[r][c] = true;
  breakout.padX = 130;
  breakout.ballX = 160;
  breakout.ballY = 155;
  breakout.vX = 3 + gDifficulty;
  breakout.vY = -3 - gDifficulty;
  breakout.score = 0;
  breakout.lastTick = 0;
  breakout.initialized = true;
  clearScreen(UI().bg);
  fillHeader("BREAKOUT", UI().accent3);
  drawButtonLegend("-", "Cik", "Sola", "Saga");
  setRGB(true, false, false);
}

void drawBreakout() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 9; c++) {
      if (!breakout.bricks[r][c]) continue;
      uint16_t col = (r % 3 == 0) ? UI().accent2 : ((r % 3 == 1) ? UI().accent3 : UI().glow);
      tft.fillRect(12 + c * 33, 40 + r * 14, 28, 10, col);
    }
  }
  tft.fillRoundRect(breakout.padX, 200, 60, 10, 3, UI().accent);
  tft.fillCircle(breakout.ballX, breakout.ballY, 4, UI().text);
  drawStatusTopRight("SKOR:", breakout.score);
}

void updateBreakout() {
  if (!breakout.initialized) initBreakout();
  if (justPressed(btn2)) { breakout.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }
  if (nowMs() - breakout.lastTick < 16) return;
  breakout.lastTick = nowMs();

  int move = mapSignedToSpeed(joy1.axisX, 7);
  if (btn3.current) move -= 2;
  if (btn4.current) move += 2;
  breakout.padX = clampValue(breakout.padX + move, 6, SCREEN_W - 66);

  breakout.ballX += breakout.vX;
  breakout.ballY += breakout.vY;

  if (breakout.ballX < 5 || breakout.ballX > SCREEN_W - 5) breakout.vX = -breakout.vX;
  if (breakout.ballY < 34) breakout.vY = -breakout.vY;

  if (breakout.ballY >= 194 && breakout.ballY <= 206 && breakout.ballX >= breakout.padX && breakout.ballX <= breakout.padX + 60) {
    breakout.vY = -abs(breakout.vY);
    if (breakout.ballX < breakout.padX + 20) breakout.vX = -4 - gDifficulty;
    else if (breakout.ballX > breakout.padX + 40) breakout.vX = 4 + gDifficulty;
  }

  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 9; c++) {
      if (!breakout.bricks[r][c]) continue;
      int bx = 12 + c * 33;
      int by = 40 + r * 14;
      if (breakout.ballX >= bx && breakout.ballX <= bx + 28 && breakout.ballY >= by && breakout.ballY <= by + 10) {
        breakout.bricks[r][c] = false;
        breakout.vY = -breakout.vY;
        breakout.score++;
        playTone(2500, 8);
      }
    }
  }

  if (breakout.ballY > 220) {
    breakout.initialized = false;
    gotoGameOver("BREAKOUT BITTI", breakout.score);
    return;
  }

  bool anyBrick = false;
  for (int r = 0; r < 5; r++) for (int c = 0; c < 9; c++) if (breakout.bricks[r][c]) anyBrick = true;
  if (!anyBrick) {
    breakout.initialized = false;
    gotoGameOver("BREAKOUT TEMIZ", breakout.score);
    return;
  }
  drawBreakout();
}

// ========================= Oyun: Flappy Bird =========================
struct FlappyState {
  int birdX;
  float birdY;
  float velY;
  int pipeX[3];
  int gapY[3];
  bool scored[3];
  int gapH;
  int score;
  unsigned long lastTick;
  bool initialized;
} flappy;

bool flappyHitPipe(int pipeX, int gapY, int gapH) {
  int birdW = 14;
  int birdH = 14;
  int birdTop = (int)flappy.birdY;
  int birdBottom = birdTop + birdH;
  int birdRight = flappy.birdX + birdW;
  if (birdRight < pipeX || flappy.birdX > pipeX + 24) return false;
  if (birdTop > gapY && birdBottom < gapY + gapH) return false;
  return true;
}

void initFlappy() {
  gLastGame = PAGE_FLAPPY;
  flappy.birdX = 70;
  flappy.birdY = 110;
  flappy.velY = 0;
  flappy.gapH = (gDifficulty == 0) ? 78 : (gDifficulty == 1 ? 64 : 52);
  flappy.score = 0;
  flappy.lastTick = 0;
  for (int i = 0; i < 3; i++) {
    flappy.pipeX[i] = 230 + i * 110;
    flappy.gapY[i] = random(48, 142);
    flappy.scored[i] = false;
  }
  flappy.initialized = true;
  clearScreen(UI().bg);
  fillHeader("FLAPPY BIRD", UI().accent2);
  drawButtonLegend("Zipla", "Cik", "Zipla", "Zipla");
  setRGB(true, true, true);
}

void drawFlappyBirdSprite(int x, int y) {
  tft.fillRoundRect(x, y, 14, 14, 4, UI().accent2);
  tft.fillCircle(x + 11, y + 5, 2, C_WHITE);
  tft.fillTriangle(x + 13, y + 7, x + 18, y + 9, x + 13, y + 11, C_ORANGE_NEON);
  tft.fillTriangle(x + 4, y + 8, x + 0, y + 12, x + 7, y + 11, UI().accent3);
}

void drawFlappy() {
  tft.fillRect(0, 30, SCREEN_W, 190, C_SKY);
  tft.fillRect(0, 198, SCREEN_W, 22, C_GREEN_NEON);
  for (int i = 0; i < 3; i++) {
    tft.fillRect(flappy.pipeX[i], 30, 24, flappy.gapY[i] - 30, C_GREEN_NEON);
    tft.fillRect(flappy.pipeX[i], flappy.gapY[i] + flappy.gapH, 24, 198 - (flappy.gapY[i] + flappy.gapH), C_GREEN_NEON);
    tft.fillRect(flappy.pipeX[i] - 2, flappy.gapY[i] - 6, 28, 6, C_GREEN_NEON);
    tft.fillRect(flappy.pipeX[i] - 2, flappy.gapY[i] + flappy.gapH, 28, 6, C_GREEN_NEON);
  }
  drawFlappyBirdSprite(flappy.birdX, (int)flappy.birdY);
  drawStatusTopRight("SKOR:", flappy.score);
}

void updateFlappy() {
  if (!flappy.initialized) initFlappy();
  if (justPressed(btn2)) { flappy.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }

  if (justPressed(btn1) || justPressed(joy1Sw) || justPressed(btn3) || justPressed(btn4)) {
    flappy.velY = -4.9f;
    playTone(2400, 8);
  }

  if (nowMs() - flappy.lastTick < 22) return;
  flappy.lastTick = nowMs();

  flappy.velY += 0.36f;
  if (flappy.velY > 5.0f) flappy.velY = 5.0f;
  flappy.birdY += flappy.velY;

  for (int i = 0; i < 3; i++) {
    flappy.pipeX[i] -= 3 + gDifficulty;
    if (!flappy.scored[i] && flappy.pipeX[i] + 24 < flappy.birdX) {
      flappy.scored[i] = true;
      flappy.score++;
      playTone(2200, 10);
    }
    if (flappy.pipeX[i] < -30) {
      flappy.pipeX[i] = SCREEN_W + random(28, 60);
      flappy.gapY[i] = random(52, 140);
      flappy.scored[i] = false;
    }
    if (flappyHitPipe(flappy.pipeX[i], flappy.gapY[i], flappy.gapH)) {
      flappy.initialized = false;
      gotoGameOver("FLAPPY BIRD BITTI", flappy.score);
      return;
    }
  }

  if (flappy.birdY < 30 || flappy.birdY > 184) {
    flappy.initialized = false;
    gotoGameOver("FLAPPY BIRD BITTI", flappy.score);
    return;
  }
  drawFlappy();
}

// ========================= Oyun: Runner =========================
struct RunnerState {
  int playerX;
  float playerY;
  float velY;
  bool onGround;
  int obsX[3];
  int obsW[3];
  int obsH[3];
  int score;
  unsigned long lastTick;
  bool initialized;
} runner;

bool runnerCollide(int ox, int ow, int oh) {
  int px = runner.playerX;
  int py = (int)runner.playerY;
  int pw = 16;
  int ph = 18;
  int oy = 200 - oh;
  return !(px + pw < ox || px > ox + ow || py + ph < oy || py > oy + oh);
}

void initRunner() {
  gLastGame = PAGE_RUNNER;
  runner.playerX = 55;
  runner.playerY = 182;
  runner.velY = 0;
  runner.onGround = true;
  for (int i = 0; i < 3; i++) {
    runner.obsX[i] = 220 + i * 100;
    runner.obsW[i] = 10 + random(10, 18);
    runner.obsH[i] = 18 + random(8, 26);
  }
  runner.score = 0;
  runner.lastTick = 0;
  runner.initialized = true;
  clearScreen(UI().bg);
  fillHeader("RUNNER", UI().accent3);
  drawButtonLegend("Zipla", "Cik", "Zipla", "Zipla");
  setRGB(false, true, false);
}

void drawRunner() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  tft.fillRect(0, 200, SCREEN_W, 4, UI().panel2);
  tft.fillRoundRect(runner.playerX, (int)runner.playerY, 16, 18, 4, UI().accent);
  for (int i = 0; i < 3; i++) tft.fillRect(runner.obsX[i], 200 - runner.obsH[i], runner.obsW[i], runner.obsH[i], UI().bad);
  drawStatusTopRight("SKOR:", runner.score);
}

void updateRunner() {
  if (!runner.initialized) initRunner();
  if (justPressed(btn2)) { runner.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }

  if ((justPressed(btn1) || justPressed(joy1Sw) || justPressed(btn3) || justPressed(btn4)) && runner.onGround) {
    runner.velY = -6.8f;
    runner.onGround = false;
    playTone(2400, 10);
  }

  if (nowMs() - runner.lastTick < 20) return;
  runner.lastTick = nowMs();

  runner.velY += 0.36f;
  runner.playerY += runner.velY;
  if (runner.playerY >= 182) {
    runner.playerY = 182;
    runner.velY = 0;
    runner.onGround = true;
  }

  for (int i = 0; i < 3; i++) {
    runner.obsX[i] -= 4 + gDifficulty;
    if (runner.obsX[i] < -30) {
      runner.obsX[i] = SCREEN_W + random(20, 80);
      runner.obsW[i] = 10 + random(10, 18);
      runner.obsH[i] = 18 + random(8, 26);
      runner.score++;
      playTone(2200, 8);
    }
    if (runnerCollide(runner.obsX[i], runner.obsW[i], runner.obsH[i])) {
      runner.initialized = false;
      gotoGameOver("RUNNER BITTI", runner.score);
      return;
    }
  }
  drawRunner();
}

// ========================= YENI OYUN: CAR RACER =========================
struct RacerState {
  int playerLane;
  int playerY;
  int enemyLane[4];
  int enemyY[4];
  int score;
  int roadOffset;
  unsigned long lastTick;
  bool initialized;
} racer;

const int racerLaneX[3] = {72, 152, 232};

bool racerCrash(int lane, int y) {
  return racer.playerLane == lane && abs(racer.playerY - y) < 24;
}

void initRacer() {
  gLastGame = PAGE_RACER;
  racer.playerLane = 1;
  racer.playerY = 186;
  racer.score = 0;
  racer.roadOffset = 0;
  racer.lastTick = 0;
  for (int i = 0; i < 4; i++) {
    racer.enemyLane[i] = random(0, 3);
    racer.enemyY[i] = -random(40, 140 * (i + 1));
  }
  racer.initialized = true;
  clearScreen(C_TRACK);
  fillHeader("CAR RACER", C_ORANGE_NEON);
  drawButtonLegend("Turbo", "Cik", "Sol", "Sag");
  setRGB(true, false, false);
}

void drawCarSprite(int x, int y, uint16_t body, uint16_t glass) {
  tft.fillRoundRect(x, y, 24, 34, 5, body);
  tft.fillRoundRect(x + 4, y + 5, 16, 8, 3, glass);
  tft.fillRect(x - 2, y + 5, 3, 8, C_BLACK);
  tft.fillRect(x + 23, y + 5, 3, 8, C_BLACK);
  tft.fillRect(x - 2, y + 22, 3, 8, C_BLACK);
  tft.fillRect(x + 23, y + 22, 3, 8, C_BLACK);
  tft.fillRect(x + 6, y + 17, 12, 10, body);
  tft.drawRect(x + 8, y + 29, 8, 3, C_YELLOW_NEON);
}

void drawRacer() {
  tft.fillRect(0, 30, SCREEN_W, 190, C_TRACK);
  tft.fillRect(0, 30, 34, 190, C_GRASS);
  tft.fillRect(286, 30, 34, 190, C_GRASS);
  tft.drawFastVLine(34, 30, 190, C_WHITE);
  tft.drawFastVLine(286, 30, 190, C_WHITE);

  for (int y = -20 + racer.roadOffset; y < 220; y += 30) {
    tft.fillRect(114, y, 8, 16, C_WHITE);
    tft.fillRect(194, y, 8, 16, C_WHITE);
  }

  drawCarSprite(racerLaneX[racer.playerLane], racer.playerY, UI().accent2, UI().glow);
  for (int i = 0; i < 4; i++) drawCarSprite(racerLaneX[racer.enemyLane[i]], racer.enemyY[i], UI().bad, C_SKY);
  drawStatusTopRight("SKOR:", racer.score);
}

void updateRacer() {
  if (!racer.initialized) initRacer();
  if (justPressed(btn2)) { racer.initialized = false; gPage = PAGE_SINGLE; drawSingleMenu(); return; }

  if (justPressed(btn3) || joy1.axisX < 0) {
    racer.playerLane--;
    if (racer.playerLane < 0) racer.playerLane = 0;
  }
  if (justPressed(btn4) || joy1.axisX > 0) {
    racer.playerLane++;
    if (racer.playerLane > 2) racer.playerLane = 2;
  }

  if (nowMs() - racer.lastTick < 24) return;
  racer.lastTick = nowMs();

  int baseSpeed = 5 + gDifficulty;
  if (btn1.current || joy1Sw.current) baseSpeed += 2;
  racer.roadOffset = (racer.roadOffset + baseSpeed) % 30;

  for (int i = 0; i < 4; i++) {
    racer.enemyY[i] += baseSpeed + i / 2;
    if (racer.enemyY[i] > 230) {
      racer.enemyY[i] = -random(50, 120);
      racer.enemyLane[i] = random(0, 3);
      racer.score++;
      if ((racer.score % 8) == 0) playTone(2300, 10);
    }
    if (racerCrash(racer.enemyLane[i], racer.enemyY[i])) {
      playTone(500, 90);
      racer.initialized = false;
      gotoGameOver("CAR RACER BITTI", racer.score);
      return;
    }
  }
  drawRacer();
}

// ========================= Oyun: Pong =========================
struct PongState {
  int p1Y, p2Y;
  int ballX, ballY;
  int vX, vY;
  int score1, score2;
  unsigned long lastTick;
  bool initialized;
} pong;

void pongResetBall(int dir) {
  pong.ballX = 160;
  pong.ballY = 120;
  pong.vX = dir * (4 + gDifficulty);
  pong.vY = random(2, 5);
  if (random(0, 2) == 0) pong.vY = -pong.vY;
}

void initPong() {
  gLastGame = PAGE_PONG;
  pong.p1Y = 90;
  pong.p2Y = 90;
  pong.ballX = 160;
  pong.ballY = 120;
  pong.vX = 4 + gDifficulty;
  pong.vY = 3;
  pong.score1 = 0;
  pong.score2 = 0;
  pong.lastTick = 0;
  pong.initialized = true;
  clearScreen(UI().bg);
  fillHeader("PONG DUEL", C_ORANGE_NEON);
  drawButtonLegend("-", "Cik", "P2 Yuk", "P2 Asag");
  setRGB(true, true, false);
}

void drawPong() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  for (int y = 40; y < 210; y += 12) tft.fillRect(158, y, 4, 6, UI().panel2);
  tft.fillRect(10, pong.p1Y, 8, 42, UI().accent);
  tft.fillRect(302, pong.p2Y, 8, 42, UI().bad);
  tft.fillRect(pong.ballX, pong.ballY, 8, 8, UI().text);
  tft.setTextColor(UI().text);
  tft.setTextSize(2);
  tft.setCursor(112, 34); tft.print(pong.score1);
  tft.setCursor(192, 34); tft.print(pong.score2);
}

void updatePong() {
  if (!pong.initialized) initPong();
  if (justPressed(btn2)) { pong.initialized = false; gPage = PAGE_MULTI; drawMultiMenu(); return; }
  if (nowMs() - pong.lastTick < 18) return;
  pong.lastTick = nowMs();

  pong.p1Y += mapSignedToSpeed(joy1.axisY, 5);
#if USE_SECOND_JOYSTICK
  pong.p2Y += mapSignedToSpeed(joy2.axisY, 5);
#else
  if (btn3.current) pong.p2Y -= 5;
  if (btn4.current) pong.p2Y += 5;
#endif
  pong.p1Y = clampValue(pong.p1Y, 34, 176);
  pong.p2Y = clampValue(pong.p2Y, 34, 176);

  pong.ballX += pong.vX;
  pong.ballY += pong.vY;
  if (pong.ballY <= 30 || pong.ballY >= 210) pong.vY = -pong.vY;
  if (pong.ballX <= 18 && pong.ballY + 8 >= pong.p1Y && pong.ballY <= pong.p1Y + 42) pong.vX = abs(pong.vX);
  if (pong.ballX + 8 >= 302 && pong.ballY + 8 >= pong.p2Y && pong.ballY <= pong.p2Y + 42) pong.vX = -abs(pong.vX);

  if (pong.ballX < 0) { pong.score2++; pongResetBall(1); }
  if (pong.ballX > SCREEN_W) { pong.score1++; pongResetBall(-1); }
  drawPong();

  if (pong.score1 >= 5 || pong.score2 >= 5) {
    pong.initialized = false;
    gotoGameOver((pong.score1 > pong.score2) ? "P1 KAZANDI" : "P2 KAZANDI", (pong.score1 > pong.score2) ? pong.score1 : pong.score2);
  }
}

// ========================= Oyun: Reflex =========================
struct ReflexState {
  int phase;
  unsigned long targetMs;
  bool initialized;
} reflex;

void initReflex() {
  gLastGame = PAGE_REFLEX;
  reflex.phase = 0;
  reflex.targetMs = nowMs() + 1200;
  reflex.initialized = true;
  clearScreen(UI().bg);
  fillHeader("REFLEX DUEL", UI().accent3);
  centerText("P1 = BTN1", 74, 3, UI().accent);
  centerText("P2 = BTN3", 114, 3, UI().bad);
  centerText("Hazir olun...", 164, 2, UI().text);
  fillFooter("Erken basan kaybeder  B2 cik");
  setRGB(true, false, true);
}

void updateReflex() {
  if (!reflex.initialized) initReflex();
  if (justPressed(btn2)) { reflex.initialized = false; gPage = PAGE_MULTI; drawMultiMenu(); return; }

  if (reflex.phase == 0) {
    if (nowMs() > reflex.targetMs) {
      reflex.phase = 1;
      reflex.targetMs = nowMs() + random(1000, 3200);
      centerText("Bekle...", 194, 2, UI().accent2);
    }
    return;
  }

  if (reflex.phase == 1) {
    if (justPressed(btn1)) { reflex.initialized = false; gotoGameOver("P2 KAZANDI", 0); return; }
    if (justPressed(btn3)) { reflex.initialized = false; gotoGameOver("P1 KAZANDI", 0); return; }
    if (nowMs() > reflex.targetMs) {
      reflex.phase = 2;
      clearScreen(UI().bg);
      fillHeader("REFLEX DUEL", UI().accent3);
      centerText("SIMDI BAS!", 96, 4, UI().good);
      fillFooter("P1=BTN1  P2=BTN3");
      playTone(2600, 40);
    }
    return;
  }

  if (reflex.phase == 2) {
    if (justPressed(btn1)) { reflex.initialized = false; gotoGameOver("P1 KAZANDI", 0); return; }
    if (justPressed(btn3)) { reflex.initialized = false; gotoGameOver("P2 KAZANDI", 0); return; }
  }
}

// ========================= Oyun: Arena =========================
struct ArenaState {
  int p1x, p1y;
  int p2x, p2y;
  int coinX, coinY;
  int s1, s2;
  unsigned long lastTick;
  bool initialized;
} arena;

void arenaSpawnCoin() {
  arena.coinX = random(20, 300);
  arena.coinY = random(50, 200);
}

bool pointHit(int px, int py, int tx, int ty, int r = 10) {
  return abs(px - tx) < r && abs(py - ty) < r;
}

void initArena() {
  gLastGame = PAGE_ARENA;
  arena.p1x = 40; arena.p1y = 120;
  arena.p2x = 260; arena.p2y = 120;
  arena.s1 = 0; arena.s2 = 0;
  arena.lastTick = 0;
  arenaSpawnCoin();
  arena.initialized = true;
  clearScreen(UI().bg);
  fillHeader("ARENA DUEL", UI().accent);
  drawButtonLegend("-", "Cik", "P2 Yuk", "P2 Asag");
  setRGB(false, true, false);
}

void drawArena() {
  tft.fillRect(0, 30, SCREEN_W, 190, UI().bg);
  for (int x = 0; x < SCREEN_W; x += 20) tft.drawFastVLine(x, 30, 190, UI().panel);
  for (int y = 30; y < 220; y += 20) tft.drawFastHLine(0, y, SCREEN_W, UI().panel);
  tft.fillCircle(arena.coinX, arena.coinY, 6, UI().accent2);
  tft.fillRoundRect(arena.p1x, arena.p1y, 14, 14, 3, UI().accent);
  tft.fillRoundRect(arena.p2x, arena.p2y, 14, 14, 3, UI().bad);
  tft.setTextColor(UI().text);
  tft.setTextSize(2);
  tft.setCursor(112, 34); tft.print(arena.s1);
  tft.setCursor(192, 34); tft.print(arena.s2);
}

void updateArena() {
  if (!arena.initialized) initArena();
  if (justPressed(btn2)) { arena.initialized = false; gPage = PAGE_MULTI; drawMultiMenu(); return; }
  if (nowMs() - arena.lastTick < 26) return;
  arena.lastTick = nowMs();

  arena.p1x += mapSignedToSpeed(joy1.axisX, 4);
  arena.p1y += mapSignedToSpeed(joy1.axisY, 4);
#if USE_SECOND_JOYSTICK
  arena.p2x += mapSignedToSpeed(joy2.axisX, 4);
  arena.p2y += mapSignedToSpeed(joy2.axisY, 4);
#else
  if (btn3.current) arena.p2y -= 4;
  if (btn4.current) arena.p2y += 4;
#endif

  arena.p1x = clampValue(arena.p1x, 4, SCREEN_W - 18);
  arena.p1y = clampValue(arena.p1y, 34, 202);
  arena.p2x = clampValue(arena.p2x, 4, SCREEN_W - 18);
  arena.p2y = clampValue(arena.p2y, 34, 202);

  if (pointHit(arena.p1x + 7, arena.p1y + 7, arena.coinX, arena.coinY, 14)) { arena.s1++; arenaSpawnCoin(); playTone(2300, 10); }
  if (pointHit(arena.p2x + 7, arena.p2y + 7, arena.coinX, arena.coinY, 14)) { arena.s2++; arenaSpawnCoin(); playTone(1800, 10); }
  drawArena();

  if (arena.s1 >= 7 || arena.s2 >= 7) {
    arena.initialized = false;
    gotoGameOver((arena.s1 > arena.s2) ? "P1 KAZANDI" : "P2 KAZANDI", (arena.s1 > arena.s2) ? arena.s1 : arena.s2);
  }
}

// ========================= Game Over =========================
void drawGameOver() {
  clearScreen(UI().bg);
  fillHeader("SONUC", UI().bad);
  centerText(gGameOverTitle, 70, 3, UI().accent2);
  tft.setTextColor(UI().text);
  tft.setTextSize(2);
  tft.setCursor(106, 118);
  tft.print("Skor: ");
  tft.print(gLastScore);
  drawCard(58, 154, 204, 26, UI().panel, UI().accent);
  drawCard(58, 188, 204, 26, UI().panel2, UI().accent2);
  tft.setTextColor(UI().text);
  tft.setTextSize(2);
  tft.setCursor(88, 161); tft.print("BTN1: Tekrar");
  tft.setCursor(96, 195); tft.print("BTN2: Menu");
  fillFooter("B1 tekrar  B2 ana menu");
  setRGB(true, false, false);
}

void resetCurrentGame() {
  if (gLastGame == PAGE_DODGE) initDodge();
  else if (gLastGame == PAGE_SNAKE) initSnake();
  else if (gLastGame == PAGE_BREAKOUT) initBreakout();
  else if (gLastGame == PAGE_FLAPPY) initFlappy();
  else if (gLastGame == PAGE_RUNNER) initRunner();
  else if (gLastGame == PAGE_RACER) initRacer();
  else if (gLastGame == PAGE_PONG) initPong();
  else if (gLastGame == PAGE_REFLEX) initReflex();
  else if (gLastGame == PAGE_ARENA) initArena();
  gPage = gLastGame;
}

void updateGameOver() {
  static bool firstDraw = true;
  if (firstDraw) {
    drawGameOver();
    firstDraw = false;
  }
  if (justPressed(btn1) || justPressed(joy1Sw)) {
    playTone(2400, 20);
    firstDraw = true;
    resetCurrentGame();
  }
  if (justPressed(btn2)) {
    playTone(1800, 20);
    firstDraw = true;
    gPage = PAGE_HOME;
    drawHome();
  }
}

// ========================= Setup =========================
void setupPins() {
  pinMode(TFT_BL, OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);
  pinMode(J1_SW, INPUT_PULLUP);
#if USE_SECOND_JOYSTICK
  pinMode(J2_SW, INPUT_PULLUP);
#endif
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  rgbOff();
  setBacklight(true);
}

void setupDisplay() {
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1);
  clearScreen(UI().bg);
}

void setup() {
  setupPins();
  initBuzzer();
  randomSeed(analogRead(J1_VRX) + analogRead(J1_VRY) + micros());
  setupDisplay();

  bootLoadingScreen();
  delay(180);
  calibrateJoysticks();
  playTone(2500, 35);

  gPage = PAGE_HOME;
  drawHome();
}

// ========================= Loop =========================
void loop() {
  updateInputs();

  switch (gPage) {
    case PAGE_HOME:      updateHome(); break;
    case PAGE_SINGLE:    updateSingleMenu(); break;
    case PAGE_MULTI:     updateMultiMenu(); break;
    case PAGE_SETTINGS:  updateSettings(); break;
    case PAGE_DODGE:     updateDodge(); break;
    case PAGE_SNAKE:     updateSnake(); break;
    case PAGE_BREAKOUT:  updateBreakout(); break;
    case PAGE_FLAPPY:    updateFlappy(); break;
    case PAGE_RUNNER:    updateRunner(); break;
    case PAGE_RACER:     updateRacer(); break;
    case PAGE_PONG:      updatePong(); break;
    case PAGE_REFLEX:    updateReflex(); break;
    case PAGE_ARENA:     updateArena(); break;
    case PAGE_GAMEOVER:  updateGameOver(); break;
    default:
      gPage = PAGE_HOME;
      drawHome();
      break;
  }
}
