#include "lvgl.h"
#include "esp_lvgl_port.h"

#define LCD_HOST           SPI2_HOST
#define LCD_PIXEL_CLK_HZ   (40 * 1000 * 1000)
#define LCD_BK_LIGHT_ON    1
#define LCD_BK_LIGHT_OFF   0

#define PIN_NUM_MOSI       23
#define PIN_NUM_CLK        18
#define PIN_NUM_CS         5
#define PIN_NUM_DC         16
#define PIN_NUM_RST        17
#define PIN_NUM_BK_LIGHT   4

// Display resolution
#define LCD_H_RES          240
#define LCD_V_RES          320

// Display rotation
#define LV_DISP_ROT_NONE   0
#define LV_DISP_ROT_90     1
#define LV_DISP_ROT_180    2
#define LV_DISP_ROT_270    3

// LVGL settings
#define LVGL_TICK_PERIOD_MS 2
#define LVGL_BUFFER_HEIGHT  50

// LVGL colors
#define COLOR_DARK_PURPLE lv_color_make(30, 15, 39)
#define COLOR_WHITE lv_color_make(31, 31, 63)
#define COLOR_ORANGE lv_color_make(0, 27, 35)
#define COLOR_BLACK lv_color_make(0, 0, 0)
#define COLOR_PINK lv_color_make(15, 31, 30)
#define COLOR_GREEN lv_color_make(7, 7, 30)
#define COLOR_CYAN lv_color_make(31, 0, 63)

extern lv_obj_t *label_co2;
extern lv_obj_t *label_temp;
extern lv_obj_t *label_humid;
extern lv_obj_t *label_time;
extern lv_obj_t *label_info;
extern lv_obj_t *label_date;
extern lv_obj_t *label_out_cond;
extern lv_obj_t *label_out_temp;

extern lv_obj_t *screen_sensor;
extern lv_obj_t *screen_info;
extern lv_obj_t *screen_weather;

void create_sensor_screen();
void create_info_screen();
void init_start_screen(void);
void check_modules_state(void);
void create_weather_screen();
