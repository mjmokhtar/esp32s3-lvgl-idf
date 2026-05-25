/*
 * fuel_can.c
 * CAN Bus driver — adapted from Waveshare TWAI example
 *
 * Key differences from original Waveshare example:
 *  - 250 kbps  (J1939 standard, NOT 500 kbps)
 *  - LISTEN_ONLY  (ESP32 never transmits on bus)
 *  - Alert-driven RX loop  (same pattern as Waveshare)
 *  - CH422G enable sequence from waveshare_twai_port.c
 *  - Frames parsed into can_fuel_data_t for HMI layer
 */

#include "fuel_can.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "fuel_can";

static can_fuel_data_t   g_data;
static SemaphoreHandle_t g_mutex;

#if USE_DUMMY_DATA == 0
static int64_t g_ts_fefc = 0;
static int64_t g_ts_ff29 = 0;
static int64_t g_ts_fd09 = 0;
static int64_t g_ts_ff55 = 0;
#endif

/* ── byte helpers ─────────────────────────────────────── */
#if USE_DUMMY_DATA == 0
static inline uint16_t le16(const uint8_t *d, int i)
{ return (uint16_t)d[i] | ((uint16_t)d[i+1]<<8); }

static inline uint32_t le32(const uint8_t *d, int i)
{ return (uint32_t)d[i]|((uint32_t)d[i+1]<<8)|((uint32_t)d[i+2]<<16)|((uint32_t)d[i+3]<<24); }

static inline uint64_t le48(const uint8_t *d, int i)
{ return (uint64_t)d[i]|((uint64_t)d[i+1]<<8)|((uint64_t)d[i+2]<<16)|
         ((uint64_t)d[i+3]<<24)|((uint64_t)d[i+4]<<32)|((uint64_t)d[i+5]<<40); }

static inline bool alive(int64_t ts)
{ return ts && ((esp_timer_get_time()-ts) < ((int64_t)CAN_TIMEOUT_MS*1000)); }
#endif /* USE_DUMMY_DATA == 0 */

/* ══════════════════════════════════════════════════════════
 *  CH422G — enable CAN transceiver (real mode only)
 *  Copy of sequence in waveshare_twai_port.c
 * ══════════════════════════════════════════════════════════ */
#if USE_DUMMY_DATA == 0
static esp_err_t ch422g_can_enable(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = CAN_I2C_SDA,
        .scl_io_num       = CAN_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CAN_I2C_FREQ_HZ,
    };
    i2c_param_config(CAN_I2C_NUM, &conf);

    esp_err_t ret = i2c_driver_install(CAN_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C install: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t buf = CH422G_VAL_OUTPUT_MODE;
    i2c_master_write_to_device(CAN_I2C_NUM, CH422G_ADDR_CONFIG, &buf, 1,
                               CAN_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);

    buf = CH422G_VAL_CAN_ENABLE;
    i2c_master_write_to_device(CAN_I2C_NUM, CH422G_ADDR_OUTPUT, &buf, 1,
                               CAN_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "CH422G: CAN transceiver enabled (GPIO19=TX GPIO20=RX)");
    return ESP_OK;
}
#endif /* ch422g_can_enable — USE_DUMMY_DATA == 0 */

/* ══════════════════════════════════════════════════════════
 *  Frame parser  (real mode only)
 * ══════════════════════════════════════════════════════════ */
#if USE_DUMMY_DATA == 0
static void parse_frame(const twai_message_t *msg)
{
    if (!msg->extd) return;

    const uint8_t *d   = msg->data;
    uint32_t       id  = msg->identifier;
    int64_t        now = esp_timer_get_time();

    xSemaphoreTake(g_mutex, portMAX_DELAY);

    if (id == CAN_ID_FUEL_LEVEL && msg->data_length_code >= 2) {
        g_data.fuel_level_pct = d[FEFC_BYTE_LEVEL] * FEFC_SCALE_LEVEL;
        g_ts_fefc = now;
    }
    else if (id == CAN_ID_FUEL_DATA && msg->data_length_code >= 8) {
        g_data.fuel_volume_liters = le32(d, FF29_BYTE_VOL_START) * FF29_SCALE_VOL;
        g_data.fuel_temp_celsius  = (float)d[FF29_BYTE_TEMP] - FF29_OFFSET_TEMP;
        g_data.fuel_status        = d[FF29_BYTE_STATUS];
        g_ts_ff29 = now;
    }
    else if (id == CAN_ID_FLOW_TOTAL && msg->data_length_code >= 6) {
        g_data.flow_total_liters = (float)le48(d, FD09_BYTE_TOTAL_START) * FD09_SCALE_TOTAL;
        g_ts_fd09 = now;
    }
    else if (id == CAN_ID_FLOW_RATE && msg->data_length_code >= 6) {
        g_data.flow_supply_lph = le16(d, FF55_BYTE_SUP_START) * FF55_SCALE_FLOW;
        g_data.flow_return_lph = le16(d, FF55_BYTE_RET_START) * FF55_SCALE_FLOW;
        g_data.flow_net_lph    = g_data.flow_supply_lph - g_data.flow_return_lph;
        g_ts_ff55 = now;
    }

    g_data.fuel_sensor_alive = alive(g_ts_fefc) && alive(g_ts_ff29);
    g_data.flow_sensor_alive = alive(g_ts_fd09) && alive(g_ts_ff55);

    xSemaphoreGive(g_mutex);
}
#endif

/* ══════════════════════════════════════════════════════════
 *  REAL CAN RX TASK
 *  Alert-driven, same pattern as waveshare_twai_port.c
 * ══════════════════════════════════════════════════════════ */
#if USE_DUMMY_DATA == 0
#define POLLING_RATE_MS 100

static void can_rx_task(void *arg)
{
    ESP_LOGI(TAG, "CAN RX task started (real hardware)");

    while (1) {
        uint32_t alerts;
        twai_read_alerts(&alerts, pdMS_TO_TICKS(POLLING_RATE_MS));

        twai_status_info_t st;
        twai_get_status_info(&st);

        if (alerts & TWAI_ALERT_ERR_PASS)
            ESP_LOGW(TAG, "Alert: controller error-passive");

        if (alerts & TWAI_ALERT_BUS_ERROR)
            ESP_LOGW(TAG, "Alert: bus error, count=%" PRIu32, st.bus_error_count);

        if (alerts & TWAI_ALERT_RX_QUEUE_FULL)
            ESP_LOGW(TAG, "Alert: RX queue full, missed=%" PRIu32 " overrun=%" PRIu32,
                     st.rx_missed_count, st.rx_overrun_count);

        /* Drain all queued frames */
        if (alerts & TWAI_ALERT_RX_DATA) {
            twai_message_t msg;
            while (twai_receive(&msg, 0) == ESP_OK)
                parse_frame(&msg);
        }

        /* Periodic timeout check */
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_data.fuel_sensor_alive = alive(g_ts_fefc) && alive(g_ts_ff29);
        g_data.flow_sensor_alive = alive(g_ts_fd09) && alive(g_ts_ff55);
        xSemaphoreGive(g_mutex);
    }
}
#endif

/* ══════════════════════════════════════════════════════════
 *  DUMMY GENERATOR TASK
 * ══════════════════════════════════════════════════════════ */
#if USE_DUMMY_DATA == 1
static void dummy_task(void *arg)
{
    ESP_LOGI(TAG,  "Dummy task running — simulation mode");
    ESP_LOGW(TAG,  "Set USE_DUMMY_DATA 0 in fuel_can.h for real hardware");

    float vol      = 2856.0f;
    float total    = 0.0f;
    float sup_base = 148.0f;
    uint32_t tick  = 0;

    while (1) {
        sup_base += ((float)(rand() % 100) - 50) * 0.04f;
        if (sup_base < 110.0f) sup_base = 110.0f;
        if (sup_base > 185.0f) sup_base = 185.0f;

        float sup = sup_base + ((float)(rand() % 100) - 50) * 0.1f;
        float ret = 112.0f   + ((float)(rand() % 100) - 50) * 0.08f;
        float net = sup - ret;
        if (net < 0.0f) net = 0.0f;

        float delta = net / 3600.0f;
        vol   -= delta;
        total += delta;
        if (vol < 0.0f) vol = 0.0f;

        float pct  = (vol / 4200.0f) * 100.0f;
        float temp = 32.0f + sinf((float)tick * 0.05f) * 3.0f;
        uint8_t status = (temp > 55.0f) ? STATUS_BIT_TEMP_ALARM : 0x00;

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_data.fuel_level_pct     = pct;
        g_data.fuel_volume_liters = vol;
        g_data.fuel_temp_celsius  = temp;
        g_data.fuel_status        = status;
        g_data.fuel_sensor_alive  = true;
        g_data.flow_supply_lph    = sup;
        g_data.flow_return_lph    = ret;
        g_data.flow_net_lph       = net;
        g_data.flow_total_liters  = total;
        g_data.flow_sensor_alive  = true;
        xSemaphoreGive(g_mutex);

        tick++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

/* ══════════════════════════════════════════════════════════
 *  fuel_can_init
 * ══════════════════════════════════════════════════════════ */
esp_err_t fuel_can_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    if (!g_mutex) return ESP_ERR_NO_MEM;
    memset(&g_data, 0, sizeof(g_data));

#if USE_DUMMY_DATA == 1
    ESP_LOGI(TAG, "USE_DUMMY_DATA=1 — TWAI init skipped");
    xTaskCreatePinnedToCore(dummy_task, "can_dummy", 4096, NULL, 5, NULL, 0);
    return ESP_OK;

#else
    /* 1. Enable CAN transceiver via CH422G */
    ESP_ERROR_CHECK(ch422g_can_enable());

    /* 2. TWAI config
     *    - 250 kbps  (J1939, not 500 kbps like Waveshare example)
     *    - LISTEN_ONLY  (passive — ESP32 never transmits)
     *    - same alert flags as waveshare_twai_port.c               */
    twai_general_config_t g_cfg =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_LISTEN_ONLY);
    g_cfg.rx_queue_len = 32;

    twai_timing_config_t t_cfg = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_cfg, &t_cfg, &f_cfg);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "install: %s", esp_err_to_name(ret)); return ret; }

    ret = twai_start();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "start: %s", esp_err_to_name(ret)); twai_driver_uninstall(); return ret; }

    /* Reconfigure alerts after start (Waveshare pattern) */
    uint32_t alerts = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS |
                      TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
    twai_reconfigure_alerts(alerts, NULL);

    ESP_LOGI(TAG, "TWAI ready: TX=GPIO%d RX=GPIO%d 250kbps LISTEN_ONLY",
             CAN_TX_GPIO, CAN_RX_GPIO);

    xTaskCreatePinnedToCore(can_rx_task, "can_rx", 4096, NULL, 6, NULL, 0);
    return ESP_OK;
#endif
}

/* ══════════════════════════════════════════════════════════
 *  fuel_can_get_data
 * ══════════════════════════════════════════════════════════ */
void fuel_can_get_data(can_fuel_data_t *out)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    memcpy(out, &g_data, sizeof(can_fuel_data_t));
    xSemaphoreGive(g_mutex);
}