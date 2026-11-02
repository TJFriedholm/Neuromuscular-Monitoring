# Header-only: interfaces, bounds and the impedance range check.
if(ESP_PLATFORM)
    idf_component_register(INCLUDE_DIRS "include")
else()
    add_library(nmt_hal INTERFACE)
    target_include_directories(nmt_hal INTERFACE include)
    target_compile_features(nmt_hal INTERFACE cxx_std_17)
endif()
