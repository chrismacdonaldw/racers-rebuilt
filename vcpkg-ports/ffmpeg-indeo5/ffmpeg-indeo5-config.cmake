get_filename_component(ffmpeg_indeo5_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

foreach(lib IN ITEMS avutil avcodec)
    if(NOT TARGET ffmpeg-indeo5::${lib})
        add_library(ffmpeg-indeo5::${lib} STATIC IMPORTED)
        set_target_properties(ffmpeg-indeo5::${lib} PROPERTIES
            IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
            IMPORTED_LOCATION_RELEASE "${ffmpeg_indeo5_root}/lib/${lib}.lib"
            IMPORTED_LOCATION_DEBUG "${ffmpeg_indeo5_root}/debug/lib/${lib}.lib"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
            INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_indeo5_root}/include"
        )
    endif()
endforeach()

set_property(TARGET ffmpeg-indeo5::avutil PROPERTY INTERFACE_LINK_LIBRARIES bcrypt)
set_property(TARGET ffmpeg-indeo5::avcodec PROPERTY INTERFACE_LINK_LIBRARIES ffmpeg-indeo5::avutil)
