# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

message(STATUS "executable: $ENV{executable}")
if ("$ENV{executable}" STREQUAL "main")
    # no work
elseif ("$ENV{executable}" STREQUAL "test_ota")
    set(TEST_COMPONENTS "ota")
elseif ("$ENV{executable}" STREQUAL "test_actions")
    set(TEST_COMPONENTS "actions")
elseif ("$ENV{executable}" STREQUAL "test_actions2")
    set(TEST_COMPONENTS "actions")
elseif ("$ENV{executable}" STREQUAL "test_nonmock")
    set(TEST_COMPONENTS "animations api_connect app_errors app_nvs common indicators led_matrix refresh routines")
elseif ("$ENV{executable}" STREQUAL "test_manual")
    set(TEST_COMPONENTS "test_manual")
else()
    message(FATAL_ERROR "Value of executable environment variable \"$ENV{executable}\" is not known!")
endif()

project(esp-firmware)