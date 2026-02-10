#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_timer.h"
#include "get_weather.h"
#include "openweather.h"

#define WEATHER_API_KEY "b1f36b777ec648cc86753529260402"
#define CITY "Tokyo"
#define WEATHER_API_URL "http://api.weatherapi.com/v1/current.json?key=" WEATHER_API_KEY "&q=" CITY "&aqi=no"

#define MAX_HTTP_OUTPUT_BUFFER 2048

static char http_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
static int response_len = 0;

static const char *TAG = "get_weather";

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

esp_err_t parse_weather_response(const char *json_string)
{
    if (json_string == NULL || strlen(json_string) == 0) {
        ESP_LOGE(TAG, "Empty JSON response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Parsing JSON response...");
    
    // Parse JSON
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
        }
        return ESP_FAIL;
    }

    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (current == NULL) {
        ESP_LOGE(TAG, "'current' object not found in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Extract data
    cJSON *temp_c = cJSON_GetObjectItem(current, "temp_c");
    cJSON *feelslike_c = cJSON_GetObjectItem(current, "feelslike_c");
    cJSON *humidity = cJSON_GetObjectItem(current, "humidity");
    cJSON *wind_kph = cJSON_GetObjectItem(current, "wind_kph");
    cJSON *condition = cJSON_GetObjectItem(current, "condition");
    
    // Check if all required fields exist
    if (!cJSON_IsNumber(temp_c) || !cJSON_IsNumber(humidity)) {
        ESP_LOGE(TAG, "Required fields missing in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Get condition text
    const char *condition_text = "Unknown";
    if (condition != NULL) {
        cJSON *text = cJSON_GetObjectItem(condition, "text");
        if (cJSON_IsString(text) && (text->valuestring != NULL)) {
            condition_text = text->valuestring;
        }
    }

    // Update global weather data with mutex protection
    if (xSemaphoreTake(weather_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_weather_data.temperature = temp_c->valuedouble;
        g_weather_data.feels_like = cJSON_IsNumber(feelslike_c) ? feelslike_c->valuedouble : temp_c->valuedouble;
        g_weather_data.humidity = humidity->valueint;
        g_weather_data.wind_speed = cJSON_IsNumber(wind_kph) ? wind_kph->valuedouble : 0.0;
        
        // Safely copy condition string
        strncpy(g_weather_data.condition, condition_text, sizeof(g_weather_data.condition) - 1);
        g_weather_data.condition[sizeof(g_weather_data.condition) - 1] = '\0';
        
        g_weather_data.updated_at = esp_timer_get_time() / 1000; // milliseconds
        
        xSemaphoreGive(weather_mutex);
        
        xEventGroupSetBits(data_events, WEATHER_DATA_READY);
        
        ESP_LOGI(TAG, "Weather parsed successfully:");
        ESP_LOGI(TAG, "  Temperature: %.1f°C (feels like %.1f°C)", 
                 g_weather_data.temperature, g_weather_data.feels_like);
        ESP_LOGI(TAG, "  Condition: %s", g_weather_data.condition);
        ESP_LOGI(TAG, "  Humidity: %d%%", g_weather_data.humidity);
        ESP_LOGI(TAG, "  Wind: %.1f km/h", g_weather_data.wind_speed);
    } else {
        ESP_LOGE(TAG, "Failed to acquire weather mutex");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Clean up
    cJSON_Delete(root);
    return ESP_OK;
}

void weather_task(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = WEATHER_API_URL,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
    };
    while (1) {
        response_len = 0;
        memset(http_response_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
        
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                    status_code,
                    esp_http_client_get_content_length(client));
            
            if (status_code == 200 && response_len > 0) {
                ESP_LOGI(TAG, "Weather data retrieved (%d bytes)", response_len);
                
                // ESP_LOGI(TAG, "Raw JSON: %s", http_response_buffer);
                
                // Parse the JSON response
                if (parse_weather_response(http_response_buffer) == ESP_OK) {
                    ESP_LOGI(TAG, "Weather data parsed and updated successfully");
                } else {
                    ESP_LOGE(TAG, "Failed to parse weather data");
                }
            } else {
                ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
            }
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));
    }
}

