// ============================================================================
//  Pyango v0.1 - Pocket flashcards for learning languages
//  MoonDevices - https://github.com/DrMoon17/pyango
//  License: MIT
///////////////////////////////////////////////////////////////////////////////
//  HOW TO USE IT
//    Short press          = move (up/down in menus, previous/next card)
//    HOLD 1 second        = select in menus / REVEAL the answer on a card
//                           (a bar fills up at the bottom while you hold)
//    Press BOTH buttons   = go back
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ---------------------------------------------------------------- Edition
// 1 = English edition: learn Japanese, Spanish and German (English menus)
// 0 = Edición en español: aprende inglés, alemán y japonés (menús en español)
#define PYANGO_ENGLISH 1

#if PYANGO_ENGLISH
  #include "decks_en.h"
  const char* const CAT_LABELS[CAT_COUNT] = {"Numbers", "Verbs", "Positions/Directions", "Everyday words"};
  const char* const LBL_ALL      = "(All together)";
  const char* const LBL_TITLE    = "Choose a language";
  const char* const LBL_HINT     = "Hold a button to reveal";
#else
  #include "decks_es.h"
  const char* const CAT_LABELS[CAT_COUNT] = {"N\xC3\xBAmeros", "Verbos", "Posiciones/Direcciones", "Palabras cotidianas"};
  const char* const LBL_ALL      = "(Todo junto)";
  const char* const LBL_TITLE    = "Elige un idioma";
  const char* const LBL_HINT     = "Mant\xC3\xA9n presionado para revelar";
#endif

// ---------------------------------------------------------------- Hardware
// 1.3" display (SH1106)? Use: U8G2_SH1106_128X64_NONAME_F_HW_I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const uint8_t PIN_LEFT  = 32;
const uint8_t PIN_RIGHT = 33;

const int W = 128;
const int H = 64;

#define FONT_CJK   u8g2_font_unifont_t_japanese3
#define FONT_TXT   u8g2_font_6x10_tf
#define FONT_SMALL u8g2_font_5x7_tf
#define FONT_TINY  u8g2_font_4x6_tf

// Latin fonts for the big word, largest first
const uint8_t* const LATIN_BIG[] = {
  u8g2_font_helvB24_tf, u8g2_font_helvB18_tf, u8g2_font_helvB14_tf,
  u8g2_font_helvB12_tf, u8g2_font_helvB10_tf, u8g2_font_helvB08_tf
};
const int N_LATIN_BIG = sizeof(LATIN_BIG) / sizeof(LATIN_BIG[0]);

// ---------------------------------------------------------------- Timing
const unsigned long T_HOLD  = 1000;  // hold time to select / reveal
const unsigned long T_SHORT = 450;   // released before this = short press
const unsigned long T_BAR   = 150;   // the hold bar appears after this
const unsigned long T_SPLASH = 2500;

// ---------------------------------------------------------------- Buttons
enum Event { EV_NONE, EV_SHORT, EV_HOLD, EV_CANCEL };

struct Button {
  uint8_t pin;
  bool stable;
  bool raw;
  unsigned long tChange;
  unsigned long tDown;
  bool holdDone;
};

Button btnL = {PIN_LEFT,  false, false, 0, 0, false};
Button btnR = {PIN_RIGHT, false, false, 0, 0, false};

// Debounced button reading. Short = on release; hold = when T_HOLD is reached;
// cancel = released between T_SHORT and T_HOLD.
Event readButton(Button& b) {
  bool r = (digitalRead(b.pin) == LOW);  // INPUT_PULLUP: pressed = LOW
  unsigned long now = millis();
  if (r != b.raw) {
    b.raw = r;
    b.tChange = now;
  }
  if (now - b.tChange > 25 && r != b.stable) {
    b.stable = r;
    if (r) {
      b.tDown = now;
      b.holdDone = false;
    } else if (!b.holdDone) {
      return (now - b.tDown < T_SHORT) ? EV_SHORT : EV_CANCEL;
    }
  }
  if (b.stable && !b.holdDone && now - b.tDown >= T_HOLD) {
    b.holdDone = true;
    return EV_HOLD;
  }
  return EV_NONE;
}

// ---------------------------------------------------------------- State
enum Screen { SC_SPLASH, SC_LANG, SC_CAT, SC_CARD };
Screen screen = SC_SPLASH;

int langSel = 0;          // selected language in the main menu
int catSel = 0;           // selected option in the category menu
int catMenu[CAT_COUNT + 1];  // category menu options (CAT_COUNT = all together)
int catMenuN = 0;

int cards[MAX_CARDS];     // indices of the cards in the current session
int nCards = 0;
int pos = 0;              // current card
bool showAnswer = false;

uint8_t screenCopy[W * H / 8];  // copy of the screen to draw the hold bar on top
bool barVisible = false;
unsigned long tBar = 0;
unsigned long tSplash = 0;
bool combo = false;       // both buttons pressed

const Deck& deck() { return DECKS[langSel]; }
const Card& card() { return deck().cards[cards[pos]]; }

// ---------------------------------------------------------------- Text helpers
void setTxt(const uint8_t* f) {
  oled.setFont(f);
  oled.setFontRefHeightText();
  oled.setFontPosBaseline();
}

void setCjkTop() {
  oled.setFont(FONT_CJK);
  oled.setFontRefHeightAll();  // real glyph height, so kanji are not cut
  oled.setFontPosTop();
}

int textW(const char* s) { return oled.getUTF8Width(s); }

void drawCentered(int y, const char* s) {
  int x = (W - textW(s)) / 2;
  oled.drawUTF8(x < 0 ? 0 : x, y, s);
}

// Copies into buf as much of s as fits in maxW (UTF-8 aware) and adds ".."
void truncateToFit(const char* s, int maxW, char* buf, size_t bufSize) {
  size_t out = 0;
  const char* p = s;
  while (*p) {
    int len = 1;
    uint8_t c = (uint8_t)*p;
    if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
    if (out + len + 3 >= bufSize) break;
    memcpy(buf + out, p, len);
    memcpy(buf + out + len, "..", 3);
    if (textW(buf) > maxW) break;
    out += len;
    p += len;
  }
  memcpy(buf + out, "..", 3);
}

// One line of text: 6x10 font, 5x7 if it doesn't fit, truncated as a last resort
void drawFit(int x, int y, int maxW, const char* s, bool center) {
  setTxt(FONT_TXT);
  if (textW(s) > maxW) setTxt(FONT_SMALL);
  char buf[96];
  const char* t = s;
  if (textW(s) > maxW) {
    truncateToFit(s, maxW, buf, sizeof(buf));
    t = buf;
  }
  if (center) x = (W - textW(t)) / 2;
  oled.drawUTF8(x < 0 ? 0 : x, y, t);
}

// Word-wrapped, centered text (6x10 font)
void drawWrapped(int y, int lineH, int maxLines, const char* s) {
  setTxt(FONT_TXT);
  char line[64] = "";
  char test[64];
  int lines = 0;
  const char* p = s;
  while (*p && lines < maxLines) {
    const char* start = p;
    while (*p && *p != ' ') p++;
    char word[48];
    size_t wl = min((size_t)(p - start), sizeof(word) - 1);
    memcpy(word, start, wl);
    word[wl] = '\0';
    while (*p == ' ') p++;

    if (line[0]) snprintf(test, sizeof(test), "%s %s", line, word);
    else snprintf(test, sizeof(test), "%s", word);

    if (textW(test) <= W - 2) {
      strncpy(line, test, sizeof(line) - 1);
    } else {
      if (line[0]) {
        drawCentered(y + lines * lineH, line);
        lines++;
      }
      strncpy(line, word, sizeof(line) - 1);
    }
    line[sizeof(line) - 1] = '\0';
  }
  if (line[0] && lines < maxLines) drawCentered(y + lines * lineH, line);
}

// ---------------------------------------------------------------- Big word
bool pixelAt(int x, int y) {
  uint8_t* buf = oled.getBufferPtr();
  return buf[x + (y / 8) * W] & (1 << (y % 8));
}

// Kanji: draw at 16 px, read the pixels back and scale up to fill the screen
void drawBigCjk(const char* word) {
  static bool src[H][W];
  const int Y0 = 8;
  oled.clearBuffer();
  setCjkTop();
  oled.drawUTF8(0, Y0, word);

  int w = textW(word);
  if (w > W) w = W;
  if (w < 1) return;

  int yMin = Y0, yMax = Y0 + oled.getMaxCharHeight() - 1;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < w; x++) {
      src[y][x] = pixelAt(x, y);
      if (src[y][x]) {
        if (y < yMin) yMin = y;
        if (y > yMax) yMax = y;
      }
    }
  }
  if (yMax >= H) yMax = H - 1;
  int h = yMax - yMin + 1;

  float s = min((float)W / w, (float)H / h);
  int ox = (W - (int)(w * s)) / 2;
  int oy = (H - (int)(h * s)) / 2;

  oled.clearBuffer();
  for (int y = 0; y < h; y++) {
    int y0 = oy + (int)(y * s), y1 = oy + (int)((y + 1) * s);
    for (int x = 0; x < w; x++) {
      if (!src[yMin + y][x]) continue;
      int x0 = ox + (int)(x * s), x1 = ox + (int)((x + 1) * s);
      if (x1 > x0 && y1 > y0) oled.drawBox(x0, y0, x1 - x0, y1 - y0);
    }
  }
}

// Latin alphabet: the largest font the word fits in
void drawBigLatin(const char* word) {
  oled.clearBuffer();
  for (int i = 0; i < N_LATIN_BIG; i++) {
    oled.setFont(LATIN_BIG[i]);
    if (textW(word) <= W - 4 || i == N_LATIN_BIG - 1) break;
  }
  oled.setFontRefHeightText();
  oled.setFontPosCenter();
  drawCentered(H / 2, word);
}

// ---------------------------------------------------------------- Screens
void drawSplash() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB18_tf);
  oled.setFontRefHeightText();
  oled.setFontPosBaseline();
  drawCentered(28, "Pyango");
  setTxt(FONT_TINY);
  drawCentered(38, "by MoonDevices  v0.1");
  oled.drawHLine(10, 46, W - 20);
  drawFit(0, 58, W, LBL_HINT, true);
}

// Menu with a title bar and up to 4 visible rows (scrolls if there are more)
void drawMenu(const char* title, const char* const* items, int n, int sel) {
  oled.clearBuffer();
  oled.drawBox(0, 0, W, 11);
  oled.setDrawColor(0);
  setTxt(FONT_TXT);
  drawCentered(9, title);
  oled.setDrawColor(1);

  const int ROWS = 4, ROW_H = 13, TOP = 12;
  int first = sel - ROWS + 1;
  if (first < 0) first = 0;
  for (int r = 0; r < ROWS && first + r < n; r++) {
    int i = first + r;
    int y = TOP + r * ROW_H;
    if (i == sel) {
      oled.drawRBox(0, y, W, ROW_H - 1, 2);
      oled.setDrawColor(0);
    }
    drawFit(4, y + 10, W - 8, items[i], false);
    oled.setDrawColor(1);
  }
  // Scroll hints
  if (first > 0) oled.drawTriangle(W - 6, 16, W - 2, 16, W - 4, 13);
  if (first + ROWS < n) oled.drawTriangle(W - 6, 60, W - 2, 60, W - 4, 63);
}

void drawLangMenu() {
  const char* items[8];
  int n = min((int)NUM_DECKS, 8);
  for (int i = 0; i < n; i++) items[i] = DECKS[i].name;
  drawMenu(LBL_TITLE, items, n, langSel);
}

void drawCatMenu() {
  const char* items[CAT_COUNT + 1];
  for (int i = 0; i < catMenuN; i++) items[i] = (catMenu[i] == CAT_COUNT) ? LBL_ALL : CAT_LABELS[catMenu[i]];
  drawMenu(deck().name, items, catMenuN, catSel);
}

void drawAnswer() {
  const Card& c = card();
  oled.clearBuffer();

  // Top bar: card number
  oled.drawBox(0, 0, W, 11);
  oled.setDrawColor(0);
  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d / %d", pos + 1, nCards);
  setTxt(FONT_TXT);
  drawCentered(9, cnt);
  oled.setDrawColor(1);

  bool cjk = deck().script == SCRIPT_CJK;
  if (cjk) {
    // Kanji + kana if both fit, otherwise just the kana
    setCjkTop();
    char buf[96];
    snprintf(buf, sizeof(buf), "%s\xE3\x80\x80%s", c.word, c.reading);  // ideographic space
    const char* t = (c.reading[0] && textW(buf) <= W) ? buf : (c.reading[0] ? c.reading : c.word);
    drawCentered(12, t);
  } else {
    oled.setFont(u8g2_font_helvB10_tf);
    oled.setFontRefHeightText();
    oled.setFontPosTop();
    if (textW(c.word) > W) { setTxt(FONT_TXT); oled.setFontPosTop(); }
    drawCentered(14, c.word);
  }

  // Pronunciation line: romaji (Japanese) or [guide] + plural/past (Latin)
  char line[96];
  if (cjk) snprintf(line, sizeof(line), "%s", c.pron);
  else if (c.pron[0] && c.reading[0]) snprintf(line, sizeof(line), "[%s] %s", c.pron, c.reading);
  else if (c.pron[0]) snprintf(line, sizeof(line), "[%s]", c.pron);
  else snprintf(line, sizeof(line), "%s", c.reading);
  if (line[0]) drawFit(0, 38, W, line, true);

  drawWrapped(50, 11, 2, c.meaning);
}

void drawCard() {
  if (showAnswer) drawAnswer();
  else if (deck().script == SCRIPT_CJK) drawBigCjk(card().word);
  else drawBigLatin(card().word);
}

void redraw() {
  switch (screen) {
    case SC_SPLASH: drawSplash(); break;
    case SC_LANG:   drawLangMenu(); break;
    case SC_CAT:    drawCatMenu(); break;
    case SC_CARD:   drawCard(); break;
  }
  memcpy(screenCopy, oled.getBufferPtr(), sizeof(screenCopy));
  oled.sendBuffer();
  barVisible = false;
}

// Bar at the bottom while a button is held
void drawHoldBar(float f) {
  if (f > 1) f = 1;
  memcpy(oled.getBufferPtr(), screenCopy, sizeof(screenCopy));
  oled.setDrawColor(0);
  oled.drawBox(0, H - 3, W, 3);
  oled.setDrawColor(1);
  oled.drawBox(0, H - 2, (int)(W * f), 2);
  oled.sendBuffer();
  barVisible = true;
}

void restoreScreen() {
  memcpy(oled.getBufferPtr(), screenCopy, sizeof(screenCopy));
  oled.sendBuffer();
  barVisible = false;
}

// ---------------------------------------------------------------- Navigation
// Builds the category menu with only the categories this language has
void openLanguage() {
  catMenuN = 0;
  for (int c = 0; c < CAT_COUNT; c++) {
    for (int i = 0; i < deck().count; i++) {
      if (deck().cards[i].category == c) { catMenu[catMenuN++] = c; break; }
    }
  }
  catMenu[catMenuN++] = CAT_COUNT;  // (All together)
  catSel = 0;
  screen = SC_CAT;
}

void openCategory() {
  int cat = catMenu[catSel];
  nCards = 0;
  for (int i = 0; i < deck().count; i++) {
    if (cat == CAT_COUNT || deck().cards[i].category == cat) cards[nCards++] = i;
  }
  pos = 0;
  showAnswer = false;
  screen = SC_CARD;
}

void goBack() {
  if (screen == SC_CARD) screen = SC_CAT;
  else if (screen == SC_CAT) screen = SC_LANG;
}

void onShort(bool left) {
  switch (screen) {
    case SC_SPLASH:
      screen = SC_LANG;
      break;
    case SC_LANG:
      langSel = left ? (langSel + NUM_DECKS - 1) % NUM_DECKS : (langSel + 1) % NUM_DECKS;
      break;
    case SC_CAT:
      catSel = left ? (catSel + catMenuN - 1) % catMenuN : (catSel + 1) % catMenuN;
      break;
    case SC_CARD:
      pos = left ? (pos + nCards - 1) % nCards : (pos + 1) % nCards;
      showAnswer = false;
      break;
  }
}

void onHold() {
  switch (screen) {
    case SC_SPLASH: screen = SC_LANG; break;
    case SC_LANG:   openLanguage(); break;
    case SC_CAT:    openCategory(); break;
    case SC_CARD:   showAnswer = !showAnswer; break;
  }
}

// ---------------------------------------------------------------- Program
void setup() {
  setCpuFrequencyMhz(80);  // lower power, plenty for this
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);

  oled.setBusClock(400000);
  oled.begin();
  oled.enableUTF8Print();

  tSplash = millis();
  redraw();
}

void loop() {
  unsigned long now = millis();
  Event eL = readButton(btnL);
  Event eR = readButton(btnR);

  // Both buttons together = back
  if (btnL.stable && btnR.stable) {
    combo = true;
    btnL.holdDone = btnR.holdDone = true;  // cancel their individual events
  }
  if (combo) {
    if (!btnL.stable && !btnR.stable) {
      combo = false;
      goBack();
      redraw();
    }
    delay(5);
    return;
  }

  bool changed = true;
  if (eL == EV_SHORT)      onShort(true);
  else if (eR == EV_SHORT) onShort(false);
  else if (eL == EV_HOLD || eR == EV_HOLD) onHold();
  else changed = false;

  if (changed) {
    redraw();
  } else if (eL == EV_CANCEL || eR == EV_CANCEL) {
    restoreScreen();
  } else {
    // Progress bar while a button is being held
    Button* b = nullptr;
    if (btnL.stable && !btnL.holdDone) b = &btnL;
    else if (btnR.stable && !btnR.holdDone) b = &btnR;

    if (b) {
      unsigned long t = now - b->tDown;
      if (t > T_BAR && now - tBar > 40) {
        drawHoldBar((float)(t - T_BAR) / (T_HOLD - T_BAR));
        tBar = now;
      }
    } else if (barVisible) {
      restoreScreen();
    }
  }

  // The splash screen moves on by itself
  if (screen == SC_SPLASH && now - tSplash > T_SPLASH) {
    screen = SC_LANG;
    redraw();
  }

  delay(5);
}
