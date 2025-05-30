# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS "esp-http-client-mock/components")

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
    # set(TEST_COMPONENTS "api_connect")
elseif ("$ENV{executable}" STREQUAL "test_manual")
    set(TEST_COMPONENTS "test_manual")
else()
    message(FATAL_ERROR "Value of executable environment variable \"$ENV{executable}\" is not known!")
endif()

project(esp-firmware)

# set(all_sources "")
# set(all_includes "")
# idf_build_get_property(_build_components BUILD_COMPONENTS)
# idf_build_get_property(proj_dir PROJECT_DIR)
# message(STATUS "All components: ${_build_components}")
# message(STATUS "proj dir: ${proj_dir}")
# execute_process(COMMAND echo HELLO
#                 OUTPUT_FILE ${proj_dir}/file.txt)

# foreach(component ${_build_components})
#     idf_component_get_property(comp_sources ${component} SRCS)
#     idf_component_get_property(comp_includes ${component} INCLUDE_DIRS)
#     idf_component_get_property(comp_dir ${component} COMPONENT_DIR)
#     message(STATUS "component: ${component}")
#     message(STATUS "sources: ${comp_sources}")
#     message(STATUS "includes: ${comp_includes}")
#     message(STATUS "dir: ${comp_dir}")
#     message(STATUS "")
#     list(APPEND all_sources ${comp_sources})
#     foreach(include_dir ${comp_includes})
#         list(APPEND all_includes "${comp_dir}/${include_dir}")
#     endforeach()
# endforeach()

# message(STATUS "all sources: ${all_sources}")
# message(STATUS "all includes: ${all_includes}")

# set(include_args "")
# foreach(include_path ${all_includes})
#     list(APPEND include_args -I${include_path})
# endforeach()
# list(APPEND include_args "-I${CMAKE_BINARY_DIR}/config")

# message(WARNING "running cbmc")

# # set(preprocessed_file ${proj_dir}/cbmc.goto)

# # # Preprocess with ESP-IDF toolchain (cross-compiler)
# # execute_process(
# #     COMMAND xtensa-esp32-elf-gcc -E ${proj_dir}/main/main.c -o ${preprocessed_file}
# #             ${include_args} -D__CPROVER__ -D__XTENSA__ -DCONFIG_FREERTOS_HZ=1000
# #     WORKING_DIRECTORY ${proj_dir}
# # )

# # # execute_process(
# # #     COMMAND cbmc ${preprocessed_file} --function main
# # #     WORKING_DIRECTORY ${proj_dir}
# # #     OUTPUT_FILE ${proj_dir}/cbmc_out.txt
# # #     ERROR_FILE ${proj_dir}/cbmc_err.txt
# # # )

# execute_process(COMMAND ${GOTO_CC_EXECUTABLE} ${proj_dir}/cbmc.goto ${all_sources} ${include_args}
#                 WORKING_DIRECTORY ${proj_dir}
#                 OUTPUT_FILE ${proj_dir}/cbmc_out.txt)