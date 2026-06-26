#include "pages/SettingsPage.h"
#include "web/wifi_manager.h"
#include "web/webserver.h"
#include "version.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <Arduino.h>

// ── keyboard geometry ─────────────────────────────────────────────────────────
//
//  Input display:  y =   0 .. 46   (47 px)
//  Row 0:          y =  47 .. 107  (60 px)
//  Row 1:          y = 110 .. 170
//  Row 2:          y = 173 .. 233
//  Row 3:          y = 236 .. 296
//  Dots bar:       y = 298

static const int KB_Y[] = { 47, 110, 173, 236 };
static const int KEY_H  = 60;
static const int KEY_W  = 24;

// Letter rows: lower, upper, numeric
static const char *RL[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char *RU[] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
static const char *RN[] = { "1234567890", "@#$%&*-()", ".,!?_/:" };
//                row 1 nums is 9 chars (same centred rendering as letter row 1)
//                row 2 nums is 7 chars (same layout as letter row 2)

// ── module-level draw helpers ─────────────────────────────────────────────────

static void drawKey(TFT_eSPI &tft, int x, int y, int w,
                    const char *lbl, uint16_t bg = 0x2104, uint16_t fg = TFT_WHITE) {
    tft.fillRoundRect(x+1, y+1, w-2, KEY_H-2, 4, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.setTextSize(1);
    tft.drawString(lbl, x + w/2, y + KEY_H/2);
}

static void drawStorageBar(TFT_eSPI &tft, int x, int y, int w, int h,
                           uint32_t used, uint32_t total) {
    tft.drawRoundRect(x, y, w, h, 2, 0x4208);
    if (total > 0 && used <= total) {
        int fill = (int)((long long)used * (w-2) / total);
        uint16_t col = 0x0540;  // green
        if (used > total * 8 / 10) col = 0xC520;  // amber
        if (used > total * 9 / 10) col = 0xA800;  // red
        if (fill > 0) tft.fillRect(x+1, y+1, fill, h-2, col);
    }
}

static String humanBytes(uint32_t b) {
    if (b >= 1024*1024) return String(b/1024/1024) + "MB";
    if (b >= 1024)      return String(b/1024) + "KB";
    return String(b) + "B";
}

// ── construction ──────────────────────────────────────────────────────────────

bool SettingsPage::s_webStarted = false;

SettingsPage::SettingsPage(TFT_eSPI &tft) : Page(tft, 3) {
    ssid = wifi_get_ssid();
}

// ── main view ─────────────────────────────────────────────────────────────────

void SettingsPage::drawMain() {
    tft.fillScreen(TFT_BLACK);
    drawTitleBar("Settings", 0x2945);

    // ── WiFi status ──────────────────────────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextDatum(ML_DATUM);

    bool connected = (WiFi.status() == WL_CONNECTED);
    bool apMode    = wifi_is_ap_mode();

    if (connected) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Connected", 10, 42);
        tft.setTextColor(0x8410, TFT_BLACK);
        tft.drawString(WiFi.localIP().toString().c_str(), 10, 55);
    } else if (apMode) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("AP: ProxiOshi", 10, 42);
        tft.setTextColor(0x8410, TFT_BLACK);
        tft.drawString(wifi_ip().c_str(), 10, 55);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Not connected", 10, 42);
    }

    // ── Web server toggle ────────────────────────────────────────────────────
    if (s_webStarted) {
        tft.setTextColor(0x07E0, TFT_BLACK); // green
        tft.drawString("Web UI: " + wifi_ip(), 10, 63);
    } else {
        tft.fillRoundRect(8, 56, 224, 22, 4, 0x0C25);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, 0x0C25);
        tft.drawString("Start Web Server", 120, 67);
        tft.setTextDatum(ML_DATUM);
    }

    tft.drawFastHLine(0, 83, 240, 0x2104);

    // ── SSID field ───────────────────────────────────────────────────────────
    tft.setTextColor(0x8410, TFT_BLACK);
    tft.drawString("SSID", 10, 93);
    tft.drawRoundRect(8, 101, 224, 24, 3, 0x4208);
    if (ssid.length()) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        String shown = ssid;
        if (shown.length() > 30) shown = shown.substring(shown.length()-30);
        tft.drawString(shown.c_str(), 14, 113);
    }

    // ── Password field ───────────────────────────────────────────────────────
    tft.setTextColor(0x8410, TFT_BLACK);
    tft.drawString("Password", 10, 134);
    tft.drawRoundRect(8, 142, 224, 24, 3, 0x4208);
    if (passwd.length()) {
        String stars;
        for (size_t i = 0; i < passwd.length(); i++) stars += '*';
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(stars.c_str(), 14, 154);
    }

    // ── Connect button ───────────────────────────────────────────────────────
    tft.fillRoundRect(8, 174, 224, 26, 4, 0x0340);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, 0x0340);
    tft.drawString("Save & Connect", 120, 187);

    tft.drawFastHLine(0, 208, 240, 0x2104);

    // ── Device storage ───────────────────────────────────────────────────────
    uint32_t lfsUsed  = LittleFS.usedBytes();
    uint32_t lfsTotal = LittleFS.totalBytes();
    uint32_t lfsFree  = lfsTotal > lfsUsed ? lfsTotal - lfsUsed : 0;

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(0x8410, TFT_BLACK);
    tft.drawString("Device", 10, 216);

    drawStorageBar(tft, 8, 225, 224, 10, lfsUsed, lfsTotal);

    tft.setTextColor(0x6B4D, TFT_BLACK);
    tft.drawString((humanBytes(lfsFree) + " free / " + humanBytes(lfsTotal)).c_str(), 10, 239);

    // ── SD card ──────────────────────────────────────────────────────────────
    bool sdMnt = webserver_sd_mounted();
    bool sdPrs = webserver_sd_present();
    uint8_t cardType = SD_MMC.cardType();

    static const char *cardNames[] = { "NONE", "MMC", "SD", "SDHC", "UNKN" };
    const char *typeName = (cardType <= 4) ? cardNames[cardType] : "?";

    tft.setTextColor(0x8410, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("SD Card", 10, 253);

    // Raw diagnostic line so the state is always visible
    char diagBuf[48];
    snprintf(diagBuf, sizeof(diagBuf), "type:%s  mnt:%d  prs:%d",
             typeName, sdMnt ? 1 : 0, sdPrs ? 1 : 0);
    tft.setTextColor(0x4A69, TFT_BLACK); // dim blue-grey
    tft.drawString(diagBuf, 10, 264);

    if (!sdMnt) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Not mounted — tap to format", 10, 277);
        tft.fillRoundRect(8, 287, 224, 26, 4, 0x6320);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, 0x6320);
        tft.drawString("Format SD Card", 120, 300);
    } else {
        uint32_t sdTotal = (uint32_t)(SD_MMC.totalBytes() / 1024 / 1024);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        tft.drawString((String(sdTotal) + " MB total").c_str(), 10, 258);
    }

    drawDotsAndVersion();
}

// ── keyboard input display ────────────────────────────────────────────────────

void SettingsPage::drawKbInput() {
    const bool isPass = (mode == Mode::KB_PASS);
    const String &field = isPass ? passwd : ssid;

    tft.fillRect(0, 0, 240, 46, 0x0841);
    tft.drawFastHLine(0, 46, 240, 0x4208);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(0x8410, 0x0841);
    tft.setTextSize(1);
    tft.drawString(isPass ? "Password:" : "SSID:", 8, 8);

    String shown;
    if (isPass)
        for (size_t i = 0; i < field.length(); i++) shown += '*';
    else
        shown = field;

    shown += cursorOn ? '_' : ' ';

    // Clip to last ~36 chars so it doesn't overflow
    if ((int)shown.length() > 36) shown = shown.substring(shown.length()-36);

    tft.setTextColor(TFT_WHITE, 0x0841);
    tft.drawString(shown.c_str(), 8, 30);
}

// ── keyboard view ─────────────────────────────────────────────────────────────

void SettingsPage::drawKeyboard() {
    tft.fillRect(0, 47, 240, 251, TFT_BLACK);
    drawKbInput();

    const char **rows = numMode ? RN : (shifted ? RU : RL);

    // Row 0: 10 keys full width
    for (int i = 0; i < 10; i++) {
        char buf[2] = { rows[0][i], 0 };
        drawKey(tft, i*KEY_W, KB_Y[0], KEY_W, buf);
    }

    // Row 1: 9 keys centred (letter) or 9 keys full-width (num)
    if (numMode) {
        for (int i = 0; i < 9; i++) {
            char buf[2] = { RN[1][i], 0 };
            drawKey(tft, i*KEY_W, KB_Y[1], KEY_W, buf);
        }
        // fill the gap of 24px on the right with nothing (already black)
    } else {
        for (int i = 0; i < 9; i++) {
            char buf[2] = { rows[1][i], 0 };
            drawKey(tft, 12 + i*KEY_W, KB_Y[1], KEY_W, buf);
        }
    }

    // Row 2: [Shft/Sym] [7 chars] [Del]
    {
        const char *shiftLbl = numMode ? "Sym" : (shifted ? "shft" : "Shft");
        uint16_t shiftBg = (!numMode && shifted) ? 0x528A : 0x2104;
        drawKey(tft, 0, KB_Y[2], 36, shiftLbl, shiftBg);

        int n = strlen(rows[2]);  // 7 for both letters and nums row 2
        for (int i = 0; i < n; i++) {
            char buf[2] = { rows[2][i], 0 };
            drawKey(tft, 36 + i*KEY_W, KB_Y[2], KEY_W, buf);
        }
        drawKey(tft, 204, KB_Y[2], 36, "Del", 0x3000);
    }

    // Row 3: [toggle] [space] [done]
    drawKey(tft,   0, KB_Y[3],  48, numMode ? "abc" : "123");
    drawKey(tft,  48, KB_Y[3], 144, "space", 0x1082);
    drawKey(tft, 192, KB_Y[3],  48, "Done", 0x0340);

    drawDotsAndVersion();
}

// ── format confirmation ───────────────────────────────────────────────────────

void SettingsPage::drawConfirmFmt() {
    tft.fillScreen(TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Format SD?", 120, 95);

    tft.setTextSize(1);
    tft.setTextColor(0x8410, TFT_BLACK);
    tft.drawString("This erases all data on", 120, 125);
    tft.drawString("the SD card.", 120, 140);

    // Format button (left, red-ish)
    tft.fillRoundRect(18, 170, 90, 34, 6, 0x8000);
    tft.setTextColor(TFT_WHITE, 0x8000);
    tft.drawString("Format", 63, 187);

    // Cancel button (right, neutral)
    tft.fillRoundRect(132, 170, 90, 34, 6, 0x2104);
    tft.setTextColor(TFT_WHITE, 0x2104);
    tft.drawString("Cancel", 177, 187);

    drawDotsAndVersion();
}

// ── keyboard hit test ─────────────────────────────────────────────────────────

char SettingsPage::kbHit(int16_t tx, int16_t ty) {
    int row = -1;
    for (int i = 0; i < 4; i++)
        if (ty >= KB_Y[i] && ty < KB_Y[i] + KEY_H) { row = i; break; }
    if (row < 0) return 0;

    const char **rows = numMode ? RN : (shifted ? RU : RL);

    if (row == 3) {
        if (tx < 48)  return '\x01';  // toggle 123/abc
        if (tx < 192) return ' ';
        return '\n';                   // Done
    }
    if (row == 0) {
        int i = tx / KEY_W;
        if (i >= 0 && i < 10) return rows[0][i];
    }
    else if (row == 1) {
        if (numMode) {
            int i = tx / KEY_W;
            if (i >= 0 && i < 9) return RN[1][i];
        } else {
            int i = (tx - 12) / KEY_W;
            if (i >= 0 && i < 9) return rows[1][i];
        }
    }
    else if (row == 2) {
        if (tx < 36)   return '\x02';  // shift
        if (tx >= 204) return '\x08';  // backspace
        int i = (tx - 36) / KEY_W;
        int n = strlen(rows[2]);
        if (i >= 0 && i < n) return rows[2][i];
    }
    return 0;
}

// ── tap handlers ──────────────────────────────────────────────────────────────

void SettingsPage::handleMainTap(int16_t x, int16_t y) {
    // Start Web Server button
    if (!s_webStarted && y >= 56 && y < 78) {
        s_webStarted = true;
        webserver_init();
        drawMain();
        return;
    }
    // SSID field
    if (y >= 101 && y < 126) {
        mode = Mode::KB_SSID;
        shifted = false; numMode = false;
        drawKeyboard();
        return;
    }
    // Password field
    if (y >= 142 && y < 167) {
        mode = Mode::KB_PASS;
        passwd = "";  // always start fresh for password entry
        shifted = false; numMode = false;
        drawKeyboard();
        return;
    }
    // Save & Connect button
    if (y >= 174 && y < 200) {
        tft.fillRoundRect(8, 174, 224, 26, 4, 0x0240);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, 0x0240);
        tft.drawString("Saving...", 120, 187);
        wifi_save_credentials(ssid, passwd);
        delay(300);
        ESP.restart();
        return;
    }
    // Format SD button (shown whenever not mounted)
    if (!webserver_sd_mounted() && y >= 287 && y < 313) {
        mode = Mode::CONFIRM_FMT;
        drawConfirmFmt();
        return;
    }
}

void SettingsPage::handleKbTap(int16_t x, int16_t y) {
    char c = kbHit(x, y);
    String &field = (mode == Mode::KB_PASS) ? passwd : ssid;

    if (c == '\n') {
        mode = Mode::MAIN;
        draw();
    } else if (c == '\x08') {
        if (field.length() > 0) field.remove(field.length()-1);
        drawKbInput();
    } else if (c == '\x01') {
        numMode = !numMode;
        shifted = false;
        drawKeyboard();
    } else if (c == '\x02') {
        shifted = !shifted;
        drawKeyboard();
    } else if (c == ' ') {
        if (field.length() < 64) { field += ' '; drawKbInput(); }
    } else if (c != 0) {
        if (field.length() < 64) {
            field += c;
            // Auto-release shift after one letter
            if (shifted && !numMode) { shifted = false; drawKeyboard(); }
            else drawKbInput();
        }
    }
}

// ── Page interface ────────────────────────────────────────────────────────────

void SettingsPage::draw() {
    switch (mode) {
        case Mode::MAIN:        drawMain();        break;
        case Mode::KB_SSID:
        case Mode::KB_PASS:     drawKeyboard();    break;
        case Mode::CONFIRM_FMT: drawConfirmFmt();  break;
    }
}

void SettingsPage::tick() {
    if (mode == Mode::KB_SSID || mode == Mode::KB_PASS) {
        if (millis() - lastBlink >= 500) {
            lastBlink = millis();
            cursorOn = !cursorOn;
            drawKbInput();
        }
    }
}

void SettingsPage::onTap(int16_t x, int16_t y) {
    switch (mode) {
        case Mode::MAIN:
            handleMainTap(x, y);
            break;
        case Mode::KB_SSID:
        case Mode::KB_PASS:
            handleKbTap(x, y);
            break;
        case Mode::CONFIRM_FMT:
            if (x >= 18 && x < 108 && y >= 170 && y < 204) {
                // Confirmed — format
                tft.fillScreen(TFT_BLACK);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.setTextSize(1);
                tft.drawString("Formatting SD card...", 120, 150);
                tft.drawString("Please wait", 120, 165);
                SD_MMC.end();
                bool ok = SD_MMC.begin("/sdcard", true, true);
                webserver_set_sd_state(ok, true);
                mode = Mode::MAIN;
                draw();
            } else {
                // Cancelled
                mode = Mode::MAIN;
                draw();
            }
            break;
    }
}

void SettingsPage::onLeave() {
    mode = Mode::MAIN;
}

bool SettingsPage::interceptsSwipe() {
    return mode == Mode::KB_SSID || mode == Mode::KB_PASS;
}
