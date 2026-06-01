#include "net_link.h"
#include "oui_table.h"
#include <WiFi.h>

static void encryption_to_str(wifi_auth_mode_t auth, char* out, size_t n) {
  const char* s;
  switch (auth) {
    case WIFI_AUTH_OPEN:            s = "OPEN"; break;
    case WIFI_AUTH_WEP:             s = "WEP"; break;
    case WIFI_AUTH_WPA_PSK:         s = "WPA"; break;
    case WIFI_AUTH_WPA2_PSK:        s = "WPA2"; break;
    case WIFI_AUTH_WPA_WPA2_PSK:    s = "WPA/2"; break;
    case WIFI_AUTH_WPA2_ENTERPRISE: s = "WPA2-E"; break;
    case WIFI_AUTH_WPA3_PSK:        s = "WPA3"; break;
    case WIFI_AUTH_WPA2_WPA3_PSK:   s = "WPA2/3"; break;
    default:                        s = "?"; break;
  }
  strncpy(out, s, n - 1);
  out[n - 1] = 0;
}

void net_link_collect(ConnProfile* p) {
  if (!p || !WiFi.isConnected()) return;

  String ssid = WiFi.SSID();
  strncpy(p->ssid, ssid.c_str(), sizeof(p->ssid) - 1);
  p->ssid[sizeof(p->ssid) - 1] = 0;

  uint8_t* bssid = WiFi.BSSID();
  if (bssid) {
    memcpy(p->bssid, bssid, 6);
    snprintf(p->bssid_str, sizeof(p->bssid_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    bool is_mobile;
    oui_lookup(bssid, p->oui_vendor, sizeof(p->oui_vendor), &is_mobile);
  }

  p->rssi = WiFi.RSSI();
  p->channel = WiFi.channel();

  // Encryption: scan momentaneo del SSID conectado para obtener auth
  // (core 3.x no expone WiFi.getEncryption() en cliente, se obtiene via scan)
  int n = WiFi.scanNetworks(false, false, false, 250U);
  uint8_t same_ssid = 0;
  bool enc_set = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == ssid) {
      same_ssid++;
      if (!enc_set) {
        encryption_to_str(WiFi.encryptionType(i), p->encryption, sizeof(p->encryption));
        enc_set = true;
      }
    }
  }
  if (!enc_set) strncpy(p->encryption, "?", sizeof(p->encryption));
  if (same_ssid == 0) same_ssid = 1;
  p->bssid_count = same_ssid;
  WiFi.scanDelete();
}
