set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE BOOL "Link Release builds with LTCG")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_library(rr_compile_options INTERFACE)
add_library(rr::compile_options ALIAS rr_compile_options)

target_compile_options(rr_compile_options INTERFACE
    /W4
    /WX
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /fp:precise
    /EHsc
)

target_compile_definitions(rr_compile_options INTERFACE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_precompile_headers(rr_compile_options INTERFACE
    <algorithm>
    <array>
    <cmath>
    <cstddef>
    <cstdint>
    <filesystem>
    <format>
    <memory>
    <optional>
    <span>
    <string>
    <string_view>
    <utility>
    <vector>
)
