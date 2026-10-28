function(rr_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;DEPENDS")

    add_library(rr_${name} STATIC ${arg_SOURCES})
    add_library(rr::${name} ALIAS rr_${name})

    target_include_directories(rr_${name} PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(rr_${name}
        PUBLIC ${arg_DEPENDS}
        PRIVATE rr::compile_options
    )
endfunction()
