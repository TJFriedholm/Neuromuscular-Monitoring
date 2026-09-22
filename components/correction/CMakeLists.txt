if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/correction.cpp"
        INCLUDE_DIRS "include"
    )
    # The device build must not ship without a sourced correction factor.
    # Define NMT_CORRECTION_DEFAULT_EXPONENT once the value and its citation are
    # available; until then this build fails loudly. See ADR-0006.
    target_compile_definitions(${COMPONENT_LIB} PUBLIC NMT_REQUIRE_CORRECTION_DEFAULT)
else()
    add_library(nmt_correction STATIC src/correction.cpp)
    target_include_directories(nmt_correction PUBLIC include)
    target_compile_features(nmt_correction PUBLIC cxx_std_17)
endif()
