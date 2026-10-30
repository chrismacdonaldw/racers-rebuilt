rr_add_module(app
    SOURCES
        application.cpp
        audio_settings.cpp
        audio_startup.cpp
        chase_camera.cpp
        checkpoint_gates.cpp
        collision_wireframe.cpp
        extensions.cpp
        frame_loop.cpp
        free_camera.cpp
        game_boot.cpp
        hud/hud_strings.cpp
        hud/place_pulse.cpp
        hud/race_hud.cpp
        hud/race_hud_values.cpp
        hud/race_results.cpp
        hud/race_time_text.cpp
        image_dump.cpp
        input_bindings.cpp
        install_discovery.cpp
        loop_sidecar.cpp
        music_folder_source.cpp
        orbiting_cube.cpp
        pace_car.cpp
        race_scene.cpp
        race_status.cpp
        racer/animation_player.cpp
        racer/body_lean.cpp
        racer/racer_assets.cpp
        racer/racer_draws.cpp
        racer/racer_grid.cpp
        racer/racer_visuals.cpp
        racer/skeleton_pose.cpp
        replay_session.cpp
        scene_stack.cpp
        sprite_test_scene.cpp
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
