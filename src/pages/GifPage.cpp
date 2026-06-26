#include "pages/GifPage.h"
#include "data.h"
#include "version.h"
#include <SD_MMC.h>

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

    auto accept = [](const String &n) {
        return n.endsWith(".gif") || n.endsWith(".png");
    };

    // LittleFS — prefix "lfs:"
    fs::File lfsRoot = LittleFS.open("/gifs", "r");
    if (lfsRoot) {
        fs::File f = lfsRoot.openNextFile();
        while (f) {
            String name = f.name();
            if (!f.isDirectory() && accept(name))
                paths.push_back("lfs:/gifs/" + name);
            f = lfsRoot.openNextFile();
        }
    }

    // SD card — prefix "sd:"
    if (SD_MMC.cardType() != CARD_NONE) {
        if (!SD_MMC.exists("/gifs")) SD_MMC.mkdir("/gifs");
        fs::File sdRoot = SD_MMC.open("/gifs", "r");
        if (sdRoot) {
            fs::File f = sdRoot.openNextFile();
            while (f) {
                String name = f.name();
                if (!f.isDirectory() && accept(name))
                    paths.push_back("sd:/gifs/" + name);
                f = sdRoot.openNextFile();
            }
        }
    }
}

void GifPage::openGif() {
    if (paths.empty()) return;

    frameBuf = (uint16_t *)ps_malloc(240 * 320 * sizeof(uint16_t));
    if (frameBuf) memset(frameBuf, 0, 240 * 320 * sizeof(uint16_t));

    const String &path = paths[currentIndex];
    if (path.endsWith(".png")) {
        isImage = true;
        imageDisplayed = false;
        openImage();
        return;
    }

    isImage = false;
    gifOpen = gif.open(path.c_str(), s_open, s_close, s_read, s_seek, s_gifDraw);
    if (!gifOpen) return;

    int   gw    = gif.getCanvasWidth();
    int   gh    = gif.getCanvasHeight();
    float scaleX = 240.0f / gw;
    float scaleY = 320.0f  / gh;
    scale  = max(scaleX, scaleY);
    cropX  = (gw - 240.0f / scale) / 2.0f;
    cropY  = (gh - 320.0f / scale) / 2.0f;
    nextFrame = millis();
}

void GifPage::closeGif() {
    if (gifOpen) { gif.close(); gifOpen = false; }
    if (frameBuf) { free(frameBuf); frameBuf = nullptr; }
    isImage = false;
    imageDisplayed = false;
}

// ── AnimatedGIF callbacks ─────────────────────────────────────────────────────

void GifPage::handleGifDraw(GIFDRAW *pDraw) {
    if (!frameBuf) return;

    int   srcY      = pDraw->iY + pDraw->y;
    float dstYfS    = (srcY     - cropY) * scale;
    float dstYfE    = (srcY + 1 - cropY) * scale;
    int   dstYstart = max(0, (int)dstYfS);
    int   dstYend   = min(319, (int)dstYfE - 1);
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
    fs::File *f;
    if (strncmp(name, "lfs:", 4) == 0)
        f = new fs::File(LittleFS.open(name + 4, "r"));
    else if (strncmp(name, "sd:", 3) == 0)
        f = new fs::File(SD_MMC.open(name + 3, FILE_READ));
    else
        f = new fs::File(LittleFS.open(name, "r"));  // legacy paths

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

// ── PNG support ───────────────────────────────────────────────────────────────

static uint16_t *s_pngRowBuf  = nullptr;
static int       s_pngRowBufW = 0;

// Open/close reuse the same GIF callbacks (identical signature and logic).
// Read/seek need separate implementations because PNGFILE != GIFFILE by type.

int32_t GifPage::s_pngRead(PNGFILE *pFile, uint8_t *buf, int32_t len) {
    auto *f    = static_cast<fs::File *>(pFile->fHandle);
    int32_t av = pFile->iSize - pFile->iPos;
    if (len > av) len = av;
    if (len <= 0) return 0;
    len = f->read(buf, len);
    pFile->iPos = f->position();
    return len;
}

int32_t GifPage::s_pngSeek(PNGFILE *pFile, int32_t pos) {
    auto *f = static_cast<fs::File *>(pFile->fHandle);
    f->seek(pos);
    pFile->iPos = f->position();
    return pFile->iPos;
}

int GifPage::s_pngDraw(PNGDRAW *pDraw) {
    GifPage *p = s_instance;
    if (!p || !p->frameBuf || !s_pngRowBuf) return 1;

    // Convert row to RGB565 little-endian (matches frameBuf byte order)
    p->png.getLineAsRGB565(pDraw, s_pngRowBuf, PNG_RGB565_LITTLE_ENDIAN, 0xFFFF);

    int   srcY    = pDraw->y;
    float dstYfS  = (srcY     - p->cropY) * p->scale;
    float dstYfE  = (srcY + 1 - p->cropY) * p->scale;
    int   dstYs   = max(0,   (int)dstYfS);
    int   dstYe   = min(319, (int)dstYfE - 1);
    if (dstYs > dstYe) return 1;

    for (int dstY = dstYs; dstY <= dstYe; dstY++) {
        uint16_t *row = p->frameBuf + dstY * 240;
        for (int dstX = 0; dstX < 240; dstX++) {
            int srcX = (int)(dstX / p->scale + p->cropX);
            if (srcX >= 0 && srcX < pDraw->iWidth)
                row[dstX] = s_pngRowBuf[srcX];
        }
    }
    return 1;
}

void GifPage::openImage() {
    const String &path = paths[currentIndex];

    int imgW = 0, imgH = 0;
    int rc = png.open(
        path.c_str(),
        s_open,    // same open callback as GIF (handles lfs:/sd: prefix)
        s_close,   // same close callback
        s_pngRead,
        s_pngSeek,
        s_pngDraw
    );
    if (rc != PNG_SUCCESS) {
        Serial.printf("GifPage: PNG open failed (%d): %s\n", rc, path.c_str());
        return;
    }

    imgW = png.getWidth();
    imgH = png.getHeight();

    float scaleX = 240.0f / imgW;
    float scaleY = 320.0f / imgH;
    scale = max(scaleX, scaleY);
    cropX = (imgW - 240.0f / scale) / 2.0f;
    cropY = (imgH - 320.0f / scale) / 2.0f;

    // Ensure row buffer is large enough
    if (imgW > s_pngRowBufW) {
        if (s_pngRowBuf) { free(s_pngRowBuf); s_pngRowBuf = nullptr; }
        s_pngRowBuf  = (uint16_t *)ps_malloc(imgW * sizeof(uint16_t));
        s_pngRowBufW = s_pngRowBuf ? imgW : 0;
    }

    png.decode(nullptr, 0);  // fills frameBuf via s_pngDraw
    png.close();
}

// ── Inverted dot/version overlay ──────────────────────────────────────────────

void GifPage::drawDotsOverGif() {
    const int cy      = DOTS_BAR_Y + DOTS_BAR_H / 2;
    const int spacing = 16;
    const int startX  = 120 - ((NUM_PAGES - 1) * spacing) / 2;

    // Pick white or dark-grey based on the average luminance of the dot-bar area.
    // Uses an exponential moving average + hysteresis so the colour only flips
    // when the background has been consistently light/dark for several frames,
    // avoiding rapid flickering.
    static uint16_t dotColor  = TFT_WHITE;
    static uint8_t  smoothLum = 128;

    if (frameBuf) {
        // Un-swap and compute perceived luminance (ITU-R BT.601) for one pixel.
        auto pixLum = [&](int x, int y) -> uint8_t {
            if (x < 0 || x >= 240 || y < 0 || y >= 320) return 128;
            uint16_t c = frameBuf[y * 240 + x];
            c = (c >> 8) | (c << 8); // un-swap to native RGB565
            uint8_t r = (c >> 11) & 0x1F;
            uint8_t g = (c >>  5) & 0x3F;
            uint8_t b =  c        & 0x1F;
            // Scale to ~8-bit then weight: 0.299R + 0.587G + 0.114B
            return (uint8_t)((r * 8u * 299u + g * 4u * 587u + b * 8u * 114u) / 1000u);
        };

        // Sample under each dot and near the version text
        uint32_t lum = 0;
        int n = 0;
        for (int i = 0; i < NUM_PAGES; i++) { lum += pixLum(startX + i * spacing, cy); n++; }
        lum += pixLum(218, cy); n++;

        // Exponential moving average — weight 7:1 keeps old value, damps per-frame noise
        smoothLum = (uint8_t)((smoothLum * 7u + lum / n) / 8u);

        // Hysteresis: only flip when average crosses well past the midpoint
        if (dotColor == TFT_WHITE   && smoothLum > 160) dotColor = TFT_DARKGREY;
        if (dotColor == TFT_DARKGREY && smoothLum <  96) dotColor = TFT_WHITE;
    }

    for (int i = 0; i < NUM_PAGES; i++) {
        int cx = startX + i * spacing;
        if (i == pageIndex)
            tft.fillCircle(cx, cy, 4, dotColor);
        else
            tft.drawCircle(cx, cy, 4, dotColor);
    }

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(dotColor);
    tft.setTextSize(1);
    tft.drawString(VERSION_STR, 237, cy);
}

// ── showByName ────────────────────────────────────────────────────────────────

void GifPage::showByName(const char *name) {
    for (int i = 0; i < (int)paths.size(); i++) {
        if (paths[i].indexOf(name) >= 0) {
            currentIndex = i;
            data_write_gif_index(currentIndex);
            break;
        }
    }
    closeGif();
    draw();
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
        drawDotsAndVersion(); // black bar is fine on error/empty screen
    }
}

void GifPage::tick() {
    if (!frameBuf) return;

    // Static image — push once then wait for a tap to cycle
    if (isImage) {
        if (!imageDisplayed) {
            tft.startWrite();
            tft.setAddrWindow(0, 0, 240, 320);
            tft.pushColors(frameBuf, 240 * 320, true);
            tft.endWrite();
            drawDotsOverGif();
            imageDisplayed = true;
        }
        return;
    }

    if (!gifOpen) return;

    unsigned long now = millis();
    if (now < nextFrame) return;

    if (!gif.playFrame(false, &frameDelay)) { gif.reset(); frameDelay = 0; }

    tft.startWrite();
    tft.setAddrWindow(0, 0, 240, 320);
    tft.pushColors(frameBuf, 240 * 320, true);
    tft.endWrite();

    drawDotsOverGif();

    // Accumulate from the scheduled time, not from now, so timing errors don't drift.
    // If we've fallen more than 500 ms behind (e.g. after an upload overlay), reset.
    nextFrame += max(frameDelay, 1);
    if (nextFrame < now - 500) nextFrame = now;
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
