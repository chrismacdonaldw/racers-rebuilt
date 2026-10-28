add_executable(racers-rebuilt-tests
    main.cpp
    app/frame_loop_test.cpp
    app/install_discovery_test.cpp
    app/scene_stack_test.cpp
    app/startup_settings_test.cpp
    app/startup_timer_test.cpp
    assets/byte_reader_test.cpp
    assets/game_path_test.cpp
    assets/install_test.cpp
    assets/jam_archive_test.cpp
    assets/jam_test.cpp
    assets/token_reader_test.cpp
    assets/token_schema_test.cpp
    assets/token_survey_test.cpp
    assets/vfs_test.cpp
    core/log_test.cpp
    core/quaternion_test.cpp
    core/result_test.cpp
    core/utf8_path_test.cpp
    platform/data_folder_test.cpp
    platform/frame_pacer_test.cpp
    platform/install_locations_test.cpp
    platform/mapped_file_test.cpp
    settings/command_line_test.cpp
    settings/registry_test.cpp
    settings/settings_file_test.cpp
    sim/input_latch_test.cpp
    sim/interpolation_test.cpp
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_DEFINITIONS DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
target_compile_definitions(racers-rebuilt-tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
target_link_libraries(racers-rebuilt-tests PRIVATE
    rr::app
    rr::assets
    rr::core
    rr::platform
    rr::settings
    rr::sim
    rr::compile_options
    doctest::doctest
)

add_test(NAME racers-rebuilt-tests COMMAND racers-rebuilt-tests)

target_include_directories(racers-rebuilt-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
