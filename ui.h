/*
  UI - 240x135 + teclado Cardputer
*/
#ifndef UI_H
#define UI_H

#include "conn_profile.h"
#include "net_clients.h"

void ui_init();
void ui_render_boot();
void ui_render_scan(const struct WiFiAP* aps, uint8_t n, uint8_t selected, bool scanning);
void ui_render_password(const char* ssid, const char* buf, uint8_t cursor);
void ui_render_connecting(const char* ssid, uint8_t attempt);
void ui_render_analyzing(uint8_t step, const char* step_name);
void ui_render_report(const ConnProfile* p, ReportTab tab);
void ui_render_clients(const ClientScanResult* clients, uint8_t scroll_pos);
void ui_render_error(const char* msg);

struct WiFiAP {
  char ssid[33];
  int8_t rssi;
  uint8_t channel;
  uint8_t enc_idx;
};

// Helpers entrada de teclado
enum UIKey {
  K_NONE = 0,
  K_UP, K_DOWN, K_LEFT, K_RIGHT,
  K_SELECT, K_BACK, K_TAB_NEXT, K_TAB_PREV,
  K_RESCAN, K_HELP, K_CHAR
};

struct UIInput {
  UIKey key;
  char ch;  // si K_CHAR
};

UIInput ui_poll_input();

#endif
