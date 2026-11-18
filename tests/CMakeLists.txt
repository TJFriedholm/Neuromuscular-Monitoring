cmake_minimum_required(VERSION 3.16)
project(nmt_host_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Unity 2.6.0, vendored. Pinned and committed as a SOUP record.
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)

# Core components. None of these include ESP-IDF headers, which is what lets
# them build and run here.
set(NMT_COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../components)
add_subdirectory(${NMT_COMPONENTS}/tof        ${CMAKE_BINARY_DIR}/tof)
add_subdirectory(${NMT_COMPONENTS}/calib      ${CMAKE_BINARY_DIR}/calib)
add_subdirectory(${NMT_COMPONENTS}/correction ${CMAKE_BINARY_DIR}/correction)
add_subdirectory(${NMT_COMPONENTS}/safety     ${CMAKE_BINARY_DIR}/safety)
add_subdirectory(${NMT_COMPONENTS}/nmt_hal    ${CMAKE_BINARY_DIR}/nmt_hal)
add_subdirectory(${NMT_COMPONENTS}/hal_mock   ${CMAKE_BINARY_DIR}/hal_mock)
# Only the pure parts of the shield drivers build here.
add_subdirectory(${NMT_COMPONENTS}/shield     ${CMAKE_BINARY_DIR}/shield)
# Only the UI model builds here; the LVGL view needs the simulator.
add_subdirectory(${NMT_COMPONENTS}/ui         ${CMAKE_BINARY_DIR}/ui)

function(nmt_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE unity ${ARGN})
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nmt_add_test(test_tof_compute       nmt_tof)
nmt_add_test(test_calib             nmt_calib nmt_hal_mock)
nmt_add_test(test_correction        nmt_correction)
nmt_add_test(test_fault             nmt_safety)
nmt_add_test(test_tof_state_machine nmt_safety)
nmt_add_test(test_hal_mock          nmt_hal_mock nmt_tof)
nmt_add_test(test_ui_model          nmt_ui_model)
nmt_add_test(test_ui_settings       nmt_ui_model)
nmt_add_test(test_shield            nmt_shield)
