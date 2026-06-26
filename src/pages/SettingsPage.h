#pragma once
#include "pages/Page.h"
#include <WString.h>

class SettingsPage : public Page {
public:
    enum class Mode { MAIN, KB_SSID, KB_PASS, CONFIRM_FMT };

private:
    Mode   mode    = Mode::MAIN;
    String ssid;
    String passwd;
    bool   shifted  = false;
    bool   numMode  = false;
    bool   cursorOn = false;
    unsigned long lastBlink = 0;

    void drawMain();
    void drawKeyboard();
    void drawKbInput();
    void drawConfirmFmt();

    void handleMainTap(int16_t x, int16_t y);
    void handleKbTap  (int16_t x, int16_t y);

    // Returns the character for the key at (x,y), or:
    //   '\x01' = toggle letters/numbers
    //   '\x02' = shift
    //   '\x08' = backspace
    //   '\n'   = done
    //   '\0'   = missed
    char kbHit(int16_t x, int16_t y);

public:
    explicit SettingsPage(TFT_eSPI &tft);

    void draw()                       override;
    void tick()                       override;
    void onTap(int16_t x, int16_t y) override;
    void onLeave()                    override;
    bool interceptsSwipe()            override;
};
