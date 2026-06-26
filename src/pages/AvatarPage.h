#pragma once
#include "pages/Page.h"
#include "data.h"
#include <LittleFS.h>
#include <stdint.h>

class AvatarPage : public Page {
    avatar_config_t cfg;
    int             ctrlPage = 0;

    // Controls UI
    bool            paramHasNone(int idx) const;
    void            drawControl(const char *label, int row, uint8_t val, uint8_t maxVal, bool hasNone) const;
    void            drawControls() const;
    uint8_t         getParam(int idx) const;
    uint8_t         getParamMax(int idx) const;
    const char     *getParamLabel(int idx) const;
    void            setParam(int idx, uint8_t val);

    // Filesystem scan helpers
    uint8_t scanParam(const char *folder, const char *prefix);
    uint8_t scanNumberedStyles(const char *folder, const char *prefix);

public:
    explicit AvatarPage(TFT_eSPI &tft);

    void draw()                       override;
    void onTap(int16_t x, int16_t y) override;
};
