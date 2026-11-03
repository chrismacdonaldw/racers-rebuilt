find_package(ffmpeg-indeo5 CONFIG REQUIRED)

rr_add_module(video
    SOURCES
        video_decoder.cpp
        yuv.cpp
    DEPENDS
        rr::assets
        rr::core
)
target_link_libraries(rr_video PRIVATE ffmpeg-indeo5::avcodec)
