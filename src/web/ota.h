#pragma once
#include <TFT_eSPI.h>

// Fetch version.json from GitHub, compare to BUILD_NUMBER, and flash if newer.
// Shows progress on the TFT. Reboots on success; returns on no-update or error.
void ota_check(TFT_eSPI &tft);
