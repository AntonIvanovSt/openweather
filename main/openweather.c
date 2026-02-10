#include <stdio.h>
#include <string.h>

#include "wifi_connect.h"
#include "get_weather.h"
#include "get_time.h"
#include "get_sensor_data.h"
#include "st7789.h"
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
lv_obj_t *screen_sensor = NULL;
lv_obj_t *screen_info = NULL;
lv_obj_t *label_time = NULL;
lv_obj_t *label_info = NULL;
lv_obj_t *label_date = NULL;

void app_main(void)
{
    sensor_mutex = xSemaphoreCreateMutex();
    time_mutex = xSemaphoreCreateMutex();
    weather_mutex = xSemaphoreCreateMutex();

    data_events = xEventGroupCreate();

    xTaskCreate(wifi_connection_task, "wifi_connection_task", 4096, NULL, 6, NULL);

    EventBits_t bits = xEventGroupWaitBits(
        data_events,
        WIFI_READY,
        pdTRUE,  // Clear bits on exit
        pdTRUE,
        pdMS_TO_TICKS(20000)
    );

    if (bits & WIFI_READY) {

        xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
        xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
        xTaskCreate(weather_task, "weather_task", 8192, NULL, 5, NULL);
    }

    init_start_screen();

    while (1) {
        // Wait for any data update (timeout 30 seconds)
        bits = xEventGroupWaitBits(
            data_events,
            SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for ANY bit (not all)
            pdMS_TO_TICKS(30000)
        );
        
        // Process sensor data
        if (bits & SENSOR_DATA_READY) {
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "Processing sensor: CO2=%d ppm, Temp=%.1f°C, Hum=%.1f%%",
                        g_sensor_data.co2_ppm, g_sensor_data.temperature, g_sensor_data.humidity);
                
                // Do something with sensor data
                // e.g., log to SD card, send to server, update display


                xSemaphoreGive(sensor_mutex);
            }
        }
        if (bits & TIME_DATA_READY) {
            if (xSemaphoreTake(time_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "Processing time: %02d:%02d:%02d",
                        g_time_data.timeinfo.tm_hour,
                        g_time_data.timeinfo.tm_min,
                        g_time_data.timeinfo.tm_sec);
                
                xSemaphoreGive(time_mutex);
            }
        }
        if (bits & WEATHER_DATA_READY) {
            if (xSemaphoreTake(weather_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "Processing weather: %.1f°C, %s",
                        g_weather_data.temperature, g_weather_data.condition);
                // add here screen update
                vTaskDelay(pdMS_TO_TICKS(500));
                xEventGroupClearBits(data_events, WEATHER_DATA_READY);

                xSemaphoreGive(weather_mutex);
            }
        }
    }
}
