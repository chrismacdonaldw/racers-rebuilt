add_executable(racers-rebuilt-tests
    main.cpp
    allocation_counter.cpp
)
foreach(folder IN ITEMS app assets audio core mods overlay platform render settings sim video)
    include(${folder}/sources.cmake)
endforeach()
set_source_files_properties(main.cpp PROPERTIES
    COMPILE_DEFINITIONS DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    SKIP_PRECOMPILE_HEADERS ON
)
target_precompile_headers(racers-rebuilt-tests PRIVATE <doctest/doctest.h>)
target_compile_definitions(racers-rebuilt-tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
target_link_libraries(racers-rebuilt-tests PRIVATE
    rr::app
    rr::assets
    rr::audio
    rr::core
    rr::mods
    rr::overlay
    rr::platform
    rr::render
    rr::settings
    rr::sim
    rr::video
    rr::compile_options
    doctest::doctest
    imgui::imgui
)

add_test(NAME racers-rebuilt-tests COMMAND racers-rebuilt-tests)

target_include_directories(racers-rebuilt-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(racers-rebuilt-tests PRIVATE RR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    RR_LDRAW_DIR="${PROJECT_SOURCE_DIR}/content/ldraw")
