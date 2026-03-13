#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "pass"
#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

esp_err_t w_init(void);
esp_err_t w_connect(char* wifi_ssid, char* wifi_password);
esp_err_t w_disconnect(void);
esp_err_t w_deinit(void);
void wifi_connection_task();
