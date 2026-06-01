#include "storage.h"
#include "config.h"
#include "classifier.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

// Pines SD del M5Stack Cardputer ADV
// Fuente: docs.m5stack.com/en/core/Cardputer-Adv
//   CS=GPIO5, MOSI=GPIO14, MISO=GPIO39, CLK=GPIO40
#define SD_CS    5
#define SD_MOSI  14
#define SD_MISO  39
#define SD_SCK   40

extern Config g_config;
static bool s_sd_ok = false;
static uint16_t s_next_idx = 1;  // contador incremental para archivos JSON

// Busca el ultimo np_NNNN.json y devuelve siguiente indice
static uint16_t find_next_index() {
  File root = SD.open("/");
  if (!root) return 1;
  uint16_t max_idx = 0;
  File f;
  while ((f = root.openNextFile())) {
    const char* name = f.name();
    // Formato esperado: np_0001.json
    if (strncmp(name, "np_", 3) == 0) {
      uint16_t idx = 0;
      const char* p = name + 3;
      while (*p >= '0' && *p <= '9') {
        idx = idx * 10 + (*p - '0');
        p++;
      }
      if (idx > max_idx) max_idx = idx;
    }
    f.close();
  }
  root.close();
  return max_idx + 1;
}

bool storage_init() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, 25000000)) {
    s_sd_ok = false;
    return false;
  }
  s_sd_ok = true;
  s_next_idx = find_next_index();
  Serial.printf("[SD] Siguiente indice JSON: %u\n", s_next_idx);

  // Crear CSV con cabecera si no existe
  if (!SD.exists("/netprofiler.csv")) {
    File f = SD.open("/netprofiler.csv", FILE_WRITE);
    if (f) {
      f.println("ts_ms,ssid,bssid,vendor,rssi,ch,enc,ip_local,cidr,subnet_class,"
                "ip_public,asn,isp,iptype,country,mobile,cgnat,nat64,"
                "rtt_cf,rtt_g,jitter,verdict,confidence,score_m,score_f");
      f.close();
    }
  }
  return true;
}

bool storage_log_profile(const ConnProfile* p) {
  if (!s_sd_ok || !g_config.log_to_sd) return false;

  // === CSV resumen ===
  File csv = SD.open("/netprofiler.csv", FILE_APPEND);
  if (csv) {
    csv.printf("%lu,\"%s\",%s,\"%s\",%d,%d,%s,",
               (unsigned long)p->last_update_ms, p->ssid, p->bssid_str,
               p->oui_vendor, p->rssi, p->channel, p->encryption);
    csv.printf("%s,%d,\"%s\",",
               p->ip_local.toString().c_str(), p->subnet_cidr,
               p->subnet_classification);
    csv.printf("%s,%s,\"%s\",%s,%s,%d,%d,%d,",
               p->ip_public_str, p->asn, p->isp_org,
               p->ip_type, p->country,
               p->mobile_flag, p->cgnat_detected, p->nat64_detected);
    csv.printf("%d,%d,%d,%s,%d,%d,%d\n",
               p->rtt_cf_ms, p->rtt_google_ms, p->jitter_ms,
               verdict_to_str(p->verdict), p->confidence_pct,
               p->score_mobile, p->score_fixed);
    csv.close();
  }

  // === JSON individual con indice incremental (sin RTC) ===
  char fname[40];
  snprintf(fname, sizeof(fname), "/np_%04u.json", s_next_idx++);
  File js = SD.open(fname, FILE_WRITE);
  if (js) {
    JsonDocument doc;
    doc["ts"] = p->last_update_ms;
    JsonObject link = doc["link"].to<JsonObject>();
    link["ssid"] = p->ssid;
    link["bssid"] = p->bssid_str;
    link["vendor"] = p->oui_vendor;
    link["rssi"] = p->rssi;
    link["channel"] = p->channel;
    link["encryption"] = p->encryption;
    link["bssid_count"] = p->bssid_count;

    JsonObject local = doc["local"].to<JsonObject>();
    local["ip"] = p->ip_local.toString();
    local["cidr"] = p->subnet_cidr;
    local["gateway"] = p->gateway.toString();
    local["dns1"] = p->dns1.toString();
    local["dns2"] = p->dns2.toString();
    local["classification"] = p->subnet_classification;
    local["ipv6"] = p->has_ipv6;

    JsonObject inet = doc["internet"].to<JsonObject>();
    inet["available"] = p->internet_available;
    inet["ip_public"] = p->ip_public_str;
    inet["asn"] = p->asn;
    inet["isp"] = p->isp_org;
    inet["type"] = p->ip_type;
    inet["country"] = p->country;
    inet["region"] = p->region;
    inet["rdns"] = p->rdns;
    inet["mobile"] = p->mobile_flag;
    inet["cgnat"] = p->cgnat_detected;
    inet["nat64"] = p->nat64_detected;
    inet["rtt_cf_ms"] = p->rtt_cf_ms;
    inet["rtt_google_ms"] = p->rtt_google_ms;
    inet["jitter_ms"] = p->jitter_ms;

    JsonObject vrd = doc["verdict"].to<JsonObject>();
    vrd["result"] = verdict_to_str(p->verdict);
    vrd["confidence"] = p->confidence_pct;
    vrd["score_mobile"] = p->score_mobile;
    vrd["score_fixed"] = p->score_fixed;
    vrd["reasons"] = p->verdict_reasons;

    serializeJsonPretty(doc, js);
    js.close();
  }

  return true;
}

bool storage_load_config() {
  if (!s_sd_ok) return false;
  if (!SD.exists("/config.json")) return false;

  File f = SD.open("/config.json", FILE_READ);
  if (!f) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  // Override valores configurables
  if (doc.containsKey("ip_api_endpoint"))
    strncpy(g_config.ip_api_endpoint, doc["ip_api_endpoint"], sizeof(g_config.ip_api_endpoint));
  if (doc.containsKey("ping_host_1"))
    strncpy(g_config.ping_host_1, doc["ping_host_1"], sizeof(g_config.ping_host_1));
  if (doc.containsKey("ping_host_2"))
    strncpy(g_config.ping_host_2, doc["ping_host_2"], sizeof(g_config.ping_host_2));
  if (doc.containsKey("log_to_sd"))
    g_config.log_to_sd = doc["log_to_sd"];
  if (doc.containsKey("enable_imu"))
    g_config.enable_imu = doc["enable_imu"];
  if (doc.containsKey("enable_audio"))
    g_config.enable_audio = doc["enable_audio"];

  // Pesos clasificador (subobjeto scoring_weights)
  if (doc.containsKey("scoring_weights")) {
    JsonObject w = doc["scoring_weights"];
    if (w.containsKey("subnet_iphone"))     g_config.w_subnet_iphone     = w["subnet_iphone"];
    if (w.containsKey("subnet_android"))    g_config.w_subnet_android    = w["subnet_android"];
    if (w.containsKey("subnet_router"))     g_config.w_subnet_router     = w["subnet_router"];
    if (w.containsKey("asn_mobile"))        g_config.w_asn_mobile        = w["asn_mobile"];
    if (w.containsKey("iptype_mobile"))     g_config.w_iptype_mobile     = w["iptype_mobile"];
    if (w.containsKey("iptype_residential")) g_config.w_iptype_residential = w["iptype_residential"];
    if (w.containsKey("cgnat"))             g_config.w_cgnat             = w["cgnat"];
    if (w.containsKey("nat64"))             g_config.w_nat64             = w["nat64"];
    if (w.containsKey("oui_mobile"))        g_config.w_oui_mobile        = w["oui_mobile"];
    if (w.containsKey("rtt_high"))          g_config.w_rtt_high          = w["rtt_high"];
    if (w.containsKey("rtt_low_stable"))    g_config.w_rtt_low_stable    = w["rtt_low_stable"];
    if (w.containsKey("verdict_threshold")) g_config.verdict_threshold   = w["verdict_threshold"];
    Serial.println("[CFG] Pesos clasificador cargados desde SD");
  }

  return true;
}
