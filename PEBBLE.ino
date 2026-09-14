#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel;
  lgfx::Bus_SPI bus;

public:
  LGFX() {

    // SPI settings
    {
      auto cfg = bus.config();

      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 10000000;
      cfg.freq_read = 10000000;

      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      // XIAO ESP32-S3
      cfg.pin_sclk = 7;   // D8 -> TFT SCK
      cfg.pin_mosi = 9;   // D10 -> TFT SDA/SDI
      cfg.pin_miso = -1;
      cfg.pin_dc   = 4;   // D3 -> TFT A0/DC

      bus.config(cfg);
      panel.setBus(&bus);
    }

    // ILI9341 settings
    {
      auto cfg = panel.config();

      cfg.pin_cs  = 2;    // D1 -> TFT CS
      cfg.pin_rst = 3;    // D2 -> TFT RST

      cfg.pin_busy = -1;

      // cfg.memory_width  = 320;
      // cfg.memory_height = 240;

      cfg.panel_width  = 320;
      cfg.panel_height = 240;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
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
#define TOUCH_PIN 1
bool lastTouchState = LOW;
int colorNumber =0;


void setup() {

  Serial.begin(115200);

  pinMode(TOUCH_PIN,INPUT);
  delay(1000);
  lcd.init();

  // Landscape
  lcd.setRotation(1);

  lcd.fillScreen(TFT_WHITE);


  
  Serial.println("TEST COMPLETE");
}

void loop() {
  bool currentTouchState = digitalRead(TOUCH_PIN);

  if(currentTouchState==HIGH && lastTouchState==LOW){
    colorNumber++;

    if(colorNumber >=5){
      colorNumber = 0;
      Serial.println("RESET TO RED");
    }

    if(colorNumber == 0){
      lcd.fillScreen(TFT_RED);
      Serial.println("RED");
    }
    else if (colorNumber == 1) {
      lcd.fillScreen(TFT_GREEN);
      Serial.println("GREEN");
    }

    else if (colorNumber == 2) {
      lcd.fillScreen(TFT_BLUE);
      Serial.println("BLUE");
    }

    else if (colorNumber == 3) {
      lcd.fillScreen(TFT_YELLOW);
      Serial.println("YELLOW");
    }

    else if (colorNumber == 4) {
      lcd.fillScreen(TFT_PURPLE);
      Serial.println("PURPLE");
    }

    else if (colorNumber == 5) {
      lcd.fillScreen(TFT_CYAN);
      Serial.println("CYAN");
    }
  
    delay(200);
  }
    lastTouchState = currentTouchState;
}

