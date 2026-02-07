#include <stdio.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "get_time.h"

static const char *TAG = "get_time";

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized!");
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    
    // Set notification callback for time synchronization
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Set SNTP server
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    
    // Set time sync notification callback
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    // Initialize SNTP
    esp_sntp_init();
}

void obtain_time(void)
{
    setenv("TZ", "JST-9", 1);  // Japan Standard Time (UTC+9)
    tzset();

    initialize_sntp();
    
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 15;
    
    char buffer[128];
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count) {
        retry++;
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        
        snprintf(buffer, sizeof(buffer), 
                "Waiting for system time\nto be set...\n\n(%d/%d)", 
                retry, retry_count);
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    
    time(&now);
    localtime_r(&now, &timeinfo);
}

void print_current_time(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);
}
