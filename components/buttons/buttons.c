#include <stdio.h>
#include "buttons.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7789.h"

// GPIO config
#define ON_OFF_BUTTON GPIO_NUM_32
#define NEXT_SCREEN_BUTTON GPIO_NUM_33
#define DEBOUNCE_TIME_MS 50

static int current_screen = 0;  // 0 = sensor screen, 1 = temp graph screen

static const char *TAG = "buttons";

void on_off_button_task(void *arg)
{
   // Configure GPIO as input
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ON_OFF_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    bool last_state = true;
    bool current_state;
    bool screen_state = true;
    
    ESP_LOGI(TAG, "Button monitoring started on GPIO %d", ON_OFF_BUTTON);

    while (1) {
        current_state = gpio_get_level(ON_OFF_BUTTON);
        
        if (last_state && !current_state) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            current_state = gpio_get_level(ON_OFF_BUTTON);
            
            if (!current_state) {
                ESP_LOGI(TAG, "Power button PRESSED");
                
                screen_state = !screen_state;
                
                gpio_set_level(PIN_NUM_BK_LIGHT, screen_state ? 1 : 0);
                ESP_LOGI(TAG, "Backlight %s", screen_state ? "ON" : "OFF");
            }
        }
        
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void next_screen_button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << NEXT_SCREEN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    bool last_state = true;
    bool current_state;

    ESP_LOGI(TAG, "Button monitoring started on GPIO %d", NEXT_SCREEN_BUTTON);

    while (1) {
        current_state = gpio_get_level(NEXT_SCREEN_BUTTON);

        if (last_state && !current_state) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            current_state = gpio_get_level(NEXT_SCREEN_BUTTON);

            if (!current_state) {
                ESP_LOGI(TAG, "Next Button PRESSED");

                // Toggle between screens
                current_screen = (current_screen + 1) % 2;  // Toggle between 0 and 1

                // Switch screen (must be done in LVGL lock)
                if (lvgl_port_lock(0)) {
                    if (current_screen == 0) {
                        lv_screen_load(screen_sensor);
                        ESP_LOGI(TAG, "Switched to Sensor screen");
                    } else {
                        lv_screen_load(screen_weather);
                        ESP_LOGI(TAG, "Switched to Temp screen");
                    }
                    lvgl_port_unlock();
                }
            }
        }

        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
