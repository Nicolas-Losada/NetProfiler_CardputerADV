#include "ui.h"
#include "classifier.h"
#include <M5Cardputer.h>
#include <M5GFX.h>

#define HEADER_H 14
#define FOOTER_H 12
#define LINE_H   11

static uint32_t s_last_key_ms = 0;
static const uint32_t DEBOUNCE_MS = 180;

// Sprite global: 240x135 @ 16bpp = ~64.8KB en SRAM
static M5Canvas s_canvas(&M5Cardputer.Display);
static bool s_canvas_ok = false;

// Helper: target de dibujo (sprite si OK, display directo si fallback)
static inline M5GFX& gfx() {
  return (M5GFX&)M5Cardputer.Display;  // para llamadas que no migramos
}

static inline void push() {
  if (s_canvas_ok) s_canvas.pushSprite(0, 0);
}

// Indicador bateria en esquina superior derecha
// Verde > 50, amarillo > 20, rojo <= 20
static void draw_battery() {
  int8_t lvl = M5.Power.getBatteryLevel();
  if (lvl < 0) lvl = 0;
  if (lvl > 100) lvl = 100;
  uint32_t color = (lvl > 50) ? TFT_GREEN :
                   (lvl > 20) ? TFT_YELLOW : TFT_RED;
  char b[8];
  snprintf(b, sizeof(b), "%d%%", lvl);
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(color);
  s_canvas.setCursor(215, 3);
  s_canvas.print(b);
}

void ui_init() {
  // Display fisico
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  // Crear sprite 240x135 16bpp; fallback a 8bpp si no hay heap
  s_canvas.setColorDepth(16);
  if (!s_canvas.createSprite(240, 135)) {
    Serial.println("[UI] 16bpp fallo, intento 8bpp...");
    s_canvas.setColorDepth(8);
    s_canvas_ok = s_canvas.createSprite(240, 135);
  } else {
    s_canvas_ok = true;
  }
  if (s_canvas_ok) {
    s_canvas.setTextSize(1);
    Serial.println("[UI] Sprite OK");
  } else {
    Serial.println("[UI] Sprite FALLA - usando display directo");
  }
}

// =====================================================================
// Pantalla BOOT
// =====================================================================
void ui_render_boot() {
  s_canvas.fillScreen(TFT_BLACK);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setTextSize(2);
  s_canvas.setCursor(30, 30);
  s_canvas.print("NetProfiler");
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(45, 60);
  s_canvas.print("Cardputer ADV");
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.setCursor(60, 100);
  s_canvas.print("Iniciando...");
  push();
}

// =====================================================================
// Pantalla SCAN
// =====================================================================
void ui_render_scan(const WiFiAP* aps, uint8_t n, uint8_t selected, bool scanning) {
  s_canvas.fillScreen(TFT_BLACK);

  // Header
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, 2);
  s_canvas.print("Redes WiFi");
  draw_battery();
  s_canvas.drawFastHLine(0, HEADER_H, 240, TFT_DARKGRAY);

  if (scanning) {
    s_canvas.setTextColor(TFT_YELLOW);
    s_canvas.setCursor(150, 2);
    s_canvas.print("escaneando...");
  }

  if (n == 0) {
    s_canvas.setTextColor(TFT_RED);
    s_canvas.setCursor(40, 60);
    s_canvas.print("Sin redes. [R] rescan");
    push();
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
      s_canvas.fillRect(0, y - 1, 240, LINE_H, TFT_DARKCYAN);
      s_canvas.setTextColor(TFT_BLACK);
    } else {
      s_canvas.setTextColor(TFT_WHITE);
    }
    s_canvas.setCursor(3, y);
    // Cifrado
    s_canvas.print(ap.enc_idx == 0 ? " " : "L");
    s_canvas.print(" ");
    // SSID truncado
    char ssid_short[20] = {};
    strncpy(ssid_short, ap.ssid, 18);
    s_canvas.print(ssid_short);
    // RSSI/canal alineado
    char r[12];
    snprintf(r, sizeof(r), "CH%d %d", ap.channel, ap.rssi);
    int16_t w = s_canvas.textWidth(r);
    s_canvas.setCursor(240 - w - 2, y);
    s_canvas.print(r);
    y += LINE_H;
  }

  // Footer
  s_canvas.drawFastHLine(0, 135 - FOOTER_H - 1, 240, TFT_DARKGRAY);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.setCursor(2, 135 - FOOTER_H + 2);
  s_canvas.print("[Enter]Conectar [r]Scan");
  s_canvas.setTextColor(TFT_YELLOW);
  s_canvas.setCursor(195, 135 - FOOTER_H + 2);
  s_canvas.printf("%d/%d", selected + 1, n);
  push();
}

// =====================================================================
// Pantalla PASSWORD
// =====================================================================
void ui_render_password(const char* ssid, const char* buf, uint8_t cursor, bool reveal) {
  s_canvas.fillScreen(TFT_BLACK);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setTextSize(1);
  s_canvas.setCursor(2, 2);
  s_canvas.print("Contrasena para:");
  draw_battery();
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, 16);
  s_canvas.print(ssid);

  // Caja input
  s_canvas.drawRect(5, 50, 230, 30, TFT_WHITE);
  s_canvas.setTextColor(TFT_GREEN);
  s_canvas.setTextSize(2);
  s_canvas.setCursor(10, 58);

  if (reveal) {
    s_canvas.print(buf);
  } else {
    // Asteriscos en lugar de texto
    for (uint8_t i = 0; i < cursor; i++) s_canvas.print('*');
  }

  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.setCursor(2, 95);
  s_canvas.printf("Long: %d", cursor);
  s_canvas.setCursor(2, 110);
  s_canvas.print("[Enter]OK [Bksp]Del [Fn]Ocult");
  push();
}

// =====================================================================
// Pantalla CONNECTING
// =====================================================================
void ui_render_connecting(const char* ssid, uint8_t attempt) {
  s_canvas.fillScreen(TFT_BLACK);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setTextSize(2);
  s_canvas.setCursor(30, 30);
  s_canvas.print("Conectando");
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, 70);
  s_canvas.print(ssid);
  s_canvas.setTextColor(TFT_YELLOW);
  s_canvas.setCursor(80, 100);
  s_canvas.printf("Intento %d...", attempt);
  push();
}

// =====================================================================
// Pantalla ANALYZING
// =====================================================================
void ui_render_analyzing(uint8_t step, const char* step_name) {
  s_canvas.fillScreen(TFT_BLACK);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setTextSize(2);
  s_canvas.setCursor(30, 20);
  s_canvas.print("Analizando");

  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, 60);
  s_canvas.printf("Paso %d/5: %s", step, step_name);

  // Barra progreso
  uint16_t w = (220 * step) / 5;
  s_canvas.drawRect(10, 90, 220, 14, TFT_WHITE);
  s_canvas.fillRect(11, 91, w > 2 ? w - 2 : 0, 12, TFT_GREEN);
  push();
}

// =====================================================================
// Pantalla REPORT con pestanas
// =====================================================================
static const char* tab_names[TAB_COUNT] = {
  "Veredicto", "Enlace", "Local", "Internet", "Sondas", "Hosts"
};

// Tab bar compacta: < N/total Nombre >
// Indicador centrado que muestra solo la pestaña activa por nombre completo
static void render_tab_bar(ReportTab tab) {
  s_canvas.fillRect(0, 0, 240, HEADER_H, TFT_BLACK);

  // Flecha izquierda
  s_canvas.setTextColor(TFT_DARKGRAY);
  s_canvas.setCursor(3, 3);
  s_canvas.print("<");

  // Indicador N/T
  char idx[8];
  snprintf(idx, sizeof(idx), "%d/%d", (int)tab + 1, TAB_COUNT);
  s_canvas.setTextColor(TFT_YELLOW);
  s_canvas.setCursor(15, 3);
  s_canvas.print(idx);

  // Nombre pestaña centrado
  const char* name = tab_names[(uint8_t)tab];
  int16_t name_w = s_canvas.textWidth(name);
  int16_t name_x = (240 - name_w) / 2;
  // Caja resaltada
  s_canvas.fillRect(name_x - 4, 0, name_w + 8, HEADER_H, TFT_CYAN);
  s_canvas.setTextColor(TFT_BLACK);
  s_canvas.setCursor(name_x, 3);
  s_canvas.print(name);

  // Flecha derecha (sustituida por bateria)
  draw_battery();

  s_canvas.drawFastHLine(0, HEADER_H, 240, TFT_DARKGRAY);
}

static void render_footer() {
  s_canvas.drawFastHLine(0, 135 - FOOTER_H - 1, 240, TFT_DARKGRAY);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.setCursor(2, 135 - FOOTER_H + 2);
  s_canvas.print("[<-][->]Tab [Esc]Salir");
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

  s_canvas.setTextSize(2);
  s_canvas.setTextColor(color);
  s_canvas.setCursor(2, y);
  s_canvas.print(verdict_to_str(p->verdict));
  y += 22;

  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y);
  s_canvas.printf("Confianza: %d%%", p->confidence_pct);
  y += LINE_H;
  s_canvas.setCursor(2, y);
  s_canvas.printf("Mob:%d Fix:%d",
                              p->score_mobile, p->score_fixed);
  y += LINE_H;
  s_canvas.setCursor(2, y);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.print("Razones:");
  y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  // Imprimir hasta 3 lineas de razones
  const char* r = p->verdict_reasons;
  uint8_t lines = 0;
  while (*r && lines < 3) {
    const char* nl = strchr(r, '\n');
    size_t len = nl ? (size_t)(nl - r) : strlen(r);
    if (len > 39) len = 39;
    char tmp[40];
    memcpy(tmp, r, len); tmp[len] = 0;
    s_canvas.setCursor(2, y);
    s_canvas.print(tmp);
    y += LINE_H;
    if (!nl) break;
    r = nl + 1;
    lines++;
  }
}

static void render_tab_link(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("SSID:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y); s_canvas.print(p->ssid); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("BSSID:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y); s_canvas.print(p->bssid_str); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y);
  s_canvas.printf("Fabric: ");
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.print(p->oui_vendor); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y);
  s_canvas.printf("RSSI: ");
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.printf("%d dBm  CH%d", p->rssi, p->channel); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("Seg: ");
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.print(p->encryption);
  s_canvas.printf("  BSSIDs:%d", p->bssid_count);
}

static void render_tab_local(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  s_canvas.setTextSize(1);
  char buf[24];

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("IP local:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y);
  s_canvas.printf("%s/%d", p->ip_local.toString().c_str(), p->subnet_cidr);
  y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("Gateway:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y);
  s_canvas.print(p->gateway.toString()); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("DNS:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y);
  s_canvas.print(p->dns1.toString());
  if (p->dns2) {
    s_canvas.print(" ");
    s_canvas.print(p->dns2.toString());
  }
  y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("Tipo:"); y += LINE_H;
  s_canvas.setTextColor(p->subnet_classification[0] == 'i' ? TFT_ORANGE : TFT_GREEN);
  s_canvas.setCursor(2, y);
  s_canvas.print(p->subnet_classification);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.printf(" IPv6:%s", p->has_ipv6 ? "si" : "no");
}

static void render_tab_internet(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  s_canvas.setTextSize(1);

  if (!p->internet_available) {
    s_canvas.setTextColor(TFT_RED);
    s_canvas.setCursor(2, 50);
    s_canvas.print("Sin internet (N/D)");
    return;
  }

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("IP publica:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y); s_canvas.print(p->ip_public_str); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y);
  s_canvas.printf("%s ", p->asn);
  s_canvas.setTextColor(TFT_WHITE);
  // ISP truncado
  char isp_short[36] = {};
  strncpy(isp_short, p->isp_org, 30);
  s_canvas.print(isp_short); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("Tipo:");
  s_canvas.setTextColor(p->mobile_flag ? TFT_ORANGE : TFT_GREEN);
  s_canvas.print(p->ip_type);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.printf(" %s", p->country); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("CGNAT:");
  s_canvas.setTextColor(p->cgnat_detected ? TFT_ORANGE : TFT_GREEN);
  s_canvas.print(p->cgnat_detected ? "SI" : "NO");
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.print("  NAT64:");
  // NAT64: N/D gris si no podemos verificar (limitacion IPv6 en core)
  if (p->nat64_detected) {
    s_canvas.setTextColor(TFT_ORANGE);
    s_canvas.print("SI");
  } else {
    s_canvas.setTextColor(TFT_GRAY);
    s_canvas.print("N/D");
  }
  y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y);
  s_canvas.printf("RTT CF:%dms G:%dms",
                              p->rtt_cf_ms, p->rtt_google_ms); y += LINE_H;
  s_canvas.setCursor(2, y);
  s_canvas.printf("Jitter: %dms", p->jitter_ms);
}

static void render_tab_probes(const ConnProfile* p) {
  int16_t y = HEADER_H + 4;
  s_canvas.setTextSize(1);

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("ICMP:");
  s_canvas.setTextColor(p->icmp_works ? TFT_GREEN : TFT_RED);
  s_canvas.print(p->icmp_works ? "OK" : "NO"); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("Portal cautivo:");
  s_canvas.setTextColor(p->captive_portal ? TFT_ORANGE : TFT_GREEN);
  s_canvas.print(p->captive_portal ? "SI" : "NO"); y += LINE_H;

  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y); s_canvas.print("rDNS:"); y += LINE_H;
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, y);
  char rdns_short[40] = {};
  strncpy(rdns_short, p->rdns[0] ? p->rdns : "(sin rDNS)", 39);
  s_canvas.print(rdns_short);
}

void ui_render_report(const ConnProfile* p, ReportTab tab) {
  s_canvas.fillScreen(TFT_BLACK);
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
  push();
}

// =====================================================================
// Pantalla CLIENTS (dispositivos en red)
// =====================================================================
void ui_render_clients(const ClientScanResult* clients, uint8_t scroll_pos) {
  s_canvas.fillScreen(TFT_BLACK);
  render_tab_bar(TAB_CLIENTS);

  int16_t y = HEADER_H + 2;
  s_canvas.setTextSize(1);

  if (clients->count == 0) {
    if (clients->scan_time_ms > 0) {
      s_canvas.setTextColor(TFT_YELLOW);
      s_canvas.setCursor(20, 50);
      s_canvas.print("Sin hosts en la LAN");
      s_canvas.setTextColor(TFT_GRAY);
      s_canvas.setCursor(20, 70);
      s_canvas.printf("Scan: %lums", clients->scan_time_ms);
    } else {
      s_canvas.setTextColor(TFT_CYAN);
      s_canvas.setCursor(20, 40);
      s_canvas.print("Hosts no escaneados");
      s_canvas.setTextColor(TFT_YELLOW);
      s_canvas.setCursor(20, 60);
      s_canvas.print("Pulsa [R] para escanear");
      s_canvas.setTextColor(TFT_GRAY);
      s_canvas.setCursor(20, 78);
      s_canvas.print("(hasta 20s en subnets /24)");
    }
    render_footer();
    push();
    return;
  }

  // Header columnas
  s_canvas.setTextColor(TFT_CYAN);
  s_canvas.setCursor(2, y);
  s_canvas.printf("Hosts: %d (%lums)", clients->count, clients->scan_time_ms);
  y += LINE_H + 1;
  s_canvas.drawFastHLine(0, y - 1, 240, TFT_DARKGRAY);

  // 7 filas visibles: IP + MAC + vendor
  const uint8_t max_vis = 7;
  uint8_t end = scroll_pos + max_vis;
  if (end > clients->count) end = clients->count;

  for (uint8_t i = scroll_pos; i < end; i++) {
    const NetClient& c = clients->clients[i];

    // Color: gateway=cyan, otros=blanco
    s_canvas.setTextColor(c.is_gateway ? TFT_CYAN : TFT_WHITE);
    s_canvas.setCursor(2, y);

    // IP corta
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             c.ip[0], c.ip[1], c.ip[2], c.ip[3]);

    // Formato compacto: IP MAC(ultimos 6) vendor
    s_canvas.printf("%-15s %02X:%02X:%02X",
                                ip_str,
                                c.mac[3], c.mac[4], c.mac[5]);

    // Vendor en color distinto si cabe
    if (c.vendor[0] && strcmp(c.vendor, "Unknown") != 0) {
      s_canvas.setTextColor(TFT_YELLOW);
      s_canvas.printf(" %s", c.vendor);
    }

    if (c.is_gateway) {
      s_canvas.setTextColor(TFT_GREEN);
      s_canvas.print(" GW");
    }

    y += LINE_H;
  }

  // Scroll indicator
  if (clients->count > max_vis) {
    s_canvas.setTextColor(TFT_YELLOW);
    s_canvas.setCursor(200, 135 - FOOTER_H + 2);
    s_canvas.printf("%d/%d", scroll_pos + 1, clients->count);
  }

  render_footer();
  push();
}

void ui_render_error(const char* msg) {
  s_canvas.fillScreen(TFT_BLACK);
  s_canvas.setTextColor(TFT_RED);
  s_canvas.setTextSize(2);
  s_canvas.setCursor(50, 30);
  s_canvas.print("ERROR");
  s_canvas.setTextSize(1);
  s_canvas.setTextColor(TFT_WHITE);
  s_canvas.setCursor(2, 70);
  s_canvas.print(msg);
  s_canvas.setTextColor(TFT_GRAY);
  s_canvas.setCursor(2, 110);
  s_canvas.print("[Esc] volver");
  push();
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
