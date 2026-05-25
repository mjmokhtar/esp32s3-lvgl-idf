# Fuel Monitoring HMI — GE U18C Locomotive

| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

| Supported LCD | Waveshare 7" RGB 800×480 (ST7701) |
| ------------- | --------------------------------- |

| Supported Touch | GT911 |
| --------------- | ----- |

| Supported Protocol | CAN J1939 @ 250 kbps |
| ------------------ | -------------------- |

---

## Overview

Industrial HMI dashboard untuk monitoring konsumsi bahan bakar lokomotif **GE U18C** secara real-time. Data dibaca dari **Eurosens Dominator** (fuel level sensor) dan **Flow Meter** via CAN bus J1939, ditampilkan di display Waveshare ESP32-S3 7" RGB menggunakan LVGL v8.

```
Eurosens Dominator (addr 01) ──┐
                               ├── CAN Bus J1939 250kbps ──► ESP32-S3 (LISTEN ONLY)
Flow Meter (addr 01)       ────┘
```

### Tampilan HMI

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ⚡ FUEL MONITORING SYSTEM — GE U18C LOCOMOTIVE        ● CAN BUS OK  19:31:46│
├───────────────┬─────────────────────────────────────────────────────────────┤
│               │  FLOW METER DATA                                            │
│  FUEL LEVEL   │  ┌──────────────┐ ┌──────────────┐                         │
│               │  │ FLOW SUPPLY  │ │ FLOW RETURN  │                         │
│    ████ 68%   │  │  148 L/h     │ │  112 L/h     │                         │
│               │  └──────────────┘ └──────────────┘                         │
│  VOLUME       │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
│  [████░░] 68% │  │ NET CONSUMP. │ │ TOTAL USED   │ │ EST. RANGE   │        │
│  0L    4200L  │  │   36 L/h     │ │   284 L      │ │   79 HRS     │        │
│               │  └──────────────┘ └──────────────┘ └──────────────┘        │
│  TEMP: 32.0°C │                                                             │
│               │  CONSUMPTION TREND — LAST 10 MINUTES                       │
│  CAN BUS:     │  ┌─────────────────────────────────────────────────────┐   │
│  CAN1 ● LIVE  │  │  — SUPPLY    — RETURN    — NET                      │   │
│  J1939 250k   │  │                      chart area                     │   │
│               │  └─────────────────────────────────────────────────────┘   │
│  ⚠ ALARMS     │                                                             │
│  ✓ All normal │                                                             │
├───────────────┴─────────────────────────────────────────────────────────────┤
│ ESP32-S3 | LVGL v8 | RGB 800×480        UPTIME: 00:00:00    WAVESHARE 7"   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware

### Komponen

| Komponen | Detail |
|---|---|
| MCU | ESP32-S3R8 (dual-core LX7 @ 240MHz, 8MB PSRAM) |
| Display | Waveshare ESP32-S3 7" RGB LCD 800×480 (ST7701) |
| Touch | GT911 (I2C, opsional) |
| Fuel Sensor | Eurosens Dominator — CAN J1939 |
| Flow Meter | Generic J1939 Flow Meter |
| CAN Transceiver | Built-in via CH422G I2C expander (GPIO19/20) |

### Koneksi CAN Bus

```
FMC650 / CAN Analyzer
        │
        ├── CAN H ──┬── Eurosens Dominator (addr 01)
        │           └── Flow Meter (addr 01)
        │
        └── CAN L ──┬── Eurosens Dominator (addr 01)
                    └── Flow Meter (addr 01)
                        (120Ω terminator di ujung bus)
```

> **Catatan:** ESP32-S3 menggunakan **TWAI_MODE_LISTEN_ONLY** — tidak mengirim apapun ke bus CAN. Aman disambungkan ke bus aktif tanpa mengganggu komunikasi sensor.

### GPIO

| Fungsi | GPIO | Keterangan |
|---|---|---|
| CAN TX | GPIO 20 | Via CH422G expander (USB_SEL HIGH) |
| CAN RX | GPIO 19 | Via CH422G expander (USB_SEL HIGH) |
| I2C SDA | GPIO 8 | CH422G + GT911 touch |
| I2C SCL | GPIO 9 | CH422G + GT911 touch |
| LCD RGB | GPIO 0,1,2,3,5,7,10,14,17,18,21,38,39,40,41,42,45,46,47,48 | RGB data + sync |

---

## CAN Bus / J1939

### Sensor Addresses

| Sensor | Node Address | Konfigurasi |
|---|---|---|
| Eurosens Dominator | `0x01` | Eurosens Configurator → Configuration CAN → Sensor address |
| Flow Meter | `0x01` | Sesuai datasheet flow meter |

### CAN IDs yang Dibaca

| CAN ID | PGN | Sensor | Data |
|---|---|---|---|
| `0x18FEFC01` | FEFC | Eurosens | Fuel Level % |
| `0x18FF2901` | FF29 | Eurosens | Volume L, Temp °C, Status |
| `0x18FD0901` | FD09 | Flow Meter | Total consumption L |
| `0x18FF5501` | FF55 | Flow Meter | Flow supply L/h, Flow return L/h |

### Parsing Formula

| Parameter | Byte | Formula | Unit |
|---|---|---|---|
| Fuel Level % | `[1]` di `18FEFC01` | `raw × 0.4` | % |
| Fuel Volume | `[0..3]` LE uint32 di `18FF2901` | `raw × 0.001` | L |
| Fuel Temp | `[6]` di `18FF2901` | `raw − 40` | °C |
| Status | `[7]` di `18FF2901` | bitmask | — |
| Total Flow | `[0..5]` LE uint48 di `18FD0901` | `raw × 0.001` | L |
| Flow Supply | `[0..1]` LE uint16 di `18FF5501` | `raw × 0.05` | L/h |
| Flow Return | `[4..5]` LE uint16 di `18FF5501` | `raw × 0.05` | L/h |

### Status Bitmask (byte[7] PGN FF29)

| Bit | Flag | Keterangan |
|---|---|---|
| 0 | `STATUS_BIT_THEFT` | Fuel theft terdeteksi |
| 1 | `STATUS_BIT_CALIB_ERR` | Error kalibrasi sensor |
| 2 | `STATUS_BIT_TEMP_ALARM` | Suhu BBM > 55°C |
| 3 | `STATUS_BIT_SENSOR_FAULT` | Hardware fault sensor |
| 4 | `STATUS_BIT_CAN_TIMEOUT` | CAN bus timeout |

---

## Struktur Project

```
main/
├── CMakeLists.txt              # Build config
├── Kconfig.projbuild           # Menu: Display + CAN Bus GPIO
├── idf_component.yml           # Dependencies: lvgl, esp_lcd_touch_gt911
├── waveshare_rgb_lcd_port.c/h  # Waveshare LCD + touch init
├── lvgl_port.c/h               # LVGL porting layer (RGB double buffer)
├── fuel_can.h                  # CAN config, PGN defines, data struct
├── fuel_can.c                  # CAN RX task (real) / dummy generator
├── fuel_hmi.h                  # HMI API dan data struct
├── fuel_hmi.c                  # LVGL widgets — semua UI
└── main.c                      # Entry point, task orchestration
```

---

## Konfigurasi

### Dummy vs Real Data

Di `fuel_can.h`, baris pertama:

```c
#define USE_DUMMY_DATA  1   /* 1 = simulasi tanpa hardware CAN */
#define USE_DUMMY_DATA  0   /* 0 = real CAN dari sensor */
```

### Sensor Address

Jika address sensor diubah via Eurosens Configurator, update di `fuel_can.h`:

```c
#define SENSOR_ADDR_FUEL  0x01   /* node address Eurosens Dominator */
#define SENSOR_ADDR_FLOW  0x01   /* node address Flow Meter */
```

CAN ID otomatis ikut berubah (suffix = address).

### CAN GPIO

Default sudah sesuai board Waveshare. Jika perlu ubah via `idf.py menuconfig` → **Example Configuration → CAN Bus**, atau langsung di `fuel_can.h`:

```c
#define CAN_TX_GPIO  GPIO_NUM_19
#define CAN_RX_GPIO  GPIO_NUM_20
```

---

## Build & Flash

```bash
# Set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash + monitor
idf.py -p PORT flash monitor
```

> Pertama kali build akan mengunduh dependencies dari component registry ke folder `managed_components/`.

### Alur Testing

```
1. Set USE_DUMMY_DATA 1  →  idf.py build flash
2. Pastikan display tampil dengan data bergerak
3. Set USE_DUMMY_DATA 0  →  idf.py build flash
4. Hubungkan sensor ke CAN bus
5. Pastikan data real masuk (cek Serial Monitor)
```

---

## Dependencies

```yaml
# idf_component.yml
dependencies:
  idf: ">=5.1.0"
  lvgl/lvgl: ">8.3.9,<9"
  esp_lcd_touch_gt911: "^1"
```

---

## Troubleshooting

| Gejala | Penyebab | Solusi |
|---|---|---|
| Build error: font not found | Font LVGL belum diaktifkan | Cek `sdkconfig.defaults` — pastikan `MONTSERRAT_10/14/18/22/28` aktif |
| Display blank | LCD init gagal | Cek log Serial Monitor, pastikan PSRAM enabled |
| CAN data tidak masuk | Baudrate mismatch | Pastikan sensor di-set 250 kbps J1939 |
| Sensor alive = false | Timeout 3 detik | Cek wiring CAN H/L, terminator 120Ω |
| Data acak / garbage | Address sensor konflik | Pastikan tiap sensor punya address unik |
| Alarm theft muncul terus | Status byte bit 0 selalu 1 | Recalibrate sensor via Eurosens Configurator |

---

## Referensi

- [ESP-IDF TWAI Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/twai.html)
- [LVGL v8 Documentation](https://docs.lvgl.io/8.3/)
- [Eurosens Dominator Manual](https://eurosens.com)
- [Waveshare ESP32-S3 7" Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7)
- [SAE J1939 Standard](https://www.sae.org/standards/content/j1939/)