cmake_minimum_required(VERSION 3.28)

project(racers-rebuilt VERSION 0.1.0 LANGUAGES CXX)

option(RR_BUILD_TESTS "Build the unit test executable" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(compile_options)
include(module)

find_package(SDL3 CONFIG REQUIRED)

add_subdirectory(src)

if(RR_BUILD_TESTS)
    find_package(doctest CONFIG REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()
