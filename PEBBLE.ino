#include <LovyanGFX.hpp>

// =====================================================
// ST7735 1.44" (128x128) CONFIGURATION
// =====================================================

class LGFX : public lgfx::LGFX_Device {

  lgfx::Panel_ST7735S panel;
  lgfx::Bus_SPI bus;

public:

  LGFX() {
    {
      auto cfg = bus.config();

      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;

      // 10 MHz works on the breadboard; drop to 4000000 if you ever see speckles
      cfg.freq_write = 10000000;
      cfg.freq_read  = 2000000;

      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 0;

      // XIAO ESP32-S3
      cfg.pin_sclk = 7;     // D8
      cfg.pin_mosi = 9;     // D10
      cfg.pin_miso = -1;
      cfg.pin_dc   = 4;     // D3

      bus.config(cfg);
      panel.setBus(&bus);
    }
    {
      auto cfg = panel.config();

      cfg.pin_cs  = 2;      // D1
      cfg.pin_rst = 3;      // D2
      cfg.pin_busy = -1;

      // 1.44" square screen: the chip is set up for 128x128
      cfg.memory_width  = 128;
      cfg.memory_height = 128;
      cfg.panel_width   = 128;
      cfg.panel_height  = 128;

      cfg.offset_x = 2;
      cfg.offset_y = 1;
      cfg.offset_rotation = 0;

      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;

      panel.config(cfg);
    }
    setPanel(&panel);
  }
};

LGFX lcd;
LGFX_Sprite face(&lcd);   // off-screen canvas: draw here, then push in one go (no flicker)

#define TOUCH_PIN 1       // D0 — TTP223 OUT (HIGH while touched)


// =====================================================
// TIMINGS — set DEMO_MODE to true to test everything in about a minute
// =====================================================

#define DEMO_MODE true

#if DEMO_MODE
const unsigned long SLEEPY_AFTER   = 20UL * 1000;        // no touch -> sleepy
const unsigned long ASLEEP_AFTER   = 40UL * 1000;        // no touch -> asleep
const unsigned long WATER_EVERY    = 60UL * 1000;        // water reminder interval
const unsigned long WATER_SNOOZE   = 15UL * 1000;        // long press while thirsty
#else
const unsigned long SLEEPY_AFTER   =  3UL * 60 * 1000;   // 3 min
const unsigned long ASLEEP_AFTER   =  5UL * 60 * 1000;   // 5 min
const unsigned long WATER_EVERY    = 45UL * 60 * 1000;   // 45 min
const unsigned long WATER_SNOOZE   = 10UL * 60 * 1000;   // 10 min
#endif

const unsigned long LONG_PRESS_MS  = 700;


// =====================================================
// LOOK & FEEL — tweak these
// =====================================================

#define FACE_CX 64
#define FACE_CY 64

// Soft colours (R, G, B)
const uint16_t COL_BG      = lgfx::color565( 10,  12,  24);  // deep navy
const uint16_t COL_EYE     = lgfx::color565(120, 205, 255);  // soft sky blue
const uint16_t COL_SPARKLE = lgfx::color565(255, 255, 255);
const uint16_t COL_BLUSH   = lgfx::color565(255, 120, 160);
const uint16_t COL_MOUTH   = lgfx::color565(235, 240, 255);
const uint16_t COL_HEART   = lgfx::color565(255,  95, 140);
const uint16_t COL_TEAR    = lgfx::color565(150, 215, 255);
const uint16_t COL_ZZZ     = lgfx::color565(170, 180, 255);
const uint16_t COL_WATER   = lgfx::color565( 80, 170, 255);

// Dimmed versions for when Pebble is fast asleep
const uint16_t COL_EYE_DIM   = lgfx::color565( 40,  75, 100);
const uint16_t COL_BLUSH_DIM = lgfx::color565( 95,  45,  65);
const uint16_t COL_ZZZ_DIM   = lgfx::color565( 70,  75, 120);

const int EYE_W   = 24;
const int EYE_H   = 34;
const int EYE_GAP = 26;     // distance of each eye from the face centre


// =====================================================
// EXPRESSIONS
// =====================================================

// The first NUM_MOODS are the ones you cycle through with a tap.
// The rest are reactions Pebble shows on its own.
enum Expr {
  NORMAL, HAPPY, LOVE, SURPRISED, SLEEPY, SAD, WINK,
  NUM_MOODS,
  GIGGLE = NUM_MOODS, ASLEEP, THIRSTY, DRANK
};
const char *EXPR_NAMES[] = {
  "normal", "happy", "love", "surprised", "sleepy", "sad", "wink",
  "giggle", "asleep", "thirsty", "drank"
};

int mood = NORMAL;         // what you picked with taps
int expr = NORMAL;         // what's on screen right now
int pendingExpr = -1;      // switched in at the middle of a blink

int cupsToday = 0;


// =====================================================
// STATE
// =====================================================

enum State {
  ST_AWAKE,      // showing your chosen mood
  ST_SLEEPY,     // ignored for a while
  ST_ASLEEP,     // ignored for a long while
  ST_REACTION,   // short reaction (giggle, waking up, drank) then back to mood
  ST_THIRSTY     // water reminder showing, waiting for a tap
};

State state = ST_AWAKE;
unsigned long reactionUntil   = 0;
unsigned long lastInteraction = 0;
unsigned long nextWaterAt     = 0;


// =====================================================
// DRAWING HELPERS  (all "Smooth"/"Wide"/"Wedge" calls are anti-aliased)
// =====================================================

// Jelly-bean eye with sparkles. openness: 1 = open, 0 = closed
void drawEye(float cx, float cy, float w, float h, float openness) {
  h *= openness;
  if (h < 5) {
    face.drawWideLine(cx - w / 2 + 2, cy, cx + w / 2 - 2, cy, 2.0f, COL_EYE);
    return;
  }
  int r = min((int)(w / 2), (int)(h / 2));
  face.fillSmoothRoundRect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, r, COL_EYE);

  if (openness > 0.6f) {
    face.fillSmoothCircle(cx + w * 0.17f, cy - h / 2 + 9, 4.5f, COL_SPARKLE);
    face.fillSmoothCircle(cx - w * 0.2f,  cy + h / 2 - 9, 2.0f, COL_SPARKLE);
  }
}

// Smooth curve: corners at cy, middle at cy + depth.
// depth > 0 = smile / closed eye (‿), depth < 0 = happy eye / frown (^ ∩)
void drawCurve(float cx, float cy, float halfW, float depth, float thick, uint16_t col) {
  const int N = 8;
  float px = 0, py = 0;
  for (int i = 0; i <= N; i++) {
    float x = -halfW + 2 * halfW * i / N;
    float u = x / halfW;
    float y = depth * (1 - u * u);
    if (i) face.drawWideLine(cx + px, cy + py, cx + x, cy + y, thick, col);
    px = x; py = y;
  }
}

void drawHeart(float cx, float cy, float s, uint16_t col) {
  float lx = cx - s * 0.5f, rx = cx + s * 0.5f, ly = cy - s * 0.25f;
  float r = s * 0.55f;
  face.fillSmoothCircle(lx, ly, r, col);
  face.fillSmoothCircle(rx, ly, r, col);
  face.drawWedgeLine(lx, ly, cx, cy + s, r, 1.0f, col);
  face.drawWedgeLine(rx, ly, cx, cy + s, r, 1.0f, col);
  face.fillSmoothCircle(lx - s * 0.1f, ly - s * 0.15f, 2, COL_SPARKLE);
}

void drawBlush(float cx, float cy, uint16_t col = COL_BLUSH) {
  face.fillSmoothRoundRect((int)cx - 7, (int)cy - 3, 14, 7, 3, col);
}

void drawSmile(float cx, float cy) {
  drawCurve(cx, cy, 6, 4, 1.2f, COL_MOUTH);
}

void drawTear(float x, float y) {
  face.drawWedgeLine(x, y - 6, x, y, 0.4f, 2.6f, COL_TEAR);
  face.fillSmoothCircle(x, y, 3, COL_TEAR);
}

// Big water drop icon (x, y = centre of the round bottom)
void drawDrop(float x, float y, float r) {
  face.drawWedgeLine(x, y - r * 2.2f, x, y, 0.6f, r, COL_WATER);
  face.fillSmoothCircle(x, y, r, COL_WATER);
  face.fillSmoothCircle(x - r * 0.35f, y - r * 0.2f, max(1.0f, r * 0.25f), COL_SPARKLE);
}

void drawOpenSmile(float cx, float cy, float r) {
  face.fillSmoothCircle(cx, cy - 2, r, COL_MOUTH);
  face.fillRect(cx - r - 1, cy - 3 - r, 2 * r + 2, r + 1, COL_BG);
}

void drawCaption(const char *text, uint16_t col) {
  // Bigger, rounder font than the default tiny pixel font
  face.setFont(&fonts::FreeSansBold9pt7b);
  face.setTextSize(1);
  face.setTextColor(col);
  face.setTextDatum(lgfx::top_center);
  face.drawString(text, FACE_CX, 108);
  face.setTextDatum(lgfx::top_left);
  face.setFont(&fonts::Font0);      // back to default for the zzz
}

void drawZzz(unsigned long now, float x, float y, unsigned long period, uint16_t col) {
  float p  = (now % period) / (float)period;
  float p2 = fmodf(p + 0.5f, 1.0f);
  face.setTextColor(col);
  face.setTextSize(1);
  face.drawString("z", x + p * 4,       y - p * 16);
  face.setTextSize(2);
  face.drawString("z", x + 2 + p2 * 4,  y - p2 * 16);
}


// =====================================================
// DRAW ONE FRAME
// =====================================================

void drawFace(unsigned long now, float open) {
  float t = now / 1000.0f;
  bool sleepyish = (expr == SLEEPY || expr == ASLEEP);

  // Gentle breathing bob (slower and deeper when sleepy)
  float bob = sleepyish ? sinf(t * 1.2f) * 2.5f : sinf(t * 2.0f) * 1.5f;

  // Giggle: wiggle side to side and bounce
  float fcx = FACE_CX;
  if (expr == GIGGLE) {
    fcx += sinf(t * 35.0f) * 2.5f;
    bob -= fabsf(sinf(t * 12.0f)) * 3.0f;
  }

  float cy = FACE_CY + bob;
  float ey = cy - 6;                       // eye line
  float lx = fcx - EYE_GAP;                // left eye x
  float rx = fcx + EYE_GAP;                // right eye x
  float my = cy + 20;                      // mouth line

  face.fillScreen(COL_BG);

  switch (expr) {

    case NORMAL:
      drawEye(lx, ey, EYE_W, EYE_H, open);
      drawEye(rx, ey, EYE_W, EYE_H, open);
      drawBlush(fcx - 40, cy + 18);
      drawBlush(fcx + 40, cy + 18);
      drawSmile(fcx, my);
      break;

    case HAPPY:
      // ^ ^ eyes
      drawCurve(lx, ey + 4, 11, -9 * open, 2.4f, COL_EYE);
      drawCurve(rx, ey + 4, 11, -9 * open, 2.4f, COL_EYE);
      drawBlush(fcx - 40, cy + 16);
      drawBlush(fcx + 40, cy + 16);
      // open D-shaped smile
      drawOpenSmile(fcx, my, 7);
      break;

    case LOVE: {
      // beating hearts
      float beat = 1.0f + 0.1f * fabsf(sinf(t * 5.0f));
      float s = 10.0f * beat * max(open, 0.2f);
      drawHeart(lx, ey - 2, s, COL_HEART);
      drawHeart(rx, ey - 2, s, COL_HEART);
      drawBlush(fcx - 40, cy + 18);
      drawBlush(fcx + 40, cy + 18);
      drawSmile(fcx, my);
      break;
    }

    case SURPRISED: {
      float r = 15.0f * max(open, 0.15f);
      face.fillSmoothCircle(lx, ey, r, COL_EYE);
      face.fillSmoothCircle(rx, ey, r, COL_EYE);
      if (open > 0.6f) {
        face.fillSmoothCircle(lx + 5, ey - 6, 4.5f, COL_SPARKLE);
        face.fillSmoothCircle(rx + 5, ey - 6, 4.5f, COL_SPARKLE);
        face.fillSmoothCircle(lx - 5, ey + 6, 2.0f, COL_SPARKLE);
        face.fillSmoothCircle(rx - 5, ey + 6, 2.0f, COL_SPARKLE);
      }
      // little "o" mouth
      face.fillSmoothCircle(fcx, my + 2, 4.5f, COL_MOUTH);
      face.fillSmoothCircle(fcx, my + 2, 2.5f, COL_BG);
      break;
    }

    case SLEEPY:
      // closed ‿ ‿ eyes
      drawCurve(lx, ey, 10, 5, 1.8f, COL_EYE);
      drawCurve(rx, ey, 10, 5, 1.8f, COL_EYE);
      drawBlush(fcx - 40, cy + 16);
      drawBlush(fcx + 40, cy + 16);
      drawCurve(fcx, my, 4, 2, 1.0f, COL_MOUTH);
      drawZzz(now, rx + 12, ey - 14, 2400, COL_ZZZ);
      break;

    case SAD: {
      const float w = 22, h = 28;
      drawEye(lx, ey + 2, w, h, open);
      drawEye(rx, ey + 2, w, h, open);
      // droopy lids: cover the OUTER top corner of each eye
      float hh = h * open;
      if (hh >= 5) {
        float top = ey + 2 - hh / 2 - 2;
        float drop = top + hh * 0.5f;
        float lOut = lx - w / 2 - 2, lIn = lx + w / 2 + 2;
        float rOut = rx + w / 2 + 2, rIn = rx - w / 2 - 2;
        face.fillTriangle(lOut, top, lIn, top, lOut, drop, COL_BG);
        face.fillTriangle(rOut, top, rIn, top, rOut, drop, COL_BG);
        // soften the lid edge
        face.drawWideLine(lIn, top, lOut, drop, 1.0f, COL_BG);
        face.drawWideLine(rIn, top, rOut, drop, 1.0f, COL_BG);
      }
      // falling tear
      float p = (now % 1600) / 1600.0f;
      if (p < 0.85f) drawTear(lx - 6, ey + 18 + p * 22);
      // frown ∩
      drawCurve(fcx, my + 4, 6, -3, 1.2f, COL_MOUTH);
      break;
    }

    case WINK:
      drawEye(lx, ey, EYE_W, EYE_H, open);
      drawCurve(rx, ey + 4, 10, -8, 2.2f, COL_EYE);   // ^ wink
      drawBlush(fcx - 40, cy + 18);
      drawBlush(fcx + 40, cy + 18);
      drawSmile(fcx, my);
      break;

    // ---------- reactions ----------

    case GIGGLE:
      // squeezed-shut >< style happy eyes
      drawCurve(lx, ey + 3, 11, -7, 2.6f, COL_EYE);
      drawCurve(rx, ey + 3, 11, -7, 2.6f, COL_EYE);
      drawBlush(fcx - 40, cy + 14);
      drawBlush(fcx + 40, cy + 14);
      drawOpenSmile(fcx, my, 9);
      break;

    case ASLEEP:
      // dimmed closed eyes, slow zzz
      drawCurve(lx, ey, 10, 4, 1.6f, COL_EYE_DIM);
      drawCurve(rx, ey, 10, 4, 1.6f, COL_EYE_DIM);
      drawBlush(fcx - 40, cy + 16, COL_BLUSH_DIM);
      drawBlush(fcx + 40, cy + 16, COL_BLUSH_DIM);
      drawZzz(now, rx + 12, ey - 14, 4000, COL_ZZZ_DIM);
      break;

    case THIRSTY: {
      // tired eyes glancing up at a bobbing water drop
      const float w = 22, h = 30;
      float ex = 3, eyy = ey - 1;
      drawEye(lx + ex, eyy, w, h, open);
      drawEye(rx + ex, eyy, w, h, open);
      float hh = h * open;
      if (hh >= 5) {   // heavy flat lids
        float top = eyy - hh / 2;
        face.fillRect(lx + ex - w / 2 - 1, top - 1, w + 2, hh * 0.3f + 1, COL_BG);
        face.fillRect(rx + ex - w / 2 - 1, top - 1, w + 2, hh * 0.3f + 1, COL_BG);
      }
      drawCurve(fcx, my + 2, 5, -2, 1.2f, COL_MOUTH);      // little pout
      drawDrop(fcx + 50, 22 + sinf(t * 4.0f) * 3.0f, 6);
      // caption pulses so it catches your eye
      bool bright = (now / 600) % 2 == 0;
      drawCaption("drink water!", bright ? COL_TEAR : COL_ZZZ_DIM);
      break;
    }

    case DRANK: {
      drawCurve(lx, ey + 4, 11, -9 * open, 2.4f, COL_EYE);
      drawCurve(rx, ey + 4, 11, -9 * open, 2.4f, COL_EYE);
      drawBlush(fcx - 40, cy + 16);
      drawBlush(fcx + 40, cy + 16);
      drawOpenSmile(fcx, my, 7);
      drawDrop(fcx + 50, 22, 6);
      char buf[24];
      snprintf(buf, sizeof(buf), "cup #%d  yay!", cupsToday);
      drawCaption(buf, COL_TEAR);
      break;
    }
  }

  face.pushSprite(0, 0);
}


// =====================================================
// BLINK  (also used to swap expressions)
// =====================================================

unsigned long nextBlinkAt = 0;
unsigned long blinkStart  = 0;
bool blinking = false;
const unsigned long BLINK_MS = 200;

void startBlink(unsigned long now) {
  blinking = true;
  blinkStart = now;
}

float blinkOpenness(unsigned long now) {
  if (!blinking) return 1.0f;
  float p = (float)(now - blinkStart) / BLINK_MS;
  if (p >= 1.0f) { blinking = false; return 1.0f; }
  if (p >= 0.5f && pendingExpr >= 0) {          // eyes shut: swap expression
    expr = pendingExpr;
    pendingExpr = -1;
  }
  return 1.0f - sinf(p * PI);
}

// Change what's on screen, with a blink in between
void showExpr(int e) {
  if (e == expr && pendingExpr < 0) return;
  pendingExpr = e;
  startBlink(millis());
  Serial.printf("-> %s\n", EXPR_NAMES[e]);
}

void react(int e, unsigned long ms) {
  state = ST_REACTION;
  reactionUntil = millis() + ms;
  showExpr(e);
}


// =====================================================
// BEHAVIOUR
// =====================================================

bool touchWasWakeUp = false;   // the touch that wakes Pebble shouldn't also count as a tap

void onPress(unsigned long now) {
  lastInteraction = now;
  touchWasWakeUp = false;

  if (state == ST_SLEEPY || state == ST_ASLEEP) {
    touchWasWakeUp = true;
    react(SURPRISED, 1200);          // *gasp* oh hi!
  }
}

void onTap(unsigned long now) {
  if (touchWasWakeUp) return;

  if (state == ST_THIRSTY) {         // "I drank!"
    cupsToday++;
    nextWaterAt = now + WATER_EVERY;
    react(DRANK, 6000);
    Serial.printf("Water logged: %d cups\n", cupsToday);
  } else if (state == ST_AWAKE) {    // next mood
    mood = (mood + 1) % NUM_MOODS;
    showExpr(mood);
  }
}

void onLongPress(unsigned long now) {
  if (touchWasWakeUp) return;

  if (state == ST_THIRSTY) {         // snooze the reminder
    nextWaterAt = now + WATER_SNOOZE;
    state = ST_AWAKE;
    showExpr(mood);
    Serial.println("Water reminder snoozed");
  } else if (state == ST_AWAKE) {    // tickle!
    react(GIGGLE, 1600);
  }
}

void updateBehaviour(unsigned long now) {
  // Reaction finished -> back to your mood
  if (state == ST_REACTION && now >= reactionUntil) {
    state = ST_AWAKE;
    lastInteraction = now;
    showExpr(mood);
  }

  // Water reminder (wakes Pebble up if it's sleeping)
  if (now >= nextWaterAt && state != ST_THIRSTY && state != ST_REACTION) {
    state = ST_THIRSTY;
    showExpr(THIRSTY);
  }

  // Getting bored...
  unsigned long idle = now - lastInteraction;
  if (state == ST_AWAKE && idle > SLEEPY_AFTER) {
    state = ST_SLEEPY;
    showExpr(SLEEPY);
  }
  if (state == ST_SLEEPY && idle > ASLEEP_AFTER) {
    state = ST_ASLEEP;
    showExpr(ASLEEP);
  }
}


// =====================================================
// TOUCH  (tap = short touch, long press = hold 0.7 s)
// =====================================================

bool lastTouch = false;
unsigned long lastTouchChange = 0;
unsigned long pressStart = 0;
bool longFired = false;

void handleTouch(unsigned long now) {
  bool touched = digitalRead(TOUCH_PIN) == HIGH;

  if (touched != lastTouch && now - lastTouchChange > 40) {
    lastTouchChange = now;
    lastTouch = touched;
    if (touched) {
      pressStart = now;
      longFired = false;
      onPress(now);
    } else if (!longFired) {
      onTap(now);
    }
  }

  if (lastTouch && !longFired && now - pressStart >= LONG_PRESS_MS) {
    longFired = true;
    onLongPress(now);
  }
}


// =====================================================
// SETUP / LOOP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TOUCH_PIN, INPUT);

  lcd.init();
  lcd.setRotation(1);   // use 3 if upside down

  face.setColorDepth(16);
  face.setPsram(false);
  if (!face.createSprite(lcd.width(), lcd.height())) {
    Serial.println("Sprite alloc failed!");
  }

  randomSeed(esp_random());
  unsigned long now = millis();
  nextBlinkAt     = now + 2000;
  lastInteraction = now;
  nextWaterAt     = now + WATER_EVERY;

  Serial.printf("Pebble awake! Screen is %d x %d%s\n",
                lcd.width(), lcd.height(), DEMO_MODE ? "  (DEMO MODE)" : "");
}

unsigned long lastFrame = 0;

void loop() {
  unsigned long now = millis();
  handleTouch(now);
  updateBehaviour(now);

  // Natural blinking (not with eyes already closed)
  if (!blinking && now >= nextBlinkAt) {
    bool eyesClosed = (expr == SLEEPY || expr == ASLEEP || expr == GIGGLE);
    if (!eyesClosed) startBlink(now);
    nextBlinkAt = now + random(2500, 6000);
  }

  // ~30 fps
  if (now - lastFrame >= 33) {
    lastFrame = now;
    drawFace(now, blinkOpenness(now));
  }
}
