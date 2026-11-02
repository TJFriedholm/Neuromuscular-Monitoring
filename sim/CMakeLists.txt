# Desktop simulator: the real UI code, drawn in an SDL2 window at 1280 x 720,
# driven by the real core logic and the mock HAL. No board needed.
#
#   cmake -S sim -B sim/build
#   cmake --build sim/build
#   ./sim/build/nmt_sim
#
# Needs SDL2 (macOS: brew install sdl2). LVGL is downloaded at configure time.

cmake_minimum_required(VERSION 3.16)
project(nmt_sim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- SDL2 -------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

# ---- LVGL 9.5.0 -------------------------------------------------------------
# The version the Waveshare BSP pins (ADR-0007). Pinned by tag AND hash, so a
# re-tagged or tampered archive fails the build instead of changing the SOUP.
# To build offline, pass -DFETCHCONTENT_SOURCE_DIR_LVGL=<unpacked lvgl-9.5.0>.
include(FetchContent)
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
    URL_HASH SHA256=34a955cdf3a2d005507b704e87357af669a114523b6d3f77b5344fdc68717bc6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lvgl)

# ---- Firmware components (the same sources the device builds) ---------------
set(NMT_COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)
foreach(c tof calib correction safety nmt_hal hal_mock ui sim_device)
    add_subdirectory(${NMT_COMPONENTS}/${c} ${CMAKE_BINARY_DIR}/components/${c})
endforeach()

# ---- Simulator --------------------------------------------------------------
add_executable(nmt_sim main.cpp sdl_display.cpp)
target_link_libraries(nmt_sim PRIVATE nmt_ui nmt_sim_device PkgConfig::SDL2)
target_compile_options(nmt_sim PRIVATE -Wall -Wextra)
if(APPLE)
    # Physical display size, to open the window at the panel's real size.
    target_link_libraries(nmt_sim PRIVATE "-framework CoreGraphics")
endif()
