/*
  Modulo A - Enlace WiFi local
  IMPORTANTE: no escanea estando conectado. Recibe datos del scan previo.
*/
#ifndef NET_LINK_H
#define NET_LINK_H

#include "conn_profile.h"
#include "ui.h"  // WiFiAP

// Llena Modulo A usando datos del scan previo (no re-escanea WiFi)
void net_link_collect(ConnProfile* p, const WiFiAP& connected_ap);

#endif
