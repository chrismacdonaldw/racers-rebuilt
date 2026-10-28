rr_add_module(app
    SOURCES
        application.cpp
    DEPENDS
        rr::core
        rr::platform
)
target_compile_definitions(rr_app PRIVATE RR_VERSION="${PROJECT_VERSION}")

add_executable(racers-rebuilt main.cpp)
set_target_properties(racers-rebuilt PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
target_link_libraries(racers-rebuilt PRIVATE rr::app rr::compile_options)
