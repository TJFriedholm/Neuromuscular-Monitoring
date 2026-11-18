# Drivers for the NME shield V1.0 (ADR-0010 to ADR-0014).
#
# On the device: everything. On the host: only the pure parts (register
# encoding, scaling, the amplitude table), so they can be unit-tested.
#
# This component drives the stimulator. firmware/ (the demo build) excludes it
# until the demo exemption from ADR-0006 is removed; bench/ uses it.

if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/amplitude_table.cpp"
             "src/ads122c04.cpp"
             "src/ad5241.cpp"
             "src/pulse_generator.cpp"
             "src/shield_stimulator.cpp"
             "src/shield_emg.cpp"
        INCLUDE_DIRS "include"
        REQUIRES nmt_hal esp_driver_i2c esp_driver_rmt esp_driver_gpio esp_driver_gptimer
                 esp_timer
    )
else()
    add_library(nmt_shield STATIC src/amplitude_table.cpp)
    target_include_directories(nmt_shield PUBLIC include)
    target_link_libraries(nmt_shield PUBLIC nmt_hal)
    target_compile_features(nmt_shield PUBLIC cxx_std_17)
endif()
