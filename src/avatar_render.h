#pragma once
#include <TFT_eSPI.h>
#include "data.h"

// Composites all avatar layers into a caller-provided buffer (must be w*h uint16_t).
// Does not push to TFT. Use this for caching.
void avatar_render_to_buf(uint16_t *fb, int w, int h, const avatar_config_t &cfg);

// Convenience: allocates a temp buffer, renders, pushes to TFT, frees.
void avatar_render(TFT_eSPI &tft, const avatar_config_t &cfg, int x, int y, int w, int h);
