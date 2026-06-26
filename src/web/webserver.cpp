#include "web/webserver.h"
#include "web/wifi_manager.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <esp_task_wdt.h>

using fs::File;

extern const char _binary_data_index_html_start[];
extern const char _binary_data_index_html_end[];

static AsyncWebServer server(80);
static File           s_sdUploadFile;
static volatile bool  s_pendingRescan    = false;
static volatile bool  s_uploadInProgress   = false;
static volatile bool  s_sdUploadInProgress = false;
static volatile size_t s_sdUploadReceived  = 0;
static volatile size_t s_sdUploadTotal     = 0;
static char            s_sdUploadName[256] = {};
static bool           s_sdMounted    = false;
static bool           s_sdPresent    = false;

// Upload buffer — lives in PSRAM so the async_tcp task only does memcpy,
// never touches the filesystem.  The main loop calls webserver_flush_upload()
// to do the actual write safely.
//
// s_uploadBufLen is volatile and both sides use __sync_synchronize() to
// prevent the dual-core cache from serving stale values across cores.
static const size_t   UPLOAD_BUF_MAX = 4 * 1024 * 1024;
static uint8_t       *s_uploadBuf    = nullptr;
static volatile size_t s_uploadBufLen    = 0;
static volatile size_t s_uploadTotal     = 0;
static char            s_uploadName[256] = {};
static volatile bool   s_uploadReady     = false;
static bool            s_preferInternal  = false;
static char            s_lastUploadName[256] = {};
static char            s_pendingPlayName[256] = {};
static volatile bool   s_playPending     = false;

void webserver_set_sd_state(bool mounted, bool present) {
    s_sdMounted = mounted;
    s_sdPresent = present;
}

// ── helpers ───────────────────────────────────────────────────────────────────

static void gifsToJson(AsyncResponseStream *s) {
    s->print('[');
    bool first = true;

    // LittleFS /gifs/
    File lfsRoot = LittleFS.open("/gifs", "r");
    if (lfsRoot) {
        File f = lfsRoot.openNextFile();
        while (f) {
            String name = f.name();
            if (!f.isDirectory() && (name.endsWith(".gif") || name.endsWith(".png"))) {
                if (!first) s->print(',');
                s->printf("{\"name\":\"%s\",\"size\":%u,\"storage\":\"lfs\"}",
                          name.c_str(), (unsigned)f.size());
                first = false;
            }
            f = lfsRoot.openNextFile();
        }
    }

    // SD /gifs/
    if (s_sdMounted) {
        if (!SD_MMC.exists("/gifs")) SD_MMC.mkdir("/gifs");
        File sdRoot = SD_MMC.open("/gifs", "r");
        if (sdRoot) {
            File f = sdRoot.openNextFile();
            while (f) {
                String name = f.name();
                if (!f.isDirectory() && (name.endsWith(".gif") || name.endsWith(".png"))) {
                    if (!first) s->print(',');
                    s->printf("{\"name\":\"%s\",\"size\":%llu,\"storage\":\"sd\"}",
                              name.c_str(), (unsigned long long)f.size());
                    first = false;
                }
                f = sdRoot.openNextFile();
            }
        }
    }

    s->print(']');
}

// ── public API ────────────────────────────────────────────────────────────────

void webserver_init() {
    if (!LittleFS.exists("/gifs"))
        LittleFS.mkdir("/gifs");

    s_uploadBuf = (uint8_t *)ps_malloc(UPLOAD_BUF_MAX);
    if (!s_uploadBuf)
        Serial.println("webserver: WARNING — failed to allocate upload buffer in PSRAM");

    // ── index page (embedded in firmware) ────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        size_t len = _binary_data_index_html_end - _binary_data_index_html_start;
        AsyncWebServerResponse *res = req->beginResponse_P(
            200, "text/html",
            (const uint8_t *)_binary_data_index_html_start, len);
        req->send(res);
    });

    // ── API: info ─────────────────────────────────────────────────────────────
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["ap"] = wifi_is_ap_mode();
        doc["ip"] = wifi_ip();
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── API: storage space ────────────────────────────────────────────────────
    server.on("/api/space", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["lfs_total"] = LittleFS.totalBytes();
        doc["lfs_used"]  = LittleFS.usedBytes();
        doc["sd_mounted"] = s_sdMounted;
        doc["sd_present"] = s_sdPresent;
        if (s_sdMounted) {
            doc["sd_total"] = SD_MMC.totalBytes();
            doc["sd_used"]  = SD_MMC.usedBytes();
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── API: SD file list ─────────────────────────────────────────────────────
    server.on("/api/sdfiles", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncResponseStream *res = req->beginResponseStream("application/json");
        res->print('[');
        if (s_sdMounted) {
            bool first = true;
            File root = SD_MMC.open("/");
            File f    = root.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    if (!first) res->print(',');
                    String name = f.name();
                    name.replace("\"", "\\\"");
                    res->printf("{\"name\":\"%s\",\"size\":%llu}",
                                name.c_str(), (unsigned long long)f.size());
                    first = false;
                }
                f = root.openNextFile();
            }
        }
        res->print(']');
        req->send(res);
    });

    // ── SD download ───────────────────────────────────────────────────────────
    server.on("/sddownload", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!s_sdMounted) { req->send(503, "text/plain", "SD not mounted"); return; }
        if (!req->hasParam("name")) { req->send(400, "text/plain", "Missing name"); return; }
        String name = req->getParam("name")->value();
        if (name.indexOf("..") >= 0) { req->send(400, "text/plain", "Invalid name"); return; }
        String path = "/" + name;
        if (!SD_MMC.exists(path)) { req->send(404, "text/plain", "Not found"); return; }
        req->send(SD_MMC, path, "application/octet-stream", true);
    });

    // ── SD upload ─────────────────────────────────────────────────────────────
    server.on("/sdupload", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            req->send(200, "text/plain", "OK");
        },
        [](AsyncWebServerRequest *req, const String &filename,
           size_t index, uint8_t *data, size_t len, bool final) {

            if (!s_sdMounted) {
                if (final) req->send(503, "text/plain", "SD not mounted");
                return;
            }

            if (index == 0) {
                uint64_t free = SD_MMC.totalBytes() - SD_MMC.usedBytes();
                if (free < 64 * 1024) {
                    if (final) req->send(507, "text/plain", "SD card full");
                    return;
                }
                strlcpy(s_sdUploadName, filename.c_str(), sizeof(s_sdUploadName));
                s_sdUploadReceived  = 0;
                s_sdUploadTotal     = req->contentLength();
                s_sdUploadInProgress = true;
                String path = "/" + filename;
                s_sdUploadFile = SD_MMC.open(path, FILE_WRITE);
                if (!s_sdUploadFile)
                    Serial.printf("webserver: SD open failed: %s\n", path.c_str());
            }

            if (s_sdUploadFile)
                s_sdUploadFile.write(data, len);
            s_sdUploadReceived += len;

            if (final) {
                if (s_sdUploadFile) {
                    s_sdUploadFile.close();
                    Serial.printf("webserver: SD uploaded %s\n", filename.c_str());
                }
                s_sdUploadInProgress = false;
            }
        }
    );

    // ── SD format ─────────────────────────────────────────────────────────────
    server.on("/sdformat", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (s_sdMounted) { req->send(400, "text/plain", "Already mounted — no need to format"); return; }
        Serial.println("webserver: formatting SD card…");
        SD_MMC.end();
        if (SD_MMC.begin("/sdcard", true, true)) {  // third arg = format_if_failed
            s_sdMounted = true;
            s_sdPresent = true;
            Serial.println("webserver: SD format + mount OK");
            req->send(200, "text/plain", "OK");
        } else {
            Serial.println("webserver: SD format failed");
            req->send(500, "text/plain", "Format failed — check the card");
        }
    });

    // ── SD delete ─────────────────────────────────────────────────────────────
    server.on("/sddelete", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!s_sdMounted) { req->send(503, "text/plain", "SD not mounted"); return; }
        if (!req->hasParam("name", true)) { req->send(400, "text/plain", "Missing name"); return; }
        String name = req->getParam("name", true)->value();
        if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
            req->send(400, "text/plain", "Invalid name"); return;
        }
        String path = "/" + name;
        if (SD_MMC.exists(path)) {
            SD_MMC.remove(path);
            Serial.printf("webserver: SD deleted %s\n", path.c_str());
            req->send(200, "text/plain", "OK");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

    // ── play a specific GIF ───────────────────────────────────────────────────
    server.on("/play", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("name", true)) { req->send(400, "text/plain", "Missing name"); return; }
        strlcpy(s_pendingPlayName, req->getParam("name", true)->value().c_str(), sizeof(s_pendingPlayName));
        __sync_synchronize();
        s_playPending = true;
        req->send(200, "text/plain", "OK");
    });

    // ── API: storage preference ───────────────────────────────────────────────
    server.on("/api/storage-pref", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["internal"] = s_preferInternal;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/api/storage-pref", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (req->hasParam("internal", true))
            s_preferInternal = req->getParam("internal", true)->value() == "1";
        req->send(200, "text/plain", "OK");
    });

    // ── API: upload status ────────────────────────────────────────────────────
    // Browser polls this after uploading to know when the main loop has finished
    // flushing the PSRAM buffer to the filesystem.
    server.on("/api/upload-status", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["flushed"] = !s_uploadInProgress && !s_uploadReady;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // ── API: gif list ─────────────────────────────────────────────────────────
    server.on("/api/gifs", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncResponseStream *res = req->beginResponseStream("application/json");
        gifsToJson(res);
        req->send(res);
    });

    // ── upload ────────────────────────────────────────────────────────────────
    // The async_tcp task ONLY copies incoming bytes into a PSRAM buffer here.
    // No filesystem calls happen on the async task.
    // webserver_flush_upload() is called by the main loop to do the actual write.
    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            req->send(200, "text/plain", "OK");
        },
        [](AsyncWebServerRequest *req, const String &filename,
           size_t index, uint8_t *data, size_t len, bool final) {

            if (!filename.endsWith(".gif") && !filename.endsWith(".png")) {
                if (final) req->send(400, "text/plain", "Only .gif and .png files are accepted");
                return;
            }

            if (!s_uploadBuf) {
                if (final) req->send(500, "text/plain", "Upload buffer not allocated");
                return;
            }

            if (index == 0) {
                strlcpy(s_uploadName, filename.c_str(), sizeof(s_uploadName));
                s_uploadBufLen     = 0;
                s_uploadTotal      = req->contentLength(); // approximate — includes multipart overhead
                s_uploadReady      = false;
                s_uploadInProgress = true;
            }

            if (s_uploadBufLen + len > UPLOAD_BUF_MAX) {
                if (final) req->send(507, "text/plain", "File too large (max 4 MB)");
                s_uploadInProgress = false;
                return;
            }
            memcpy(s_uploadBuf + s_uploadBufLen, data, len);
            s_uploadBufLen += len;

            if (final) {
                // Memory barrier: ensure s_uploadBuf contents and s_uploadBufLen are
                // visible to core 1 before the main loop sees s_uploadReady = true.
                __sync_synchronize();
                s_uploadReady = true;
            }
        }
    );

    // ── delete ────────────────────────────────────────────────────────────────
    server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("name", true)) { req->send(400, "text/plain", "Missing name"); return; }
        String name    = req->getParam("name", true)->value();
        String storage = req->hasParam("storage", true) ? req->getParam("storage", true)->value() : "lfs";
        if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
            req->send(400, "text/plain", "Invalid name"); return;
        }
        String path = "/gifs/" + name;
        bool removed = false;
        if (storage == "sd" && s_sdMounted) {
            removed = SD_MMC.exists(path) && SD_MMC.remove(path);
        } else {
            removed = LittleFS.exists(path) && LittleFS.remove(path);
        }
        if (removed) {
            s_pendingRescan = true;
            Serial.printf("webserver: deleted %s from %s\n", path.c_str(), storage.c_str());
            req->send(200, "text/plain", "OK");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

    // ── save wifi credentials and restart ────────────────────────────────────
    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
        String ssid     = req->hasParam("ssid",     true) ? req->getParam("ssid",     true)->value() : "";
        String password = req->hasParam("password", true) ? req->getParam("password", true)->value() : "";

        if (wifi_save_credentials(ssid, password)) {
            req->send(200, "text/plain", "OK");
            delay(300);
            ESP.restart();
        } else {
            req->send(500, "text/plain", "Failed to save credentials");
        }
    });

    server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.printf("webserver: listening on http://%s\n", wifi_ip().c_str());
}

bool        webserver_sd_mounted()       { return s_sdMounted; }
bool        webserver_sd_present()       { return s_sdPresent; }
size_t      webserver_bytes_received()   { return s_uploadBufLen; }
size_t      webserver_upload_total()     { return s_uploadTotal; }
const char *webserver_upload_name()      { return s_uploadName; }
const char *webserver_last_upload_name() { return s_lastUploadName; }

bool webserver_pending_play(char *nameOut, size_t len) {
    if (!s_playPending) return false;
    __sync_synchronize();
    s_playPending = false;
    strlcpy(nameOut, s_pendingPlayName, len);
    return true;
}

bool webserver_flush_upload(void (*onProgress)(size_t, size_t, const char *)) {
    if (!s_uploadReady) return false;
    // Memory barrier: ensure we read the s_uploadBuf and s_uploadBufLen values
    // that core 0 wrote, not whatever is in core 1's cache.
    __sync_synchronize();
    s_uploadReady = false;

    size_t bufLen = s_uploadBufLen; // snapshot after barrier
    Serial.printf("webserver: flushing %s — %u bytes\n", s_uploadName, bufLen);

    // Decide destination now that we know the actual file size.
    size_t lfsFree = LittleFS.totalBytes() - LittleFS.usedBytes();
    bool toSd;
    if (s_sdMounted && !s_preferInternal) {
        // SD preferred (default) — always use SD when it's available
        toSd = true;
    } else if (bufLen <= lfsFree) {
        // Internal preferred and it fits
        toSd = false;
    } else if (s_sdMounted) {
        // Internal preferred but doesn't fit — spill to SD
        toSd = true;
        Serial.printf("webserver: LittleFS too full (%u free, need %u) — spilling to SD\n", lfsFree, bufLen);
    } else {
        Serial.println("webserver: not enough space and no SD card");
        s_uploadInProgress = false;
        return false;
    }

    String path = "/gifs/" + String(s_uploadName);
    File f;
    if (!toSd) {
        f = LittleFS.open(path, "w");
    } else {
        if (!SD_MMC.exists("/gifs")) SD_MMC.mkdir("/gifs");
        f = SD_MMC.open(path, FILE_WRITE);
    }

    bool ok = false;
    if (f) {
        const size_t CHUNK = 4096;

        // SD_MMC DMA cannot read from PSRAM — bounce through an internal-DRAM buffer.
        uint8_t *dmaBuf = toSd
            ? (uint8_t *)heap_caps_malloc(CHUNK, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
            : nullptr;

        size_t written = 0;
        while (written < bufLen) {
            size_t chunk = min(CHUNK, bufLen - written);
            if (dmaBuf) {
                memcpy(dmaBuf, s_uploadBuf + written, chunk);
                f.write(dmaBuf, chunk);
            } else {
                f.write(s_uploadBuf + written, chunk);
            }
            written += chunk;
            esp_task_wdt_reset();
            if (onProgress) onProgress(written, bufLen, s_uploadName);
            delay(1);
        }

        if (dmaBuf) heap_caps_free(dmaBuf);
        f.close();
        ok = true;
        strlcpy(s_lastUploadName, s_uploadName, sizeof(s_lastUploadName));
        Serial.printf("webserver: wrote %u bytes to %s\n",
                      bufLen, toSd ? "SD" : "LittleFS");
    } else {
        Serial.printf("webserver: failed to open %s for write\n", path.c_str());
    }

    s_uploadInProgress = false;
    if (ok) s_pendingRescan = true;
    return ok;
}

bool webserver_pending_rescan() {
    if (!s_pendingRescan) return false;
    s_pendingRescan = false;
    return true;
}

bool webserver_sd_upload_in_progress() { return s_sdUploadInProgress; }
size_t webserver_sd_bytes_received()   { return s_sdUploadReceived; }
size_t webserver_sd_upload_total()     { return s_sdUploadTotal; }
const char *webserver_sd_upload_name() { return s_sdUploadName; }

bool webserver_upload_in_progress() {
    return s_uploadInProgress;
}
