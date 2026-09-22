# Dual-target: ESP-IDF component when built for the device, plain static
# library when built for host unit tests.
if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/tof_compute.cpp"
        INCLUDE_DIRS "include"
    )
else()
    add_library(nmt_tof STATIC src/tof_compute.cpp)
    target_include_directories(nmt_tof PUBLIC include)
    target_compile_features(nmt_tof PUBLIC cxx_std_17)
endif()
