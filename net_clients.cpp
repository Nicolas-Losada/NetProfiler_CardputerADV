#include "net_clients.h"
#include "oui_table.h"
#include <WiFi.h>
#include <ESP32Ping.h>
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

// Leer tabla ARP de lwIP y agregar clientes encontrados
static void read_arp_table(ClientScanResult* result, IPAddress gateway) {
  // Iterar tabla ARP del stack lwIP
  for (uint8_t i = 0; i < ARP_TABLE_SIZE && result->count < MAX_CLIENTS; i++) {
    ip4_addr_t* arp_ip = nullptr;
    struct netif* arp_netif = nullptr;
    struct eth_addr* arp_mac = nullptr;

    if (etharp_get_entry(i, &arp_ip, &arp_netif, &arp_mac)) {
      if (!arp_ip || !arp_mac) continue;

      // Saltar 0.0.0.0 y broadcast
      uint32_t ip_val = ip4_addr_get_u32(arp_ip);
      if (ip_val == 0 || ip_val == 0xFFFFFFFF) continue;

      // Verificar duplicado
      IPAddress found_ip(
        (ip_val) & 0xFF,
        (ip_val >> 8) & 0xFF,
        (ip_val >> 16) & 0xFF,
        (ip_val >> 24) & 0xFF
      );

      bool dup = false;
      for (uint8_t j = 0; j < result->count; j++) {
        if (result->clients[j].ip == found_ip) { dup = true; break; }
      }
      if (dup) continue;

      NetClient& c = result->clients[result->count];
      c.ip = found_ip;
      memcpy(c.mac, arp_mac->addr, 6);
      snprintf(c.mac_str, sizeof(c.mac_str),
               "%02X:%02X:%02X:%02X:%02X:%02X",
               c.mac[0], c.mac[1], c.mac[2],
               c.mac[3], c.mac[4], c.mac[5]);

      // OUI lookup
      bool is_mobile = false;
      oui_lookup(c.mac, c.vendor, sizeof(c.vendor), &is_mobile);

      // Marcar gateway
      c.is_gateway = (found_ip == gateway);
      c.rtt_us = 0;

      result->count++;
    }
  }
}

void net_clients_scan(ClientScanResult* result, IPAddress local_ip,
                      IPAddress subnet_mask, IPAddress gateway) {
  result->count = 0;
  uint32_t t0 = millis();

  if (!WiFi.isConnected()) return;

  // Calcular rango subred
  uint32_t ip_u32 = ((uint32_t)local_ip[0]) | ((uint32_t)local_ip[1] << 8) |
                    ((uint32_t)local_ip[2] << 16) | ((uint32_t)local_ip[3] << 24);
  uint32_t mask_u32 = ((uint32_t)subnet_mask[0]) | ((uint32_t)subnet_mask[1] << 8) |
                      ((uint32_t)subnet_mask[2] << 16) | ((uint32_t)subnet_mask[3] << 24);

  uint32_t net = ip_u32 & mask_u32;
  uint32_t bcast = net | (~mask_u32);
  uint32_t host_count = (~mask_u32) & 0xFFFFFFFF;

  // Limitar scan: si subred es /16 o mayor -> solo scan primeras 254 IPs
  // Para /24 -> 254 hosts, /28 (iPhone) -> 14 hosts
  uint32_t max_hosts = host_count - 1;
  if (max_hosts > 254) max_hosts = 254;

  Serial.printf("[CLIENTS] Subnet /%d -> %d hosts a escanear\n",
                __builtin_popcount(mask_u32), max_hosts);

  // Ping sweep: enviar 1 ping rapido a cada IP
  for (uint32_t h = 1; h <= max_hosts && result->count < MAX_CLIENTS; h++) {
    uint32_t target = net + (h << 0); // host en little-endian

    // Convertir a IPAddress
    IPAddress target_ip(
      (target) & 0xFF,
      (target >> 8) & 0xFF,
      (target >> 16) & 0xFF,
      (target >> 24) & 0xFF
    );

    // Saltar nuestra propia IP
    if (target_ip == local_ip) continue;

    // Ping con timeout corto (100ms)
    bool alive = Ping.ping(target_ip, 1);

    // Yield para no watchdog
    yield();

    // Cada 16 IPs, leer ARP parcialmente (no esperar al final)
    if (h % 16 == 0) {
      read_arp_table(result, gateway);
    }
  }

  // Lectura final ARP (recoge los ultimos)
  read_arp_table(result, gateway);

  result->scan_time_ms = millis() - t0;
  Serial.printf("[CLIENTS] Encontrados: %d en %lu ms\n",
                result->count, result->scan_time_ms);
}
