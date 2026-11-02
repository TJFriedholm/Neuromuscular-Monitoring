if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/mock_stimulator.cpp" "src/cmap_source.cpp"
        INCLUDE_DIRS "include"
        REQUIRES nmt_hal tof
    )
else()
    add_library(nmt_hal_mock STATIC src/mock_stimulator.cpp src/cmap_source.cpp)
    target_include_directories(nmt_hal_mock PUBLIC include)
    target_link_libraries(nmt_hal_mock PUBLIC nmt_hal nmt_tof)
    target_compile_features(nmt_hal_mock PUBLIC cxx_std_17)
endif()
