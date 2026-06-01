#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\conn_profile.h"
/*
  NetProfiler - Estructura compartida ConnProfile
  Resultados de los modulos A/B/C/D + veredicto del clasificador
*/
#ifndef CONN_PROFILE_H
#define CONN_PROFILE_H

#include <Arduino.h>
#include <IPAddress.h>

enum Verdict {
  V_UNKNOWN = 0,
  V_MOBILE = 1,
  V_FIXED = 2,
  V_INDETERMINATE = 3
};

struct ConnProfile {
  // === Estado general ===
  bool wifi_connected;
  bool internet_available;
  uint32_t last_update_ms;

  // === Modulo A: Enlace WiFi ===
  char ssid[33];
  uint8_t bssid[6];
  char bssid_str[18];        // "AA:BB:CC:DD:EE:FF"
  int8_t rssi;
  uint8_t channel;
  uint8_t bssid_count;       // cuantos BSSID comparten el SSID
  char encryption[12];       // "OPEN","WPA2","WPA3","WPA2/3"
  char oui_vendor[32];       // fabricante derivado de OUI

  // === Modulo B: Capa local ===
  IPAddress ip_local;
  IPAddress gateway;
  IPAddress dns1;
  IPAddress dns2;
  IPAddress subnet_mask;
  uint8_t subnet_cidr;       // 28, 24, 16, etc
  char subnet_classification[24]; // "iPhone hotspot", "Android hotspot", "Router"
  bool has_ipv6;

  // === Modulo C: Internet ===
  IPAddress ip_public;
  char ip_public_str[40];    // soporta IPv6 textual
  char asn[16];              // "AS12345"
  char isp_org[64];
  char ip_type[20];          // "residential","mobile","hosting","business"
  char country[4];
  char region[32];
  char rdns[64];
  bool mobile_flag;
  bool cgnat_detected;       // IP publica en 100.64.0.0/10
  bool nat64_detected;       // prefijo 64:ff9b::/96
  uint16_t rtt_cf_ms;        // 1.1.1.1
  uint16_t rtt_google_ms;    // 8.8.8.8
  uint16_t jitter_ms;

  // === Modulo D: Sondas ===
  bool icmp_works;
  bool captive_portal;

  // === Veredicto ===
  Verdict verdict;
  uint8_t confidence_pct;
  int16_t score_mobile;      // suma pesos
  int16_t score_fixed;       // suma pesos
  char verdict_reasons[256]; // razones legibles

  ConnProfile() {
    reset();
  }

  void reset() {
    memset(this, 0, sizeof(ConnProfile));
    verdict = V_UNKNOWN;
  }
};

// Estados maquina principal
enum AppState {
  ST_BOOT,
  ST_SCAN,           // listado redes WiFi
  ST_PASSWORD,       // entrada contrasena
  ST_CONNECTING,
  ST_ANALYZING,      // ejecuta modulos A/B/C/D
  ST_REPORT,         // muestra pestañas
  ST_ERROR
};

// Pestañas del reporte
enum ReportTab {
  TAB_VERDICT = 0,
  TAB_LINK = 1,
  TAB_LOCAL = 2,
  TAB_INTERNET = 3,
  TAB_PROBES = 4,
  TAB_CLIENTS = 5,
  TAB_COUNT = 6
};

#endif
