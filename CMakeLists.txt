cmake_minimum_required(VERSION 3.20)
project(pi_carplay_headunit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json 3.11 REQUIRED)

add_library(headunit_core src/core/config.cpp)
target_include_directories(headunit_core PUBLIC src)
target_link_libraries(headunit_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(headunit src/main.cpp)
target_link_libraries(headunit PRIVATE headunit_core)

# Interface-only: HAL, plugin, and state-machine headers. Links no platform
# library (no libusb, SDL, or FFmpeg). Platform code arrives behind these in D3.
add_library(hu_interfaces INTERFACE)
target_include_directories(hu_interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

enable_testing()
find_package(GTest REQUIRED)
add_executable(unit_tests tests/unit/config_test.cpp tests/unit/interfaces_test.cpp)
target_link_libraries(unit_tests PRIVATE headunit_core hu_interfaces GTest::gtest_main)
target_compile_definitions(unit_tests PRIVATE HU_CONFIG_DIR="${PROJECT_SOURCE_DIR}/config")
add_test(NAME unit COMMAND unit_tests)
