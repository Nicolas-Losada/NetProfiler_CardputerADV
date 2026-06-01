#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\config.h"
/*
  NetProfiler - Configuracion global
  Valores por defecto; sobreescribibles via SD/config.json
*/
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

struct Config {
  // === API IP intelligence ===
  char ip_api_endpoint[64];   // "http://ip-api.com/json/?fields=..."
  char ip_api_token[64];      // opcional, p.ej. ipinfo.io
  uint16_t ip_api_timeout_ms;

  // === Destinos de ping ===
  char ping_host_1[32];       // "1.1.1.1"
  char ping_host_2[32];       // "8.8.8.8"
  uint8_t ping_count;
  uint16_t ping_timeout_ms;

  // === Servicios "what is my IP" ===
  char myip_endpoint[64];     // "http://api.ipify.org"

  // === Pesos clasificador ===
  int8_t w_subnet_iphone;     // +40 mobile
  int8_t w_subnet_android;    // +35 mobile
  int8_t w_subnet_router;     // +25 fixed
  int8_t w_asn_mobile;        // +50 mobile
  int8_t w_iptype_mobile;     // +35 mobile
  int8_t w_iptype_residential;// +30 fixed
  int8_t w_cgnat;             // +25 mobile
  int8_t w_nat64;             // +20 mobile
  int8_t w_oui_mobile;        // +15 mobile
  int8_t w_rtt_high;          // +10 mobile (rtt > 80ms)
  int8_t w_rtt_low_stable;    // +15 fixed
  int8_t verdict_threshold;   // diferencia minima para no INDETERMINADO

  // === Flags modulos opcionales ===
  bool enable_imu;
  bool enable_audio;
  bool enable_traceroute;
  bool log_to_sd;

  Config() { load_defaults(); }

  void load_defaults() {
    strcpy(ip_api_endpoint, "http://ip-api.com/json/?fields=66846719");
    ip_api_token[0] = 0;
    ip_api_timeout_ms = 5000;

    strcpy(ping_host_1, "1.1.1.1");
    strcpy(ping_host_2, "8.8.8.8");
    ping_count = 4;
    ping_timeout_ms = 1500;

    strcpy(myip_endpoint, "http://api.ipify.org");

    w_subnet_iphone = 40;
    w_subnet_android = 35;
    w_subnet_router = 25;
    w_asn_mobile = 50;
    w_iptype_mobile = 35;
    w_iptype_residential = 30;
    w_cgnat = 25;
    w_nat64 = 20;
    w_oui_mobile = 15;
    w_rtt_high = 10;
    w_rtt_low_stable = 15;
    verdict_threshold = 20;

    enable_imu = false;
    enable_audio = false;
    enable_traceroute = false;
    log_to_sd = true;
  }
};

extern Config g_config;

#endif
