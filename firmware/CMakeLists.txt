# ESP-IDF project for the ESP32-P4 board (Waveshare ESP32-P4-WIFI6-Touch-LCD-5).
#
#   . ~/esp/esp-idf/export.sh
#   idf.py -C firmware build
#   idf.py -C firmware -p /dev/cu.usbmodem<...> flash monitor
#
# For now this is the DEMO build: the real UI and core logic, driven by the
# simulated patient in components/sim_device, exactly as the desktop simulator
# is. There are no stimulator or EMG drivers yet, so nothing leaves the board.

cmake_minimum_required(VERSION 3.16)

# Exempts the build from the sourced-correction-exponent check (ADR-0006). Must
# be OFF in any build that drives the stimulator.
set(NMT_DEMO_BUILD ON)

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nmt_firmware)
