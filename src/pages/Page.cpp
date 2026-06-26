#include "pages/Page.h"
#include "version.h"

void Page::drawDotsAndVersion() const {
    const int cy      = DOTS_BAR_Y + DOTS_BAR_H / 2;
    const int spacing = 16;
    const int startX  = 120 - ((NUM_PAGES - 1) * spacing) / 2;

    tft.fillRect(0, DOTS_BAR_Y, 240, DOTS_BAR_H, TFT_BLACK);

    for (int i = 0; i < NUM_PAGES; i++) {
        if (i == pageIndex)
            tft.fillCircle(startX + i * spacing, cy, 4, TFT_WHITE);
        else
            tft.drawCircle(startX + i * spacing, cy, 4, TFT_DARKGREY);
    }

    // Version tag at the right edge of the dots bar
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(VERSION_STR, 237, cy);
}

uint16_t Page::drawTitleBar(const char *title, uint16_t color) const {
    tft.fillRect(0, 0, 240, 30, color);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, color);
    tft.setTextSize(2);
    tft.drawString(title, 8, 15);

    // Version in top-right corner of title bar
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(0xAD75, color);  // muted light on whatever header bg
    tft.setTextSize(1);
    tft.drawString(VERSION_STR, 236, 15);

    return color;
}
