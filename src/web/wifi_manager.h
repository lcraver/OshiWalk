#pragma once
#include <TFT_eSPI.h>
#include <WString.h>

// Connect to saved WiFi credentials; fall back to AP mode "ProxiOshi" if
// no credentials are stored or the connection times out.  Shows status on
// the TFT during the attempt.
void   wifi_init(TFT_eSPI &tft);
bool   wifi_save_credentials(const String &ssid, const String &password);
String wifi_get_ssid();   // reads saved SSID from NVS (empty string if none)

bool   wifi_is_ap_mode();
String wifi_ip();
