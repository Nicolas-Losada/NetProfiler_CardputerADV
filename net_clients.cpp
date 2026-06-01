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

// Envia ARP request explicito (mas confiable que ping para hosts que filtran ICMP)
static void send_arp_request(IPAddress target) {
  struct netif* netif = netif_default;
  if (!netif) return;

  ip4_addr_t target_addr;
  IP4_ADDR(&target_addr, target[0], target[1], target[2], target[3]);

  // etharp_request envia ARP broadcast preguntando "quien tiene esta IP?"
  // Si el host existe, respondera y la tabla ARP se actualizara automaticamente
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

  // Calcular rango subred
  uint32_t ip_u32 = ((uint32_t)local_ip[0]) | ((uint32_t)local_ip[1] << 8) |
                    ((uint32_t)local_ip[2] << 16) | ((uint32_t)local_ip[3] << 24);
  uint32_t mask_u32 = ((uint32_t)subnet_mask[0]) | ((uint32_t)subnet_mask[1] << 8) |
                      ((uint32_t)subnet_mask[2] << 16) | ((uint32_t)subnet_mask[3] << 24);

  uint32_t net = ip_u32 & mask_u32;
  uint32_t host_count = (~mask_u32) & 0xFFFFFFFF;

  // Limitar scan en subnets grandes
  uint32_t max_hosts = host_count - 1;
  if (max_hosts > 254) max_hosts = 254;

  // CIDR de la mascara
  uint8_t cidr = 0;
  for (uint8_t i = 0; i < 32; i++) {
    if (mask_u32 & (1UL << i)) cidr++;
  }

  Serial.printf("[CLIENTS] Net /%d local=%s gw=%s mask=%s hosts=%lu\n",
                cidr, local_ip.toString().c_str(),
                gateway.toString().c_str(),
                subnet_mask.toString().c_str(),
                (unsigned long)max_hosts);

  // ============================================================
  // FASE 1: ARP request broadcast a TODOS los hosts (rapido)
  // No espera respuesta, solo manda el broadcast
  // ============================================================
  Serial.println("[CLIENTS] Fase 1: ARP broadcast...");

  // Gateway primero (siempre debe responder)
  if (gateway != IPAddress(0,0,0,0)) {
    send_arp_request(gateway);
    delay(10);
  }

  for (uint32_t h = 1; h <= max_hosts; h++) {
    uint32_t target = net + h;
    IPAddress target_ip(
      (target) & 0xFF,
      (target >> 8) & 0xFF,
      (target >> 16) & 0xFF,
      (target >> 24) & 0xFF
    );
    if (target_ip == local_ip) continue;

    send_arp_request(target_ip);

    // Pausa corta para no saturar; cada 32 envios deja respirar
    if (h % 32 == 0) {
      delay(20);
      yield();
    } else {
      delayMicroseconds(500);
    }
  }

  // Esperar respuestas ARP (200ms)
  Serial.println("[CLIENTS] Esperando respuestas ARP...");
  delay(200);
  read_arp_table(result, gateway);
  uint8_t after_arp = result->count;
  Serial.printf("[CLIENTS] Post-ARP: %d hosts\n", after_arp);

  // ============================================================
  // FASE 2: Ping sweep para hosts que filtran ARP gratuito
  // ============================================================
  Serial.println("[CLIENTS] Fase 2: Ping sweep...");

  for (uint32_t h = 1; h <= max_hosts && result->count < MAX_CLIENTS; h++) {
    uint32_t target = net + h;
    IPAddress target_ip(
      (target) & 0xFF,
      (target >> 8) & 0xFF,
      (target >> 16) & 0xFF,
      (target >> 24) & 0xFF
    );
    if (target_ip == local_ip) continue;

    // Saltar IPs ya encontradas
    bool known = false;
    for (uint8_t j = 0; j < result->count; j++) {
      if (result->clients[j].ip == target_ip) { known = true; break; }
    }
    if (known) continue;

    Ping.ping(target_ip, 1);
    yield();

    // Lectura ARP intermedia cada 16 IPs
    if (h % 16 == 0) {
      read_arp_table(result, gateway);
    }
  }

  // Lectura final
  delay(100);
  read_arp_table(result, gateway);

  result->scan_time_ms = millis() - t0;
  Serial.printf("[CLIENTS] Total: %d hosts en %lu ms\n",
                result->count, result->scan_time_ms);

  // Debug: listar todos
  for (uint8_t i = 0; i < result->count; i++) {
    const NetClient& c = result->clients[i];
    Serial.printf("  [%d] %s %s %s%s\n",
                  i, c.ip.toString().c_str(),
                  c.mac_str, c.vendor,
                  c.is_gateway ? " [GW]" : "");
  }
}
