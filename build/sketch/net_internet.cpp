#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\net_internet.cpp"
#include "net_internet.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>

extern Config g_config;

// === Detector CGNAT: IP en 100.64.0.0/10 (RFC 6598) ===
static bool is_cgnat_range(const IPAddress& ip) {
  // 100.64.0.0/10 == 100.64.0.0 a 100.127.255.255
  return (ip[0] == 100) && (ip[1] >= 64) && (ip[1] <= 127);
}

bool net_internet_check_reachability(ConnProfile* p) {
  if (!WiFi.isConnected()) {
    p->internet_available = false;
    return false;
  }
  // Ping rapido a 1.1.1.1 para confirmar internet
  bool ok = Ping.ping(IPAddress(1,1,1,1), 1);
  p->internet_available = ok;
  p->icmp_works = ok;
  return ok;
}

bool net_internet_fetch_ip_intel(ConnProfile* p) {
  if (!p->internet_available) return false;

  HTTPClient http;
  http.setTimeout(g_config.ip_api_timeout_ms);

  // ip-api.com plan free: HTTP solo, ~45 req/min
  // fields=66846719 incluye: status,country,countryCode,region,regionName,
  //   city,zip,lat,lon,timezone,isp,org,as,asname,reverse,mobile,proxy,hosting,query
  if (!http.begin(g_config.ip_api_endpoint)) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  // Stream parsing con filtro para ahorrar RAM (sin PSRAM)
  StaticJsonDocument<256> filter;
  filter["query"] = true;
  filter["as"] = true;
  filter["asname"] = true;
  filter["isp"] = true;
  filter["org"] = true;
  filter["mobile"] = true;
  filter["hosting"] = true;
  filter["country"] = true;
  filter["countryCode"] = true;
  filter["regionName"] = true;
  filter["reverse"] = true;

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filter));
  http.end();
  if (err) return false;

  const char* query = doc["query"] | "";
  const char* as_str = doc["as"] | "";
  const char* isp = doc["isp"] | (doc["org"] | "");
  bool mobile = doc["mobile"] | false;
  bool hosting = doc["hosting"] | false;
  const char* country = doc["countryCode"] | "";
  const char* region = doc["regionName"] | "";
  const char* reverse = doc["reverse"] | "";

  strncpy(p->ip_public_str, query, sizeof(p->ip_public_str) - 1);
  p->ip_public.fromString(query);

  // Extraer "AS12345" del campo as completo "AS12345 Provider Name"
  if (strncmp(as_str, "AS", 2) == 0) {
    const char* sp = strchr(as_str, ' ');
    size_t len = sp ? (size_t)(sp - as_str) : strlen(as_str);
    if (len >= sizeof(p->asn)) len = sizeof(p->asn) - 1;
    memcpy(p->asn, as_str, len);
    p->asn[len] = 0;
  }

  strncpy(p->isp_org, isp, sizeof(p->isp_org) - 1);
  strncpy(p->country, country, sizeof(p->country) - 1);
  strncpy(p->region, region, sizeof(p->region) - 1);
  strncpy(p->rdns, reverse, sizeof(p->rdns) - 1);

  p->mobile_flag = mobile;

  // ip_type heuristica
  if (mobile) strcpy(p->ip_type, "mobile");
  else if (hosting) strcpy(p->ip_type, "hosting");
  else strcpy(p->ip_type, "residential");

  // CGNAT por API: ip-api no lo marca explicito, lo detectamos por rango
  p->cgnat_detected = is_cgnat_range(p->ip_public);

  return true;
}

void net_internet_measure_rtt(ConnProfile* p) {
  if (!p->internet_available) return;

  IPAddress cf; cf.fromString(g_config.ping_host_1);
  IPAddress gg; gg.fromString(g_config.ping_host_2);

  uint32_t total_cf = 0, total_gg = 0;
  uint8_t ok_cf = 0, ok_gg = 0;
  uint16_t samples[8] = {0};
  uint8_t sample_count = 0;

  for (uint8_t i = 0; i < g_config.ping_count; i++) {
    if (Ping.ping(cf, 1)) {
      uint32_t t = Ping.averageTime();
      total_cf += t;
      ok_cf++;
      if (sample_count < 8) samples[sample_count++] = t;
    }
    if (Ping.ping(gg, 1)) {
      total_gg += Ping.averageTime();
      ok_gg++;
    }
  }
  p->rtt_cf_ms = ok_cf ? (total_cf / ok_cf) : 0;
  p->rtt_google_ms = ok_gg ? (total_gg / ok_gg) : 0;

  // Jitter: max-min de muestras CF
  if (sample_count >= 2) {
    uint16_t mn = samples[0], mx = samples[0];
    for (uint8_t i = 1; i < sample_count; i++) {
      if (samples[i] < mn) mn = samples[i];
      if (samples[i] > mx) mx = samples[i];
    }
    p->jitter_ms = mx - mn;
  }
}

void net_internet_detect_cgnat(ConnProfile* p) {
  // CGNAT-rango ya detectado en fetch_ip_intel
  // Aqui podriamos hacer un traceroute para ver saltos en 100.64/10 entre
  // gateway local e IP publica. Implementacion futura (lwIP raw).
  // Por ahora, basta con el flag.
}

void net_internet_collect(ConnProfile* p) {
  if (!net_internet_check_reachability(p)) {
    return;
  }
  net_internet_fetch_ip_intel(p);
  net_internet_measure_rtt(p);
  net_internet_detect_cgnat(p);
}
