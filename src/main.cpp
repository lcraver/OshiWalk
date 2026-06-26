#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <xpt2046.h>
#include <LittleFS.h>

#include "pins.h"
#include "data.h"
#include "streetpass.h"
#include "pages/Page.h"
#include "pages/AvatarPage.h"
#include "pages/StreetPassPage.h"
#include "pages/GifPage.h"

TFT_eSPI tft   = TFT_eSPI();
XPT2046  touch(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);

Page *pages[NUM_PAGES];
int   currentPage = 0;

bool    touching    = false;
int16_t touchStartX = 0;
int16_t lastTouchX  = 0;
int16_t lastTouchY  = 0;

#define SWIPE_THRESHOLD 40

// ── setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    pinMode(PWR_EN_PIN, OUTPUT); digitalWrite(PWR_EN_PIN, HIGH);
    pinMode(PWR_ON_PIN, OUTPUT); digitalWrite(PWR_ON_PIN, HIGH);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

    if (!LittleFS.begin(true))
        Serial.println("LittleFS mount failed");

    data_init();

    SPI.begin(TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_MOSI_PIN);
    touch.begin(240, 320);

    touch_calibration_t cal[4];
    if (data_read(cal))
        touch.setCal(cal[0].rawX, cal[2].rawX, cal[0].rawY, cal[2].rawY, 240, 320);
    else
        touch.setCal(1788, 285, 1877, 311, 240, 320);
    touch.setRotation(0);

    tft.begin();
    tft.setRotation(0);
    tft.setSwapBytes(true);

    streetpass_init();

    pages[0] = new AvatarPage(tft);
    pages[1] = new StreetPassPage(tft);
    pages[2] = new GifPage(tft);

    pages[currentPage]->draw();
}

// ── loop ──────────────────────────────────────────────────────────────────────

void loop() {
    // StreetPass always ticks regardless of active page
    streetpass_tick();

    // Active page tick (GIF playback, StreetPass redraw, etc.)
    pages[currentPage]->tick();

    // ── Power button: long press → shut down ──────────────────────────────────
    static unsigned long btnPressedAt = 0;
    static bool          btnArmed     = false;
    if (digitalRead(BUTTON2_PIN) == LOW) {
        if (!btnArmed) { btnArmed = true; btnPressedAt = millis(); }
        else if (millis() - btnPressedAt >= 2000) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.drawString("Goodbye", 120, 155);
            delay(600);
            digitalWrite(PWR_ON_PIN, LOW);
        }
    } else {
        btnArmed = false;
    }

    // ── Touch handling ────────────────────────────────────────────────────────
    bool pressed = touch.pressed();
    if (pressed) {
        int16_t x = touch.X();
        int16_t y = touch.Y();
        if (!touching) { touching = true; touchStartX = x; }
        lastTouchX = x;
        lastTouchY = y;
    } else if (touching) {
        touching = false;
        int16_t dx = lastTouchX - touchStartX;

        if (dx < -SWIPE_THRESHOLD) {
            // Swipe left → next page
            pages[currentPage]->onLeave();
            currentPage = (currentPage + 1) % NUM_PAGES;
            pages[currentPage]->draw();
        } else if (dx > SWIPE_THRESHOLD) {
            // Swipe right → previous page
            pages[currentPage]->onLeave();
            currentPage = (currentPage + NUM_PAGES - 1) % NUM_PAGES;
            pages[currentPage]->draw();
        } else {
            // Tap — delegate to active page
            pages[currentPage]->onTap(lastTouchX, lastTouchY);
        }
    }

    delay(16);
}
