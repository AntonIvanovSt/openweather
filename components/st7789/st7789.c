#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "openweather.h"
#include "st7789.h"

static const char *TAG = "st7789";

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

lv_obj_t* create_background(lv_color_t color)
{
    if (lvgl_port_lock(0)) {
        lv_obj_t *screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
        lvgl_port_unlock();
        return screen;
    }
    return NULL;
}


lv_obj_t* create_label(lv_obj_t *display_label,
                  const lv_font_t *font, lv_color_t color,
                  int x, int y, const char *text)
{
    if (lvgl_port_lock(0)) {

        lv_obj_t *value_label = lv_label_create(display_label);
        lv_label_set_text(value_label, text);

        lv_obj_set_style_text_font(value_label, font, 0);

        lv_obj_set_style_text_color(value_label, color, 0);
        lv_obj_set_style_text_opa(value_label, LV_OPA_COVER, 0);
 
        lv_obj_set_pos(value_label, x, y);
        lvgl_port_unlock();
        return value_label;
    }
    return NULL;
}

// in lv_color_make order is BRG with RGB565 notation
// max values are: 31, 31, 63

lv_obj_t *create_line(lv_point_precise_t *line_array, lv_obj_t *display_label,
                      lv_color_t color, int x, int y)
{
    if (lvgl_port_lock(0)) {
        lv_obj_t *line = lv_line_create(display_label);
        lv_line_set_points(line, line_array, 2);

        // Position the line
        lv_obj_set_pos(line, x, y);

        // Style the line
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_color(line, color, 0);
        lvgl_port_unlock();
        return line;
    }
    return NULL;
}

void create_info_screen(void)
{
    screen_info = create_background(COLOR_BLACK);

    label_info = create_label(screen_info, &lv_font_montserrat_14, 
                              COLOR_ORANGE, 20, 20, "Start Initializing");
}

// Create the sensor screen
void create_sensor_screen(void)
{
    extern lv_font_t jet_mono_light_32;
    extern lv_font_t noto_sans_jp_24;
    extern lv_font_t jb_mono_bold_48;
    extern lv_font_t jb_mono_bold_64;
    extern lv_font_t jb_mono_reg_20;

    static lv_point_precise_t line_points1[] = {
        {0, 0},      // Start point (x, y)
        {240, 0}     // End point (x, y)
    };
    static lv_point_precise_t line_points2[] = {
        {0, 0},      // Start point (x, y)
        {0, 80}     // End point (x, y)
    };

    screen_sensor = create_background(COLOR_BLACK);

    create_line(line_points1, screen_sensor, COLOR_ORANGE, 0, 160);
    create_line(line_points1, screen_sensor, COLOR_ORANGE, 0, 240);
    create_line(line_points2, screen_sensor, COLOR_ORANGE, 120, 160);

    label_time = create_label(screen_sensor, &jb_mono_bold_64, COLOR_ORANGE, 20, 30, "00:00");
    label_date = create_label(screen_sensor, &lv_font_montserrat_14, COLOR_CYAN, 80, 90, "YYYY/mm/dd");

    label_co2 = create_label(screen_sensor, &jb_mono_reg_20, COLOR_DARK_PURPLE, 10, 250, "CO2");
    label_co2 = create_label(screen_sensor, &jb_mono_reg_20, COLOR_DARK_PURPLE, 190, 250, "ppm");
    label_co2 = create_label(screen_sensor, &jb_mono_bold_48, COLOR_ORANGE, 70, 260, "--");

    label_temp = create_label(screen_sensor, &noto_sans_jp_24, COLOR_DARK_PURPLE, 10, 170, "湿度");
    label_temp = create_label(screen_sensor, &jb_mono_reg_20, COLOR_DARK_PURPLE, 80, 170, "°C");
    label_temp = create_label(screen_sensor, &jet_mono_light_32, COLOR_ORANGE, 10, 200, "--");

    label_humid = create_label(screen_sensor, &noto_sans_jp_24, COLOR_DARK_PURPLE, 130, 170, "温度");
    label_humid = create_label(screen_sensor, &jb_mono_reg_20, COLOR_DARK_PURPLE, 210, 170, "%");
    label_humid = create_label(screen_sensor, &jet_mono_light_32, COLOR_ORANGE, 130, 200, "--");
}

void create_weather_screen()
{
    extern lv_font_t jb_mono_reg_20;
    extern lv_font_t jet_mono_light_32;
    extern lv_font_t jb_mono_bold_64;

    screen_weather = create_background(COLOR_BLACK);
    
    if (lvgl_port_lock(0)) {
        // Temperature
        label_out_temp = create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                                      90, 30, "°C");
        label_out_temp = create_label(screen_weather, &jb_mono_bold_64, COLOR_ORANGE, 
                                      20, 30, "--");
        
        // Feels like
        label_out_feels = create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                                       90, 50, "(-- °C)");
        
        // Humidity
        create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                    10, 150, "H:");
        create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                    70, 150, "%");
        label_out_humidity = create_label(screen_weather, &jb_mono_reg_20, COLOR_ORANGE, 
                                          30, 150, "--");
        
        // Condition
        label_out_cond = create_label(screen_weather, &jb_mono_reg_20, COLOR_CYAN, 
                                      50, 120, "Loading...");
        
        // Wind
        create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                    10, 230, "W:");
        create_label(screen_weather, &jb_mono_reg_20, COLOR_DARK_PURPLE, 
                    70, 230, "km/h");
        label_out_wind = create_label(screen_weather, &jb_mono_reg_20, COLOR_ORANGE, 
                                      30, 230, "--");
        
        lvgl_port_unlock();
    }
}

void check_modules_state(void)
{
    bool display_initialized = false;
    char log_buffer[64];
    lv_obj_t *log_sensor = NULL;
    lv_obj_t *log_time = NULL;
    lv_obj_t *log_weather = NULL;

    lv_label_set_text(label_info, "Waiting modules...");

    log_sensor = create_label(screen_info, &lv_font_montserrat_14, COLOR_ORANGE, 20, 60, "  Sensor: ---");
    log_time = create_label(screen_info, &lv_font_montserrat_14, COLOR_ORANGE, 20, 80, "  Time: ---");
    log_weather = create_label(screen_info, &lv_font_montserrat_14, COLOR_ORANGE, 20, 100, "  Weather: ---");

    while (!display_initialized) {
        ESP_LOGI(TAG, "Waiting for all data sources...");
        
        EventBits_t bits = xEventGroupWaitBits(
            data_events,
            SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(5000)
        );
        
        if (lvgl_port_lock(0)) {
            snprintf(log_buffer, sizeof(log_buffer), "  Sensor: %s", 
                    (bits & SENSOR_DATA_READY) ? "READY" : "FAILED");
            lv_label_set_text(log_sensor, log_buffer);
            
            snprintf(log_buffer, sizeof(log_buffer), "  Time: %s", 
                    (bits & TIME_DATA_READY) ? "READY" : "FAILED");
            lv_label_set_text(log_time, log_buffer);
            
            snprintf(log_buffer, sizeof(log_buffer), "  Weather: %s", 
                    (bits & WEATHER_DATA_READY) ? "READY" : "FAILED");
            lv_label_set_text(log_weather, log_buffer);
            
            lvgl_port_unlock();
        }
        // Check if all ready
        if ((bits & (SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY)) == 
            (SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY)) {
            
            xEventGroupClearBits(data_events, 
                SENSOR_DATA_READY | TIME_DATA_READY | WEATHER_DATA_READY);
            
            ESP_LOGI(TAG, "All data ready! Initializing display...");
            
            if (lvgl_port_lock(0)) {
                create_sensor_screen();
                create_weather_screen();
                lv_screen_load(screen_weather);
                lvgl_port_unlock();
            }
            
            display_initialized = true;
        }
    }

}

void init_start_screen(void)
{
    init_lcd(0);
    create_weather_screen();
    // create_info_screen();
    if (lvgl_port_lock(0)) {
        // lv_screen_load(screen_info);
        lv_screen_load(screen_weather);
        lvgl_port_unlock();
    }
}
