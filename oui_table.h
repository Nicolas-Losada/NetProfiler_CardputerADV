/*
  NetProfiler - Tabla minima OUI -> Fabricante
  Solo OUIs comunes para clasificar movil/router
*/
#ifndef OUI_TABLE_H
#define OUI_TABLE_H

#include <Arduino.h>

struct OuiEntry {
  uint8_t prefix[3];
  const char* vendor;
  bool is_mobile;
};

// Lista compacta: cubre Apple, Samsung, Xiaomi, Huawei, Google, OnePlus,
// y routers comunes (TP-Link, Cisco, Ubiquiti, MikroTik, Asus, Netgear)
static const OuiEntry OUI_TABLE[] PROGMEM = {
  // === Apple (mobile/iPhone hotspot) ===
  {{0x00,0x1B,0x63}, "Apple",       true},
  {{0x14,0x10,0x9F}, "Apple",       true},
  {{0x3C,0x15,0xC2}, "Apple",       true},
  {{0x40,0xA3,0x6B}, "Apple",       true},
  {{0x68,0x96,0x7B}, "Apple",       true},
  {{0x80,0xEA,0x96}, "Apple",       true},
  {{0xA4,0x83,0xE7}, "Apple",       true},
  {{0xD8,0x96,0x95}, "Apple",       true},
  {{0xE4,0x8B,0x7F}, "Apple",       true},
  {{0xF0,0xDB,0xE2}, "Apple",       true},

  // === Samsung (mobile/hotspot) ===
  {{0x00,0x12,0xFB}, "Samsung",     true},
  {{0x08,0x37,0x3D}, "Samsung",     true},
  {{0x1C,0x5A,0x3E}, "Samsung",     true},
  {{0x34,0x14,0x5F}, "Samsung",     true},
  {{0x5C,0x49,0x7D}, "Samsung",     true},
  {{0x78,0x1F,0xDB}, "Samsung",     true},
  {{0xA0,0x21,0xB7}, "Samsung",     true},
  {{0xE8,0xE5,0xD6}, "Samsung",     true},

  // === Xiaomi ===
  {{0x28,0x6C,0x07}, "Xiaomi",      true},
  {{0x50,0x8F,0x4C}, "Xiaomi",      true},
  {{0x64,0xB4,0x73}, "Xiaomi",      true},
  {{0x8C,0xBE,0xBE}, "Xiaomi",      true},
  {{0xC4,0x6A,0xB7}, "Xiaomi",      true},
  {{0xF8,0xA4,0x5F}, "Xiaomi",      true},

  // === Huawei ===
  {{0x00,0x18,0x82}, "Huawei",      true},
  {{0x18,0xCF,0x5E}, "Huawei",      true},
  {{0x44,0x6E,0xE5}, "Huawei",      true},
  {{0x70,0x72,0x3C}, "Huawei",      true},
  {{0xAC,0x6A,0xA3}, "Huawei",      true},

  // === Google Pixel ===
  {{0x3C,0x5A,0xB4}, "Google",      true},
  {{0x60,0x90,0x84}, "Google",      true},
  {{0x94,0xEB,0x2C}, "Google",      true},

  // === OnePlus / Motorola ===
  {{0x64,0xA2,0xF9}, "OnePlus",     true},
  {{0x80,0x6A,0xBD}, "OnePlus",     true},
  {{0x00,0x0E,0xC7}, "Motorola",    true},

  // === Routers: TP-Link ===
  {{0x00,0x14,0x78}, "TP-Link",     false},
  {{0x14,0xCC,0x20}, "TP-Link",     false},
  {{0x50,0xC7,0xBF}, "TP-Link",     false},
  {{0x98,0xDA,0xC4}, "TP-Link",     false},
  {{0xC0,0xC9,0xE3}, "TP-Link",     false},

  // === Routers: Cisco ===
  {{0x00,0x0C,0x29}, "Cisco",       false},
  {{0x00,0x1A,0xA1}, "Cisco",       false},
  {{0x40,0xF4,0xEC}, "Cisco",       false},

  // === Routers: Ubiquiti ===
  {{0x00,0x15,0x6D}, "Ubiquiti",    false},
  {{0x18,0xE8,0x29}, "Ubiquiti",    false},
  {{0xDC,0x9F,0xDB}, "Ubiquiti",    false},

  // === Routers: MikroTik ===
  {{0x00,0x0C,0x42}, "MikroTik",    false},
  {{0x4C,0x5E,0x0C}, "MikroTik",    false},
  {{0xCC,0x2D,0xE0}, "MikroTik",    false},

  // === Routers: Asus ===
  {{0x00,0x1F,0xC6}, "Asus",        false},
  {{0x04,0xD9,0xF5}, "Asus",        false},
  {{0xAC,0x9E,0x17}, "Asus",        false},

  // === Routers: Netgear ===
  {{0x00,0x09,0x5B}, "Netgear",     false},
  {{0x20,0x4E,0x7F}, "Netgear",     false},
  {{0xC4,0x04,0x15}, "Netgear",     false},

  // === ISPs LATAM (modems Movistar, Claro, Tigo, etc) ===
  {{0x00,0x1A,0x2B}, "Huawei ISP",  false},
  {{0xC4,0x6E,0x1F}, "TP-Link ISP", false},
};

static const uint16_t OUI_TABLE_SIZE = sizeof(OUI_TABLE) / sizeof(OuiEntry);

inline void oui_lookup(const uint8_t bssid[6], char* vendor_out, size_t out_size, bool* is_mobile_out) {
  for (uint16_t i = 0; i < OUI_TABLE_SIZE; i++) {
    OuiEntry entry;
    memcpy_P(&entry, &OUI_TABLE[i], sizeof(OuiEntry));
    if (entry.prefix[0] == bssid[0] &&
        entry.prefix[1] == bssid[1] &&
        entry.prefix[2] == bssid[2]) {
      strncpy(vendor_out, entry.vendor, out_size - 1);
      vendor_out[out_size - 1] = 0;
      if (is_mobile_out) *is_mobile_out = entry.is_mobile;
      return;
    }
  }
  strncpy(vendor_out, "Unknown", out_size - 1);
  vendor_out[out_size - 1] = 0;
  if (is_mobile_out) *is_mobile_out = false;
}

#endif
