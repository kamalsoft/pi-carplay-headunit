cmake_minimum_required(VERSION 3.20)
project(pi_carplay_headunit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json 3.11 REQUIRED)

# Build profile. Apple builds default to macos_sim, everything else to pi_prod.
if(APPLE)
  set(_hu_env_default macos_sim)
else()
  set(_hu_env_default pi_prod)
endif()
set(HU_ENV "${_hu_env_default}" CACHE STRING "Build profile: macos_sim or pi_prod")
set_property(CACHE HU_ENV PROPERTY STRINGS macos_sim pi_prod)
if(NOT HU_ENV MATCHES "^(macos_sim|pi_prod)$")
  message(FATAL_ERROR "HU_ENV must be macos_sim or pi_prod, got '${HU_ENV}'")
endif()

# Mock CarPlay simulator. Default ON only for macos_sim; never linked into pi_prod.
if(HU_ENV STREQUAL "macos_sim")
  set(_hu_sim_default ON)
else()
  set(_hu_sim_default OFF)
endif()
option(HU_SIMULATOR "Build the mock CarPlay stream simulator (macos_sim only)" ${_hu_sim_default})
if(HU_SIMULATOR AND NOT HU_ENV STREQUAL "macos_sim")
  message(FATAL_ERROR "HU_SIMULATOR=ON requires HU_ENV=macos_sim. pi_prod must not link the simulator.")
endif()

add_library(headunit_core src/core/config.cpp)
target_include_directories(headunit_core PUBLIC src)
target_link_libraries(headunit_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(headunit src/main.cpp)
target_link_libraries(headunit PRIVATE headunit_core)

# Interface-only: HAL, plugin, and state-machine headers. Links no platform
# library (no libusb, SDL, or FFmpeg). Platform code arrives behind these in D3.
add_library(hu_interfaces INTERFACE)
target_include_directories(hu_interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Simulator: standard library only, behind the D1 headers. Not linked into headunit.
set(HU_UNIT_TEST_SOURCES tests/unit/config_test.cpp tests/unit/interfaces_test.cpp)
if(HU_SIMULATOR)
  add_library(hu_simulator STATIC src/simulator/simulator.cpp)
  target_link_libraries(hu_simulator PUBLIC hu_interfaces)
  list(APPEND HU_UNIT_TEST_SOURCES tests/unit/simulator_test.cpp)
endif()

enable_testing()
find_package(GTest REQUIRED)
add_executable(unit_tests ${HU_UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE headunit_core hu_interfaces GTest::gtest_main)
if(HU_SIMULATOR)
  target_link_libraries(unit_tests PRIVATE hu_simulator)
endif()
target_compile_definitions(unit_tests PRIVATE HU_CONFIG_DIR="${PROJECT_SOURCE_DIR}/config")
add_test(NAME unit COMMAND unit_tests)

# CPC200 protocol framing and handshake (D2). Derived from node-CarPlay (MIT).
# Uses the UsbTransport interface only; links no platform library.
add_library(hu_protocol STATIC src/core/protocol/cpc200.cpp)
target_link_libraries(hu_protocol PUBLIC hu_interfaces)
target_sources(unit_tests PRIVATE tests/unit/protocol_test.cpp)
target_link_libraries(unit_tests PRIVATE hu_protocol)
