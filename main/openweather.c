#include <stdio.h>
#include <string.h>

#include "wifi_connect.h"
#include "get_weather.h"
#include "get_time.h"
#include "get_sensor_data.h"
#include "st7789.h"
#include "buttons.h"
#include "openweather.h"

static const char *TAG = "main";

EventGroupHandle_t data_events;

SemaphoreHandle_t sensor_mutex;
SemaphoreHandle_t time_mutex;
SemaphoreHandle_t weather_mutex;

sensor_data_t g_sensor_data = {0};
time_data_t g_time_data = {0};
weather_data_t g_weather_data = {0};

lv_obj_t *label_co2 = NULL;
lv_obj_t *label_temp = NULL;
lv_obj_t *label_humid = NULL;
lv_obj_t *label_time = NULL;
lv_obj_t *label_info = NULL;
lv_obj_t *label_date = NULL;

lv_obj_t *label_out_temp = NULL;
lv_obj_t *label_out_feels = NULL;
lv_obj_t *label_out_humidity = NULL;
lv_obj_t *label_out_cond = NULL;
lv_obj_t *label_out_wind = NULL;

lv_obj_t *screen_sensor = NULL;
lv_obj_t *screen_info = NULL;
lv_obj_t *screen_weather = NULL;

void app_main(void)
{
    sensor_mutex = xSemaphoreCreateMutex();
    time_mutex = xSemaphoreCreateMutex();
    weather_mutex = xSemaphoreCreateMutex();

    data_events = xEventGroupCreate();

    init_start_screen();

    // xTaskCreate(wifi_connection_task, "wifi_connection_task", 4096, NULL, 6, NULL);

    // EventBits_t bits = xEventGroupWaitBits(
    //     data_events,
    //     WIFI_READY,
    //     pdTRUE,  // Clear bits on exit
    //     pdTRUE,
    //     pdMS_TO_TICKS(20000)
    // );
    //
    // if (bits & WIFI_READY) {
    //
    //     xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    //     xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
    //     xTaskCreate(weather_task, "weather_task", 8192, NULL, 5, NULL);
    //
    //     vTaskDelay(pdMS_TO_TICKS(500));
    //
    //     xTaskCreate(on_off_button_task, "on_off_button_task", 4096, NULL, 3, NULL);
    //     xTaskCreate(next_screen_button_task, "next_screen_button_task", 4096, NULL, 3, NULL);
    // }

    // check_modules_state();

    // while (1) {
    //     bits = xEventGroupWaitBits(
    //         data_events,
    //         SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY,
    //         pdTRUE,  // Clear bits on exit
    //         pdFALSE, // Wait for ANY bit (not all)
    //         portMAX_DELAY
    //     );
    //
    //     char sensor_buffer[64];
    //     if (bits & SENSOR_DATA_READY) {
    //         if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //             ESP_LOGI(TAG, "Processing sensor: CO2=%d ppm, Temp=%.1f°C, Hum=%.1f%%",
    //                     g_sensor_data.co2_ppm, g_sensor_data.temperature, g_sensor_data.humidity);
    //
    //             if (lvgl_port_lock(0)) {
    //                 if (label_co2) {
    //                     if (g_sensor_data.co2_ppm > 1000) {
    //                         lv_obj_set_pos(label_co2, 63, 260);
    //                     } else {
    //                         lv_obj_set_pos(label_co2, 80, 260);
    //                     }
    //                     snprintf(sensor_buffer, sizeof(sensor_buffer), "%d", g_sensor_data.co2_ppm);
    //                     lv_label_set_text(label_co2, sensor_buffer);
    //                 }
    //                 if (label_temp) {
    //                     snprintf(sensor_buffer, sizeof(sensor_buffer), "%.1f", g_sensor_data.temperature);
    //                     lv_label_set_text(label_temp, sensor_buffer);
    //                 }
    //                 if (label_humid) {
    //                     snprintf(sensor_buffer, sizeof(sensor_buffer), "%.1f", g_sensor_data.humidity);
    //                     lv_label_set_text(label_humid, sensor_buffer);
    //                 }
    //                 lvgl_port_unlock();
    //             }
    //
    //             xSemaphoreGive(sensor_mutex);
    //         }
    //     }
    //     char time_buffer[64];
    //     if (bits & TIME_DATA_READY) {
    //         if (xSemaphoreTake(time_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //             ESP_LOGI(TAG, "Processing time: %02d:%02d:%02d",
    //                     g_time_data.timeinfo.tm_hour,
    //                     g_time_data.timeinfo.tm_min,
    //                     g_time_data.timeinfo.tm_sec);
    //
    //             if (lvgl_port_lock(0)) {
    //                 strftime(time_buffer, sizeof(time_buffer), "%I:%M %p", &g_time_data.timeinfo);
    //                 lv_label_set_text(label_time, time_buffer);
    //
    //                 strftime(time_buffer, sizeof(time_buffer), "%Y/%m/%d", &g_time_data.timeinfo);
    //                 lv_label_set_text(label_date, time_buffer);
    //                 lvgl_port_unlock();
    //             }
    //             xSemaphoreGive(time_mutex);
    //         }
    //     }
    //     char weather_buffer[128];
    //     if (bits & WEATHER_DATA_READY) {
    //         if (xSemaphoreTake(weather_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //
    //             ESP_LOGI(TAG, "Processing weather: %.1f°C, %s",
    //                     g_weather_data.temperature, g_weather_data.condition);
    //
    //             if (lvgl_port_lock(0)) {
    //                 // Update temperature
    //                 snprintf(weather_buffer, sizeof(weather_buffer), "%d", 
    //                         (int)g_weather_data.temperature);
    //                 lv_label_set_text(label_out_temp, weather_buffer);
    //
    //                 // Update feels like
    //                 snprintf(weather_buffer, sizeof(weather_buffer), "(%d°C)", 
    //                         (int)g_weather_data.feels_like);
    //                 lv_label_set_text(label_out_feels, weather_buffer);
    //
    //                 // Update humidity
    //                 snprintf(weather_buffer, sizeof(weather_buffer), "%d", 
    //                         g_weather_data.humidity);
    //                 lv_label_set_text(label_out_humidity, weather_buffer);
    //
    //                 // Update condition
    //                 lv_label_set_text(label_out_cond, g_weather_data.condition);
    //
    //                 // Update wind speed
    //                 snprintf(weather_buffer, sizeof(weather_buffer), "%.1f", 
    //                         g_weather_data.wind_speed);
    //                 lv_label_set_text(label_out_wind, weather_buffer);
    //
    //                 lvgl_port_unlock();
    //             }
    //
    //             xSemaphoreGive(weather_mutex);
    //         }
    //     }
    // }
}
