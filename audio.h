/*
  Audio - feedback sonoro opcional (ES8311 via M5.Speaker)
  Activable via config.json -> enable_audio
*/
#ifndef AUDIO_H
#define AUDIO_H

#include "conn_profile.h"

void audio_init();
void audio_beep_done();              // beep corto fin analisis
void audio_beep_verdict(Verdict v);  // tono segun veredicto
void audio_beep_error();             // beep error

#endif
