#include "avatar_render.h"
#include <PNGdec.h>
#include <LittleFS.h>

static const int kSrcW = 60;
static const int kSrcH = 60;

// ── style name tables (must match AvatarPage) ─────────────────────────────────
static const char *kBGBase[]     = {"BG"};
static const char *kBGShape[]    = {"Square", "Triangle2"};
static const char *kBodyBase[]   = {"Body"};
static const char *kClothNames[] = {"Blazer", "Blouse1", "Hoodie", "Relaxed", "Striped", "Sweater", "TShirt"};
static const char *kAccBodyNames[]= {"Choker", "HairBow", "Hat1", "Hat2", "Headphones"};
static const char *kAccFaceNames[]= {"Glasses1", "Glasses2", "Glasses3", "Sunglasses"};

// ── module-level render context (single-threaded, reused per call) ────────────
static struct RenderCtx {
    uint16_t *fb;
    int fbW, fbH;
    int lastOutY;
    PNG png;
} s_ctx;

// ── PNG file callbacks ─────────────────────────────────────────────────────────
static void *png_open(const char *name, int32_t *size)
{
    auto *f = new fs::File(LittleFS.open(name, "r"));
    if (f && *f) { *size = f->size(); return f; }
    delete f;
    return nullptr;
}
static void png_close(void *handle)
{
    auto *f = static_cast<fs::File *>(handle);
    if (f) { f->close(); delete f; }
}
static int32_t png_read(PNGFILE *pf, uint8_t *buf, int32_t len)
{
    return static_cast<fs::File *>(pf->fHandle)->read(buf, len);
}
static int32_t png_seek(PNGFILE *pf, int32_t pos)
{
    static_cast<fs::File *>(pf->fHandle)->seek(pos);
    return static_cast<fs::File *>(pf->fHandle)->position();
}
static int png_draw(PNGDRAW *pd)
{
    RenderCtx *ctx = &s_ctx;
    if (!ctx->fb) return 1;

    int outY = (pd->y * ctx->fbH) / kSrcH;
    if (outY >= ctx->fbH || outY == ctx->lastOutY) return 1;
    ctx->lastOutY = outY;

    uint8_t  *src = pd->pPixels;
    uint16_t *row = ctx->fb + outY * ctx->fbW;

    for (int outX = 0; outX < ctx->fbW; outX++) {
        int     srcX = (outX * kSrcW) / ctx->fbW;
        uint8_t *px  = src + srcX * 4;
        uint8_t  r = px[0], g = px[1], b = px[2], a = px[3];
        if (a == 0) continue;
        if (a == 255) {
            row[outX] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
        } else {
            uint16_t dst  = row[outX];
            uint8_t  dstR = (dst >> 8) & 0xF8;
            uint8_t  dstG = (dst >> 3) & 0xFC;
            uint8_t  dstB = (dst << 3) & 0xF8;
            uint8_t  nr   = ((uint16_t)r * a + (uint16_t)dstR * (255 - a)) >> 8;
            uint8_t  ng   = ((uint16_t)g * a + (uint16_t)dstG * (255 - a)) >> 8;
            uint8_t  nb   = ((uint16_t)b * a + (uint16_t)dstB * (255 - a)) >> 8;
            row[outX] = ((uint16_t)(nr & 0xF8) << 8) | ((uint16_t)(ng & 0xFC) << 3) | (nb >> 3);
        }
    }
    return 1;
}

// ── draw one layer into s_ctx.fb ──────────────────────────────────────────────
static void draw_layer(const char *folder, const char *style, uint8_t colorIdx)
{
    char path[96];
    snprintf(path, sizeof(path), "/StreetPassCharacterData/%s/%s_%d.png",
             folder, style, (int)colorIdx);
    s_ctx.lastOutY = -1;
    if (s_ctx.png.open(path, png_open, png_close, png_read, png_seek, png_draw) == PNG_SUCCESS) {
        s_ctx.png.decode(nullptr, 0);
        s_ctx.png.close();
    }
}

// ── public render functions ────────────────────────────────────────────────────
void avatar_render_to_buf(uint16_t *fb, int w, int h, const avatar_config_t &cfg)
{
    memset(fb, 0, w * h * sizeof(uint16_t));
    s_ctx.fb  = fb;
    s_ctx.fbW = w;
    s_ctx.fbH = h;

    char name[24];

    // 1. Background base (mandatory)
    if (cfg.bgBase < sizeof(kBGBase)/sizeof(kBGBase[0]))
        draw_layer("BG_Base", kBGBase[cfg.bgBase], cfg.bgBaseColor);
    // BG shape (0 = none)
    if (cfg.bgShape > 0 && cfg.bgShape <= sizeof(kBGShape)/sizeof(kBGShape[0]))
        draw_layer("BG_Shape", kBGShape[cfg.bgShape - 1], cfg.bgShapeColor);

    // 2. Hair back (0 = none; N = Group_N)
    if (cfg.hairStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.hairStyle);
        draw_layer("Hair_Back", name, cfg.hairColor);
    }

    // 3. Body base (mandatory) + clothing (0 = none)
    if (cfg.bodyShape < sizeof(kBodyBase)/sizeof(kBodyBase[0]))
        draw_layer("Body_Base", kBodyBase[cfg.bodyShape], cfg.bodyShapeColor);
    if (cfg.bodyClothing > 0 && cfg.bodyClothing <= sizeof(kClothNames)/sizeof(kClothNames[0]))
        draw_layer("Body_Clothing", kClothNames[cfg.bodyClothing - 1], cfg.bodyClothingColor);

    // 4. Hair front (0 = none)
    if (cfg.hairStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.hairStyle);
        draw_layer("Hair_Front", name, cfg.hairColor);
    }

    // 5. Face (each part 0 = none; N = style N)
    if (cfg.earStyle > 0) {
        snprintf(name, sizeof(name), "Ear%d", (int)cfg.earStyle);
        draw_layer("Face_Ears", name, cfg.skinColor);
    }
    if (cfg.faceShape > 0) {
        snprintf(name, sizeof(name), "Shape%d", (int)cfg.faceShape);
        draw_layer("Face_Base", name, cfg.skinColor);
    }
    if (cfg.eyebrowStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.eyebrowStyle);
        draw_layer("Face_Eyebrows", name, cfg.eyebrowColor);
    }
    if (cfg.eyeStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.eyeStyle);
        draw_layer("Face_Eyes", name, cfg.eyeColor);
    }
    if (cfg.noseStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.noseStyle);
        draw_layer("Face_Nose", name, cfg.skinColor);
    }
    if (cfg.mouthStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.mouthStyle);
        draw_layer("Face_Mouth", name, cfg.mouthColor);
    }

    // 5b. Bangs — after face (0 = none)
    if (cfg.bangsStyle > 0) {
        snprintf(name, sizeof(name), "Group_%d", (int)cfg.bangsStyle);
        draw_layer("Hair_Bangs", name, cfg.hairColor);
    }

    // 6. Accessories (0 = none)
    if (cfg.accBodyStyle > 0 && cfg.accBodyStyle <= sizeof(kAccBodyNames)/sizeof(kAccBodyNames[0]))
        draw_layer("Accessory_Base", kAccBodyNames[cfg.accBodyStyle - 1], cfg.accBodyColor);
    if (cfg.accFaceStyle > 0 && cfg.accFaceStyle <= sizeof(kAccFaceNames)/sizeof(kAccFaceNames[0]))
        draw_layer("Accessory_Face", kAccFaceNames[cfg.accFaceStyle - 1], cfg.accFaceColor);

    s_ctx.fb = nullptr;
}

void avatar_render(TFT_eSPI &tft, const avatar_config_t &cfg, int x, int y, int w, int h)
{
    uint16_t *fb = (uint16_t *)ps_malloc(w * h * sizeof(uint16_t));
    if (!fb) return;
    avatar_render_to_buf(fb, w, h, cfg);
    tft.pushImage(x, y, w, h, fb);
    free(fb);
}
