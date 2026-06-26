#pragma once
#include "pages/Page.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>
#include <vector>

class GifPage : public Page {
    static GifPage *s_instance;  // for C-style AnimatedGIF callbacks

    AnimatedGIF      gif;
    std::vector<String> paths;
    int              currentIndex = 0;

    bool          gifOpen   = false;
    float         scale     = 1.0f;
    float         cropX     = 0.0f;
    float         cropY     = 0.0f;
    int           frameDelay = 0;
    unsigned long nextFrame  = 0;
    uint16_t     *frameBuf   = nullptr;

    // Internal helpers
    void scanGifs();
    void openGif();
    void closeGif();
    void handleGifDraw(GIFDRAW *pDraw);

    // AnimatedGIF C-style callbacks (route to s_instance)
    static void    s_gifDraw(GIFDRAW *pDraw);
    static void   *s_open  (const char *name, int32_t *size);
    static void    s_close (void *handle);
    static int32_t s_read  (GIFFILE *pFile, uint8_t *buf, int32_t len);
    static int32_t s_seek  (GIFFILE *pFile, int32_t pos);

public:
    explicit GifPage(TFT_eSPI &tft);

    void draw()                       override;
    void tick()                       override;
    void onTap(int16_t x, int16_t y) override;
    void onLeave()                    override;
};
