idf_component_register(SRCS "openweather.c"
    "fonts/noto_sans_jp_24.c"
    "fonts/jet_mono_light_32.c"
    "fonts/jb_mono_bold_48.c"
    "fonts/jb_mono_bold_64.c"
    "fonts/jb_mono_reg_24.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_connect 
                    get_weather 
                    st7789 
                    get_time 
                    get_sensor_data
                    buttons)
