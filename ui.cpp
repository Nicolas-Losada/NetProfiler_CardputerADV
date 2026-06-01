#include "ui.h"
#include "classifier.h"
#include <M5Cardputer.h>

#define HEADER_H 14
#define FOOTER_H 12
#define LINE_H   11

static uint32_t s_last_key_ms = 0;
static const uint32_t DEBOUNCE_MS = 180;

void ui_init() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
}

// =====================================================================
// Pantalla BOOT
// =====================================================================
void ui_render_boot() {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(30, 30);
  M5Cardputer.Display.print("NetProfiler");
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(45, 60);
  M5Cardputer.Display.print("Cardputer ADV");
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.setCursor(60, 100);
  M5Cardputer.Display.print("Iniciando...");
}

// =====================================================================
// Pantalla SCAN
// =====================================================================
void ui_render_scan(const WiFiAP* aps, uint8_t n, uint8_t selected, bool scanning) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  // Header
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, 2);
  M5Cardputer.Display.print("Redes WiFi");
  M5Cardputer.Display.drawFastHLine(0, HEADER_H, 240, TFT_DARKGRAY);

  if (scanning) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(150, 2);
    M5Cardputer.Display.print("scaneando...");
  }

  if (n == 0) {
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(40, 60);
    M5Cardputer.Display.print("Sin redes. [R] rescan");
    return;
  }

  // Lista visible
  const uint8_t max_visible = 8;
  uint8_t start = 0;
  if (selected >= max_visible) start = selected - max_visible + 1;
  uint8_t end = start + max_visible;
  if (end > n) end = n;

  int16_t y = HEADER_H + 2;
  for (uint8_t i = start; i < end; i++) {
    const WiFiAP& ap = aps[i];
    if (i == selected) {
      M5Cardputer.Display.fillRect(0, y - 1, 240, LINE_H, TFT_DARKCYAN);
      M5Cardputer.Display.setTextColor(TFT_BLACK);
    } else {
      M5Cardputer.Display.setTextColor(TFT_WHITE);
    }
    M5Cardputer.Display.setCursor(3, y);
    // Cifrado
    M5Cardputer.Display.print(ap.enc_idx == 0 ? " " : "L");
    M5Cardputer.Display.print(" ");
    // SSID truncado
    char ssid_short[20] = {};
    strncpy(ssid_short, ap.ssid, 18);
    M5Cardputer.Display.print(ssid_short);
    // RSSI/canal alineado
    char r[12];
    snprintf(r, sizeof(r), "CH%d %d", ap.channel, ap.rssi);
    int16_t w = strlen(r) * 6;
    M5Cardputer.Display.setCursor(240 - w - 2, y);
    M5Cardputer.Display.print(r);
    y += LINE_H;
  }

  // Footer
  M5Cardputer.Display.drawFastHLine(0, 135 - FOOTER_H - 1, 240, TFT_DARKGRAY);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.setCursor(2, 135 - FOOTER_H + 2);
  M5Cardputer.Display.print("[Enter]Conectar [r]Scan");
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(195, 135 - FOOTER_H + 2);
  M5Cardputer.Display.printf("%d/%d", selected + 1, n);
}

// =====================================================================
// Pantalla PASSWORD
// =====================================================================
void ui_render_password(const char* ssid, const char* buf, uint8_t cursor) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(2, 2);
  M5Cardputer.Display.print("Contrasena para:");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, 16);
  M5Cardputer.Display.print(ssid);

  // Caja input
  M5Cardputer.Display.drawRect(5, 50, 230, 30, TFT_WHITE);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(10, 58);
  // Mostrar buffer (con asteriscos opcionalmente)
  M5Cardputer.Display.print(buf);

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.setCursor(2, 110);
  M5Cardputer.Display.print("[Enter]OK [Bksp]Borr [Esc]");
}

// =====================================================================
// Pantalla CONNECTING
// =====================================================================
void ui_render_connecting(const char* ssid, uint8_t attempt) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(30, 30);
  M5Cardputer.Display.print("Conectando");
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, 70);
  M5Cardputer.Display.print(ssid);
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(80, 100);
  M5Cardputer.Display.printf("Intento %d...", attempt);
}

// =====================================================================
// Pantalla ANALYZING
// =====================================================================
void ui_render_analyzing(uint8_t step, const char* step_name) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(30, 20);
  M5Cardputer.Display.print("Analizando");

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, 60);
  M5Cardputer.Display.printf("Paso %d/6: %s", step, step_name);

  // Barra progreso
  uint16_t w = (220 * step) / 6;
  M5Cardputer.Display.drawRect(10, 90, 220, 14, TFT_WHITE);
  M5Cardputer.Display.fillRect(11, 91, w - 2, 12, TFT_GREEN);
}

// =====================================================================
// Pantalla REPORT con pestanas
// =====================================================================
static const char* tab_names[TAB_COUNT] = {
  "Vrdcto", "Enlace", "Local", "Net", "Sonda", "Hosts"
};

static void render_tab_bar(ReportTab tab) {
  M5Cardputer.Display.fillRect(0, 0, 240, HEADER_H, TFT_BLACK);
  int16_t x = 2;
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    bool active = (i == (uint8_t)tab);
    M5Cardputer.Display.setTextColor(active ? TFT_BLACK : TFT_GRAY);
    if (active) {
      int16_t w = strlen(tab_names[i]) * 6 + 4;
      M5Cardputer.Display.fillRect(x - 1, 0, w, HEADER_H, TFT_CYAN);
    }
    M5Cardputer.Display.setCursor(x + 1, 3);
    M5Cardputer.Display.print(tab_names[i]);
    x += strlen(tab_names[i]) * 6 + 5;
  }
  M5Cardputer.Display.drawFastHLine(0, HEADER_H, 240, TFT_DARKGRAY);
}

static void render_footer() {
  M5Cardputer.Display.drawFastHLine(0, 135 - FOOTER_H - 1, 240, TFT_DARKGRAY);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.setCursor(2, 135 - FOOTER_H + 2);
  M5Cardputer.Display.print("[<-][->]Tab [Esc]Salir");
}

static void render_tab_verdict(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  uint32_t color;
  switch (p->verdict) {
    case V_MOBILE:        color = TFT_ORANGE; break;
    case V_FIXED:         color = TFT_GREEN; break;
    case V_INDETERMINATE: color = TFT_YELLOW; break;
    default:              color = TFT_RED; break;
  }

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(color);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.print(verdict_to_str(p->verdict));
  y += 22;

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("Confianza: %d%%", p->confidence_pct);
  y += LINE_H;
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("Mob:%d Fix:%d",
                              p->score_mobile, p->score_fixed);
  y += LINE_H;
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.print("Razones:");
  y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  // Imprimir hasta 3 lineas de razones
  const char* r = p->verdict_reasons;
  uint8_t lines = 0;
  while (*r && lines < 3) {
    const char* nl = strchr(r, '\n');
    size_t len = nl ? (size_t)(nl - r) : strlen(r);
    if (len > 39) len = 39;
    char tmp[40];
    memcpy(tmp, r, len); tmp[len] = 0;
    M5Cardputer.Display.setCursor(2, y);
    M5Cardputer.Display.print(tmp);
    y += LINE_H;
    if (!nl) break;
    r = nl + 1;
    lines++;
  }
}

static void render_tab_link(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("SSID:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print(p->ssid); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("BSSID:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print(p->bssid_str); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("Fabric: ");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.print(p->oui_vendor); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("RSSI: ");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.printf("%d dBm  CH%d", p->rssi, p->channel); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("Seg: ");
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.print(p->encryption);
  M5Cardputer.Display.printf("  BSSIDs:%d", p->bssid_count);
}

static void render_tab_local(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  M5Cardputer.Display.setTextSize(1);
  char buf[24];

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("IP local:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("%s/%d", p->ip_local.toString().c_str(), p->subnet_cidr);
  y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("Gateway:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.print(p->gateway.toString()); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("DNS:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.print(p->dns1.toString());
  if (p->dns2) {
    M5Cardputer.Display.print(" ");
    M5Cardputer.Display.print(p->dns2.toString());
  }
  y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("Tipo:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(p->subnet_classification[0] == 'i' ? TFT_ORANGE : TFT_GREEN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.print(p->subnet_classification);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.printf(" IPv6:%s", p->has_ipv6 ? "si" : "no");
}

static void render_tab_internet(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  M5Cardputer.Display.setTextSize(1);

  if (!p->internet_available) {
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(2, 50);
    M5Cardputer.Display.print("Sin internet (N/D)");
    return;
  }

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("IP publica:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print(p->ip_public_str); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("%s ", p->asn);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  // ISP truncado
  char isp_short[36] = {};
  strncpy(isp_short, p->isp_org, 30);
  M5Cardputer.Display.print(isp_short); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("Tipo:");
  M5Cardputer.Display.setTextColor(p->mobile_flag ? TFT_ORANGE : TFT_GREEN);
  M5Cardputer.Display.print(p->ip_type);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.printf(" %s", p->country); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("CGNAT:");
  M5Cardputer.Display.setTextColor(p->cgnat_detected ? TFT_ORANGE : TFT_GREEN);
  M5Cardputer.Display.print(p->cgnat_detected ? "SI" : "NO");
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.print("  NAT64:");
  M5Cardputer.Display.setTextColor(p->nat64_detected ? TFT_ORANGE : TFT_GREEN);
  M5Cardputer.Display.print(p->nat64_detected ? "SI" : "NO");
  y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("RTT CF:%dms G:%dms",
                              p->rtt_cf_ms, p->rtt_google_ms); y += LINE_H;
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("Jitter: %dms", p->jitter_ms);
}

static void render_tab_probes(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("ICMP:");
  M5Cardputer.Display.setTextColor(p->icmp_works ? TFT_GREEN : TFT_RED);
  M5Cardputer.Display.print(p->icmp_works ? "OK" : "NO"); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("Portal cautivo:");
  M5Cardputer.Display.setTextColor(p->captive_portal ? TFT_ORANGE : TFT_GREEN);
  M5Cardputer.Display.print(p->captive_portal ? "SI" : "NO"); y += LINE_H;

  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y); M5Cardputer.Display.print("rDNS:"); y += LINE_H;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, y);
  char rdns_short[40] = {};
  strncpy(rdns_short, p->rdns[0] ? p->rdns : "(sin rDNS)", 39);
  M5Cardputer.Display.print(rdns_short);
}

void ui_render_report(const ConnProfile* p, ReportTab tab) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  render_tab_bar(tab);
  switch (tab) {
    case TAB_VERDICT:  render_tab_verdict(p); break;
    case TAB_LINK:     render_tab_link(p); break;
    case TAB_LOCAL:    render_tab_local(p); break;
    case TAB_INTERNET: render_tab_internet(p); break;
    case TAB_PROBES:   render_tab_probes(p); break;
    case TAB_CLIENTS:  break; // render via ui_render_clients separado
    default: break;
  }
  render_footer();
}

// =====================================================================
// Pantalla CLIENTS (dispositivos en red)
// =====================================================================
void ui_render_clients(const ClientScanResult* clients, uint8_t scroll_pos) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  render_tab_bar(TAB_CLIENTS);

  int16_t y = HEADER_H + 2;
  M5Cardputer.Display.setTextSize(1);

  if (clients->count == 0) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(20, 50);
    M5Cardputer.Display.print("Escaneando hosts...");
    M5Cardputer.Display.setTextColor(TFT_GRAY);
    M5Cardputer.Display.setCursor(20, 70);
    M5Cardputer.Display.print("[R] re-escanear");
    render_footer();
    return;
  }

  // Header columnas
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.setCursor(2, y);
  M5Cardputer.Display.printf("Hosts: %d (%lums)", clients->count, clients->scan_time_ms);
  y += LINE_H + 1;
  M5Cardputer.Display.drawFastHLine(0, y - 1, 240, TFT_DARKGRAY);

  // 7 filas visibles: IP + MAC + vendor
  const uint8_t max_vis = 7;
  uint8_t end = scroll_pos + max_vis;
  if (end > clients->count) end = clients->count;

  for (uint8_t i = scroll_pos; i < end; i++) {
    const NetClient& c = clients->clients[i];

    // Color: gateway=cyan, otros=blanco
    M5Cardputer.Display.setTextColor(c.is_gateway ? TFT_CYAN : TFT_WHITE);
    M5Cardputer.Display.setCursor(2, y);

    // IP corta
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             c.ip[0], c.ip[1], c.ip[2], c.ip[3]);

    // Formato compacto: IP MAC(ultimos 6) vendor
    M5Cardputer.Display.printf("%-15s %02X:%02X:%02X",
                                ip_str,
                                c.mac[3], c.mac[4], c.mac[5]);

    // Vendor en color distinto si cabe
    if (c.vendor[0] && strcmp(c.vendor, "Unknown") != 0) {
      M5Cardputer.Display.setTextColor(TFT_YELLOW);
      M5Cardputer.Display.printf(" %s", c.vendor);
    }

    if (c.is_gateway) {
      M5Cardputer.Display.setTextColor(TFT_GREEN);
      M5Cardputer.Display.print(" GW");
    }

    y += LINE_H;
  }

  // Scroll indicator
  if (clients->count > max_vis) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(200, 135 - FOOTER_H + 2);
    M5Cardputer.Display.printf("%d/%d", scroll_pos + 1, clients->count);
  }

  render_footer();
}

void ui_render_error(const char* msg) {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_RED);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(50, 30);
  M5Cardputer.Display.print("ERROR");
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(2, 70);
  M5Cardputer.Display.print(msg);
  M5Cardputer.Display.setTextColor(TFT_GRAY);
  M5Cardputer.Display.setCursor(2, 110);
  M5Cardputer.Display.print("[Esc] volver");
}

// =====================================================================
// Lectura teclado
// =====================================================================
UIInput ui_poll_input(bool raw) {
  UIInput in;
  in.key = K_NONE;
  in.ch = 0;

  uint32_t now = millis();
  if (now - s_last_key_ms < DEBOUNCE_MS) return in;

  if (!M5Cardputer.Keyboard.isChange()) return in;
  if (!M5Cardputer.Keyboard.isPressed()) return in;

  Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

  // Teclas especiales primero
  if (st.enter) { in.key = K_SELECT; s_last_key_ms = now; return in; }
  if (st.del)   { in.key = K_BACK;   s_last_key_ms = now; return in; }
  if (st.tab)   { in.key = K_TAB_NEXT; s_last_key_ms = now; return in; }
  if (st.fn)    { in.key = K_HELP;   s_last_key_ms = now; return in; }

  // Iterar palabras (chars)
  for (auto c : st.word) {
    s_last_key_ms = now;

    // En modo raw (entrada texto/password) todos los chars son literales
    // excepto el escape (`)
    if (raw) {
      if (c == '`') { in.key = K_BACK; return in; }
      in.key = K_CHAR;
      in.ch = c;
      return in;
    }

    // Modo navegacion: , . ; / son flechas
    switch (c) {
      case ',':  in.key = K_LEFT;  return in;
      case '/':  in.key = K_RIGHT; return in;
      case ';':  in.key = K_UP;    return in;
      case '.':  in.key = K_DOWN;  return in;
      case '`':  in.key = K_BACK;  return in;
      default:
        in.key = K_CHAR;
        in.ch = c;
        return in;
    }
  }

  return in;
}
