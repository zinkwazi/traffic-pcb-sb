# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# Add newly added components to one of these lines:
# 1. Add here if the component is compatible with IDF >= v4.3
set(EXTRA_COMPONENT_DIRS "../esp-firmware/components")

set(TEST_COMPONENTS "animations api_connect app_errors common led_matrix")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-unity-test-app)