# Bench firmware for bringing up the NME shield V1.0 drivers.
#
#   . ~/esp/esp-idf/export.sh
#   idf.py -C bench build
#   idf.py -C bench -p /dev/cu.usbmodem<...> flash monitor
#
# A serial command line for exercising each driver on its own (type `help`).
# It has no display, no clinical UI and no TOF/correction logic, so it cannot
# show a clinical number. It stimulates: use resistive dummy loads only, never
# a person.

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)
# Build only main and what it requires, so the UI and LVGL stay out.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nmt_bench)
