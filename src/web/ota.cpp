#include "web/ota.h"
#include "version.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#define MANIFEST_URL \
    "https://raw.githubusercontent.com/lcraver/OshiWalk/main/version.json"

// ── TFT helpers ───────────────────────────────────────────────────────────────

static TFT_eSPI *s_tft = nullptr;

static void otaSplash(const char *line1, const char *line2,
                      uint16_t col1, uint16_t col2) {
    s_tft->fillScreen(TFT_BLACK);
    s_tft->setTextDatum(MC_DATUM);
    s_tft->setTextSize(2);
    s_tft->setTextColor(col1, TFT_BLACK);
    s_tft->drawString(line1, 120, 120);
    s_tft->setTextSize(1);
    s_tft->setTextColor(col2, TFT_BLACK);
    s_tft->drawString(line2, 120, 152);
}

static void otaProgress(int cur, int total) {
    if (total == 0) return;
    int pct = (cur * 100) / total;

    // Progress bar: 200 px wide, centred
    const int BAR_X = 20, BAR_Y = 180, BAR_W = 200, BAR_H = 16;
    int fill = (pct * BAR_W) / 100;

    s_tft->fillRect(BAR_X,        BAR_Y, fill,          BAR_H, TFT_GREEN);
    s_tft->fillRect(BAR_X + fill, BAR_Y, BAR_W - fill,  BAR_H, TFT_DARKGREY);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    s_tft->setTextDatum(MC_DATUM);
    s_tft->setTextSize(1);
    s_tft->setTextColor(TFT_WHITE, TFT_BLACK);
    s_tft->drawString(buf, 120, BAR_Y + BAR_H + 14);
}

// ── OTA check ─────────────────────────────────────────────────────────────────

void ota_check(TFT_eSPI &tft) {
    s_tft = &tft;

    otaSplash("Checking for", "updates...", TFT_WHITE, TFT_DARKGREY);

    WiFiClientSecure client;
    client.setInsecure();

    // ── Fetch manifest ────────────────────────────────────────────────────────
    HTTPClient http;
    http.begin(client, MANIFEST_URL);
    http.setTimeout(8000);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[OTA] manifest fetch failed: %d\n", code);
        http.end();
        otaSplash("Update check", "failed", TFT_RED, TFT_DARKGREY);
        delay(2000);
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();

    if (err) {
        Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
        otaSplash("Update check", "failed", TFT_RED, TFT_DARKGREY);
        delay(2000);
        return;
    }

    int   remoteBuild = doc["build"] | 0;
    const char *binUrl = doc["url"]  | "";

    Serial.printf("[OTA] local=%d  remote=%d\n", BUILD_NUMBER, remoteBuild);

    if (remoteBuild <= BUILD_NUMBER) {
        otaSplash("Up to date", "v1." + String(BUILD_NUMBER), TFT_GREEN, TFT_DARKGREY);
        delay(2000);
        return;
    }

    // ── Flash ─────────────────────────────────────────────────────────────────
    char verBuf[24];
    snprintf(verBuf, sizeof(verBuf), "v1.%d  ->  v1.%d", BUILD_NUMBER, remoteBuild);
    otaSplash("Updating...", verBuf, TFT_CYAN, TFT_DARKGREY);

    httpUpdate.onProgress([](int cur, int total) { otaProgress(cur, total); });
    httpUpdate.rebootOnUpdate(true);

    WiFiClientSecure dlClient;
    dlClient.setInsecure();

    t_httpUpdate_return result = httpUpdate.update(dlClient, binUrl);

    // Only reached if update failed (success triggers reboot)
    switch (result) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] update failed (%d): %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            otaSplash("Update failed", httpUpdate.getLastErrorString().c_str(),
                      TFT_RED, TFT_DARKGREY);
            delay(3000);
            break;
        case HTTP_UPDATE_NO_UPDATES:
            otaSplash("Up to date", "", TFT_GREEN, TFT_DARKGREY);
            delay(2000);
            break;
        default:
            break;
    }
}
