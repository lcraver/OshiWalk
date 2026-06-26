#include "pages/StreetPassPage.h"
#include "streetpass.h"
#include "avatar_render.h"
#include <Arduino.h>

static const uint16_t kHdrClr  = 0x5819;
static const int      ROW_H    = 60;
static const int      START_Y  = 58;
static const int      AV_SIZE  = 60;  // must match StreetPassPage::AV_SIZE

static const uint32_t kCacheRefreshMs = 30000;

StreetPassPage::StreetPassPage(TFT_eSPI &tft) : Page(tft, 1)
{
    for (int i = 0; i < MAX_ROW; i++) {
        avatarCache[i] = nullptr;
        cacheValid[i]  = false;
    }
    lastRefreshMs = 0;
}

StreetPassPage::~StreetPassPage()
{
    for (int i = 0; i < MAX_ROW; i++) {
        if (avatarCache[i]) { free(avatarCache[i]); avatarCache[i] = nullptr; }
    }
}

void StreetPassPage::refreshCache()
{
    int total = streetpass_count();
    int shown = total < MAX_ROW ? total : MAX_ROW;

    for (int i = 0; i < shown; i++) {
        sp_encounter_t enc;
        streetpass_get(total - 1 - i, &enc);

        if (!avatarCache[i])
            avatarCache[i] = (uint16_t *)ps_malloc(AV_SIZE * AV_SIZE * sizeof(uint16_t));

        if (avatarCache[i]) {
            avatar_render_to_buf(avatarCache[i], AV_SIZE, AV_SIZE, enc.avatar);
            cacheValid[i] = true;
        }
    }

    for (int i = shown; i < MAX_ROW; i++)
        cacheValid[i] = false;

    lastRefreshMs = millis();
}

void StreetPassPage::draw()
{
    tft.fillScreen(TFT_BLACK);
    drawTitleBar("StreetPass", kHdrClr);

    int total = streetpass_count();

    char buf[48];
    snprintf(buf, sizeof(buf), "%d device%s encountered", total, total == 1 ? "" : "s");
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buf, 120, 44);

    if (total == 0) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("No encounters yet",        120, 160);
        tft.drawString("Broadcasting every 5s...", 120, 178);
    } else {
        int shown = total < MAX_ROW ? total : MAX_ROW;

        for (int i = 0; i < shown; i++) {
            sp_encounter_t enc;
            streetpass_get(total - 1 - i, &enc);
            int y = START_Y + i * ROW_H;

            // Blit cached avatar (or placeholder if cache isn't ready yet)
            if (cacheValid[i] && avatarCache[i]) {
                tft.pushImage(2, y, AV_SIZE, AV_SIZE, avatarCache[i]);
            } else {
                tft.fillRect(2, y, AV_SIZE, AV_SIZE, 0x2104);
            }

            // MAC (last 3 bytes)
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X",
                     enc.mac[3], enc.mac[4], enc.mac[5]);
            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(buf, AV_SIZE + 6, y + 4);

            // Full MAC
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                     enc.mac[0], enc.mac[1], enc.mac[2],
                     enc.mac[3], enc.mac[4], enc.mac[5]);
            tft.setTextColor(0x4A49, TFT_BLACK);
            tft.drawString(buf, AV_SIZE + 6, y + 18);

            // Encounter count
            snprintf(buf, sizeof(buf), "x%u", enc.count);
            tft.setTextDatum(TR_DATUM);
            tft.setTextColor(enc.firstSeen > 0 ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
            tft.drawString(buf, 236, y + 10);
        }

        if (total > MAX_ROW) {
            snprintf(buf, sizeof(buf), "+ %d more", total - MAX_ROW);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.drawString(buf, 120, START_Y + MAX_ROW * ROW_H + 4);
        }
    }

    drawDotsAndVersion();
}

void StreetPassPage::tick()
{
    bool hasNew = streetpass_has_new();
    if (hasNew) streetpass_clear_new();

    bool timerUp = (millis() - lastRefreshMs) >= kCacheRefreshMs;

    if (hasNew || timerUp) {
        refreshCache();
        draw();
    }
}
