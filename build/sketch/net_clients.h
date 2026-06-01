#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\net_clients.h"
/*
  Descubrimiento de dispositivos en red local via ARP scan
  Ping sweep + lectura tabla ARP de lwIP
*/
#ifndef NET_CLIENTS_H
#define NET_CLIENTS_H

#include <Arduino.h>
#include <IPAddress.h>

#define MAX_CLIENTS 32

struct NetClient {
  IPAddress ip;
  uint8_t mac[6];
  char mac_str[18];    // "AA:BB:CC:DD:EE:FF"
  char vendor[24];     // OUI lookup
  bool is_gateway;
  uint16_t rtt_us;     // microsegundos respuesta
};

struct ClientScanResult {
  NetClient clients[MAX_CLIENTS];
  uint8_t count;
  uint32_t scan_time_ms;
};

// Escanea subred: ping sweep + ARP table read
// Requiere WiFi conectado. Bloqueante (~10-30s dependiendo subred)
void net_clients_scan(ClientScanResult* result, IPAddress local_ip,
                      IPAddress subnet_mask, IPAddress gateway);

#endif
