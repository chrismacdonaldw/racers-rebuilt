rr_add_module(audio
    SOURCES
        alp.cpp
        decoded_file.cpp
        looping_stream.cpp
        miniaudio.cpp
        mixer.cpp
        music_catalog.cpp
        music_player.cpp
        music_source.cpp
        peak_limiter.cpp
        stb_vorbis.cpp
        stream_data_source.cpp
        voice.cpp
        volume_curve.cpp
    DEPENDS
        rr::assets
        rr::core
)
target_include_directories(rr_audio SYSTEM PRIVATE
    "${MINIAUDIO_INCLUDE_DIR}"
    "${STB_INCLUDE_DIR}"
)
target_compile_definitions(rr_audio PRIVATE
    MA_NO_ENCODING
    MA_NO_GENERATION
    MA_NO_RESOURCE_MANAGER
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS
    MA_ENABLE_WASAPI
)
set_source_files_properties(miniaudio.cpp PROPERTIES
    COMPILE_DEFINITIONS "MINIAUDIO_IMPLEMENTATION;STB_VORBIS_HEADER_ONLY"
)
set_source_files_properties(stb_vorbis.cpp PROPERTIES COMPILE_OPTIONS "/wd4701;/wd4703")
