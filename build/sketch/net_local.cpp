#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\net_local.cpp"
#include "net_local.h"
#include <WiFi.h>

static uint8_t mask_to_cidr(IPAddress mask) {
  uint32_t m = ((uint32_t)mask[0] << 24) | ((uint32_t)mask[1] << 16) |
               ((uint32_t)mask[2] << 8)  |  (uint32_t)mask[3];
  uint8_t cidr = 0;
  while (m & 0x80000000UL) { cidr++; m <<= 1; }
  return cidr;
}

// Clasificacion subred (uno de los discriminadores mas fuertes)
// 172.20.10.0/28 -> iPhone hotspot
// 192.168.43.0/24 -> Android hotspot legacy
// 192.168.x / 10.x -> Router fijo
static void classify_subnet(const IPAddress& ip, uint8_t cidr, char* out, size_t n) {
  // iPhone hotspot: 172.20.10.0/28
  if (ip[0] == 172 && ip[1] == 20 && ip[2] == 10 && cidr >= 28) {
    strncpy(out, "iPhone hotspot", n - 1);
  }
  // Android hotspot legacy
  else if (ip[0] == 192 && ip[1] == 168 && ip[2] == 43) {
    strncpy(out, "Android hotspot", n - 1);
  }
  // Android moderno: 192.168.x con x raro o 192.168.0.x sin gateway clasico
  else if (ip[0] == 192 && ip[1] == 168) {
    strncpy(out, "Router/Hotspot", n - 1);
  }
  // 10.x private
  else if (ip[0] == 10) {
    strncpy(out, "Router/Corp", n - 1);
  }
  // 172.16-31 (no iPhone)
  else if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) {
    strncpy(out, "Router privado", n - 1);
  }
  else {
    strncpy(out, "Desconocida", n - 1);
  }
  out[n - 1] = 0;
}

void net_local_collect(ConnProfile* p) {
  if (!p || !WiFi.isConnected()) return;

  p->ip_local = WiFi.localIP();
  p->gateway = WiFi.gatewayIP();
  p->dns1 = WiFi.dnsIP(0);
  p->dns2 = WiFi.dnsIP(1);
  p->subnet_mask = WiFi.subnetMask();
  p->subnet_cidr = mask_to_cidr(p->subnet_mask);

  classify_subnet(p->ip_local, p->subnet_cidr,
                  p->subnet_classification,
                  sizeof(p->subnet_classification));

  // IPv6: comprobar si hay direccion IPv6 link-local valida
  // En core 3.x el tipo cambio; usamos IPAddress generico
  IPAddress v6_addr = WiFi.linkLocalIPv6();
  p->has_ipv6 = ((uint32_t)v6_addr != 0);
}
