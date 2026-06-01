/*
  Modulo A - Enlace WiFi local
*/
#ifndef NET_LINK_H
#define NET_LINK_H

#include "conn_profile.h"

// Llena Modulo A: ssid, bssid, rssi, channel, encryption, oui_vendor, bssid_count
void net_link_collect(ConnProfile* p);

#endif
