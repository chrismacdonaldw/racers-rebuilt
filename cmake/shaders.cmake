find_program(RR_DXC dxc REQUIRED)

function(rr_add_shader target name stage source)
    set(dxil "${CMAKE_CURRENT_BINARY_DIR}/shaders/${name}.dxil")
    set(embedded "${CMAKE_CURRENT_BINARY_DIR}/shaders/${name}.cpp")
    add_custom_command(
        OUTPUT "${dxil}"
        COMMAND "${RR_DXC}" -nologo -T ${stage}_6_0 -E main -O3 -WX -Fo "${dxil}" "${source}"
        DEPENDS "${source}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${embedded}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${dxil} -DOUTPUT=${embedded} -DSYMBOL=k${name}
            -P "${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake"
        DEPENDS "${dxil}" "${PROJECT_SOURCE_DIR}/cmake/embed_file.cmake"
        VERBATIM
    )
    target_sources(${target} PRIVATE "${embedded}")
endfunction()
