#include "streetpass.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <nvs.h>
#include <string.h>

// ── packet format ─────────────────────────────────────────────────────────────

#define SP_MAGIC 0x53504153u  // "SPAS"

typedef struct __attribute__((packed)) {
    uint32_t        magic;
    uint8_t         mac[6];
    avatar_config_t avatar;
} sp_packet_t;

// ── persistent store (NVS blob) ───────────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t         mac[6];
    uint16_t        count;
    avatar_config_t avatar;
} sp_stored_t;

#define SP_NVS_NS  "streetpass"
#define SP_NVS_KEY "enc2"

// ── state ─────────────────────────────────────────────────────────────────────

static sp_encounter_t s_encounters[SP_MAX_ENCOUNTERS];
static int            s_count        = 0;
static bool           s_hasNew       = false;
static unsigned long  s_lastBroadcast = 0;

// Single-slot queue filled by the ISR-context receive callback.
static volatile bool    s_pending = false;
static uint8_t          s_pendingMac[6];
static avatar_config_t  s_pendingAvatar;

static const uint8_t BROADCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ── NVS helpers ───────────────────────────────────────────────────────────────

static void nvs_load() {
    nvs_handle_t h;
    if (nvs_open(SP_NVS_NS, NVS_READONLY, &h) != ESP_OK) return;

    size_t sz = 0;
    if (nvs_get_blob(h, SP_NVS_KEY, NULL, &sz) != ESP_OK || sz == 0) {
        nvs_close(h); return;
    }

    int n = (int)(sz / sizeof(sp_stored_t));
    if (n > SP_MAX_ENCOUNTERS) n = SP_MAX_ENCOUNTERS;

    sp_stored_t *buf = (sp_stored_t *)malloc(sz);
    if (buf && nvs_get_blob(h, SP_NVS_KEY, buf, &sz) == ESP_OK) {
        s_count = n;
        for (int i = 0; i < n; i++) {
            memcpy(s_encounters[i].mac, buf[i].mac, 6);
            s_encounters[i].count     = buf[i].count;
            s_encounters[i].avatar    = buf[i].avatar;
            s_encounters[i].firstSeen = 0;
            s_encounters[i].lastSeen  = 0;
        }
    }
    free(buf);
    nvs_close(h);
}

static void nvs_save() {
    nvs_handle_t h;
    if (nvs_open(SP_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    sp_stored_t buf[SP_MAX_ENCOUNTERS];
    for (int i = 0; i < s_count; i++) {
        memcpy(buf[i].mac, s_encounters[i].mac, 6);
        buf[i].count  = s_encounters[i].count;
        buf[i].avatar = s_encounters[i].avatar;
    }
    nvs_set_blob(h, SP_NVS_KEY, buf, s_count * sizeof(sp_stored_t));
    nvs_commit(h);
    nvs_close(h);
}

// ── ESP-NOW callbacks ─────────────────────────────────────────────────────────

static void on_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (len < (int)(sizeof(uint32_t) + 6)) return;  // need at least magic + mac
    const sp_packet_t *pkt = (const sp_packet_t *)data;
    if (pkt->magic != SP_MAGIC) return;
    if (!s_pending) {
        memcpy(s_pendingMac, pkt->mac, 6);
        if (len >= (int)sizeof(sp_packet_t))
            s_pendingAvatar = pkt->avatar;
        else
            memset(&s_pendingAvatar, 0, sizeof(s_pendingAvatar));
        s_pending = true;
    }
}

static void on_send(const uint8_t *, esp_now_send_status_t) {}

// ── encounter tracking ────────────────────────────────────────────────────────

static int find_encounter(const uint8_t *mac) {
    for (int i = 0; i < s_count; i++)
        if (memcmp(s_encounters[i].mac, mac, 6) == 0) return i;
    return -1;
}

static void record_encounter(const uint8_t *mac, const avatar_config_t *avatar) {
    int idx = find_encounter(mac);
    unsigned long now = millis();

    if (idx >= 0) {
        s_encounters[idx].count++;
        s_encounters[idx].lastSeen = now;
        s_encounters[idx].avatar   = *avatar;
    } else {
        if (s_count >= SP_MAX_ENCOUNTERS) return;
        idx = s_count++;
        memcpy(s_encounters[idx].mac, mac, 6);
        s_encounters[idx].count     = 1;
        s_encounters[idx].firstSeen = now;
        s_encounters[idx].lastSeen  = now;
        s_encounters[idx].avatar    = *avatar;
    }

    s_hasNew = true;
    nvs_save();
}

static void broadcast() {
    sp_packet_t pkt;
    pkt.magic = SP_MAGIC;
    WiFi.macAddress(pkt.mac);
    data_read_avatar(&pkt.avatar);
    esp_now_send(BROADCAST_ADDR, (uint8_t *)&pkt, sizeof(pkt));
}

// ── public API ────────────────────────────────────────────────────────────────

void streetpass_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_send);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    nvs_load();

    // Broadcast immediately so nearby devices know we're here.
    s_lastBroadcast = millis();
    broadcast();

    Serial.printf("StreetPass ready. MAC: %s  Loaded %d encounters.\n",
                  WiFi.macAddress().c_str(), s_count);
}

void streetpass_tick() {
    // Process any pending receive from the callback.
    if (s_pending) {
        uint8_t mac[6];
        avatar_config_t avatar;
        memcpy(mac, s_pendingMac, 6);
        avatar = s_pendingAvatar;
        s_pending = false;

        uint8_t own[6];
        WiFi.macAddress(own);
        if (memcmp(mac, own, 6) != 0)
            record_encounter(mac, &avatar);
    }

    // Periodic broadcast.
    if (millis() - s_lastBroadcast >= SP_BROADCAST_INTERVAL_MS) {
        s_lastBroadcast = millis();
        broadcast();
    }
}

int streetpass_count() { return s_count; }

bool streetpass_get(int index, sp_encounter_t *out) {
    if (index < 0 || index >= s_count) return false;
    *out = s_encounters[index];
    return true;
}

bool streetpass_has_new()  { return s_hasNew; }
void streetpass_clear_new() { s_hasNew = false; }
