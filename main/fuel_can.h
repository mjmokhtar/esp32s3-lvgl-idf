/*
 * fuel_can.h
 * CAN Bus — Fuel Monitoring System
 * Waveshare ESP32-S3 7" board + Eurosens Dominator + Flow Meter
 * Protocol : J1939 @ 250 kbps, single CAN bus, LISTEN-ONLY
 *
 * Board note:
 *   CAN transceiver is routed through CH422G I2C expander.
 *   GPIO 19 = CAN_TX, GPIO 20 = CAN_RX (enabled when USB_SEL HIGH via CH422G)
 *
 * ┌──────────────────────────────────────────────────────────┐
 *   MASTER SWITCH — 1 = dummy/simulation, 0 = real CAN data
 * └──────────────────────────────────────────────────────────┘
 */

#pragma once

#include "driver/twai.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════
 *  MASTER SWITCH
 * ══════════════════════════════════════════════════════════ */
#define USE_DUMMY_DATA          1   /* 1 = dummy/sim  |  0 = real CAN */

/* ══════════════════════════════════════════════════════════
 *  Board hardware — CH422G I2C expander (same as LCD/touch)
 *  These values match waveshare_rgb_lcd_port.c
 * ══════════════════════════════════════════════════════════ */
#define CAN_I2C_NUM             0           /* same I2C bus as touch */
#define CAN_I2C_SDA             8
#define CAN_I2C_SCL             9
#define CAN_I2C_FREQ_HZ         400000
#define CAN_I2C_TIMEOUT_MS      1000

/* CH422G register writes to enable CAN transceiver (USB_SEL HIGH) */
#define CH422G_ADDR_CONFIG      0x24        /* set output mode */
#define CH422G_ADDR_OUTPUT      0x38        /* set output pins  */
#define CH422G_VAL_OUTPUT_MODE  0x01        /* OE = output */
#define CH422G_VAL_CAN_ENABLE   0x20        /* USB_SEL = HIGH → CAN TX/RX on GPIO19/20 */

/* ══════════════════════════════════════════════════════════
 *  CAN GPIO (active when CH422G USB_SEL = HIGH)
 * ══════════════════════════════════════════════════════════ */
#define CAN_TX_GPIO             GPIO_NUM_19
#define CAN_RX_GPIO             GPIO_NUM_20

/* ══════════════════════════════════════════════════════════
 *  Sensor node addresses (set in Eurosens Configurator)
 *
 *  All sensors on 1 CAN bus — each must have a unique address.
 *  CAN ID suffix = node address byte.
 * ══════════════════════════════════════════════════════════ */
#define SENSOR_ADDR_FUEL        0x01    /* Eurosens Dominator */
#define SENSOR_ADDR_FLOW        0x01    /* Flow Meter         */

/* ══════════════════════════════════════════════════════════
 *  J1939 CAN IDs  (29-bit extended frame)
 *
 *  Eurosens Dominator (addr 01):
 *    0x18FEFC01 → Fuel Level %
 *    0x18FF2901 → Fuel Volume L, Fuel Temp °C, Status byte
 *
 *  Flow Meter (addr 01):
 *    0x18FD0901 → Total consumption L
 *    0x18FF5501 → Flow rate Supply L/h, Flow rate Return L/h
 * ══════════════════════════════════════════════════════════ */
#define CAN_ID_FUEL_LEVEL       (0x18FEFC00 | SENSOR_ADDR_FUEL)   /* 0x18FEFC01 */
#define CAN_ID_FUEL_DATA        (0x18FF2900 | SENSOR_ADDR_FUEL)   /* 0x18FF2901 */
#define CAN_ID_FLOW_TOTAL       (0x18FD0900 | SENSOR_ADDR_FLOW)   /* 0x18FD0901 */
#define CAN_ID_FLOW_RATE        (0x18FF5500 | SENSOR_ADDR_FLOW)   /* 0x18FF5501 */

/* ══════════════════════════════════════════════════════════
 *  PGN byte layout — Eurosens Dominator
 *
 *  0x18FEFC01  8 bytes:
 *    [1]        → Fuel Level %     raw × 0.4 = %
 *
 *  0x18FF2901  8 bytes:
 *    [0..3] LE  → Fuel Volume      raw × 0.001 = L
 *    [6]        → Fuel Temp        raw − 40    = °C
 *    [7]        → Status bitmask
 *
 *  Status bitmask:
 *    bit 0 → Theft detected
 *    bit 1 → Calibration error
 *    bit 2 → Temperature alarm
 *    bit 3 → Sensor hardware fault
 *    bit 4 → CAN timeout
 * ══════════════════════════════════════════════════════════ */
#define FEFC_BYTE_LEVEL         1
#define FEFC_SCALE_LEVEL        0.4f

#define FF29_BYTE_VOL_START     0
#define FF29_SCALE_VOL          0.001f
#define FF29_BYTE_TEMP          6
#define FF29_OFFSET_TEMP        40.0f
#define FF29_BYTE_STATUS        7

#define STATUS_BIT_THEFT        (1 << 0)
#define STATUS_BIT_CALIB_ERR    (1 << 1)
#define STATUS_BIT_TEMP_ALARM   (1 << 2)
#define STATUS_BIT_SENSOR_FAULT (1 << 3)
#define STATUS_BIT_CAN_TIMEOUT  (1 << 4)

/* ══════════════════════════════════════════════════════════
 *  PGN byte layout — Flow Meter
 *
 *  0x18FD0901  8 bytes:
 *    [0..5] LE  → Total consumption   raw × 0.001 = L
 *
 *  0x18FF5501  8 bytes:
 *    [0..1] LE  → Flow rate supply    raw × 0.05 = L/h
 *    [4..5] LE  → Flow rate return    raw × 0.05 = L/h
 * ══════════════════════════════════════════════════════════ */
#define FD09_BYTE_TOTAL_START   0
#define FD09_SCALE_TOTAL        0.001f

#define FF55_BYTE_SUP_START     0
#define FF55_BYTE_RET_START     4
#define FF55_SCALE_FLOW         0.05f

/* ══════════════════════════════════════════════════════════
 *  Timeout — sensor declared dead if silent for N ms
 * ══════════════════════════════════════════════════════════ */
#define CAN_TIMEOUT_MS          3000

/* ══════════════════════════════════════════════════════════
 *  Parsed data struct (written by CAN task, read by HMI task)
 * ══════════════════════════════════════════════════════════ */
typedef struct {
    float    fuel_level_pct;
    float    fuel_volume_liters;
    float    fuel_temp_celsius;
    uint8_t  fuel_status;           /* STATUS_BIT_* bitmask */
    bool     fuel_sensor_alive;

    float    flow_supply_lph;
    float    flow_return_lph;
    float    flow_net_lph;
    float    flow_total_liters;
    bool     flow_sensor_alive;
} can_fuel_data_t;

/* ══════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════ */

/**
 * @brief  Init CH422G + TWAI driver, then start CAN RX task.
 *         If USE_DUMMY_DATA == 1, starts dummy generator instead.
 *         Call from app_main before fuel_hmi_create().
 */
esp_err_t fuel_can_init(void);

/**
 * @brief  Thread-safe copy of latest sensor data.
 *         Always returns valid struct (zeros if not yet received).
 */
void fuel_can_get_data(can_fuel_data_t *out);

#ifdef __cplusplus
}
#endif