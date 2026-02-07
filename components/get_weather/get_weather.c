#include <stdio.h>
#include <string.h>
#include "get_weather.h"

#define WEATHER_API_KEY "b1f36b777ec648cc86753529260402"
#define CITY "Tokyo"
#define WEATHER_API_URL "http://api.weatherapi.com/v1/current.json?key=" WEATHER_API_KEY "&q=" CITY "&aqi=no"

#define MAX_HTTP_OUTPUT_BUFFER 2048

static char http_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
static int response_len = 0;

static const char *TAG = "WEATHER";

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                     evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (response_len + evt->data_len < MAX_HTTP_OUTPUT_BUFFER) {
                memcpy(http_response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
                http_response_buffer[response_len] = 0;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (response_len > 0) {
                ESP_LOGI(TAG, "Total response length: %d bytes", response_len);
                printf("\n======== Data =========\n%s\n=====================\n\n",
                       http_response_buffer);
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

void get_weather_data(void)
{
    response_len = 0;
    memset(http_response_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
    
    esp_http_client_config_t config = {
        .url = WEATHER_API_URL,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                status_code,
                esp_http_client_get_content_length(client));
        
        if (status_code == 200) {
            ESP_LOGI(TAG, "Weather data successfully retrieved!");
        } else {
            ESP_LOGE(TAG, "HTTP request returned status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}

