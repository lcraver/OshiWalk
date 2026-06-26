#include "pages/GifPage.h"
#include "data.h"

GifPage *GifPage::s_instance = nullptr;

// ── constructor ───────────────────────────────────────────────────────────────

GifPage::GifPage(TFT_eSPI &tft) : Page(tft, 2) {
    s_instance = this;
    gif.begin(LITTLE_ENDIAN_PIXELS);
    scanGifs();
    int saved = data_read_gif_index();
    if (saved >= 0 && saved < (int)paths.size())
        currentIndex = saved;
}

// ── GIF management ────────────────────────────────────────────────────────────

void GifPage::scanGifs() {
    paths.clear();
    fs::File root = LittleFS.open("/gifs", "r");
    fs::File f    = root.openNextFile();
    while (f) {
        String name = f.name();
        if (!f.isDirectory() && name.endsWith(".gif"))
            paths.push_back("/gifs/" + name);
        f = root.openNextFile();
    }
}

void GifPage::openGif() {
    if (paths.empty()) return;

    gifOpen = gif.open(paths[currentIndex].c_str(),
                       s_open, s_close, s_read, s_seek, s_gifDraw);
    if (!gifOpen) return;

    int   gw    = gif.getCanvasWidth();
    int   gh    = gif.getCanvasHeight();
    float scaleX = 240.0f / gw;
    float scaleY = (float)DOTS_BAR_Y / gh;
    scale  = max(scaleX, scaleY);
    cropX  = (gw - 240.0f  / scale) / 2.0f;
    cropY  = (gh - (float)DOTS_BAR_Y / scale) / 2.0f;

    frameBuf = (uint16_t *)ps_malloc(240 * DOTS_BAR_Y * sizeof(uint16_t));
    if (frameBuf) memset(frameBuf, 0, 240 * DOTS_BAR_Y * sizeof(uint16_t));
    nextFrame = millis();
}

void GifPage::closeGif() {
    if (gifOpen) { gif.close(); gifOpen = false; }
    if (frameBuf) { free(frameBuf); frameBuf = nullptr; }
}

// ── AnimatedGIF callbacks ─────────────────────────────────────────────────────

void GifPage::handleGifDraw(GIFDRAW *pDraw) {
    if (!frameBuf) return;

    int   srcY      = pDraw->iY + pDraw->y;
    float dstYfS    = (srcY     - cropY) * scale;
    float dstYfE    = (srcY + 1 - cropY) * scale;
    int   dstYstart = max(0, (int)dstYfS);
    int   dstYend   = min(DOTS_BAR_Y - 1, (int)dstYfE - 1);
    if (dstYstart > dstYend) return;

    for (int dstY = dstYstart; dstY <= dstYend; dstY++) {
        uint16_t *row = frameBuf + dstY * 240;
        for (int dstX = 0; dstX < 240; dstX++) {
            int srcX   = (int)(dstX / scale + cropX);
            int localX = srcX - pDraw->iX;
            if (localX >= 0 && localX < pDraw->iWidth) {
                uint8_t idx = pDraw->pPixels[localX];
                if (!pDraw->ucHasTransparency || idx != pDraw->ucTransparent)
                    row[dstX] = pDraw->pPalette[idx];
            }
        }
    }
}

void GifPage::s_gifDraw(GIFDRAW *pDraw) {
    if (s_instance) s_instance->handleGifDraw(pDraw);
}

void *GifPage::s_open(const char *name, int32_t *size) {
    auto *f = new fs::File(LittleFS.open(name, "r"));
    if (f && *f) { *size = f->size(); return f; }
    delete f;
    return nullptr;
}
void GifPage::s_close(void *handle) {
    auto *f = static_cast<fs::File *>(handle);
    if (f) { f->close(); delete f; }
}
int32_t GifPage::s_read(GIFFILE *pFile, uint8_t *buf, int32_t len) {
    auto *f    = static_cast<fs::File *>(pFile->fHandle);
    int32_t av = pFile->iSize - pFile->iPos;
    if (len > av) len = av;
    if (len <= 0)  return 0;
    len = f->read(buf, len);
    pFile->iPos = f->position();
    return len;
}
int32_t GifPage::s_seek(GIFFILE *pFile, int32_t pos) {
    auto *f = static_cast<fs::File *>(pFile->fHandle);
    f->seek(pos);
    pFile->iPos = f->position();
    return pFile->iPos;
}

// ── Page interface ────────────────────────────────────────────────────────────

void GifPage::draw() {
    tft.fillScreen(TFT_BLACK);
    if (paths.empty()) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("No GIFs found", 120, 150);
        drawDotsAndVersion();
        return;
    }

    openGif();

    if (!gifOpen) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("GIF load failed", 120, 150);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        char buf[24];
        snprintf(buf, sizeof(buf), "error: %d", gif.getLastError());
        tft.drawString(buf, 120, 170);
    }

    drawDotsAndVersion();
}

void GifPage::tick() {
    if (!gifOpen || !frameBuf) return;

    unsigned long now = millis();
    if (now < nextFrame) return;

    if (!gif.playFrame(false, &frameDelay)) { gif.reset(); frameDelay = 16; }

    tft.startWrite();
    tft.setAddrWindow(0, 0, 240, DOTS_BAR_Y);
    tft.pushColors(frameBuf, 240 * DOTS_BAR_Y, true);
    tft.endWrite();

    // Re-stamp dots + version over the pushed frame
    drawDotsAndVersion();

    nextFrame = now + max(frameDelay, 16);
}

void GifPage::onTap(int16_t /*x*/, int16_t /*y*/) {
    if (paths.size() <= 1) return;
    closeGif();
    currentIndex = (currentIndex + 1) % (int)paths.size();
    data_write_gif_index(currentIndex);
    openGif();
}

void GifPage::onLeave() {
    closeGif();
}
