#include <LovyanGFX.hpp>

// =====================================================
// ST7735 1.8" (128x160) CONFIGURATION
// =====================================================
// shows a pair of static eye

class LGFX : public lgfx::LGFX_Device {

  lgfx::Panel_ST7735S panel;   // <-- your board is ST7735, not ILI9341
  lgfx::Bus_SPI bus;

public:

  LGFX() {

    // -------------------------
    // SPI
    // -------------------------
    {
      auto cfg = bus.config();

      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;

      // Long breadboard jumpers can't handle very fast SPI.
      // If you still see random lines, drop this to 2000000.
      cfg.freq_write = 10000000;
      cfg.freq_read  = 2000000;

      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      // XIAO ESP32-S3
      cfg.pin_sclk = 7;     // D8
      cfg.pin_mosi = 9;     // D10
      cfg.pin_miso = -1;
      cfg.pin_dc   = 4;     // D3

      bus.config(cfg);
      panel.setBus(&bus);
    }

    // -------------------------
    // ST7735
    // -------------------------
    {
      auto cfg = panel.config();

      cfg.pin_cs  = 2;      // D1
      cfg.pin_rst = 3;      // D2
      cfg.pin_busy = -1;

      // "Black tab" ST7735: memory and glass are both 128x160, no offset
      cfg.memory_width  = 128;
      cfg.memory_height = 160;

      cfg.panel_width   = 128;
      cfg.panel_height  = 160;

      // Fine-tune here if the white test border is still cut off on an edge
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.readable = false;
      cfg.invert = false;    // if black shows as white, set true
      cfg.rgb_order = false; // if cyan shows as yellow, set true

      panel.config(cfg);
    }

    setPanel(&panel);
  }
};

LGFX lcd;

// Set to false once the border looks right on all 4 edges
#define SHOW_TEST_BORDER false

// Nudge the whole face (pixels). Negative X = left, negative Y = up.
#define FACE_SHIFT_X -15
#define FACE_SHIFT_Y 0


// =====================================================
// DRAW ONE HAPPY FACE  (screen is 160 x 128 in landscape)
// =====================================================

void drawEye(int cx, int cy) {
  lcd.fillCircle(cx,     cy,     22, TFT_WHITE);   // white of eye
  lcd.fillCircle(cx,     cy + 2, 13, TFT_CYAN);    // iris
  lcd.fillCircle(cx,     cy + 3,  7, TFT_BLACK);   // pupil
  lcd.fillCircle(cx - 4, cy - 3,  3, TFT_WHITE);   // highlight
}

void drawFace() {

  lcd.fillScreen(TFT_BLACK);

  const int sx = FACE_SHIFT_X;
  const int sy = FACE_SHIFT_Y;

  // Eyes
  drawEye(48 + sx, 52 + sy);
  drawEye(112 + sx, 52 + sy);

  // Mouth (white ellipse with a black one cut out = smile)
  lcd.fillEllipse(80 + sx, 98 + sy, 12, 6, TFT_WHITE);
  lcd.fillEllipse(80 + sx, 96 + sy,  8, 3, TFT_BLACK);

  // Cheeks
  lcd.fillCircle(20 + sx,  88 + sy, 5, TFT_PINK);
  lcd.fillCircle(140 + sx, 88 + sy, 5, TFT_PINK);

  if (SHOW_TEST_BORDER) {
    lcd.drawRect(0, 0, lcd.width(), lcd.height(), TFT_WHITE);
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  lcd.init();
  lcd.setRotation(1);   // landscape; use 3 if upside down

  drawFace();

  Serial.printf("Face displayed! Screen is %d x %d\n", lcd.width(), lcd.height());
}


// =====================================================
// LOOP
// =====================================================

void loop() {
}