#pragma once
#include "pages/Page.h"
#include <AnimatedGIF.h>
#include <PNGdec.h>
#include <LittleFS.h>
#include <vector>

class GifPage : public Page {
    static GifPage *s_instance;  // for C-style AnimatedGIF callbacks

    AnimatedGIF      gif;
    PNG              png;
    std::vector<String> paths;
    int              currentIndex = 0;

    bool          gifOpen        = false;
    bool          isImage        = false; // true when current item is a static PNG
    bool          imageDisplayed = false;
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
    void openImage();
    void handleGifDraw(GIFDRAW *pDraw);
    void drawDotsOverGif();

    // AnimatedGIF callbacks
    static void    s_gifDraw(GIFDRAW *pDraw);
    static void   *s_open  (const char *name, int32_t *size);
    static void    s_close (void *handle);
    static int32_t s_read  (GIFFILE *pFile, uint8_t *buf, int32_t len);
    static int32_t s_seek  (GIFFILE *pFile, int32_t pos);

    // PNGdec callbacks
    static int     s_pngDraw(PNGDRAW *pDraw);
    static int32_t s_pngRead(PNGFILE *pFile, uint8_t *buf, int32_t len);
    static int32_t s_pngSeek(PNGFILE *pFile, int32_t pos);

public:
    explicit GifPage(TFT_eSPI &tft);

    void draw()                       override;
    void tick()                       override;
    void onTap(int16_t x, int16_t y) override;
    void onLeave()                    override;

    // Re-scan /gifs on LittleFS; safe to call from the main loop after an upload/delete.
    void rescan() { closeGif(); scanGifs(); draw(); }

    // After a rescan, seek to the GIF whose filename contains `name` and redraw.
    void showByName(const char *name);
};
