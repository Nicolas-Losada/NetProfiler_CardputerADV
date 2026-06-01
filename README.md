# NetProfiler

Perfilador y clasificador de conexiones WiFi para **M5Stack Cardputer ADV** (ESP32-S3).

Analiza el enlace WiFi, la red local y el backhaul de internet para determinar si la salida es de una **operadora celular** o un **ISP fijo**, con un puntaje de confianza.

![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-teal)
![Device](https://img.shields.io/badge/Device-Cardputer%20ADV-red)
![License](https://img.shields.io/badge/License-MIT-green)

---

## Que hace

NetProfiler se conecta como **cliente (station mode)** a una red WiFi propia (hotspot movil o router domestico) y ejecuta un analisis completo de 6 pasos para recolectar metadata observable del enlace. Con esa informacion, un motor de clasificacion por scoring ponderado emite un veredicto: **CELULAR**, **ISP FIJO** o **INDETERMINADO**.

Es el equivalente a un speedtest + "what's my IP" + analizador de red + descubridor de hosts, todo en un dispositivo portatil.

---

## Hardware

| Componente | Detalle |
|------------|---------|
| Dispositivo | M5Stack Cardputer ADV |
| SoC | ESP32-S3FN8 (dual-core Xtensa LX7, 240 MHz) |
| Memoria | 512 KB SRAM, 8 MB Flash, sin PSRAM |
| Pantalla | IPS 1.14" 240x135 px (ST7789V2) |
| Teclado | 56 teclas, matriz 4x14 |
| WiFi | 2.4 GHz 802.11 b/g/n (WiFi 4) |
| Storage | microSD (FAT32) para logging |

---

## Modulos de analisis

### Modulo A — Enlace WiFi

Recolecta informacion del punto de acceso conectado:

- **SSID** y **BSSID** (direccion MAC del AP)
- **RSSI** (intensidad de senal en dBm)
- **Canal** WiFi (1-13)
- **Cifrado**: OPEN, WPA2, WPA3, WPA2/3
- **Fabricante** del AP derivado de la tabla OUI (Apple, Samsung, Xiaomi, TP-Link, etc.)
- **Conteo de BSSIDs** que comparten el SSID (detecta redes mesh vs hotspot unico)
- Soporte para redes con **SSID oculto**

### Modulo B — Red local (DHCP)

Analiza la configuracion de red asignada por DHCP:

- **IP local** y **mascara de subred** (notacion CIDR)
- **Gateway** e **IPs de DNS**
- **Clasificacion de subred** (discriminador fuerte):
  - `172.20.10.0/28` → iPhone hotspot
  - `192.168.43.0/24` → Android hotspot legacy
  - `192.168.x.x` / `10.x.x.x` → Router/Corporativo
- **Deteccion IPv6** (link-local via SLAAC)

### Modulo C — Internet / Backhaul

Consulta servicios externos para perfilar la conexion de salida:

- **IP publica** via api.ipify.org (configurable)
- **ASN** y **organizacion ISP** via ip-api.com (configurable, respeta limite 45 req/min)
- **Tipo de IP**: mobile, residential, hosting, business
- **Pais** y **region**
- **rDNS / PTR** de la IP publica
- **Flag mobile** de la API de inteligencia de IP
- **Deteccion CGNAT**: IP publica en rango `100.64.0.0/10` (RFC 6598)
- **Deteccion NAT64**: prefijo `64:ff9b::/96` (RFC 6052), indicio de red movil IPv6-first
- **RTT** a Cloudflare (1.1.1.1) y Google (8.8.8.8)
- **Jitter** calculado como diferencia max-min de muestras

### Modulo D — Sondas activas

Pruebas adicionales sobre la conexion:

- **ICMP**: verifica si el ping funciona
- **Portal cautivo**: detecta redireccion de red
- **rDNS**: resolucion inversa de la IP publica

### Modulo E — Hosts en red (ARP scan)

Descubre todos los dispositivos conectados a la misma red:

- **Ping sweep** de toda la subred (adaptado al tamano: /28 = 14 hosts, /24 = 254 hosts)
- Lectura de **tabla ARP de lwIP** para obtener MACs reales
- **IP** y **direccion MAC** de cada dispositivo
- **Fabricante** por OUI lookup
- **Gateway** marcado visualmente
- Re-escaneo bajo demanda

---

## Motor de clasificacion (scoring)

Combina las senales recolectadas en un puntaje ponderado:

| Senal | Peso | Direccion |
|-------|------|-----------|
| Subred iPhone (`172.20.10.0/28`) | +40 | CELULAR |
| Subred Android (`192.168.43.x`) | +35 | CELULAR |
| Subred Router (`192.168.x/10.x`) | +25 | ISP FIJO |
| ASN marcado mobile por API | +50 | CELULAR |
| Tipo IP mobile | +35 | CELULAR |
| Tipo IP residential | +30 | ISP FIJO |
| CGNAT detectado (`100.64/10`) | +25 | CELULAR |
| NAT64 detectado | +20 | CELULAR |
| OUI de fabricante movil | +15 | CELULAR |
| RTT alto (>80ms) o jitter alto (>30ms) | +10 | CELULAR |
| RTT bajo (<30ms) y estable (<10ms jitter) | +15 | ISP FIJO |

**Veredicto:**
- Si `score_mobile - score_fixed > umbral` → **CELULAR**
- Si `score_fixed - score_mobile > umbral` → **ISP FIJO**
- Si diferencia < umbral → **INDETERMINADO**

**Confianza:** `|diferencia| / suma_total * 100`

Todos los pesos son configurables via `config.json` en la microSD.

---

## Interfaz de usuario

### Pantallas

| Pantalla | Descripcion |
|----------|-------------|
| **Boot** | Logo y mensaje de inicio |
| **Scan WiFi** | Lista de redes detectadas (incluye ocultas) con SSID, RSSI, canal, cifrado |
| **Contrasena** | Entrada de clave WPA con teclado fisico |
| **Conectando** | Indicador de progreso de conexion |
| **Analizando** | Barra de progreso con 6 pasos nombrados |
| **Reporte** | 6 pestanas navegables con resultados completos |

### Pestanas del reporte

| # | Pestana | Contenido |
|---|---------|-----------|
| 1 | **Veredicto** | Resultado grande (CELULAR/ISP FIJO), confianza %, scores, razones |
| 2 | **Enlace** | SSID, BSSID, fabricante, RSSI, canal, cifrado, BSSIDs |
| 3 | **Local** | IP local/CIDR, gateway, DNS, clasificacion subred, IPv6 |
| 4 | **Internet** | IP publica, ASN, ISP, tipo, pais, CGNAT, NAT64, RTT, jitter |
| 5 | **Sondas** | ICMP, portal cautivo, rDNS |
| 6 | **Hosts** | Dispositivos en red: IP, MAC, fabricante, gateway marcado |

### Controles

| Tecla | Accion |
|-------|--------|
| `Enter` | Seleccionar / Confirmar |
| `Backspace` | Borrar caracter / Volver atras |
| `,` | Pestana anterior / Izquierda |
| `/` | Pestana siguiente / Derecha |
| `;` | Arriba (navegar lista) |
| `.` | Abajo (navegar lista) |
| `R` | Re-escanear (WiFi o hosts segun contexto) |
| `Tab` | Siguiente pestana |
| `Fn` | Ayuda |
| `` ` `` | Escape / Volver al scan |

---

## Persistencia (microSD)

### Archivos generados

| Archivo | Formato | Contenido |
|---------|---------|-----------|
| `/netprofiler.csv` | CSV | Resumen de cada medicion (una fila por analisis) |
| `/np_XXXXX.json` | JSON | Perfil completo individual con timestamp |

### Configuracion

Archivo `/config.json` en la raiz de la microSD (opcional):

```json
{
  "ip_api_endpoint": "http://ip-api.com/json/?fields=66846719",
  "ping_host_1": "1.1.1.1",
  "ping_host_2": "8.8.8.8",
  "log_to_sd": true,
  "enable_imu": false,
  "enable_audio": false
}
```

Si no existe `config.json`, se usan valores por defecto.

---

## Arquitectura

### FreeRTOS dual-core

```
Core 0: NetWorker (prioridad 5)
  └─ Sondeos de red (no bloquea UI)
  └─ Modulos A/B/C/D/E
  └─ Clasificador
  └─ Escritura SD
  └─ Escribe a ConnProfile (mutex)

Core 1: UI Worker (loop principal)
  └─ Render pantalla
  └─ Lectura teclado
  └─ Lee ConnProfile (mutex)
```

### Estructura de archivos

```
NetProfiler/
├── NetProfiler.ino      # Main: setup, loop, maquina de estados, FreeRTOS
├── conn_profile.h       # Estructura compartida ConnProfile + enums
├── config.h             # Configuracion global con defaults
├── oui_table.h          # Tabla OUI → fabricante (70+ entradas)
├── net_link.h/.cpp      # Modulo A: enlace WiFi
├── net_local.h/.cpp     # Modulo B: DHCP/IP local
├── net_internet.h/.cpp  # Modulo C: IP publica, ASN, CGNAT, RTT
├── net_clients.h/.cpp   # Modulo E: ARP scan, hosts en red
├── classifier.h/.cpp    # Motor de scoring ponderado
├── ui.h/.cpp            # Renderizado pantalla + teclado
├── storage.h/.cpp       # Logging JSON/CSV a microSD
├── NetProfiler.bin      # Binario compilado
└── README.md
```

---

## Instalacion

### Requisitos

- Arduino IDE 2.x o Arduino CLI
- ESP32 Core >= 3.0.0
- Librerias:
  - M5Cardputer
  - M5Unified
  - M5GFX
  - ArduinoJson v7
  - ESP32Ping (GitHub: marian-craciunescu/ESP32Ping)

### Configuracion Arduino IDE

| Opcion | Valor |
|--------|-------|
| Placa | ESP32S3 Dev Module |
| PSRAM | Disabled |
| Flash Size | 8MB |
| Partition Scheme | 8M with spiffs (3MB APP) |
| USB CDC On Boot | Enabled |
| Upload Speed | 115200 |

### Opcion A: Binario precompilado

1. Descarga `NetProfiler.bin`
2. Conecta Cardputer ADV via USB-C
3. Sube con Arduino IDE (Sketch → Subir) o esptool

### Opcion B: Compilar desde fuente

```bash
git clone https://github.com/TU_USUARIO/NetProfiler.git
```

1. Abre `NetProfiler.ino` en Arduino IDE
2. Configura placa como se indica arriba
3. Ctrl+R para compilar, Ctrl+U para subir

---

## Uso

1. **Enciende** el Cardputer ADV
2. **Selecciona** una red WiFi de la lista (tu hotspot o router)
3. **Ingresa** la contrasena con el teclado fisico
4. **Espera** el analisis (6 pasos, ~20-40 segundos)
5. **Navega** las pestanas del reporte con `,` y `/`
6. **Revisa** los hosts conectados en la pestana "Hosts"

### Casos de uso

- **Verificar tipo de conexion**: Hotspot celular vs WiFi fijo
- **Auditar red propia**: Ver dispositivos conectados y sus fabricantes
- **Diagnosticar calidad**: RTT, jitter, CGNAT
- **Documentar**: Exportar perfiles JSON/CSV desde microSD

---

## Limitaciones

- Solo WiFi **2.4 GHz** (limitacion hardware ESP32-S3)
- Hotspots en 5 GHz no seran detectados
- Solo **diagnostico pasivo** de la propia conexion
- NO captura trafico de otros, NO hace deauth, NO recolecta credenciales
- Sin PSRAM: buffers JSON acotados, maximo 20 APs en scan, 32 hosts en ARP
- API ip-api.com free: limite 45 req/min, solo HTTP
- ARP scan en subredes grandes (/16) limitado a primeras 254 IPs

---

## Uso de recursos

| Recurso | Valor |
|---------|-------|
| Flash | 1225 KB (37%) |
| RAM | 54 KB (16%) |
| Stack tarea red | 8 KB |
| APs max en scan | 20 |
| Hosts max en ARP | 32 |
| Entradas OUI | 70+ |

---

## Creditos

- Hardware: [M5Stack Cardputer ADV](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3)
- Librerias: [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX), [ArduinoJson](https://github.com/bblanchon/ArduinoJson), [ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping)
- APIs: [ip-api.com](http://ip-api.com), [ipify.org](https://api.ipify.org)
- Desarrollado con asistencia de Claude (Anthropic)
