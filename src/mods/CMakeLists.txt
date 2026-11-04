rr_add_module(mods
    SOURCES
        folder_hash.cpp
        hook_registry.cpp
        mod.cpp
        mod_check.cpp
        mod_list.cpp
        mod_manifest.cpp
        mod_resolution.cpp
        mod_toggles.cpp
        mod_version.cpp
        override_folder.cpp
    DEPENDS
        rr::assets
        rr::core
        rr::settings
)
target_include_directories(rr_mods SYSTEM PRIVATE
    $<TARGET_PROPERTY:tomlplusplus::tomlplusplus,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(rr_mods PRIVATE TOML_HEADER_ONLY=1 TOML_EXCEPTIONS=0)
