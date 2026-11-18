idf_component_register(
    SRCS "bench_main.cpp"
    REQUIRES shield nmt_hal console esp_driver_i2c esp_driver_gpio esp_timer
)
