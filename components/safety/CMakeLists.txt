if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/fault.cpp" "src/tof_state_machine.cpp"
        INCLUDE_DIRS "include"
    )
else()
    add_library(nmt_safety STATIC src/fault.cpp src/tof_state_machine.cpp)
    target_include_directories(nmt_safety PUBLIC include)
    target_compile_features(nmt_safety PUBLIC cxx_std_17)
endif()
