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

void net_link_collect(ConnProfile* p, const WiFiAP& connected_ap) {
  if (!p || !WiFi.isConnected()) return;

  // SSID y BSSID actuales (los reportados por WiFi)
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

  // Encryption: usar el auth_mode capturado en el scan PRE-conexion
  // NO escanear estando conectado (rompe la asociacion en ESP32-S3)
  encryption_to_str((wifi_auth_mode_t)connected_ap.auth_mode,
                    p->encryption, sizeof(p->encryption));

  // bssid_count: contado en do_scan(), no rescanear aqui
  p->bssid_count = connected_ap.ssid_count > 0 ? connected_ap.ssid_count : 1;
}
