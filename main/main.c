/*
 * main.c
 * Fuel Monitoring HMI — GE U18C Locomotive
 * ESP32-S3 Waveshare 7" RGB 800x480
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

/* NOTE: waveshare_rgb_lcd_port.h defines TAG as static "example".
 * Include it BEFORE defining our own TAG to avoid redefinition. */
#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"
#include "fuel_can.h"
#include "fuel_hmi.h"

/* Use a different symbol name to avoid clash with waveshare header */
#define MAIN_TAG "main"

static uint32_t g_uptime_sec = 0;

static void uptime_cb(void *arg)
{
    g_uptime_sec++;
}

static void hmi_update_task(void *arg)
{
    can_fuel_data_t can;
    hmi_fuel_data_t hmi;

    while (1) {
        fuel_can_get_data(&can);

        hmi.fuel_level_pct     = can.fuel_level_pct;
        hmi.fuel_volume_liters = can.fuel_volume_liters;
        hmi.fuel_temp_celsius  = can.fuel_temp_celsius;
        hmi.sensor_status      = can.fuel_status;
        hmi.can1_alive         = can.fuel_sensor_alive && can.flow_sensor_alive;
        hmi.flow_supply_lph    = can.flow_supply_lph;
        hmi.flow_return_lph    = can.flow_return_lph;
        hmi.flow_net_lph       = can.flow_net_lph;
        hmi.total_consumed_L   = can.flow_total_liters;
        hmi.est_range_hours    = (can.flow_net_lph > 0.5f)
                                 ? (uint32_t)(can.fuel_volume_liters / can.flow_net_lph)
                                 : 9999;
        hmi.uptime_seconds     = g_uptime_sec;
        snprintf(hmi.loco_id, sizeof(hmi.loco_id), "U18C-001");

        if (lvgl_port_lock(50)) {
            fuel_hmi_update(&hmi);
            fuel_hmi_tick_uptime(g_uptime_sec);
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "=== Fuel Monitoring HMI ===");
#if USE_DUMMY_DATA
    ESP_LOGW(MAIN_TAG, "Mode: DUMMY/SIMULATION (USE_DUMMY_DATA=1)");
#else
    ESP_LOGI(MAIN_TAG, "Mode: REAL CAN DATA (USE_DUMMY_DATA=0)");
#endif

    ESP_ERROR_CHECK(fuel_can_init());
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());

    if (lvgl_port_lock(-1)) {
        fuel_hmi_create();
        // lvg_demo_widgets();
        lvgl_port_unlock();
    }
    ESP_LOGI(MAIN_TAG, "HMI ready");

    const esp_timer_create_args_t t_args = {
        .callback = uptime_cb,
        .name     = "uptime",
    };
    esp_timer_handle_t t_handle;
    ESP_ERROR_CHECK(esp_timer_create(&t_args, &t_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(t_handle, 1000000ULL));

    xTaskCreatePinnedToCore(hmi_update_task, "hmi_upd",
                            4096, NULL, 3, NULL, 1);
}