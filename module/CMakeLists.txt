# DO NOT EDIT THIS FILE
# It is automatically generated and will be overwritten
# Extra module CMake logic belongs in module_extra.cmake
cmake_minimum_required(VERSION 3.18.4)

if(NOT DEFINED NAP_ROOT)
    cmake_path(GET CMAKE_CURRENT_SOURCE_DIR STEM LAST_ONLY dir_name)
    if(dir_name MATCHES "^module$")
        set(path_from_module_to_root ../../..)
    else()
        set(path_from_module_to_root ../..)
    endif()
    set(NAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/${path_from_module_to_root})
endif()
include(${NAP_ROOT}/cmake/macros_and_functions.cmake)

directory_name_to_module_name()
if(TARGET ${MODULE_NAME})
    return()
endif()

project(${MODULE_NAME})
include(${NAP_ROOT}/cmake/nap_module.cmake)
