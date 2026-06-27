#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <xpt2046.h>
#include <LittleFS.h>
#include <SD_MMC.h>

#include "pins.h"
#include "data.h"
#include "streetpass.h"
#include "web/webserver.h"
#include "pages/Page.h"
#include "pages/AvatarPage.h"
#include "pages/StreetPassPage.h"
#include "pages/GifPage.h"
#include "pages/SettingsPage.h"
#include "boot_logo.h"

TFT_eSPI tft   = TFT_eSPI();
XPT2046  touch(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);

Page *pages[NUM_PAGES];
int   currentPage = 2; // start on GIF page

bool    touching    = false;
int16_t touchStartX = 0;
int16_t lastTouchX  = 0;
int16_t lastTouchY  = 0;

#define SWIPE_THRESHOLD 40

// ── upload overlay ─────────────────────────────────────────────────────────────

static void drawUploadOverlay(const char *title, size_t written, size_t total, const char *name) {
    const int bx = 16, by = 88, bw = 208, bh = 144;
    tft.fillRoundRect(bx, by, bw, bh, 10, 0x0C4A);
    tft.drawRoundRect(bx, by, bw, bh, 10, 0x4208);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, 0x0C4A);
    tft.drawString(title, 120, by + 24);

    // Filename — truncate to fit
    String fn = name;
    tft.setTextSize(1);
    tft.setTextColor(0x07FF, 0x0C4A); // cyan
    if (fn.length() > 26) fn = fn.substring(0, 23) + "...";
    tft.drawString(fn.c_str(), 120, by + 48);

    // Progress bar
    const int px = bx + 12, py = by + 66, pw = bw - 24, ph = 14;
    tft.drawRoundRect(px, py, pw, ph, 3, 0x4208);
    if (total > 0) {
        int fill = (int)((long long)written * (pw - 2) / total);
        if (fill > 0) tft.fillRoundRect(px + 1, py + 1, fill, ph - 2, 3, 0x07E0);
    }

    // Percentage + size
    tft.setTextColor(0x8410, 0x0C4A);
    char buf[40];
    if (total > 0) {
        snprintf(buf, sizeof(buf), "%d%%  %u / %u KB",
                 (int)(written * 100 / total), (unsigned)(written / 1024), (unsigned)(total / 1024));
    } else {
        snprintf(buf, sizeof(buf), "%u KB received", (unsigned)(written / 1024));
    }
    tft.drawString(buf, 120, by + 100);
}

static void onUploadProgress(size_t written, size_t total, const char *name) {
    drawUploadOverlay("Saving...", written, total, name);
}

// ── setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    pinMode(PWR_EN_PIN, OUTPUT); digitalWrite(PWR_EN_PIN, HIGH);
    pinMode(PWR_ON_PIN, OUTPUT); digitalWrite(PWR_ON_PIN, HIGH);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

    // ── Display first — user sees something immediately ───────────────────────
    tft.begin();
    tft.setRotation(0);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("ProxiOshi", 120, 152);
    tft.setTextSize(1);
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.drawString("Booting... please wait", 120, 174);

    // ── Slow init happens while splash is visible ─────────────────────────────
    #define T(label) Serial.printf("[boot] %6lu ms  " label "\n", millis())

    T("LittleFS start");
    if (!LittleFS.begin(true))
        Serial.println("LittleFS mount failed");
    T("LittleFS done");

    data_init();
    T("data_init done");

    // SD card — 1-bit SDMMC (CLK=12, CMD=11, D0=13)
    SD_MMC.setPins(SD_SCLK_PIN, SD_MOSI_PIN, SD_MISO_PIN);
    T("SD start");
    bool sdMounted = SD_MMC.begin("/sdcard", true);
    // cardType() is unreliable for unformatted cards — the driver can fail before
    // setting the type even when a card is physically present.  If begin() failed,
    // assume a card may be there and show the format button; the format attempt
    // will surface a clear error if the slot is actually empty.
    bool sdPresent = sdMounted || (SD_MMC.cardType() != CARD_NONE) || !sdMounted;
    if (!sdMounted) Serial.println("SD: begin() failed — showing format option");
    webserver_set_sd_state(sdMounted, sdPresent);
    T("SD done");

    SPI.begin(TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_MOSI_PIN);
    touch.begin(240, 320);

    touch_calibration_t cal[4];
    if (data_read(cal))
        touch.setCal(cal[0].rawX, cal[2].rawX, cal[0].rawY, cal[2].rawY, 240, 320);
    else
        touch.setCal(1788, 285, 1877, 311, 240, 320);
    touch.setRotation(0);
    T("touch done");

    T("streetpass start");
    streetpass_init();
    T("streetpass done");

    T("AvatarPage start");
    pages[0] = new AvatarPage(tft);
    T("AvatarPage done");
    pages[1] = new StreetPassPage(tft);
    T("StreetPassPage done");
    T("GifPage start");
    pages[2] = new GifPage(tft);
    T("GifPage done");
    pages[3] = new SettingsPage(tft);
    T("all pages done — drawing");

    #undef T

    pages[currentPage]->draw();
}

// ── loop ──────────────────────────────────────────────────────────────────────

void loop() {
    // ── Upload overlay + filesystem flush ─────────────────────────────────────
    static bool gifClosedForUpload = false;
    bool uploading   = webserver_upload_in_progress();
    bool sdUploading = webserver_sd_upload_in_progress();
    bool anyUploading = uploading || sdUploading;

    if (anyUploading && !gifClosedForUpload) {
        pages[2]->onLeave();
        gifClosedForUpload = true;
    }

    // Refresh the receiving overlay every 200 ms so progress is visible
    if (anyUploading) {
        static unsigned long lastOverlayMs = 0;
        unsigned long now = millis();
        if (now - lastOverlayMs >= 200) {
            lastOverlayMs = now;
            if (sdUploading)
                drawUploadOverlay("Receiving (SD)...",
                                  webserver_sd_bytes_received(),
                                  webserver_sd_upload_total(),
                                  webserver_sd_upload_name());
            else
                drawUploadOverlay("Receiving...",
                                  webserver_bytes_received(),
                                  webserver_upload_total(),
                                  webserver_upload_name());
        }
    }

    // SD upload: wifi task does the write — just wait for it to finish then rescan
    static bool sdUploadWasDone = false;
    if (sdUploadWasDone && !sdUploading) {
        sdUploadWasDone   = false;
        gifClosedForUpload = false;
        drawUploadOverlay("Done!", 1, 1, "");
        delay(800);
        static_cast<GifPage *>(pages[2])->rescan();
        pages[currentPage]->onLeave();
        currentPage = 2;
        static_cast<GifPage *>(pages[2])->draw();
        return;
    }
    if (sdUploading) sdUploadWasDone = true;

    // Write buffered LittleFS upload to FS from this task — passes progress callback
    // so the overlay updates as each chunk is written.
    if (webserver_flush_upload(onUploadProgress)) {
        gifClosedForUpload = false;
        drawUploadOverlay("Done!", 1, 1, "");
        delay(800);

        // Grab the filename before rescan clears anything
        String uploaded = webserver_last_upload_name();
        webserver_pending_rescan(); // clear the flag

        GifPage *gifPage = static_cast<GifPage *>(pages[2]);
        gifPage->rescan(); // rebuilds the path list

        // Switch to GIF page and jump straight to the new GIF
        pages[currentPage]->onLeave();
        currentPage = 2;
        if (uploaded.length() > 0)
            gifPage->showByName(uploaded.c_str());
        else
            gifPage->draw();

        return;
    }

    // StreetPass always ticks regardless of active page
    streetpass_tick();

    // Active page tick — skip while uploading
    if (!anyUploading)
        pages[currentPage]->tick();

    // Rescan triggered by delete (upload rescan is handled above)
    if (!anyUploading && webserver_pending_rescan()) {
        static_cast<GifPage *>(pages[2])->rescan();
        if (currentPage != 2) pages[currentPage]->draw();
    }

    // Web UI requested a specific GIF — switch to it
    char playName[256];
    if (!uploading && webserver_pending_play(playName, sizeof(playName))) {
        GifPage *gifPage = static_cast<GifPage *>(pages[2]);
        pages[currentPage]->onLeave();
        currentPage = 2;
        gifPage->showByName(playName);
    }

    // ── Power button: long press → shut down ──────────────────────────────────
    // Wait for button release before arming, so a held boot-press doesn't
    // immediately trigger shutdown.
    static bool          btnReleased   = false;
    static unsigned long btnPressedAt  = 0;
    static bool          btnArmed      = false;
    static bool          popupShown    = false;
    static int           lastBarFill   = -1;

    static const unsigned long HOLD_MS  = 2000;
    static const int POP_W = 200, POP_H = 80;
    static const int POP_X = (240 - POP_W) / 2, POP_Y = (320 - POP_H) / 2;
    static const int BAR_X = POP_X + 10, BAR_Y = POP_Y + 52, BAR_W = POP_W - 20, BAR_H = 12;

    bool btnDown = digitalRead(BUTTON2_PIN) == LOW;
    if (!btnReleased) {
        if (!btnDown) btnReleased = true;
    } else if (btnDown) {
        if (!btnArmed) {
            btnArmed = true;
            btnPressedAt = millis();
            popupShown = false;
            lastBarFill = -1;
        } else {
            unsigned long held = millis() - btnPressedAt;

            // Draw popup on first frame of hold
            if (!popupShown) {
                tft.fillRoundRect(POP_X, POP_Y, POP_W, POP_H, 8, 0x1082);
                tft.drawRoundRect(POP_X, POP_Y, POP_W, POP_H, 8, 0x4208);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_WHITE, 0x1082);
                tft.setTextSize(1);
                tft.drawString("Hold to power off", 120, POP_Y + 22);
                tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 3, 0x4208);
                popupShown = true;
                lastBarFill = 0;
            }

            // Update progress bar
            int fill = (int)((long long)held * (BAR_W - 2) / HOLD_MS);
            if (fill > BAR_W - 2) fill = BAR_W - 2;
            if (fill != lastBarFill) {
                tft.fillRoundRect(BAR_X + 1, BAR_Y + 1, fill, BAR_H - 2, 2, 0xF800);
                lastBarFill = fill;
            }

            if (held >= HOLD_MS) {
                tft.fillScreen(TFT_BLACK);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setTextSize(2);
                tft.drawString("Goodbye", 120, 155);
                delay(400);
                // Cut power via hardware latch; deep sleep as fallback if latch
                // doesn't drop immediately (e.g. still powered via USB).
                digitalWrite(PWR_ON_PIN, LOW);
                digitalWrite(PWR_EN_PIN, LOW);
                esp_deep_sleep_start();
            }
        }
    } else {
        if (btnArmed && popupShown) {
            // Released early — redraw the current page to clear the popup
            pages[currentPage]->draw();
        }
        btnArmed = false;
        popupShown = false;
        lastBarFill = -1;
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

        bool swipe = abs(dx) >= SWIPE_THRESHOLD
                     && !pages[currentPage]->interceptsSwipe();
        if (swipe && dx < 0) {
            // Swipe left → next page
            pages[currentPage]->onLeave();
            currentPage = (currentPage + 1) % NUM_PAGES;
            pages[currentPage]->draw();
        } else if (swipe && dx > 0) {
            // Swipe right → previous page
            pages[currentPage]->onLeave();
            currentPage = (currentPage + NUM_PAGES - 1) % NUM_PAGES;
            pages[currentPage]->draw();
        } else {
            // Tap (or intercepted swipe) — delegate to active page
            pages[currentPage]->onTap(lastTouchX, lastTouchY);
        }
    }

    delay(1); // 1 ms yield — keeps GIF timing accurate without starving other tasks
}
