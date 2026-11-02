# Dual-target, like the core components. Two parts:
#   nmt_ui_model  what each page shows, settings, navigation (pure;
#                 host-testable without LVGL)
#   nmt_ui        the LVGL widgets that draw it, plus the compiled fonts
set(NMT_UI_MODEL_SRCS src/monitor_model.cpp src/menu_model.cpp src/settings.cpp)
set(NMT_UI_VIEW_SRCS
    src/app.cpp
    src/widgets.cpp
    src/page_monitor.cpp
    src/page_menu.cpp
    src/page_entry.cpp
)
set(NMT_UI_FONTS
    fonts/plex_sans_32.c
    fonts/plex_sans_32_semibold.c
    fonts/plex_mono_32.c
    fonts/plex_sans_46_medium.c
    fonts/plex_sans_46_semibold.c
    fonts/plex_mono_46_medium.c
    fonts/plex_sans_77_bold.c
    fonts/plex_mono_120_semibold.c
    fonts/plex_mono_140_semibold.c
)

if(ESP_PLATFORM)
    # lvgl__lvgl is the component manager's name for lvgl/lvgl.
    idf_component_register(
        SRCS ${NMT_UI_MODEL_SRCS} ${NMT_UI_VIEW_SRCS} ${NMT_UI_FONTS}
        INCLUDE_DIRS "include"
        REQUIRES tof correction safety lvgl__lvgl
    )
else()
    add_library(nmt_ui_model STATIC ${NMT_UI_MODEL_SRCS})
    target_include_directories(nmt_ui_model PUBLIC include)
    target_link_libraries(nmt_ui_model PUBLIC nmt_tof nmt_correction nmt_safety)
    target_compile_features(nmt_ui_model PUBLIC cxx_std_17)

    # The view needs LVGL, which only the simulator provides. Host unit tests
    # build the model alone.
    if(TARGET lvgl)
        add_library(nmt_ui STATIC ${NMT_UI_VIEW_SRCS} ${NMT_UI_FONTS})
        target_link_libraries(nmt_ui PUBLIC nmt_ui_model lvgl)
    endif()
endif()
