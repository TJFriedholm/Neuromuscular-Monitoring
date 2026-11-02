# A simulated device: the real core logic driven by synthetic CMAP data from
# hal_mock. Used by the desktop simulator and by the firmware's demo build,
# never by a build that stimulates a patient.
if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/sim_device.cpp"
        INCLUDE_DIRS "include"
        REQUIRES ui calib correction nmt_hal hal_mock safety tof
    )
else()
    add_library(nmt_sim_device STATIC src/sim_device.cpp)
    target_include_directories(nmt_sim_device PUBLIC include)
    target_link_libraries(nmt_sim_device PUBLIC nmt_ui_model nmt_calib nmt_correction nmt_hal
                                                nmt_hal_mock nmt_safety nmt_tof)
    target_compile_features(nmt_sim_device PUBLIC cxx_std_17)
endif()
