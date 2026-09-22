if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/calib.cpp"
        INCLUDE_DIRS "include"
    )
else()
    add_library(nmt_calib STATIC src/calib.cpp)
    target_include_directories(nmt_calib PUBLIC include)
    target_compile_features(nmt_calib PUBLIC cxx_std_17)
endif()
