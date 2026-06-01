/*
  Modulo C - Internet/Backhaul
  IP publica, ASN, CGNAT, NAT64, RTT
*/
#ifndef NET_INTERNET_H
#define NET_INTERNET_H

#include "conn_profile.h"

bool net_internet_check_reachability(ConnProfile* p);
bool net_internet_fetch_ip_intel(ConnProfile* p);
void net_internet_measure_rtt(ConnProfile* p);
void net_internet_detect_cgnat(ConnProfile* p);

// Hace todo lo anterior secuencialmente
void net_internet_collect(ConnProfile* p);

#endif
