# NetProfiler v1.1

Perfilador y clasificador de la **propia conexion** de un **M5Stack Cardputer ADV** (ESP32-S3FN8).

Conecta como station a un AP propio o autorizado y diagnostica el enlace local + el backhaul de internet. Clasifica si la salida es de **operadora celular**, **ISP fijo** o **indeterminada** con un puntaje de confianza, y descubre los hosts de la LAN (estilo `arp -a` / `nmap -sn`).

Solo diagnostico pasivo. No incluye captura de trafico ajeno, modo monitor, deauth ni nada ofensivo.

![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-teal)
![Device](https://img.shields.io/badge/Device-Cardputer%20ADV-red)
![Version](https://img.shields.io/badge/Version-1.1-brightgreen)

---

## Diferencia con WiRa

| Aspecto | WiRa | NetProfiler |
|---------|------|-------------|
| Foco | Radar WiFi + medidor RSSI + profiler basico | **Profiler completo + LAN scan** |
| Arquitectura | Monohilo | **FreeRTOS dual-core** |
| Modulos analisis | A + B + C | **A + B + C + D + E** |
| Reporte | 1 pantalla | **6 pestanas navegables** |
| Hosts LAN | no | **ARP + ping sweep con budget 20s** |
| Sondas | no | **Portal cautivo, NAT64 (no concluyente), ICMP** |
| OUI table | basica | **70+ entradas** |
| Pesos clasificador | hardcoded | **configurables via config.json en SD** |
| Audio feedback | no | **ES8311 opcional** |
| Persistencia | sin nombre seguro | **indice incremental np_0001.json** |

---

## Hardware objetivo

| Componente | Detalle |
|------------|---------|
| Dispositivo | M5Stack Cardputer ADV |
| SoC | ESP32-S3FN8 (dual-core LX7, 240 MHz) |
| Memoria | 512 KB SRAM, 8 MB Flash, **sin PSRAM** |
| Pantalla | IPS 1.14" 240x135 (ST7789V2) |
| Teclado | 56 teclas matriz 4x14 |
| WiFi | 2.4 GHz 802.11 b/g/n |
| Storage | microSD SPI (CS=GPIO5, MOSI=14, MISO=39, SCK=40) |
| Audio | ES8311 codec + altavoz 1W (opcional) |

---

## Maquina de estados

7 estados (`enum AppState` en `conn_profile.h`):

| Estado | Funcion |
|--------|---------|
| `ST_BOOT` | Splash inicial |
| `ST_SCAN` | Lista de redes WiFi detectadas |
| `ST_PASSWORD` | Entrada de contrasena con teclado fisico |
| `ST_CONNECTING` | Intento de asociacion |
| `ST_ANALYZING` | Ejecucion del perfil (5 pasos) |
| `ST_REPORT` | Reporte con 6 pestanas navegables |
| `ST_ERROR` | Mensaje de error + opcion volver |

---

## Arquitectura FreeRTOS

```
Core 1 (loop principal):              Core 0 (net_task_fn):
+-------------------+                  +-----------------------+
| - M5Cardputer.update                 | - net_link_collect    |
| - keyboard input                     | - net_local_collect   |
| - state machine                      | - net_internet_collect|
| - ui_render_*                        | - classifier_run      |
+-------------------+                  | - storage_log_profile |
         ^                             | - net_clients_scan    |
         |  mutex                      |   (bajo demanda)      |
         +-----> g_profile <-----------+
```

Tarea de red corre en Core 0 con stack 8 KB. La UI nunca se bloquea por sondeos.

---

## Modulos del proyecto

```
NetProfiler/
├── NetProfiler.ino           # Main: setup, loop, FreeRTOS task, scan/connect
├── conn_profile.h            # Struct ConnProfile + enums (estados, tabs, veredict)
├── config.h                  # Config global (endpoints API, pesos, flags opcionales)
├── oui_table.h               # 70+ OUIs Apple/Samsung/Xiaomi/TP-Link/Cisco/etc
├── net_link.h/.cpp           # Modulo A: enlace WiFi (sin re-scan estando conectado)
├── net_local.h/.cpp          # Modulo B: DHCP/IP local
├── net_internet.h/.cpp       # Modulo C: IP publica, ASN, CGNAT, ping, portal cautivo, NAT64
├── net_clients.h/.cpp        # Modulo E: descubrimiento hosts LAN (ARP + ping)
├── classifier.h/.cpp         # Motor de scoring ponderado
├── ui.h/.cpp                 # UI 240x135 con sprite anti-flicker
├── storage.h/.cpp            # microSD JSON + CSV + config.json
├── audio.h/.cpp              # ES8311 feedback (opcional, off por defecto)
└── NetProfiler.bin           # Binario compilado
```

---

## Modulos de analisis

### A - Enlace WiFi local (`net_link.cpp`)
Recibe los datos del AP del scan PRE-conexion (no re-escanea estando conectado, que rompia la asociacion en ESP32-S3). Recolecta:
- SSID, BSSID, fabricante OUI
- RSSI, canal
- Cifrado: OPEN/WEP/WPA/WPA2/WPA3/WPA2-3/ENT
- `bssid_count`: cuantos BSSID comparten el mismo SSID (deteccion mesh)

### B - Red local (`net_local.cpp`)
- IP local, gateway, DNS1/2, mascara
- CIDR (`__builtin_popcount`)
- Clasificacion subnet:
  - `172.20.10.0/28` -> "iPhone hotspot"
  - `192.168.43.0/24` -> "Android hotspot"
  - `192.168.x` con `/28+` -> "Posible hotspot"
  - `192.168.x` o `10.x` -> "Router/Hotspot"
  - `10.x` resto -> "Router/Corp"
- IPv6 link-local

### C - Internet (`net_internet.cpp`)
- Ping a 1.1.1.1 (`net_internet_check_reachability`)
- IP publica + ASN + ISP + tipo via [ip-api.com](http://ip-api.com) plan gratuito (45 req/min)
  - Filtro de campos con ArduinoJson v7 para ahorrar RAM (sin PSRAM)
- Pais, region, rDNS
- Flag `mobile` de la API
- **CGNAT detection**: IP publica en rango `100.64.0.0/10` (RFC 6598)
- **NAT64 detection**: implementacion conservadora - marca **N/D** si no se puede verificar el prefijo `64:ff9b::/96` (RFC 6052) directamente
- **Portal cautivo**: pide `http://connectivitycheck.gstatic.com/generate_204` con `setFollowRedirects(DISABLE)` - si la respuesta no es 204, hay portal
- RTT a 1.1.1.1 y 8.8.8.8 (4 muestras cada uno)
- Jitter = max-min de muestras

### D - Sondas (`net_internet.cpp`)
- `icmp_works`: el ping a 1.1.1.1 retorno OK
- `captive_portal`: detectado via generate_204
- `nat64_detected`: false o **N/D** (limitacion IPv6 en core 3.x)

### E - Hosts en la LAN (`net_clients.cpp`)
**Bajo demanda** (no se ejecuta en el analisis principal):
- Se dispara al pulsar `R` en la pestaña `TAB_CLIENTS`
- **Budget global 20s** - aborta limpiamente si se excede
- **Fase 1**: ARP broadcast a TODAS las IPs de la subnet (rapido, ~500us/host)
- **Fase 2**: ping sweep complementario (solo IPs no encontradas en ARP)
  - En subnets `/24` o mayores: limitado a 60 hosts para no exceder budget
- Lee la tabla ARP de lwIP (`etharp_get_entry`) para extraer MAC reales
- OUI lookup para cada MAC

Hasta **32 hosts** simultaneos.

---

## Motor de scoring (clasificador)

Pesos por defecto en `config.h` (sobrescribibles desde `config.json` en SD):

| Senal | Peso | Direccion |
|-------|------|-----------|
| Subnet iPhone `172.20.10.0/28` | +40 | CELULAR |
| Subnet Android `192.168.43.x` | +35 | CELULAR |
| Subnet "Router/AP" | +25 | ISP FIJO |
| Flag `mobile` de ip-api.com | +50 | CELULAR |
| Tipo IP `mobile` | +35 | CELULAR |
| Tipo IP `residential` | +30 | ISP FIJO |
| CGNAT detectado | +25 | CELULAR |
| NAT64 detectado | +20 | CELULAR |
| OUI del BSSID es de fabricante movil | +15 | CELULAR |
| RTT > 80ms o jitter > 30ms | +10 | CELULAR |
| RTT < 30ms y jitter < 10ms | +15 | ISP FIJO |

**Veredicto:**
- `diff = score_mobile - score_fixed`
- `diff > 20` -> **CELULAR**
- `diff < -20` -> **ISP FIJO**
- en medio -> **INDETERMINADO**

**Confianza:** `|diff| / (score_mobile + score_fixed) * 100`, capada a 100.

El campo `verdict_reasons` acumula hasta 256 chars con las razones legibles del veredicto.

---

## Reporte (6 pestanas)

Navegacion: `,` anterior, `/` siguiente (en `TAB_CLIENTS` tambien `;` `.` para scroll).

| # | Pestana | Contenido |
|---|---------|-----------|
| 1 | **Veredicto** | Etiqueta grande + confianza % + scores Mob/Fix + razones |
| 2 | **Enlace** | SSID, BSSID, fabricante OUI, RSSI, canal, cifrado, conteo BSSID |
| 3 | **Local** | IP local/CIDR, gateway, DNS, clasificacion subnet, IPv6 |
| 4 | **Internet** | IP publica, ASN, ISP, tipo, pais, CGNAT, **NAT64 N/D**, RTT CF/G, jitter |
| 5 | **Sondas** | ICMP, portal cautivo, rDNS |
| 6 | **Hosts** | Lista LAN: IP + MAC + vendor + gateway, scroll arriba/abajo |

**Tab bar centrada**: indicador `< N/6 Nombre >` que muestra solo la pestaña activa por nombre completo (`textWidth()` real para centrar).

**Dirty flag**: el reporte solo se redibuja cuando cambia la pestaña, llegan datos nuevos o hay input. No redibuja cada 40ms del loop.

---

## Persistencia (microSD)

### Pinout
```
CS   = GPIO 5      (corregido en v1.1; antes era 12)
MOSI = GPIO 14
MISO = GPIO 39
SCK  = GPIO 40
```
Fuente: [docs.m5stack.com/en/core/Cardputer-Adv](https://docs.m5stack.com/en/core/Cardputer-Adv)

### Archivos generados

| Archivo | Formato | Contenido |
|---------|---------|-----------|
| `/netprofiler.csv` | CSV | Una fila por perfil (timestamp, SSID, veredict, IP publica, ASN, RTT, etc.) |
| `/np_NNNN.json` | JSON | Perfil completo. NNNN = indice incremental buscado al iniciar |
| `/config.json` | JSON | **Opcional**: sobreescribe endpoints, pesos del clasificador, flags |

### Ejemplo `config.json`

```json
{
  "ip_api_endpoint": "http://ip-api.com/json/?fields=66846719",
  "ping_host_1": "1.1.1.1",
  "ping_host_2": "8.8.8.8",
  "log_to_sd": true,
  "enable_imu": false,
  "enable_audio": true,
  "scoring_weights": {
    "subnet_iphone": 40,
    "subnet_android": 35,
    "subnet_router": 25,
    "asn_mobile": 50,
    "iptype_mobile": 35,
    "iptype_residential": 30,
    "cgnat": 25,
    "nat64": 20,
    "oui_mobile": 15,
    "rtt_high": 10,
    "rtt_low_stable": 15,
    "verdict_threshold": 20
  }
}
```

---

## Controles

### En `ST_SCAN` (lista WiFi)
| Tecla | Accion |
|-------|--------|
| `;` | Arriba |
| `.` | Abajo |
| `Enter` | Seleccionar AP (entrar a password o conectar si abierto) |
| `r/R` | Re-escanear redes |

### En `ST_PASSWORD` (modo raw)
| Tecla | Accion |
|-------|--------|
| Cualquier char ASCII | Insertar literal (`, . ; /` incluidos) |
| `Bksp` | Borrar caracter |
| `Enter` | Confirmar y conectar |
| `` ` `` | Cancelar |
| `Fn` | Alternar mostrar/ocultar password con asteriscos |

### En `ST_REPORT` (todas las pestanas)
| Tecla | Accion |
|-------|--------|
| `,` | Pestana anterior |
| `/` | Pestana siguiente |
| `Tab` | Pestana siguiente (alternativa) |
| `r/R` (no en TAB_CLIENTS) | Re-perfilar |
| `Bksp` o `` ` `` | Volver al scan |

### En `TAB_CLIENTS` (hosts LAN)
| Tecla | Accion |
|-------|--------|
| `;` | Scroll arriba |
| `.` | Scroll abajo |
| `r/R` | Disparar scan LAN (hasta 20s) |
| `,` / `/` | Cambiar pestana |

---

## UI con sprite anti-flicker

- `M5Canvas s_canvas` 240x135 @ 16bpp en SRAM (~65 KB)
- Fallback automatico a 8bpp si no hay heap suficiente
- Todas las vistas dibujan en sprite y hacen `pushSprite(0, 0)` al final
- Bateria mostrada en esquina superior de cada vista (verde >50%, amarillo >20%, rojo <=20%) via `M5.Power.getBatteryLevel()`

---

## Audio (opcional)

Si `enable_audio = true` en `config.json`:
- Beep al terminar el analisis (tono segun veredicto)
- Tono ascendente para CELULAR
- Tono descendente para ISP FIJO
- Beep medio para INDETERMINADO

Usa `M5Cardputer.Speaker` (M5Unified expone el ES8311 del ADV).

---

## Instalacion

### Requisitos

- Arduino IDE 2.x o Arduino CLI
- ESP32 Core >= 3.0.0
- Librerias:
  - M5Cardputer
  - M5Unified
  - M5GFX
  - **ArduinoJson v7**
  - ESP32Ping (GitHub: marian-craciunescu/ESP32Ping)

### Configuracion Arduino IDE

| Opcion | Valor obligatorio |
|--------|-------------------|
| Placa | **ESP32S3 Dev Module** |
| PSRAM | **Disabled** |
| Flash Size | 8MB |
| Partition Scheme | 8M with spiffs (3MB APP / 1.5MB SPIFFS) |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240MHz |

### Cargar binario precompilado

1. Descarga `NetProfiler.bin` del repositorio
2. Conecta el Cardputer ADV via USB-C
3. Arduino IDE: Herramientas -> Placa -> ESP32S3 Dev Module
4. Sketch -> Subir (Ctrl+U)

O por linea de comandos:
```bash
esptool.py --chip esp32s3 --port COM3 write_flash 0x0 NetProfiler.bin
```

### Compilar desde fuente

```bash
git clone https://github.com/Nicolas-Losada/NetProfiler_CardputerADV.git
cd NetProfiler_CardputerADV
# Abrir NetProfiler.ino en Arduino IDE, configurar placa, Ctrl+U
```

---

## Uso de recursos

| Recurso | Valor |
|---------|-------|
| Flash | **1267 KB (38%)** de 3.3 MB |
| RAM global | **55 KB (16%)** de 327 KB |
| Sprite canvas | ~65 KB SRAM |
| Stack tarea red | 8 KB |
| MAX_NETWORKS scan | 20 (g_aps) |
| MAX_CLIENTS ARP | 32 |
| OUI table | 70+ entradas en PROGMEM |
| Verdict reasons buffer | 256 chars |

---

## Decisiones de diseño

1. **Scan WiFi solo PRE-conexion** - re-escanear estando conectado tumbaba la asociacion en ESP32-S3
2. **Sprite 16bpp con fallback 8bpp** - cabe en SRAM con margen (65 KB de 250 KB libres)
3. **NAT64 conservador** - el core Arduino-ESP32 no expone IPv6 globales facilmente; mejor marcar N/D que falso negativo
4. **Hosts bajo demanda** - el ARP/ping sweep tarda hasta 20s; no debe bloquear el flujo de analisis principal
5. **FreeRTOS dual-core** - red en Core 0, UI en Core 1, con mutex para `g_profile`
6. **ip-api.com HTTP** - el plan gratuito no soporta TLS; respetar limite 45 req/min
7. **Tab bar Opcion A** - indicador centrado `< N/6 Nombre >` permite nombres completos legibles en 240 px

---

## Limitaciones conocidas

- Solo WiFi **2.4 GHz** (hardware ESP32-S3)
- Hotspots 5 GHz invisibles
- ip-api.com plan gratuito: 45 req/min, solo HTTP (sin TLS)
- NAT64 marcado como **N/D** (limitacion del core para inspeccionar IPv6 globales)
- ARP scan en subnets `/16` o mayores limitado a primeras 254 IPs
- Sin RTC: archivos JSON usan indice incremental (`np_0001.json`), no fecha real
- Si el AP cambia BSSID entre scan y conexion, el primer intento dirigido falla; hay reintento sin BSSID

---

## Bugs corregidos en v1.1

| Bug | Severidad | Fix |
|-----|-----------|-----|
| `net_link_collect()` escaneaba WiFi estando conectado y tumbaba la asociacion | Critico | Recibe `WiFiAP` del scan PRE-conexion |
| ARP/ping sweep podia tardar minutos congelando UI | Critico | Budget global 20s + ping limitado en /24+ |
| Hosts se escaneaban siempre aunque no se mire la pestana | Alto | Movido a disparo bajo demanda (`R` en TAB_CLIENTS) |
| Tab bar de 6 pestanas no cabia en 240 px | Alto | Indicador centrado con `textWidth()` real |
| Pantalla parpadeaba al redibujar | Alto | Sprite `M5Canvas` con `pushSprite` |
| Portal cautivo y NAT64 anunciados pero no implementados | Alto | Sonda real `generate_204`; NAT64 marcado N/D |
| Pesos clasificador no configurables | Alto | Cargados desde `config.json` |
| **SD_CS = 12** (incorrecto) | Critico | **Corregido a GPIO 5** (fuente: docs M5) |
| `StaticJsonDocument` deprecado en ArduinoJson v7 | Warning | Migrado a `JsonDocument` |
| `createNestedObject()` deprecado | Warning | `doc["x"].to<JsonObject>()` |
| Typo "scaneando" | Cosmetico | Corregido a "escaneando" |
| JSON con timestamp colisiona sin RTC | Medio | Indice incremental `np_0001.json` |
| `same_status_count` no usado | Limpieza | Eliminado |

---

## Solucion de problemas

### microSD no se detecta
- Verifica FAT32 (no exFAT, no NTFS)
- Confirma microSD <= 32 GB
- En v1.0 el CS era GPIO 12 (incorrecto) - actualizar a v1.1+

### El AP de mi celular no aparece
- Verifica que el hotspot esta en **2.4 GHz**
- Algunos hotspots usan canal 12/13 (latam) - el firmware ya hace `setCountry` permisivo
- Activa el hotspot ANTES de pulsar `R` en scan

### "Sin internet (N/D)" en pestana Internet aun conectado
- El ping a 1.1.1.1 falla - el AP puede tener firewall ICMP
- ip-api.com puede haber respondido con error (rate limit alcanzado)
- Espera 1-2 minutos y pulsa `R` en el reporte para re-perfilar

### Pestana Hosts vacia
- Pulsa `R` en esa pestana - el scan es bajo demanda, no automatico
- Espera hasta 20s (budget duro)
- Verifica que tu red tiene otros dispositivos activos

### Compilacion falla "Wrong chip" al subir
- Selecciona **ESP32S3 Dev Module**, no ESP32 generico
- Reset al modo bootloader: mantener BOOT, presionar RESET, soltar BOOT

---

## Creditos

- Hardware: [M5Stack Cardputer ADV](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3)
- Librerias: [M5Cardputer](https://github.com/m5stack/M5Cardputer), [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX), [ArduinoJson](https://github.com/bblanchon/ArduinoJson), [ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping)
- APIs externas: [ip-api.com](http://ip-api.com), [ipify.org](https://api.ipify.org), [connectivitycheck.gstatic.com](http://connectivitycheck.gstatic.com)
- Pinout SD: [docs.m5stack.com/en/core/Cardputer-Adv](https://docs.m5stack.com/en/core/Cardputer-Adv)
- Desarrollado con asistencia de Claude (Anthropic)

---

## Licencia

MIT - libre para uso educativo y de diagnostico de redes propias o autorizadas.
