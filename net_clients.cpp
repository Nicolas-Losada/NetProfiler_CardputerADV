#include "net_clients.h"
#include "oui_table.h"
#include <WiFi.h>
#include <ESP32Ping.h>
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

static const uint32_t SCAN_BUDGET_MS = 20000;  // 20s maximo total
static const uint32_t PING_BUDGET_HOSTS = 60;  // max hosts en fase ping para subnets grandes

// Helper unificado: IP a u32 big-endian (byte 0 en bits altos)
static inline uint32_t ip_to_u32_be(const IPAddress& ip) {
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
         ((uint32_t)ip[2] << 8)  |  (uint32_t)ip[3];
}

static inline IPAddress u32_be_to_ip(uint32_t v) {
  return IPAddress((v >> 24) & 0xFF, (v >> 16) & 0xFF,
                   (v >> 8) & 0xFF, v & 0xFF);
}

// Leer tabla ARP de lwIP y agregar clientes encontrados
static void read_arp_table(ClientScanResult* result, IPAddress gateway) {
  for (uint8_t i = 0; i < ARP_TABLE_SIZE && result->count < MAX_CLIENTS; i++) {
    ip4_addr_t* arp_ip = nullptr;
    struct netif* arp_netif = nullptr;
    struct eth_addr* arp_mac = nullptr;

    if (etharp_get_entry(i, &arp_ip, &arp_netif, &arp_mac)) {
      if (!arp_ip || !arp_mac) continue;

      uint32_t ip_val = ip4_addr_get_u32(arp_ip);
      if (ip_val == 0 || ip_val == 0xFFFFFFFF) continue;

      // ip4_addr_get_u32 devuelve en network byte order (BE en ESP32)
      // Construir IPAddress respetando ese orden
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

      bool is_mobile = false;
      oui_lookup(c.mac, c.vendor, sizeof(c.vendor), &is_mobile);

      c.is_gateway = (found_ip == gateway);
      c.rtt_us = 0;

      result->count++;
    }
  }
}

static void send_arp_request(IPAddress target) {
  struct netif* netif = netif_default;
  if (!netif) return;
  ip4_addr_t target_addr;
  IP4_ADDR(&target_addr, target[0], target[1], target[2], target[3]);
  etharp_request(netif, &target_addr);
}

void net_clients_scan(ClientScanResult* result, IPAddress local_ip,
                      IPAddress subnet_mask, IPAddress gateway) {
  result->count = 0;
  uint32_t t0 = millis();

  if (!WiFi.isConnected()) {
    Serial.println("[CLIENTS] No WiFi");
    return;
  }

  uint32_t ip_u32   = ip_to_u32_be(local_ip);
  uint32_t mask_u32 = ip_to_u32_be(subnet_mask);
  uint32_t net      = ip_u32 & mask_u32;
  uint32_t host_count = (~mask_u32);

  uint32_t max_hosts = host_count - 1;
  if (max_hosts > 254) max_hosts = 254;

  uint8_t cidr = __builtin_popcount(mask_u32);

  Serial.printf("[CLIENTS] Net /%d local=%s gw=%s budget=%lums\n",
                cidr, local_ip.toString().c_str(),
                gateway.toString().c_str(),
                (unsigned long)SCAN_BUDGET_MS);

  // ============================================================
  // FASE 1: ARP broadcast (rapido, descubre la mayoria)
  // ============================================================
  Serial.println("[CLIENTS] Fase 1: ARP broadcast...");

  if (gateway != IPAddress(0,0,0,0)) {
    send_arp_request(gateway);
    delay(10);
  }

  for (uint32_t h = 1; h <= max_hosts; h++) {
    // Budget check
    if (millis() - t0 > SCAN_BUDGET_MS) {
      Serial.println("[CLIENTS] Budget excedido en fase ARP");
      goto done;
    }

    uint32_t target = net + h;
    IPAddress target_ip = u32_be_to_ip(target);
    if (target_ip == local_ip) continue;

    send_arp_request(target_ip);

    if (h % 32 == 0) {
      delay(20);
      yield();
    } else {
      delayMicroseconds(500);
    }
  }

  // Esperar respuestas ARP (300ms)
  delay(300);
  read_arp_table(result, gateway);
  Serial.printf("[CLIENTS] Post-ARP: %d hosts (%lums)\n",
                result->count, millis() - t0);

  // ============================================================
  // FASE 2: Ping sweep (solo si queda tiempo y subnet pequeña)
  // En subnets grandes (cidr <= 24), confiar en ARP y limitar
  // el ping sweep a PING_BUDGET_HOSTS hosts
  // ============================================================
  {
    uint32_t ping_max = max_hosts;
    if (cidr <= 24 && ping_max > PING_BUDGET_HOSTS) {
      ping_max = PING_BUDGET_HOSTS;
      Serial.printf("[CLIENTS] Subnet /%d grande, limitando ping a %lu hosts\n",
                    cidr, (unsigned long)ping_max);
    }

    Serial.printf("[CLIENTS] Fase 2: Ping sweep (max %lu hosts)...\n",
                  (unsigned long)ping_max);

    for (uint32_t h = 1; h <= ping_max && result->count < MAX_CLIENTS; h++) {
      if (millis() - t0 > SCAN_BUDGET_MS) {
        Serial.println("[CLIENTS] Budget excedido en ping sweep");
        goto done;
      }

      uint32_t target = net + h;
      IPAddress target_ip = u32_be_to_ip(target);
      if (target_ip == local_ip) continue;

      bool known = false;
      for (uint8_t j = 0; j < result->count; j++) {
        if (result->clients[j].ip == target_ip) { known = true; break; }
      }
      if (known) continue;

      Ping.ping(target_ip, 1);
      yield();

      if (h % 16 == 0) {
        read_arp_table(result, gateway);
      }
    }
  }

  delay(100);
  read_arp_table(result, gateway);

done:
  result->scan_time_ms = millis() - t0;
  Serial.printf("[CLIENTS] Total: %d hosts en %lu ms\n",
                result->count, result->scan_time_ms);
  for (uint8_t i = 0; i < result->count; i++) {
    const NetClient& c = result->clients[i];
    Serial.printf("  [%d] %s %s %s%s\n",
                  i, c.ip.toString().c_str(),
                  c.mac_str, c.vendor,
                  c.is_gateway ? " [GW]" : "");
  }
}
