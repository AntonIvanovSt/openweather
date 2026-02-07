idf_component_register(SRCS "openweather.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_connect get_weather st7789 get_time)
