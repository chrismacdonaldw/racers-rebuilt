rr_add_module(overlay
    SOURCES
        input_gate.cpp
        mod_menu.cpp
        overlay.cpp
        quick_config_menu.cpp
        racer_tools.cpp
        setting_menus.cpp
    DEPENDS
        rr::core
        rr::mods
        rr::platform
        rr::render
        rr::settings
)
target_link_libraries(rr_overlay PRIVATE imgui::imgui)
