# Bench firmware for bringing up the NME shield V1.0 drivers.
#
#   . ~/esp/esp-idf/export.sh
#   idf.py -C bench build
#   idf.py -C bench -p /dev/cu.usbmodem<...> flash monitor
#
# A serial command line for exercising each driver on its own (type `help`).
# It has no display, no clinical UI and no TOF/correction logic, so it cannot
# show a clinical number.
#
# Two builds:
#   default     NO STIMULATION. I2C scans, ADC and pot register tests, EMG
#               sampling. STIM_PULSE and STIM_OUTPUT_ENABLE are held low by
#               plain GPIO; the pulse generator is not linked in.
#   stimulating opt in, and only into its own build directory, so the default
#               bench/build can never be left in stimulating mode:
#                 idf.py -C bench -B bench/build-stim -DNMT_BENCH_STIM=ON build
#               Resistive dummy loads on CN1 only, never a person.

cmake_minimum_required(VERSION 3.16)

option(NMT_BENCH_STIM "Link the pulse generator and the stimulating commands" OFF)
if(NMT_BENCH_STIM)
    get_filename_component(_build_dir_name "${CMAKE_BINARY_DIR}" NAME)
    if(NOT _build_dir_name STREQUAL "build-stim")
        message(FATAL_ERROR
            "NMT_BENCH_STIM=ON must be built in bench/build-stim (idf.py -B bench/build-stim). "
            "Delete ${CMAKE_BINARY_DIR} and rebuild without -DNMT_BENCH_STIM=ON.")
    endif()
endif()

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)
# Build only main and what it requires, so the UI and LVGL stay out.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nmt_bench)
