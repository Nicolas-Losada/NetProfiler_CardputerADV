#include <Arduino.h>
#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
/*
  =====================================================================
  NetProfiler - M5Stack Cardputer ADV (ESP32-S3FN8)
  =====================================================================
  Perfilador de conexion a internet:
  - WiFi station -> recolecta enlace + DHCP + IP publica + ASN
  - Clasifica si backhaul es CELULAR / ISP FIJO / INDETERMINADO
  - Logging JSON+CSV a microSD
  - Solo diagnostico pasivo del propio enlace
  =====================================================================
*/

#include <M5Cardputer.h>
#include <WiFi.h>

#include "conn_profile.h"
#include "config.h"
#include "ui.h"
#include "net_link.h"
#include "net_local.h"
#include "net_internet.h"
#include "classifier.h"
#include "storage.h"
#include "net_clients.h"

// === Estado global ===
Config g_config;
static ConnProfile g_profile;
static SemaphoreHandle_t g_profile_mutex = nullptr;

static AppState g_state = ST_BOOT;
static ReportTab g_tab = TAB_VERDICT;

// === Scan / password ===
static WiFiAP g_aps[20];
static uint8_t g_ap_count = 0;
static uint8_t g_ap_selected = 0;
static char g_password[65] = {0};
static uint8_t g_pwd_cursor = 0;

// === Clientes red ===
static ClientScanResult g_clients;
static uint8_t g_clients_scroll = 0;
static volatile bool g_request_client_scan = false;

// === FreeRTOS ===
static TaskHandle_t g_net_task = nullptr;
static volatile bool g_request_analyze = false;
static volatile uint8_t g_analyze_step = 0;
static const char* g_analyze_step_name = "";

// =====================================================================
// Tarea de red (core 0)
// =====================================================================
#line 55 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void net_task_fn(void* arg);
#line 113 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void do_scan();
#line 147 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static bool connect_to(const char* ssid, const char* pwd);
#line 160 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
void setup();
#line 196 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void handle_input_scan(const UIInput& in);
#line 225 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void handle_input_password(const UIInput& in);
#line 248 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void handle_input_report(const UIInput& in);
#line 276 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
void loop();
#line 55 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\NetProfiler.ino"
static void net_task_fn(void* arg) {
  for (;;) {
    if (g_request_analyze) {
      g_request_analyze = false;

      ConnProfile local;
      local.reset();
      local.wifi_connected = WiFi.isConnected();
      local.last_update_ms = millis();

      g_analyze_step = 1; g_analyze_step_name = "Enlace WiFi";
      net_link_collect(&local);

      g_analyze_step = 2; g_analyze_step_name = "DHCP/IP local";
      net_local_collect(&local);

      g_analyze_step = 3; g_analyze_step_name = "Internet/ASN";
      net_internet_collect(&local);

      g_analyze_step = 4; g_analyze_step_name = "Clasificacion";
      classifier_run(&local);

      g_analyze_step = 5; g_analyze_step_name = "Hosts LAN";
      // ARP scan: descubrir dispositivos en subred
      net_clients_scan(&g_clients, local.ip_local,
                       local.subnet_mask, local.gateway);
      g_clients_scroll = 0;

      g_analyze_step = 6; g_analyze_step_name = "Guardando";
      if (g_config.log_to_sd) {
        storage_log_profile(&local);
      }

      // Copiar a global protegido
      if (g_profile_mutex && xSemaphoreTake(g_profile_mutex, portMAX_DELAY)) {
        memcpy(&g_profile, &local, sizeof(ConnProfile));
        xSemaphoreGive(g_profile_mutex);
      }

      g_analyze_step = 0;
      g_state = ST_REPORT;
    }

    // Re-scan clientes bajo demanda
    if (g_request_client_scan && WiFi.isConnected()) {
      g_request_client_scan = false;
      net_clients_scan(&g_clients, WiFi.localIP(),
                       WiFi.subnetMask(), WiFi.gatewayIP());
      g_clients_scroll = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// =====================================================================
// Scan de redes
// =====================================================================
static void do_scan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // show_hidden=true -> capta hotspots con SSID oculto (Xiaomi/Android).
  // 500ms/canal -> mas tiempo para que hotspot movil responda al probe.
  // Hotspots Android suelen estar en canal 1/6/11; algunos en 12/13.
  // OJO: si el hotspot esta en 5GHz, el ESP32-S3 NO lo vera nunca (solo 2.4GHz).
  int n = WiFi.scanNetworks(false, true, false, 500U);
  g_ap_count = 0;
  for (int i = 0; i < n && g_ap_count < 20; i++) {
    String s = WiFi.SSID(i);
    WiFiAP& ap = g_aps[g_ap_count];
    if (s.length() == 0) {
      // Red oculta: mostrar con BSSID para poder seleccionarla igual
      snprintf(ap.ssid, sizeof(ap.ssid), "(oculta) %s",
               WiFi.BSSIDstr(i).c_str());
    } else {
      strncpy(ap.ssid, s.c_str(), sizeof(ap.ssid) - 1);
      ap.ssid[sizeof(ap.ssid) - 1] = 0;
    }
    ap.rssi = WiFi.RSSI(i);
    ap.channel = WiFi.channel(i);
    ap.enc_idx = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? 0 : 1;
    g_ap_count++;
  }
  WiFi.scanDelete();
  g_ap_selected = 0;
}

// =====================================================================
// Conexion
// =====================================================================
static bool connect_to(const char* ssid, const char* pwd) {
  WiFi.begin(ssid, pwd);
  uint32_t t0 = millis();
  while (millis() - t0 < 15000) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(200);
  }
  return false;
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);

  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== NetProfiler ===");

  ui_init();
  ui_render_boot();

  // SD opcional (no abortar si falla)
  if (storage_init()) {
    storage_load_config();
    Serial.println("[SD] OK");
  } else {
    Serial.println("[SD] no detectada, sin logging");
  }

  // Mutex profile
  g_profile_mutex = xSemaphoreCreateMutex();

  // Tarea red en core 0
  xTaskCreatePinnedToCore(net_task_fn, "net", 8192, nullptr, 5, &g_net_task, 0);

  delay(800);

  // Estado inicial: scan
  do_scan();
  g_state = ST_SCAN;
}

// =====================================================================
// Helpers UI
// =====================================================================
static void handle_input_scan(const UIInput& in) {
  switch (in.key) {
    case K_UP:
      if (g_ap_selected > 0) g_ap_selected--;
      break;
    case K_DOWN:
      if (g_ap_selected + 1 < g_ap_count) g_ap_selected++;
      break;
    case K_SELECT:
      if (g_ap_count > 0) {
        g_password[0] = 0;
        g_pwd_cursor = 0;
        if (g_aps[g_ap_selected].enc_idx == 0) {
          // Red abierta -> conectar directo
          g_state = ST_CONNECTING;
        } else {
          g_state = ST_PASSWORD;
        }
      }
      break;
    case K_CHAR:
      if (in.ch == 'r' || in.ch == 'R') {
        do_scan();
      }
      break;
    default: break;
  }
}

static void handle_input_password(const UIInput& in) {
  switch (in.key) {
    case K_BACK:
      if (g_pwd_cursor > 0) {
        g_pwd_cursor--;
        g_password[g_pwd_cursor] = 0;
      } else {
        g_state = ST_SCAN;
      }
      break;
    case K_SELECT:
      g_state = ST_CONNECTING;
      break;
    case K_CHAR:
      if (g_pwd_cursor + 1 < sizeof(g_password)) {
        g_password[g_pwd_cursor++] = in.ch;
        g_password[g_pwd_cursor] = 0;
      }
      break;
    default: break;
  }
}

static void handle_input_report(const UIInput& in) {
  switch (in.key) {
    case K_LEFT:
    case K_TAB_PREV:
      g_tab = (ReportTab)((g_tab + TAB_COUNT - 1) % TAB_COUNT);
      break;
    case K_RIGHT:
    case K_TAB_NEXT:
      g_tab = (ReportTab)((g_tab + 1) % TAB_COUNT);
      break;
    case K_BACK:
      WiFi.disconnect();
      do_scan();
      g_state = ST_SCAN;
      break;
    case K_CHAR:
      if (in.ch == 'r' || in.ch == 'R') {
        g_request_analyze = true;
        g_state = ST_ANALYZING;
      }
      break;
    default: break;
  }
}

// =====================================================================
// LOOP
// =====================================================================
void loop() {
  M5Cardputer.update();
  UIInput in = ui_poll_input();

  switch (g_state) {
    case ST_SCAN:
      handle_input_scan(in);
      ui_render_scan(g_aps, g_ap_count, g_ap_selected, false);
      break;

    case ST_PASSWORD:
      handle_input_password(in);
      ui_render_password(g_aps[g_ap_selected].ssid, g_password, g_pwd_cursor);
      break;

    case ST_CONNECTING: {
      ui_render_connecting(g_aps[g_ap_selected].ssid, 1);
      bool ok = connect_to(g_aps[g_ap_selected].ssid, g_password);
      if (ok) {
        g_request_analyze = true;
        g_state = ST_ANALYZING;
      } else {
        ui_render_error("Fallo conexion");
        delay(2000);
        g_state = ST_SCAN;
      }
      break;
    }

    case ST_ANALYZING:
      ui_render_analyzing(g_analyze_step, g_analyze_step_name);
      // El cambio a ST_REPORT lo hace la net_task
      break;

    case ST_REPORT:
      handle_input_report(in);
      if (g_tab == TAB_CLIENTS) {
        // Scroll up/down en lista clientes
        if (in.key == K_UP && g_clients_scroll > 0) g_clients_scroll--;
        if (in.key == K_DOWN && g_clients_scroll + 7 < g_clients.count) g_clients_scroll++;
        if (in.key == K_CHAR && (in.ch == 'r' || in.ch == 'R')) {
          g_request_client_scan = true;
          g_clients.count = 0; // forzar pantalla "Escaneando..."
        }
        ui_render_clients(&g_clients, g_clients_scroll);
      } else {
        if (g_profile_mutex && xSemaphoreTake(g_profile_mutex, pdMS_TO_TICKS(50))) {
          ui_render_report(&g_profile, g_tab);
          xSemaphoreGive(g_profile_mutex);
        }
      }
      break;

    case ST_ERROR:
      if (in.key == K_BACK) g_state = ST_SCAN;
      break;

    default: break;
  }

  delay(40);
}

