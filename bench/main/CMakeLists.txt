idf_component_register(
    SRCS "bench_main.cpp"
    REQUIRES shield nmt_hal console esp_driver_i2c esp_driver_gpio esp_timer esp_system esp_hw_support
)

# 1 only in the opt-in stimulating build (see ../CMakeLists.txt).
if(NMT_BENCH_STIM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NMT_BENCH_STIM=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NMT_BENCH_STIM=0)
endif()
