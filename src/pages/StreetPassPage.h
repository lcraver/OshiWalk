#pragma once
#include "pages/Page.h"
#include <stdint.h>

class StreetPassPage : public Page {
    static const int MAX_ROW = 6;
    static const int AV_SIZE = 60;

    uint16_t *avatarCache[MAX_ROW];   // PSRAM framebuf per visible slot
    bool      cacheValid[MAX_ROW];
    uint32_t  lastRefreshMs;

    void refreshCache();

public:
    explicit StreetPassPage(TFT_eSPI &tft);
    ~StreetPassPage() override;

    void draw() override;
    void tick() override;
};
