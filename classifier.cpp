#include "classifier.h"
#include "config.h"
#include "oui_table.h"
#include <Arduino.h>

extern Config g_config;

const char* verdict_to_str(Verdict v) {
  switch (v) {
    case V_MOBILE:        return "CELULAR";
    case V_FIXED:         return "ISP FIJO";
    case V_INDETERMINATE: return "INDETERMINADO";
    default:              return "DESCONOCIDO";
  }
}

static void append_reason(char* dst, size_t n, const char* fmt, ...) {
  size_t len = strlen(dst);
  if (len + 4 >= n) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(dst + len, n - len, fmt, ap);
  va_end(ap);
  size_t nl = strlen(dst);
  if (nl + 2 < n) { dst[nl] = '\n'; dst[nl+1] = 0; }
}

void classifier_run(ConnProfile* p) {
  p->score_mobile = 0;
  p->score_fixed = 0;
  p->verdict_reasons[0] = 0;

  // === Subred local (uno de los discriminadores mas fuertes) ===
  if (strstr(p->subnet_classification, "iPhone")) {
    p->score_mobile += g_config.w_subnet_iphone;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d Subnet iPhone /28", g_config.w_subnet_iphone);
  }
  else if (strstr(p->subnet_classification, "Android")) {
    p->score_mobile += g_config.w_subnet_android;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d Subnet Android", g_config.w_subnet_android);
  }
  else if (strstr(p->subnet_classification, "Router")) {
    p->score_fixed += g_config.w_subnet_router;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d Subnet Router", g_config.w_subnet_router);
  }

  // === IP intel (mas fuerte) ===
  if (p->mobile_flag) {
    p->score_mobile += g_config.w_asn_mobile;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d ASN movil", g_config.w_asn_mobile);
  }

  if (strcmp(p->ip_type, "mobile") == 0) {
    p->score_mobile += g_config.w_iptype_mobile;
  }
  else if (strcmp(p->ip_type, "residential") == 0) {
    p->score_fixed += g_config.w_iptype_residential;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d IP residencial", g_config.w_iptype_residential);
  }

  // === CGNAT ===
  if (p->cgnat_detected) {
    p->score_mobile += g_config.w_cgnat;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d CGNAT (100.64/10)", g_config.w_cgnat);
  }

  // === NAT64 ===
  if (p->nat64_detected) {
    p->score_mobile += g_config.w_nat64;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d NAT64", g_config.w_nat64);
  }

  // === OUI BSSID ===
  bool is_mobile_oui = false;
  if (p->oui_vendor[0]) {
    uint8_t bssid[6];
    memcpy(bssid, p->bssid, 6);
    char tmp[32];
    oui_lookup(bssid, tmp, sizeof(tmp), &is_mobile_oui);
    if (is_mobile_oui) {
      p->score_mobile += g_config.w_oui_mobile;
      append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                    "+%d OUI movil (%s)", g_config.w_oui_mobile, p->oui_vendor);
    }
  }

  // === RTT/Jitter ===
  if (p->rtt_cf_ms > 80 || p->jitter_ms > 30) {
    p->score_mobile += g_config.w_rtt_high;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d RTT alto/jitter", g_config.w_rtt_high);
  }
  else if (p->rtt_cf_ms > 0 && p->rtt_cf_ms < 30 && p->jitter_ms < 10) {
    p->score_fixed += g_config.w_rtt_low_stable;
    append_reason(p->verdict_reasons, sizeof(p->verdict_reasons),
                  "+%d RTT bajo estable", g_config.w_rtt_low_stable);
  }

  // === Veredicto ===
  int16_t diff = p->score_mobile - p->score_fixed;
  if (diff > g_config.verdict_threshold) {
    p->verdict = V_MOBILE;
  }
  else if (diff < -g_config.verdict_threshold) {
    p->verdict = V_FIXED;
  }
  else {
    p->verdict = V_INDETERMINATE;
  }

  // Confianza: |diff| / suma_total * 100, cap 100
  int16_t total = p->score_mobile + p->score_fixed;
  if (total <= 0) {
    p->confidence_pct = 0;
  } else {
    int32_t conf = (int32_t)abs(diff) * 100 / total;
    if (conf > 100) conf = 100;
    p->confidence_pct = (uint8_t)conf;
  }
}
