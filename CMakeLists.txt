cmake_minimum_required(VERSION 3.28)

project(racers-rebuilt VERSION 0.5.0 LANGUAGES CXX)

option(RR_BUILD_TESTS "Build the unit test executable" ON)
option(RR_BUILD_TOOLS "Build developer tools such as jam-dump" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(compile_options)
include(module)
include(shaders)

find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_path(STB_INCLUDE_DIR stb_vorbis.c REQUIRED)
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)

add_subdirectory(src)

if(RR_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(RR_BUILD_TESTS)
    find_package(doctest CONFIG REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()
