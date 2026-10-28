add_executable(racers-rebuilt-tests
    main.cpp
    core/log_test.cpp
    core/quaternion_test.cpp
    core/result_test.cpp
    core/utf8_path_test.cpp
    platform/data_folder_test.cpp
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_DEFINITIONS DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
target_compile_definitions(racers-rebuilt-tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
target_link_libraries(racers-rebuilt-tests PRIVATE
    rr::core
    rr::platform
    rr::compile_options
    doctest::doctest
)

add_test(NAME racers-rebuilt-tests COMMAND racers-rebuilt-tests)

target_include_directories(racers-rebuilt-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
