rr_add_module(app
    SOURCES
        application.cpp
        audio_settings.cpp
        audio_startup.cpp
        extensions.cpp
        frame_loop.cpp
        free_camera.cpp
        game_boot.cpp
        image_dump.cpp
        install_discovery.cpp
        loop_sidecar.cpp
        music_folder_source.cpp
        pace_car.cpp
        scene_stack.cpp
        startup_settings.cpp
        startup_timer.cpp
        test_scene.cpp
        track_model.cpp
        track_space.cpp
        track_viewer_scene.cpp
        video_settings.cpp
    DEPENDS
        rr::assets
        rr::audio
        rr::core
        rr::mods
        rr::overlay
        rr::platform
        rr::render
        rr::settings
        rr::sim
)
target_include_directories(rr_app SYSTEM PRIVATE
    $<TARGET_PROPERTY:tomlplusplus::tomlplusplus,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(rr_app PRIVATE
    RR_VERSION="${PROJECT_VERSION}"
    TOML_HEADER_ONLY=1
    TOML_EXCEPTIONS=0
)

add_executable(racers-rebuilt main.cpp)
set_target_properties(racers-rebuilt PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
target_link_libraries(racers-rebuilt PRIVATE rr::app rr::compile_options)
