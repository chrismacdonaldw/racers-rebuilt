rr_add_module(app
    SOURCES
        application.cpp
        frame_loop.cpp
        install_discovery.cpp
        scene_stack.cpp
        startup_settings.cpp
        startup_timer.cpp
        test_scene.cpp
        video_settings.cpp
    DEPENDS
        rr::assets
        rr::core
        rr::platform
        rr::render
        rr::settings
        rr::sim
)
target_compile_definitions(rr_app PRIVATE RR_VERSION="${PROJECT_VERSION}")

add_executable(racers-rebuilt main.cpp)
set_target_properties(racers-rebuilt PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
target_link_libraries(racers-rebuilt PRIVATE rr::app rr::compile_options)
