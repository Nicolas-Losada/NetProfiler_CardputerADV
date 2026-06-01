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
#include <esp_wifi.h>

#include "conn_profile.h"
#include "config.h"
#include "ui.h"
#include "net_link.h"
#include "net_local.h"
#include "net_internet.h"
#include "classifier.h"
#include "storage.h"
#include "net_clients.h"
#include "audio.h"

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
static void net_task_fn(void* arg) {
  for (;;) {
    if (g_request_analyze) {
      g_request_analyze = false;

      ConnProfile local;
      local.reset();
      local.wifi_connected = WiFi.isConnected();
      local.last_update_ms = millis();

      // 5 pasos (sin Hosts - movido a bajo demanda en TAB_CLIENTS)
      g_analyze_step = 1; g_analyze_step_name = "Enlace WiFi";
      net_link_collect(&local, g_aps[g_ap_selected]);

      g_analyze_step = 2; g_analyze_step_name = "DHCP/IP local";
      net_local_collect(&local);

      g_analyze_step = 3; g_analyze_step_name = "Internet/ASN";
      net_internet_collect(&local);

      g_analyze_step = 4; g_analyze_step_name = "Clasificacion";
      classifier_run(&local);

      // Limpiar clientes - se descubriran bajo demanda en TAB_CLIENTS
      g_clients.count = 0;
      g_clients.scan_time_ms = 0;
      g_clients_scroll = 0;

      g_analyze_step = 5; g_analyze_step_name = "Guardando";
      if (g_config.log_to_sd) {
        storage_log_profile(&local);
      }

      // Copiar a global protegido
      if (g_profile_mutex && xSemaphoreTake(g_profile_mutex, portMAX_DELAY)) {
        memcpy(&g_profile, &local, sizeof(ConnProfile));
        xSemaphoreGive(g_profile_mutex);
      }

      // Feedback audio segun veredicto
      audio_beep_verdict(local.verdict);

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
  WiFi.disconnect(true, true);  // borrar config + WiFi off momentaneo
  delay(200);
  WiFi.mode(WIFI_STA);

  // setCountry: habilita canales 12/13 (algunos routers latam los usan)
  // WY = worldwide, permite mas canales sin restricciones de pais
  wifi_country_t country = {
    .cc = "01", .schan = 1, .nchan = 13,
    .max_tx_power = 20, .policy = WIFI_COUNTRY_POLICY_MANUAL
  };
  esp_wifi_set_country(&country);

  Serial.println("[SCAN] Iniciando scan amplio...");

  // show_hidden=true -> capta hotspots con SSID oculto
  // 600ms/canal -> mas tiempo para respuesta probe
  int n = WiFi.scanNetworks(false, true, false, 600U);
  Serial.printf("[SCAN] Redes encontradas: %d\n", n);

  g_ap_count = 0;
  for (int i = 0; i < n && g_ap_count < 20; i++) {
    String s = WiFi.SSID(i);
    WiFiAP& ap = g_aps[g_ap_count];

    ap.hidden = (s.length() == 0);
    if (ap.hidden) {
      snprintf(ap.ssid, sizeof(ap.ssid), "(oculta) %s",
               WiFi.BSSIDstr(i).c_str());
    } else {
      strncpy(ap.ssid, s.c_str(), sizeof(ap.ssid) - 1);
      ap.ssid[sizeof(ap.ssid) - 1] = 0;
    }

    uint8_t* b = WiFi.BSSID(i);
    if (b) memcpy(ap.bssid, b, 6);

    ap.rssi = WiFi.RSSI(i);
    ap.channel = WiFi.channel(i);
    ap.auth_mode = WiFi.encryptionType(i);
    ap.enc_idx = (ap.auth_mode == WIFI_AUTH_OPEN) ? 0 : 1;

    Serial.printf("  [%d] %s CH%d RSSI%d AUTH=%d %s\n",
                  i, ap.ssid, ap.channel, ap.rssi, ap.auth_mode,
                  WiFi.BSSIDstr(i).c_str());

    g_ap_count++;
  }
  WiFi.scanDelete();
  g_ap_selected = 0;

  // Contar ssid_count para cada AP (cuantos BSSID comparten el mismo SSID)
  for (uint8_t i = 0; i < g_ap_count; i++) {
    g_aps[i].ssid_count = 0;
    for (uint8_t j = 0; j < g_ap_count; j++) {
      if (strcmp(g_aps[i].ssid, g_aps[j].ssid) == 0) {
        g_aps[i].ssid_count++;
      }
    }
  }
}

// =====================================================================
// Conexion robusta:
// - Pasa BSSID + canal explicito (mas rapido en routers con multiples BSS)
// - Maneja todos los status codes
// - Espera DHCP completo (IP valida != 0.0.0.0)
// - Timeout 25s (routers fijos pueden tardar mas que hotspots)
// =====================================================================
static const char* wifi_status_str(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "SSID_NO_DISP";
    case WL_SCAN_COMPLETED:  return "SCAN_OK";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "FAIL_AUTH";
    case WL_CONNECTION_LOST: return "PERDIDA";
    case WL_DISCONNECTED:    return "DISC";
    default:                 return "?";
  }
}

static bool connect_to(const WiFiAP& ap, const char* pwd) {
  Serial.printf("[CONN] -> %s CH%d AUTH=%d hidden=%d\n",
                ap.ssid, ap.channel, ap.auth_mode, ap.hidden);
  Serial.printf("[CONN] BSSID %02X:%02X:%02X:%02X:%02X:%02X\n",
                ap.bssid[0], ap.bssid[1], ap.bssid[2],
                ap.bssid[3], ap.bssid[4], ap.bssid[5]);

  // Limpiar estado WiFi anterior
  WiFi.disconnect(true, true);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // sleep off durante analisis (mejor RTT)

  // Conexion dirigida: SSID + pwd + canal + BSSID + connect=true
  // Funciona igual para visibles y ocultas - el SSID en ap.ssid es el real
  // (oculta: usuario debe haber escrito SSID antes en una pantalla aparte;
  // por ahora el placeholder "(oculta) MAC" no servira como SSID real)
  if (ap.hidden) {
    // SSID real desconocido. Intentar con BSSID/canal de todos modos
    // (algunos routers permiten conexion por probe directo)
    Serial.println("[CONN] SSID oculto - intento dirigido por BSSID");
    WiFi.begin(ap.ssid, pwd, ap.channel, ap.bssid, true);
  } else {
    WiFi.begin(ap.ssid, pwd, ap.channel, ap.bssid, true);
  }

  uint32_t t0 = millis();
  wl_status_t last_st = WL_IDLE_STATUS;

  while (millis() - t0 < 25000) {
    wl_status_t st = WiFi.status();

    if (st != last_st) {
      Serial.printf("[CONN] Status: %s (%d ms)\n",
                    wifi_status_str(st), (int)(millis() - t0));
      last_st = st;
    }

    // Conectado: esperar IP valida (DHCP completo)
    if (st == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      if (ip != IPAddress(0,0,0,0)) {
        Serial.printf("[CONN] OK IP: %s\n", ip.toString().c_str());
        delay(300); // estabilizar
        return true;
      }
    }

    // Fallo definitivo
    if (st == WL_NO_SSID_AVAIL) {
      Serial.println("[CONN] SSID no disponible");
      return false;
    }
    if (st == WL_CONNECT_FAILED) {
      Serial.println("[CONN] Auth fallo (contrasena?)");
      return false;
    }

    delay(150);
  }

  Serial.println("[CONN] Timeout");
  WiFi.disconnect(true);
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

  // Audio (opcional, si enable_audio en config.json)
  audio_init();

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
  // En PASSWORD usar raw mode para aceptar , . ; / como chars literales
  bool raw = (g_state == ST_PASSWORD);
  UIInput in = ui_poll_input(raw);

  switch (g_state) {
    case ST_SCAN:
      handle_input_scan(in);
      ui_render_scan(g_aps, g_ap_count, g_ap_selected, false);
      break;

    case ST_PASSWORD: {
      static bool reveal = false;
      handle_input_password(in);
      // Fn alterna revelar/ocultar
      if (in.key == K_HELP) reveal = !reveal;
      ui_render_password(g_aps[g_ap_selected].ssid, g_password, g_pwd_cursor, reveal);
      break;
    }

    case ST_CONNECTING: {
      ui_render_connecting(g_aps[g_ap_selected].ssid, 1);
      bool ok = connect_to(g_aps[g_ap_selected], g_password);
      if (!ok) {
        // Reintento: a veces el primer intento falla por canal incorrecto
        Serial.println("[CONN] Reintento sin BSSID dirigido...");
        ui_render_connecting(g_aps[g_ap_selected].ssid, 2);
        WiFi.disconnect(true, true);
        delay(500);
        WiFi.mode(WIFI_STA);
        WiFi.begin(g_aps[g_ap_selected].ssid, g_password);
        uint32_t t0 = millis();
        while (millis() - t0 < 20000) {
          if (WiFi.status() == WL_CONNECTED &&
              WiFi.localIP() != IPAddress(0,0,0,0)) {
            ok = true;
            break;
          }
          if (WiFi.status() == WL_CONNECT_FAILED ||
              WiFi.status() == WL_NO_SSID_AVAIL) break;
          delay(200);
        }
      }
      if (ok) {
        g_request_analyze = true;
        g_state = ST_ANALYZING;
      } else {
        ui_render_error("Fallo conexion");
        delay(2500);
        g_state = ST_SCAN;
      }
      break;
    }

    case ST_ANALYZING:
      ui_render_analyzing(g_analyze_step, g_analyze_step_name);
      // El cambio a ST_REPORT lo hace la net_task
      break;

    case ST_REPORT: {
      static ReportTab last_tab = (ReportTab)255;
      static uint8_t last_scroll = 255;
      static uint8_t last_client_count = 255;
      static bool first_render = true;

      ReportTab prev_tab = g_tab;
      handle_input_report(in);

      bool dirty = first_render || (g_tab != last_tab);
      first_render = false;

      if (g_tab == TAB_CLIENTS) {
        if (in.key == K_UP && g_clients_scroll > 0) { g_clients_scroll--; dirty = true; }
        if (in.key == K_DOWN && g_clients_scroll + 7 < g_clients.count) {
          g_clients_scroll++; dirty = true;
        }
        if (in.key == K_CHAR && (in.ch == 'r' || in.ch == 'R')) {
          g_request_client_scan = true;
          g_clients.count = 0;
          g_clients.scan_time_ms = 0;
          dirty = true;
        }
        // Cambios desde la tarea de red
        if (g_clients_scroll != last_scroll || g_clients.count != last_client_count) {
          dirty = true;
          last_scroll = g_clients_scroll;
          last_client_count = g_clients.count;
        }
        if (dirty) ui_render_clients(&g_clients, g_clients_scroll);
      } else {
        if (dirty) {
          if (g_profile_mutex && xSemaphoreTake(g_profile_mutex, pdMS_TO_TICKS(50))) {
            ui_render_report(&g_profile, g_tab);
            xSemaphoreGive(g_profile_mutex);
          }
        }
      }
      last_tab = g_tab;
      break;
    }

    case ST_ERROR:
      if (in.key == K_BACK) g_state = ST_SCAN;
      break;

    default: break;
  }

  delay(40);
}
