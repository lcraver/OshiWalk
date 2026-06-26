#include "web/wifi_manager.h"
#include <WiFi.h>
#include <nvs.h>
#include <Arduino.h>

static bool   s_apMode = false;
static String s_ip;

static void splash(TFT_eSPI &tft, const char *line1, const char *line2,
                   uint16_t col1, uint16_t col2) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(col1, TFT_BLACK);
    tft.drawString(line1, 120, 130);
    tft.setTextSize(1);
    tft.setTextColor(col2, TFT_BLACK);
    tft.drawString(line2, 120, 158);
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.drawString("tap to continue", 120, 200);
}


static bool readCredentials(String &ssid, String &password) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return false;
    char buf[128] = {};
    size_t len = sizeof(buf);
    bool ok = false;
    if (nvs_get_str(h, "ssid", buf, &len) == ESP_OK && len > 1) {
        ssid = buf;
        len = sizeof(buf);
        if (nvs_get_str(h, "password", buf, &len) == ESP_OK) {
            password = buf;
            ok = true;
        }
    }
    nvs_close(h);
    return ok;
}

bool wifi_save_credentials(const String &ssid, const String &password) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_str(h, "ssid",     ssid.c_str())     == ESP_OK
           && nvs_set_str(h, "password", password.c_str()) == ESP_OK
           && nvs_commit(h)                                 == ESP_OK;
    nvs_close(h);
    return ok;
}

void wifi_init(TFT_eSPI &tft) {
    String ssid, password;
    readCredentials(ssid, password);

    if (ssid.length() > 0) {
        splash(tft, "Connecting...", ssid.c_str(), TFT_WHITE, TFT_DARKGREY);

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
            delay(200);

        if (WiFi.status() == WL_CONNECTED) {
            s_apMode = false;
            s_ip     = WiFi.localIP().toString();
            splash(tft, "Connected!", s_ip.c_str(), TFT_GREEN, TFT_DARKGREY);
            return;
        }

        // Credentials existed but connection failed — fall through to AP
        WiFi.disconnect(true);
    }

    // AP fallback
    s_apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ProxiOshi");
    s_ip = WiFi.softAPIP().toString();
    splash(tft, "AP: ProxiOshi", s_ip.c_str(), TFT_YELLOW, TFT_DARKGREY);
}

bool   wifi_is_ap_mode() { return s_apMode; }
String wifi_ip()         { return s_ip; }

String wifi_get_ssid() {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return "";
    char buf[128] = {};
    size_t len = sizeof(buf);
    bool ok = nvs_get_str(h, "ssid", buf, &len) == ESP_OK;
    nvs_close(h);
    return ok ? String(buf) : "";
}
