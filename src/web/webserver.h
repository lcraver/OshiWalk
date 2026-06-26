#pragma once
#include <stddef.h>

// Call before webserver_init() with the result of SD_MMC.begin() and whether
// a card was physically detected at all (cardType() != CARD_NONE).
void webserver_set_sd_state(bool mounted, bool present);
bool webserver_sd_mounted();
bool webserver_sd_present();

void webserver_init();

bool        webserver_pending_rescan();
bool        webserver_upload_in_progress();
bool        webserver_sd_upload_in_progress();
size_t      webserver_sd_bytes_received();
size_t      webserver_sd_upload_total();
const char *webserver_sd_upload_name();
size_t      webserver_bytes_received();    // bytes buffered so far
size_t      webserver_upload_total();      // total expected (Content-Length)
const char *webserver_upload_name();       // filename currently being uploaded
const char *webserver_last_upload_name();  // filename of the most recently completed upload
bool        webserver_pending_play(char *nameOut, size_t len); // returns true (once) when the web UI requested a specific GIF
// Write any buffered upload to the filesystem. Call from the main loop only.
// onProgress(bytesWritten, totalBytes, filename) is called after each chunk.
bool webserver_flush_upload(void (*onProgress)(size_t, size_t, const char *) = nullptr);
