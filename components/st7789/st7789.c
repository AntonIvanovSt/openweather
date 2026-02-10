#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "openweather.h"
#include "st7789.h"

static const char *TAG = "LVGL_DEMO";

static lv_disp_t *lvgl_disp = NULL;

void init_lcd(int rotation)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    static esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Configure backlight
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON);

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LVGL_BUFFER_HEIGHT * sizeof(uint16_t),
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    // Rotate display to portrait mode if needed
    lv_disp_set_rotation(lvgl_disp, rotation); // 0 - no rotation


    ESP_LOGI(TAG, "Setup complete");
}

void create_background(void)
{
    if (lvgl_port_lock(0)) {

        lv_obj_t *screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

        lvgl_port_unlock();
    }
}


void create_label(const lv_font_t *font, int x, int y, char *text)
{
    if (lvgl_port_lock(0)) {

        lv_obj_t *label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, text);

        lv_obj_set_style_text_font(label, font, 0);

        lv_obj_set_style_text_color(label, lv_color_make(31, 31, 63), 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
 
        lv_obj_set_pos(label, x, y);

        lvgl_port_unlock();
    }
}

static void lvgl_update_timer_cb(lv_timer_t *timer)
{
    char text_buffer[64];
    if (lvgl_port_lock(0)) {
        if (label_co2) {
            if (g_sensor_data.co2_ppm > 1000) {
                lv_obj_set_pos(label_co2, 63, 260);
            } else {
                lv_obj_set_pos(label_co2, 80, 260);
            }
            snprintf(text_buffer, sizeof(text_buffer), "%d", g_sensor_data.co2_ppm);
            lv_label_set_text(label_co2, text_buffer);
        }
        
        if (label_temp) {
            snprintf(text_buffer, sizeof(text_buffer), "%.1f", g_sensor_data.temperature);
            lv_label_set_text(label_temp, text_buffer);
        }
        
        if (label_humid) {
            snprintf(text_buffer, sizeof(text_buffer), "%.1f", g_sensor_data.humidity);
            lv_label_set_text(label_humid, text_buffer);
        }
        // strftime(text_buffer, sizeof(text_buffer), "%I:%M %p", &g_time_data);
        // lv_label_set_text(label_time, text_buffer);
        //
        // strftime(text_buffer, sizeof(text_buffer), "%Y/%m/%d", &g_time_data);
        // lv_label_set_text(label_date, text_buffer);
        lvgl_port_unlock();
    }
}
// in lv_color_make order is BRG with RGB565 notation
// max values are: 31, 31, 63
void create_sensor_co2(const lv_font_t *font_label, const lv_font_t *font_value)
{
    // CO2 Label and Value
    lv_obj_t *label_co2_text = lv_label_create(screen_sensor);  // Changed from lv_screen_active()
    lv_label_set_text(label_co2_text, "CO2");
    lv_obj_set_style_text_font(label_co2_text, font_label, 0);
    lv_obj_set_style_text_color(label_co2_text, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_co2_text, 10, 250);
    
    lv_obj_t *label_co2_mark = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_co2_mark, "ppm");
    lv_obj_set_style_text_font(label_co2_mark, font_label, 0);
    lv_obj_set_style_text_color(label_co2_mark, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_co2_mark, 190, 250);

    label_co2 = lv_label_create(screen_sensor);  // Changed from lv_screen_active()
    lv_label_set_text(label_co2, "   -- ");
    lv_obj_set_style_text_font(label_co2, font_value, 0);
    lv_obj_set_style_text_color(label_co2, COLOR_ORANGE, 0);
    lv_obj_set_pos(label_co2, 70, 260);
}

void create_sensor_temp(const lv_font_t *font_mark, const lv_font_t *font_label, const lv_font_t *font_value)
{
    // Temperature Label and Value
    lv_obj_t *label_temp_text = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_temp_text, "湿度");
    lv_obj_set_style_text_font(label_temp_text, font_label, 0);
    lv_obj_set_style_text_color(label_temp_text, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_temp_text, 10, 170);
    
    lv_obj_t *label_temp_mark = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_temp_mark, "°C");
    lv_obj_set_style_text_font(label_temp_mark, font_mark, 0);
    lv_obj_set_style_text_color(label_temp_mark, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_temp_mark, 80, 170);

    label_temp = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_temp, " -- ");
    lv_obj_set_style_text_font(label_temp, font_value, 0);
    lv_obj_set_style_text_color(label_temp, COLOR_ORANGE, 0);
    lv_obj_set_pos(label_temp, 5, 200);
}

void create_sensor_hum(const lv_font_t *font_mark, const lv_font_t *font_label, const lv_font_t *font_value)
{
    // Humidity Label and Value
    lv_obj_t *label_humid_text = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_humid_text, "温度");
    lv_obj_set_style_text_font(label_humid_text, font_label, 0);
    lv_obj_set_style_text_color(label_humid_text, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_humid_text, 130, 170);
    
    lv_obj_t *label_humid_mark = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_humid_mark, "%");
    lv_obj_set_style_text_font(label_humid_mark, font_mark, 0);
    lv_obj_set_style_text_color(label_humid_mark, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_humid_mark, 210, 170);

    label_humid = lv_label_create(screen_sensor);  // Changed
    lv_label_set_text(label_humid, " -- ");
    lv_obj_set_style_text_font(label_humid, font_value, 0);
    lv_obj_set_style_text_color(label_humid, COLOR_ORANGE, 0);
    lv_obj_set_pos(label_humid, 125, 200);
}

void create_sensor_lines()
{
    // Create line
    static lv_point_precise_t line_points1[] = {
        {0, 0},      // Start point (x, y)
        {240, 0}     // End point (x, y)
    };
    static lv_point_precise_t line_points2[] = {
        {0, 0},      // Start point (x, y)
        {0, 80}     // End point (x, y)
    };

    // Create line object
    lv_obj_t *line1 = lv_line_create(screen_sensor);
    lv_line_set_points(line1, line_points1, 2);

    // Position the line
    lv_obj_set_pos(line1, 0, 160);  // Position where line starts

    // Style the line
    lv_obj_set_style_line_width(line1, 2, 0);
    lv_obj_set_style_line_color(line1, COLOR_ORANGE, 0);

    // Create line object
    lv_obj_t *line2 = lv_line_create(screen_sensor);
    lv_line_set_points(line2, line_points1, 2);

    // Position the line
    lv_obj_set_pos(line2, 0, 240);  // Position where line starts

    // Style the line
    lv_obj_set_style_line_width(line2, 2, 0);
    lv_obj_set_style_line_color(line2, COLOR_ORANGE, 0);

    // Create line object
    lv_obj_t *line3 = lv_line_create(screen_sensor);
    lv_line_set_points(line3, line_points2, 2);

    // Position the line
    lv_obj_set_pos(line3, 120, 160);  // Position where line starts

    // Style the line
    lv_obj_set_style_line_width(line3, 2, 0);
    lv_obj_set_style_line_color(line3, COLOR_ORANGE, 0);

}

void create_info_screen()
{
    screen_info = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_info, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_info, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *rect = lv_obj_create(screen_info);
 
    // Set position and size
    lv_obj_set_pos(rect, 10, 10);           // x, y position
    lv_obj_set_size(rect, 220, 290);        // width, height

    // Style the rectangle
    lv_obj_set_style_bg_color(rect, COLOR_BLACK, 0);  // Background color
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);              // Full opacity
    lv_obj_set_style_border_width(rect, 2, 0);                    // Border width
    lv_obj_set_style_border_color(rect, COLOR_ORANGE, 0); // Border color
    lv_obj_set_style_radius(rect, 10, 0);

    label_info = lv_label_create(screen_info);
    lv_label_set_text(label_info, "Please, wait");
    lv_obj_set_style_text_font(label_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_info, COLOR_DARK_PURPLE, 0);
    lv_obj_set_pos(label_info, 20, 20);
}

// Create the sensor screen
void create_sensor_screen()
{
    
    // Create sensor screen
    screen_sensor = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_sensor, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_sensor, LV_OPA_COVER, LV_PART_MAIN);
    
    create_sensor_lines();
    // Add time label
    label_time = lv_label_create(screen_sensor);
    lv_label_set_text(label_time, "00:00");
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_time, COLOR_ORANGE, 0);
    lv_obj_set_pos(label_time, 20, 30);
    
    label_date = lv_label_create(screen_sensor);
    lv_label_set_text(label_date, "YYYY/mm/dd");
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, COLOR_CYAN, 0);
    lv_obj_set_pos(label_date, 80, 90);

    // Create sensor labels
    create_sensor_co2(&lv_font_montserrat_14, &lv_font_montserrat_14);
    create_sensor_temp(&lv_font_montserrat_14, &lv_font_montserrat_14, &lv_font_montserrat_14);
    create_sensor_hum(&lv_font_montserrat_14, &lv_font_montserrat_14, &lv_font_montserrat_14);
}

void init_start_screen(void)
{
    bool display_initialized = false;

    init_lcd(0);

    if (lvgl_port_lock(0)) {
        create_info_screen();
        lv_screen_load(screen_info);
        lvgl_port_unlock();
    }
    while (!display_initialized) {
        ESP_LOGI(TAG, "Waiting for all data sources...");
        
        EventBits_t bits = xEventGroupWaitBits(
            data_events,
            SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY,
            pdFALSE,
            pdTRUE,   // Wait for ALL
            pdMS_TO_TICKS(5000)  // Check every 5 seconds
        );
        
        // Show status
        ESP_LOGI(TAG, "Data status:");
        ESP_LOGI(TAG, "  Sensor: %s", (bits & SENSOR_DATA_READY) ? "READY" : "FAILED");
        ESP_LOGI(TAG, "  Time:   %s", (bits & TIME_DATA_READY) ? "READY" : "FAILED");
        ESP_LOGI(TAG, "  Weather: %s", (bits & WEATHER_DATA_READY) ? "READY" : "FAILED");

        // lv_obj_t label_sensor_ready = lv_label_create(screen_info);
        
        // Check if all ready
        if ((bits & (SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY)) == 
            (SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY)) {
            
            xEventGroupClearBits(data_events, 
                SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY);
            
            ESP_LOGI(TAG, "All data ready! Initializing display...");
            
            if (lvgl_port_lock(0)) {
                create_sensor_screen();
                lv_screen_load(screen_sensor);
                lvgl_port_unlock();
            }
            
            display_initialized = true;
        }
    }
}
