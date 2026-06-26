#pragma once

#include <stdint.h>
#include "data.h"

#define SP_BROADCAST_INTERVAL_MS 5000
#define SP_MAX_ENCOUNTERS        50

typedef struct {
    uint8_t         mac[6];
    uint32_t        firstSeen;  // millis() of first encounter this boot (0 = from NVS, not this boot)
    uint32_t        lastSeen;   // millis() of most recent encounter this boot
    uint16_t        count;      // total across all boots
    avatar_config_t avatar;     // received avatar config (all zeros if unknown)
} sp_encounter_t;

// Call once after NVS is initialized (i.e. after data_init()).
void streetpass_init();

// Call every loop iteration.
void streetpass_tick();

int  streetpass_count();
bool streetpass_get(int index, sp_encounter_t *out);

// True if a new encounter arrived since last call to streetpass_clear_new().
bool streetpass_has_new();
void streetpass_clear_new();
