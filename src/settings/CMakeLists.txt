rr_add_module(settings
    SOURCES
        command_line.cpp
        registry.cpp
        progress.cpp
        racers.cpp
        setting_preset.cpp
        settings_file.cpp
        toml_file.cpp
        trophy_table.cpp
    DEPENDS
        rr::assets
        rr::core
)
target_include_directories(rr_settings SYSTEM PRIVATE
    $<TARGET_PROPERTY:tomlplusplus::tomlplusplus,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(rr_settings PRIVATE TOML_HEADER_ONLY=1 TOML_EXCEPTIONS=0)
