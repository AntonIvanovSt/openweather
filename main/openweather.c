#include <stdio.h>
#include <string.h>
#include "wifi_connect.h"
#include "get_weather.h"
#include "get_time.h"
#include "st7789.h"

static const char *TAG = "openweather";

void app_main(void)
{
    char text[] = "Hello";
    init_lcd(0);
    create_background();
    create_label(&lv_font_montserrat_14, 10, 10, text);

    wifi_connection_task();

    get_weather_data();

    obtain_time();
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        print_current_time();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
