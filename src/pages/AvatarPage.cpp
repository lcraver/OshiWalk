#include "pages/AvatarPage.h"
#include "avatar_render.h"

// ── avatar display geometry ────────────────────────────────────────────────────
static const int kAvX  = 240 / 2 - 60 / 2;
static const int kAvY  = 30;
static const int kAvW  = 60;
static const int kAvH  = 60;

// ── controls geometry ──────────────────────────────────────────────────────────
static const int kCtrlY    = 90;
static const int kCtrlHdrH = 14;
static const int kRowH     = 20;
static const int kHdrClr   = 0x780F;

// ── param indices ──────────────────────────────────────────────────────────────
static const int P_BG_BASE         = 0;
static const int P_BG_BASE_COLOR   = 1;
static const int P_BG_SHAPE        = 2;
static const int P_BG_SHAPE_COLOR  = 3;
static const int P_BODY_SHAPE      = 4;
static const int P_BODY_COLOR      = 5;
static const int P_BODY_CLOTH      = 6;
static const int P_BODY_CLOTH_COLOR= 7;
static const int P_HAIR_STYLE      = 8;
static const int P_HAIR_COLOR      = 9;
static const int P_BANGS_STYLE     = 10;
static const int P_SKIN_COLOR      = 11;
static const int P_FACE_SHAPE      = 12;
static const int P_EAR_STYLE       = 13;
static const int P_EYEBROW_STYLE   = 14;
static const int P_EYEBROW_COLOR   = 15;
static const int P_EYE_STYLE       = 16;
static const int P_EYE_COLOR       = 17;
static const int P_NOSE_STYLE      = 18;
static const int P_MOUTH_STYLE     = 19;
static const int P_MOUTH_COLOR     = 20;
static const int P_ACC_BODY_STYLE  = 21;
static const int P_ACC_BODY_COLOR  = 22;
static const int P_ACC_FACE_STYLE  = 23;
static const int P_ACC_FACE_COLOR  = 24;
static const int P_COUNT           = 25;

static const int kPageStart[]  = {0, 4, 8, 11, 14, 21};
static const int kPageEnd[]    = {4, 8, 11, 14, 21, 25};
static const int kNumCtrlPages = 6;
static const char *kPageNames[]= {"Background", "Body", "Hair", "Face", "Details", "Accessories"};

// ── named-style arrays (must match avatar_render.cpp) ─────────────────────────
static const char *kBGBase[]     = {"BG"};
static const char *kBGShape[]    = {"Square", "Triangle2"};
static const char *kBodyBase[]   = {"Body"};
static const char *kClothNames[] = {"Blazer", "Blouse1", "Hoodie", "Relaxed", "Striped", "Sweater", "TShirt"};
static const char *kAccBodyNames[]= {"Choker", "HairBow", "Hat1", "Hat2", "Headphones"};
static const char *kAccFaceNames[]= {"Glasses1", "Glasses2", "Glasses3", "Sunglasses"};

// Color counts for named styles
static uint8_t BGBaseCounts [sizeof(kBGBase)     / sizeof(kBGBase[0])]      = {0};
static uint8_t BGShapeCounts[sizeof(kBGShape)    / sizeof(kBGShape[0])]     = {0};
static uint8_t BodyBaseCount[sizeof(kBodyBase)   / sizeof(kBodyBase[0])]    = {0};
static uint8_t ClothCounts  [sizeof(kClothNames) / sizeof(kClothNames[0])]  = {0};
static uint8_t AccBodyCounts[sizeof(kAccBodyNames)/sizeof(kAccBodyNames[0])] = {0};
static uint8_t AccFaceCounts[sizeof(kAccFaceNames)/sizeof(kAccFaceNames[0])] = {0};

// Counts for numbered styles
static const int kMaxStyles = 16;
static uint8_t hairStyleCount              = 0;
static uint8_t hairColorCounts[kMaxStyles] = {0};
static uint8_t bangsCount                  = 0;
static uint8_t faceShapeCount              = 0;
static uint8_t earStyleCount               = 0;
static uint8_t skinColorCount              = 1;
static uint8_t eyebrowStyleCount           = 0;
static uint8_t eyebrowColorCounts[kMaxStyles] = {0};
static uint8_t eyeStyleCount               = 0;
static uint8_t eyeColorCounts[kMaxStyles]  = {0};
static uint8_t noseStyleCount              = 0;
static uint8_t mouthStyleCount             = 0;
static uint8_t mouthColorCounts[kMaxStyles]= {0};

// ── filesystem helpers ─────────────────────────────────────────────────────────
uint8_t AvatarPage::scanParam(const char *folder, const char *prefix)
{
    String path = "/StreetPassCharacterData/" + String(folder);
    fs::File root = LittleFS.open(path.c_str(), "r");
    if (!root) return 0;
    fs::File f = root.openNextFile();
    uint8_t count = 0;
    while (f) {
        String name = f.name();
        if (!f.isDirectory() && name.endsWith(".png") && name.startsWith(prefix))
            count++;
        f = root.openNextFile();
    }
    return count;
}

uint8_t AvatarPage::scanNumberedStyles(const char *folder, const char *prefix)
{
    uint8_t count = 0;
    for (uint8_t i = 1; i <= kMaxStyles; i++) {
        char path[96];
        snprintf(path, sizeof(path), "/StreetPassCharacterData/%s/%s%d_0.png", folder, prefix, (int)i);
        if (LittleFS.exists(path))
            count = i;
        else if (count > 0)
            break;
    }
    return count;
}

// ── constructor ────────────────────────────────────────────────────────────────
AvatarPage::AvatarPage(TFT_eSPI &tft) : Page(tft, 0)
{
    data_read_avatar(&cfg);
    // Filesystem scan deferred to first Edit entry — see doScan()
}

// ── lazy filesystem scan ───────────────────────────────────────────────────────
void AvatarPage::doScan()
{
    if (scanned) return;
    scanned = true;

    for (int i = 0; i < (int)(sizeof(kBGBase)/sizeof(kBGBase[0])); i++)
        BGBaseCounts[i]  = scanParam("BG_Base",        kBGBase[i]);
    for (int i = 0; i < (int)(sizeof(kBGShape)/sizeof(kBGShape[0])); i++)
        BGShapeCounts[i] = scanParam("BG_Shape",       kBGShape[i]);
    for (int i = 0; i < (int)(sizeof(kBodyBase)/sizeof(kBodyBase[0])); i++)
        BodyBaseCount[i] = scanParam("Body_Base",      kBodyBase[i]);
    for (int i = 0; i < (int)(sizeof(kClothNames)/sizeof(kClothNames[0])); i++)
        ClothCounts[i]   = scanParam("Body_Clothing",  kClothNames[i]);
    for (int i = 0; i < (int)(sizeof(kAccBodyNames)/sizeof(kAccBodyNames[0])); i++)
        AccBodyCounts[i] = scanParam("Accessory_Base", kAccBodyNames[i]);
    for (int i = 0; i < (int)(sizeof(kAccFaceNames)/sizeof(kAccFaceNames[0])); i++)
        AccFaceCounts[i] = scanParam("Accessory_Face", kAccFaceNames[i]);

    hairStyleCount = scanNumberedStyles("Hair_Back", "Group_");
    for (uint8_t i = 0; i < hairStyleCount && i < kMaxStyles; i++) {
        char name[12];
        snprintf(name, sizeof(name), "Group_%d", (int)(i + 1));
        hairColorCounts[i] = scanParam("Hair_Back", name);
    }

    bangsCount     = scanNumberedStyles("Hair_Bangs",   "Group_");
    faceShapeCount = scanNumberedStyles("Face_Base",    "Shape");
    earStyleCount  = scanNumberedStyles("Face_Ears",    "Ear");

    skinColorCount = scanParam("Face_Base", "Shape1");
    if (skinColorCount == 0) skinColorCount = 1;

    eyebrowStyleCount = scanNumberedStyles("Face_Eyebrows", "Group_");
    for (uint8_t i = 0; i < eyebrowStyleCount && i < kMaxStyles; i++) {
        char name[12];
        snprintf(name, sizeof(name), "Group_%d", (int)(i + 1));
        eyebrowColorCounts[i] = scanParam("Face_Eyebrows", name);
    }

    eyeStyleCount = scanNumberedStyles("Face_Eyes", "Group_");
    for (uint8_t i = 0; i < eyeStyleCount && i < kMaxStyles; i++) {
        char name[12];
        snprintf(name, sizeof(name), "Group_%d", (int)(i + 1));
        eyeColorCounts[i] = scanParam("Face_Eyes", name);
    }

    noseStyleCount  = scanNumberedStyles("Face_Nose",  "Group_");

    mouthStyleCount = scanNumberedStyles("Face_Mouth", "Group_");
    for (uint8_t i = 0; i < mouthStyleCount && i < kMaxStyles; i++) {
        char name[12];
        snprintf(name, sizeof(name), "Group_%d", (int)(i + 1));
        mouthColorCounts[i] = scanParam("Face_Mouth", name);
    }

    // Clamp config values to detected maxima
    auto clamp = [](uint8_t &v, uint8_t maxV) { if (v >= maxV) v = 0; };

    clamp(cfg.bgBase,         (uint8_t)(sizeof(kBGBase)/sizeof(kBGBase[0])));
    clamp(cfg.bgBaseColor,    BGBaseCounts[cfg.bgBase] > 0 ? BGBaseCounts[cfg.bgBase] : 1);
    clamp(cfg.bodyShape,      (uint8_t)(sizeof(kBodyBase)/sizeof(kBodyBase[0])));
    clamp(cfg.bodyShapeColor, BodyBaseCount[cfg.bodyShape] > 0 ? BodyBaseCount[cfg.bodyShape] : 1);

    clamp(cfg.bgShape,      (uint8_t)(sizeof(kBGShape)/sizeof(kBGShape[0])) + 1);
    if (cfg.bgShape > 0)    clamp(cfg.bgShapeColor, BGShapeCounts[cfg.bgShape - 1] > 0 ? BGShapeCounts[cfg.bgShape - 1] : 1);

    clamp(cfg.bodyClothing, (uint8_t)(sizeof(kClothNames)/sizeof(kClothNames[0])) + 1);
    if (cfg.bodyClothing > 0) clamp(cfg.bodyClothingColor, ClothCounts[cfg.bodyClothing - 1] > 0 ? ClothCounts[cfg.bodyClothing - 1] : 1);

    clamp(cfg.hairStyle,  (uint8_t)(hairStyleCount + 1));
    if (cfg.hairStyle > 0 && cfg.hairStyle - 1 < kMaxStyles && hairColorCounts[cfg.hairStyle - 1] > 0)
        clamp(cfg.hairColor, hairColorCounts[cfg.hairStyle - 1]);
    clamp(cfg.bangsStyle, (uint8_t)(bangsCount + 1));

    clamp(cfg.skinColor,  skinColorCount);
    clamp(cfg.faceShape,  (uint8_t)(faceShapeCount + 1));
    clamp(cfg.earStyle,   (uint8_t)(earStyleCount + 1));

    clamp(cfg.eyebrowStyle, (uint8_t)(eyebrowStyleCount + 1));
    if (cfg.eyebrowStyle > 0 && cfg.eyebrowStyle - 1 < kMaxStyles && eyebrowColorCounts[cfg.eyebrowStyle - 1] > 0)
        clamp(cfg.eyebrowColor, eyebrowColorCounts[cfg.eyebrowStyle - 1]);
    clamp(cfg.eyeStyle, (uint8_t)(eyeStyleCount + 1));
    if (cfg.eyeStyle > 0 && cfg.eyeStyle - 1 < kMaxStyles && eyeColorCounts[cfg.eyeStyle - 1] > 0)
        clamp(cfg.eyeColor, eyeColorCounts[cfg.eyeStyle - 1]);
    clamp(cfg.noseStyle,  (uint8_t)(noseStyleCount + 1));
    clamp(cfg.mouthStyle, (uint8_t)(mouthStyleCount + 1));
    if (cfg.mouthStyle > 0 && cfg.mouthStyle - 1 < kMaxStyles && mouthColorCounts[cfg.mouthStyle - 1] > 0)
        clamp(cfg.mouthColor, mouthColorCounts[cfg.mouthStyle - 1]);

    clamp(cfg.accBodyStyle, (uint8_t)(sizeof(kAccBodyNames)/sizeof(kAccBodyNames[0])) + 1);
    clamp(cfg.accFaceStyle, (uint8_t)(sizeof(kAccFaceNames)/sizeof(kAccFaceNames[0])) + 1);
}

// ── param accessors ────────────────────────────────────────────────────────────
uint8_t AvatarPage::getParam(int i) const
{
    switch (i) {
        case P_BG_BASE:          return cfg.bgBase;
        case P_BG_BASE_COLOR:    return cfg.bgBaseColor;
        case P_BG_SHAPE:         return cfg.bgShape;
        case P_BG_SHAPE_COLOR:   return cfg.bgShapeColor;
        case P_BODY_SHAPE:       return cfg.bodyShape;
        case P_BODY_COLOR:       return cfg.bodyShapeColor;
        case P_BODY_CLOTH:       return cfg.bodyClothing;
        case P_BODY_CLOTH_COLOR: return cfg.bodyClothingColor;
        case P_HAIR_STYLE:       return cfg.hairStyle;
        case P_HAIR_COLOR:       return cfg.hairColor;
        case P_BANGS_STYLE:      return cfg.bangsStyle;
        case P_SKIN_COLOR:       return cfg.skinColor;
        case P_FACE_SHAPE:       return cfg.faceShape;
        case P_EAR_STYLE:        return cfg.earStyle;
        case P_EYEBROW_STYLE:    return cfg.eyebrowStyle;
        case P_EYEBROW_COLOR:    return cfg.eyebrowColor;
        case P_EYE_STYLE:        return cfg.eyeStyle;
        case P_EYE_COLOR:        return cfg.eyeColor;
        case P_NOSE_STYLE:       return cfg.noseStyle;
        case P_MOUTH_STYLE:      return cfg.mouthStyle;
        case P_MOUTH_COLOR:      return cfg.mouthColor;
        case P_ACC_BODY_STYLE:   return cfg.accBodyStyle;
        case P_ACC_BODY_COLOR:   return cfg.accBodyColor;
        case P_ACC_FACE_STYLE:   return cfg.accFaceStyle;
        case P_ACC_FACE_COLOR:   return cfg.accFaceColor;
        default:                 return 0;
    }
}

void AvatarPage::setParam(int i, uint8_t v)
{
    switch (i) {
        case P_BG_BASE:          cfg.bgBase            = v; break;
        case P_BG_BASE_COLOR:    cfg.bgBaseColor        = v; break;
        case P_BG_SHAPE:         cfg.bgShape            = v; break;
        case P_BG_SHAPE_COLOR:   cfg.bgShapeColor       = v; break;
        case P_BODY_SHAPE:       cfg.bodyShape          = v; break;
        case P_BODY_COLOR:       cfg.bodyShapeColor     = v; break;
        case P_BODY_CLOTH:       cfg.bodyClothing       = v; break;
        case P_BODY_CLOTH_COLOR: cfg.bodyClothingColor  = v; break;
        case P_HAIR_STYLE:       cfg.hairStyle          = v; break;
        case P_HAIR_COLOR:       cfg.hairColor          = v; break;
        case P_BANGS_STYLE:      cfg.bangsStyle         = v; break;
        case P_SKIN_COLOR:       cfg.skinColor          = v; break;
        case P_FACE_SHAPE:       cfg.faceShape          = v; break;
        case P_EAR_STYLE:        cfg.earStyle           = v; break;
        case P_EYEBROW_STYLE:    cfg.eyebrowStyle       = v; break;
        case P_EYEBROW_COLOR:    cfg.eyebrowColor       = v; break;
        case P_EYE_STYLE:        cfg.eyeStyle           = v; break;
        case P_EYE_COLOR:        cfg.eyeColor           = v; break;
        case P_NOSE_STYLE:       cfg.noseStyle          = v; break;
        case P_MOUTH_STYLE:      cfg.mouthStyle         = v; break;
        case P_MOUTH_COLOR:      cfg.mouthColor         = v; break;
        case P_ACC_BODY_STYLE:   cfg.accBodyStyle       = v; break;
        case P_ACC_BODY_COLOR:   cfg.accBodyColor       = v; break;
        case P_ACC_FACE_STYLE:   cfg.accFaceStyle       = v; break;
        case P_ACC_FACE_COLOR:   cfg.accFaceColor       = v; break;
        default: break;
    }
}

bool AvatarPage::paramHasNone(int i) const
{
    switch (i) {
        case P_BG_SHAPE:
        case P_BODY_CLOTH:
        case P_HAIR_STYLE:
        case P_BANGS_STYLE:
        case P_FACE_SHAPE:
        case P_EAR_STYLE:
        case P_EYEBROW_STYLE:
        case P_EYE_STYLE:
        case P_NOSE_STYLE:
        case P_MOUTH_STYLE:
        case P_ACC_BODY_STYLE:
        case P_ACC_FACE_STYLE:
            return true;
        default:
            return false;
    }
}

// Returns true for params that must always have a value (can't be set to None).
static bool paramRequired(int i)
{
    switch (i) {
        case P_BODY_CLOTH:
        case P_FACE_SHAPE:
        case P_EAR_STYLE:
        case P_EYE_STYLE:
        case P_NOSE_STYLE:
            return true;
        default:
            return false;
    }
}

uint8_t AvatarPage::getParamMax(int i) const
{
    // For params with a none option, max = count + 1 (slot 0 is "none").
    // For color params and base params, max = count (0-indexed, no none).
    uint8_t v;
    switch (i) {
        case P_BG_BASE:          v = (uint8_t)(sizeof(kBGBase)/sizeof(kBGBase[0])); break;
        case P_BG_BASE_COLOR:    v = BGBaseCounts[cfg.bgBase]; break;
        case P_BG_SHAPE:         v = (uint8_t)(sizeof(kBGShape)/sizeof(kBGShape[0])) + 1; break;
        case P_BG_SHAPE_COLOR:   v = cfg.bgShape > 0 ? BGShapeCounts[cfg.bgShape - 1] : 1; break;
        case P_BODY_SHAPE:       v = (uint8_t)(sizeof(kBodyBase)/sizeof(kBodyBase[0])); break;
        case P_BODY_COLOR:       v = BodyBaseCount[cfg.bodyShape]; break;
        case P_BODY_CLOTH:       v = (uint8_t)(sizeof(kClothNames)/sizeof(kClothNames[0])) + 1; break;
        case P_BODY_CLOTH_COLOR: v = cfg.bodyClothing > 0 ? ClothCounts[cfg.bodyClothing - 1] : 1; break;
        case P_HAIR_STYLE:       v = hairStyleCount + 1; break;
        case P_HAIR_COLOR:       v = (cfg.hairStyle > 0 && cfg.hairStyle - 1 < kMaxStyles) ? hairColorCounts[cfg.hairStyle - 1] : 0; break;
        case P_BANGS_STYLE:      v = bangsCount + 1; break;
        case P_SKIN_COLOR:       v = skinColorCount; break;
        case P_FACE_SHAPE:       v = faceShapeCount + 1; break;
        case P_EAR_STYLE:        v = earStyleCount + 1; break;
        case P_EYEBROW_STYLE:    v = eyebrowStyleCount + 1; break;
        case P_EYEBROW_COLOR:    v = (cfg.eyebrowStyle > 0 && cfg.eyebrowStyle - 1 < kMaxStyles) ? eyebrowColorCounts[cfg.eyebrowStyle - 1] : 0; break;
        case P_EYE_STYLE:        v = eyeStyleCount + 1; break;
        case P_EYE_COLOR:        v = (cfg.eyeStyle > 0 && cfg.eyeStyle - 1 < kMaxStyles) ? eyeColorCounts[cfg.eyeStyle - 1] : 0; break;
        case P_NOSE_STYLE:       v = noseStyleCount + 1; break;
        case P_MOUTH_STYLE:      v = mouthStyleCount + 1; break;
        case P_MOUTH_COLOR:      v = (cfg.mouthStyle > 0 && cfg.mouthStyle - 1 < kMaxStyles) ? mouthColorCounts[cfg.mouthStyle - 1] : 0; break;
        case P_ACC_BODY_STYLE:   v = (uint8_t)(sizeof(kAccBodyNames)/sizeof(kAccBodyNames[0])) + 1; break;
        case P_ACC_BODY_COLOR:   v = (cfg.accBodyStyle > 0 && cfg.accBodyStyle - 1 < (int)sizeof(AccBodyCounts)) ? AccBodyCounts[cfg.accBodyStyle - 1] : 0; break;
        case P_ACC_FACE_STYLE:   v = (uint8_t)(sizeof(kAccFaceNames)/sizeof(kAccFaceNames[0])) + 1; break;
        case P_ACC_FACE_COLOR:   v = (cfg.accFaceStyle > 0 && cfg.accFaceStyle - 1 < (int)sizeof(AccFaceCounts)) ? AccFaceCounts[cfg.accFaceStyle - 1] : 0; break;
        default:                 v = 0; break;
    }
    return v > 0 ? v : 1;
}

const char *AvatarPage::getParamLabel(int i) const
{
    switch (i) {
        case P_BG_BASE:          return "BG Type";
        case P_BG_BASE_COLOR:    return "BG Color";
        case P_BG_SHAPE:         return "BG Shape";
        case P_BG_SHAPE_COLOR:   return "Shape Color";
        case P_BODY_SHAPE:       return "Body Shape";
        case P_BODY_COLOR:       return "Body Color";
        case P_BODY_CLOTH:       return "Clothing";
        case P_BODY_CLOTH_COLOR: return "Cloth Color";
        case P_HAIR_STYLE:       return "Hair Style";
        case P_HAIR_COLOR:       return "Hair Color";
        case P_BANGS_STYLE:      return "Bangs";
        case P_SKIN_COLOR:       return "Skin Color";
        case P_FACE_SHAPE:       return "Face Shape";
        case P_EAR_STYLE:        return "Ear Style";
        case P_EYEBROW_STYLE:    return "Eyebrow";
        case P_EYEBROW_COLOR:    return "Brow Color";
        case P_EYE_STYLE:        return "Eyes";
        case P_EYE_COLOR:        return "Eye Color";
        case P_NOSE_STYLE:       return "Nose";
        case P_MOUTH_STYLE:      return "Mouth";
        case P_MOUTH_COLOR:      return "Mouth Color";
        case P_ACC_BODY_STYLE:   return "Body Acc";
        case P_ACC_BODY_COLOR:   return "Body Acc Clr";
        case P_ACC_FACE_STYLE:   return "Face Acc";
        case P_ACC_FACE_COLOR:   return "Face Acc Clr";
        default:                 return "";
    }
}

// ── view (default) ─────────────────────────────────────────────────────────────
//
//  Title bar       y =   0 .. 30
//  Avatar          y =  40 .. 200  (160 × 160, centred)
//  Subtitle        y = 212
//  [Edit Avatar]   y = 248 .. 282

static const int kViewAvSize = 160;
static const int kViewAvX    = (240 - kViewAvSize) / 2;
static const int kViewAvY    = 40;
static const int kEditBtnY   = 248;

void AvatarPage::drawView() const
{
    tft.fillScreen(TFT_BLACK);
    drawTitleBar("My Avatar", kHdrClr);

    avatar_render(tft, cfg, kViewAvX, kViewAvY, kViewAvSize, kViewAvSize);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(0x4208, TFT_BLACK);
    tft.drawString("Broadcasting to nearby devices", 120, 212);

    // Edit button
    tft.fillRoundRect(20, kEditBtnY, 200, 34, 6, 0x2945);
    tft.setTextColor(TFT_WHITE, 0x2945);
    tft.setTextSize(1);
    tft.drawString("Edit Avatar", 120, kEditBtnY + 17);

    drawDotsAndVersion();
}

// ── editor ─────────────────────────────────────────────────────────────────────
void AvatarPage::enterEdit()
{
    // Show a quick loading hint the first time since the scan takes a moment
    if (!scanned) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(0x4208, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString("Loading editor...", 120, 160);
        doScan();
    }
    mode = Mode::EDIT;
    draw();
}

void AvatarPage::drawControl(const char *label, int row, uint8_t val, uint8_t maxVal, bool hasNone) const
{
    const int y  = kCtrlY + kCtrlHdrH + row * kRowH;
    const int cy = y + kRowH / 2;

    if (row > 0)
        tft.drawLine(0, y, 239, y, 0x2104);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(0x7BEF, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(label, 4, cy);

    char valBuf[8];
    if (hasNone && val == 0)
        strcpy(valBuf, "None");
    else
        snprintf(valBuf, sizeof(valBuf), "%d", hasNone ? (int)val : (int)(val + 1));

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("<", 72, cy);
    tft.drawString(valBuf, 120, cy);
    tft.drawString(">", 168, cy);

    for (int p = 0; p < maxVal; p++) {
        int dx = 185 + p * 7;
        if (dx > 235) break;
        if (p == val)
            tft.fillCircle(dx, cy, 2, TFT_WHITE);
        else
            tft.drawCircle(dx, cy, 2, 0x4208);
    }
}

// Dice button: right of avatar, y=30..90 area
static const int kDiceBtnX = 154;
static const int kDiceBtnY = 48;
static const int kDiceBtnW = 80;
static const int kDiceBtnH = 26;

void AvatarPage::randomizeAvatar()
{
    // Set styles before their dependent color params so getParamMax is correct.
    auto rnd = [&](int idx) {
        uint8_t minV = paramRequired(idx) ? 1 : 0;
        uint8_t maxV = getParamMax(idx);
        setParam(idx, maxV > minV ? minV + random(maxV - minV) : minV);
    };

    rnd(P_BG_BASE);       rnd(P_BG_BASE_COLOR);
    rnd(P_BG_SHAPE);      if (cfg.bgShape   > 0) rnd(P_BG_SHAPE_COLOR);
    rnd(P_BODY_SHAPE);    rnd(P_BODY_COLOR);
    rnd(P_BODY_CLOTH);    if (cfg.bodyClothing > 0) rnd(P_BODY_CLOTH_COLOR);
    rnd(P_HAIR_STYLE);    if (cfg.hairStyle > 0) rnd(P_HAIR_COLOR);
    rnd(P_BANGS_STYLE);
    rnd(P_SKIN_COLOR);
    rnd(P_FACE_SHAPE);
    rnd(P_EAR_STYLE);
    rnd(P_EYEBROW_STYLE); if (cfg.eyebrowStyle > 0) rnd(P_EYEBROW_COLOR);
    rnd(P_EYE_STYLE);     if (cfg.eyeStyle > 0) rnd(P_EYE_COLOR);
    rnd(P_NOSE_STYLE);
    rnd(P_MOUTH_STYLE);   if (cfg.mouthStyle > 0) rnd(P_MOUTH_COLOR);
    rnd(P_ACC_BODY_STYLE); if (cfg.accBodyStyle > 0) rnd(P_ACC_BODY_COLOR);
    rnd(P_ACC_FACE_STYLE); if (cfg.accFaceStyle > 0) rnd(P_ACC_FACE_COLOR);

    data_write_avatar(&cfg);
}

void AvatarPage::drawEditor() const
{
    tft.fillScreen(TFT_BLACK);

    // Title bar with back arrow
    tft.fillRect(0, 0, 240, 30, kHdrClr);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, kHdrClr);
    tft.setTextSize(1);
    tft.drawString("< Back", 8, 15);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Edit Avatar", 130, 15);

    avatar_render(tft, cfg, kAvX, kAvY, kAvW, kAvH);

    // Dice roll button — to the right of the small avatar preview
    tft.fillRoundRect(kDiceBtnX, kDiceBtnY, kDiceBtnW, kDiceBtnH, 5, 0x3186);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, 0x3186);
    tft.setTextSize(1);
    tft.drawString("Randomize", kDiceBtnX + kDiceBtnW / 2, kDiceBtnY + kDiceBtnH / 2);

    // Controls panel
    tft.fillRect(0, kCtrlY, 240, DOTS_BAR_Y - kCtrlY, TFT_BLACK);
    tft.drawLine(0, kCtrlY, 239, kCtrlY, 0x2104);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xAD75, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(kPageNames[ctrlPage], 110, kCtrlY + 7);
    for (int p = 0; p < kNumCtrlPages; p++) {
        int dx = 200 + p * 7;
        if (p == ctrlPage)
            tft.fillCircle(dx, kCtrlY + 7, 2, TFT_WHITE);
        else
            tft.drawCircle(dx, kCtrlY + 7, 2, 0x4208);
    }
    tft.drawLine(0, kCtrlY + kCtrlHdrH, 239, kCtrlY + kCtrlHdrH, 0x2104);

    int start = kPageStart[ctrlPage];
    int end   = kPageEnd[ctrlPage];
    for (int i = start; i < end; i++)
        drawControl(getParamLabel(i), i - start, getParam(i), getParamMax(i), paramHasNone(i));

    drawDotsAndVersion();
}

// ── Page interface ─────────────────────────────────────────────────────────────
void AvatarPage::draw()
{
    if (mode == Mode::VIEW)
        drawView();
    else
        drawEditor();
}

void AvatarPage::onLeave()
{
    mode = Mode::VIEW;
}

void AvatarPage::onTap(int16_t x, int16_t y)
{
    if (mode == Mode::VIEW) {
        if (y >= kEditBtnY && y < kEditBtnY + 34)
            enterEdit();
        return;
    }

    // EDIT mode — back button (title bar)
    if (y < 30) {
        mode = Mode::VIEW;
        drawView();
        return;
    }

    // Tap avatar/button area (between title bar and controls)
    if (y < kCtrlY + kCtrlHdrH) {
        // Dice button
        if (x >= kDiceBtnX && x < kDiceBtnX + kDiceBtnW &&
            y >= kDiceBtnY && y < kDiceBtnY + kDiceBtnH) {
            randomizeAvatar();
            drawEditor();
            return;
        }
        // Tap elsewhere in avatar area → cycle control page
        ctrlPage = (ctrlPage + 1) % kNumCtrlPages;
        drawEditor();
        return;
    }

    int row      = (y - (kCtrlY + kCtrlHdrH)) / kRowH;
    int paramIdx = kPageStart[ctrlPage] + row;
    if (paramIdx >= kPageEnd[ctrlPage] || paramIdx >= P_COUNT)
        return;

    uint8_t val    = getParam(paramIdx);
    uint8_t maxVal = getParamMax(paramIdx);
    uint8_t minVal = paramRequired(paramIdx) ? 1 : 0;

    if (x < 120)
        val = (val <= minVal) ? maxVal - 1 : val - 1;
    else
        val = (val + 1 >= maxVal) ? minVal : val + 1;

    setParam(paramIdx, val);
    data_write_avatar(&cfg);
    avatar_render(tft, cfg, kAvX, kAvY, kAvW, kAvH);

    // Redraw only the controls panel, not the whole screen
    tft.fillRect(0, kCtrlY, 240, DOTS_BAR_Y - kCtrlY, TFT_BLACK);
    tft.drawLine(0, kCtrlY, 239, kCtrlY, 0x2104);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xAD75, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(kPageNames[ctrlPage], 110, kCtrlY + 7);
    for (int p = 0; p < kNumCtrlPages; p++) {
        int dx = 200 + p * 7;
        if (p == ctrlPage)
            tft.fillCircle(dx, kCtrlY + 7, 2, TFT_WHITE);
        else
            tft.drawCircle(dx, kCtrlY + 7, 2, 0x4208);
    }
    tft.drawLine(0, kCtrlY + kCtrlHdrH, 239, kCtrlY + kCtrlHdrH, 0x2104);
    int start = kPageStart[ctrlPage];
    int end   = kPageEnd[ctrlPage];
    for (int i = start; i < end; i++)
        drawControl(getParamLabel(i), i - start, getParam(i), getParamMax(i), paramHasNone(i));
}
