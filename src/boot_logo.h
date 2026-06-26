#pragma once
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include <LittleFS.h>

// Draws /logo.png from LittleFS centred at (cx, cy) on the TFT.
// The image is drawn at its native size (expected 64x64 max).
// Returns false if the file is missing or decode fails.

namespace _boot_logo_impl {

static TFT_eSPI *s_tft;
static int s_x, s_y;

static void *png_open(const char *name, int32_t *size) {
    auto *f = new fs::File(LittleFS.open(name, "r"));
    if (f && *f) { *size = f->size(); return f; }
    delete f; return nullptr;
}
static void png_close(void *h) {
    auto *f = static_cast<fs::File *>(h);
    if (f) { f->close(); delete f; }
}
static int32_t png_read(PNGFILE *pf, uint8_t *buf, int32_t len) {
    return static_cast<fs::File *>(pf->fHandle)->read(buf, len);
}
static int32_t png_seek(PNGFILE *pf, int32_t pos) {
    static_cast<fs::File *>(pf->fHandle)->seek(pos);
    return static_cast<fs::File *>(pf->fHandle)->position();
}
static int png_draw(PNGDRAW *pd) {
    uint16_t row[320];
    s_tft->setSwapBytes(false);
    PNG png_dummy; // needed for convertTo16 signature
    png_dummy.getLineAsRGB565(pd, row, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
    s_tft->setSwapBytes(false);
    s_tft->pushImage(s_x, s_y + pd->y, pd->iWidth, 1, row);
    return 1;
}

} // namespace

inline bool drawBootLogo(TFT_eSPI &tft, int cx, int cy) {
    static PNG png;
    int32_t size;
    void *fh = _boot_logo_impl::png_open("/logo.png", &size);
    if (!fh) return false;

    // Peek at dimensions to centre it
    if (png.open("/logo.png",
                 _boot_logo_impl::png_open,
                 _boot_logo_impl::png_close,
                 _boot_logo_impl::png_read,
                 _boot_logo_impl::png_seek,
                 _boot_logo_impl::png_draw) != PNG_SUCCESS) {
        _boot_logo_impl::png_close(fh);
        return false;
    }
    // close the peek handle we opened manually above
    _boot_logo_impl::png_close(fh);

    _boot_logo_impl::s_tft = &tft;
    _boot_logo_impl::s_x   = cx - png.getWidth()  / 2;
    _boot_logo_impl::s_y   = cy - png.getHeight() / 2;

    png.decode(nullptr, 0);
    png.close();
    tft.setSwapBytes(true); // restore default
    return true;
}
