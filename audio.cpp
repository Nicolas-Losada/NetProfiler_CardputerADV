// =====================================================================
// audio.cpp - Feedback ES8311 (Cardputer ADV) via M5.Speaker
// Solo se activa si g_config.enable_audio = true
// =====================================================================
#include "audio.h"
#include "config.h"
#include <M5Cardputer.h>

extern Config g_config;
static bool s_audio_ok = false;

void audio_init() {
  if (!g_config.enable_audio) {
    s_audio_ok = false;
    return;
  }
  // M5Cardputer.Speaker ya viene inicializado por M5Cardputer.begin()
  // en el ADV. Validamos que el subsistema responda.
  s_audio_ok = M5Cardputer.Speaker.isEnabled();
  if (s_audio_ok) {
    M5Cardputer.Speaker.setVolume(80);  // 0-255
    Serial.println("[AUDIO] ES8311 OK");
  } else {
    Serial.println("[AUDIO] Speaker no disponible");
  }
}

void audio_beep_done() {
  if (!s_audio_ok) return;
  M5Cardputer.Speaker.tone(1500, 80);
}

void audio_beep_verdict(Verdict v) {
  if (!s_audio_ok) return;
  switch (v) {
    case V_MOBILE:
      // Tono ascendente
      M5Cardputer.Speaker.tone(800, 100);
      delay(120);
      M5Cardputer.Speaker.tone(1200, 100);
      delay(120);
      M5Cardputer.Speaker.tone(1600, 150);
      break;
    case V_FIXED:
      // Tono descendente
      M5Cardputer.Speaker.tone(1600, 100);
      delay(120);
      M5Cardputer.Speaker.tone(1200, 100);
      delay(120);
      M5Cardputer.Speaker.tone(800, 150);
      break;
    case V_INDETERMINATE:
      M5Cardputer.Speaker.tone(1000, 200);
      break;
    default:
      break;
  }
}

void audio_beep_error() {
  if (!s_audio_ok) return;
  M5Cardputer.Speaker.tone(400, 200);
  delay(220);
  M5Cardputer.Speaker.tone(300, 200);
}
