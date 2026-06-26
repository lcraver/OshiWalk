#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

#define NUM_PAGES   3
#define DOTS_BAR_Y  298   // first pixel row of the dots/version bar
#define DOTS_BAR_H  22    // height of the dots/version bar

class Page {
protected:
    TFT_eSPI &tft;
    int       pageIndex;

    // Draws page-dot indicator + version tag; call at the end of draw()
    void drawDotsAndVersion() const;

    // Draw a standard title bar (y=0..29) and return its colour for callers
    // that need to match bg when drawing text on top.
    uint16_t drawTitleBar(const char *title, uint16_t color) const;

public:
    Page(TFT_eSPI &tft, int idx) : tft(tft), pageIndex(idx) {}
    virtual ~Page() {}

    virtual void draw()                       = 0;
    virtual void tick()                       {}
    virtual void onTap(int16_t x, int16_t y) {}
    virtual void onLeave()                    {}
};
