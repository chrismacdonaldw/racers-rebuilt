rr_add_module(overlay
    SOURCES
        overlay.cpp
        setting_menus.cpp
    DEPENDS
        rr::core
        rr::platform
        rr::render
        rr::settings
)
target_link_libraries(rr_overlay PRIVATE imgui::imgui)
