add_executable(jam-dump main.cpp)
target_link_libraries(jam-dump PRIVATE rr::assets rr::core rr::compile_options)
