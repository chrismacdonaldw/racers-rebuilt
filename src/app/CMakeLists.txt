rr_add_module(app
    SOURCES
        application.cpp
        audio_settings.cpp
        audio_startup.cpp
        frame_loop.cpp
        image_dump.cpp
        install_discovery.cpp
        loop_sidecar.cpp
        music_folder_source.cpp
        scene_stack.cpp
        startup_settings.cpp
        startup_timer.cpp
        test_scene.cpp
        video_settings.cpp
    DEPENDS
        rr::assets
        rr::audio
        rr::core
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
