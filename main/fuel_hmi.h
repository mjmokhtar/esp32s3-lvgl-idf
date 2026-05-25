/*
 * fuel_hmi.h  v2
 * 1 CAN bus only (Eurosens Dominator CAN1)
 * No right status panel
 */

#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_SCREEN_W    800
#define HMI_SCREEN_H    480
#define HMI_TOPBAR_H    38
#define HMI_BOTBAR_H    26
#define HMI_LEFT_W      240
#define HMI_CONTENT_H   (HMI_SCREEN_H - HMI_TOPBAR_H - HMI_BOTBAR_H)

/* Colors */
#define HMI_COL_BG          lv_color_make(13,  13,  13 )
#define HMI_COL_PANEL       lv_color_make(15,  15,  15 )
#define HMI_COL_CARD        lv_color_make(20,  20,  20 )
#define HMI_COL_BORDER      lv_color_make(42,  42,  42 )
#define HMI_COL_ACCENT      lv_color_make(255, 153, 0  )
#define HMI_COL_BLUE        lv_color_make(68,  170, 255)
#define HMI_COL_GREEN       lv_color_make(68,  204, 136)
#define HMI_COL_RED         lv_color_make(255, 85,  85 )
#define HMI_COL_TEXT_PRI    lv_color_make(204, 204, 204)
#define HMI_COL_TEXT_DIM    lv_color_make(102, 102, 102)
#define HMI_COL_TEXT_HINT   lv_color_make(68,  68,  68 )

/* Alarm bits (PGN FF29 status byte) */
#define HMI_ALARM_THEFT         (1 << 0)
#define HMI_ALARM_CALIB_ERR     (1 << 1)
#define HMI_ALARM_TEMP_HIGH     (1 << 2)
#define HMI_ALARM_SENSOR_FAULT  (1 << 3)
#define HMI_ALARM_CAN_TIMEOUT   (1 << 4)

typedef struct {
    /* Eurosens Dominator — CAN1 only */
    float    fuel_level_pct;
    float    fuel_volume_liters;   /* max 4200 L GE U18C */
    float    fuel_temp_celsius;
    uint8_t  sensor_status;        /* bitmask HMI_ALARM_* */
    bool     can1_alive;

    /* Flow meter — also on CAN1 */
    float    flow_supply_lph;
    float    flow_return_lph;
    float    flow_net_lph;
    float    total_consumed_L;

    /* Derived */
    uint32_t est_range_hours;
    uint32_t uptime_seconds;
    char     loco_id[16];
} hmi_fuel_data_t;

void fuel_hmi_create(void);
void fuel_hmi_update(const hmi_fuel_data_t *data);
void fuel_hmi_tick_uptime(uint32_t uptime_sec);

#ifdef __cplusplus
}
#endif