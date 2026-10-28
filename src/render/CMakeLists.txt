rr_add_module(render
    SOURCES
        camera.cpp
        pipelines.cpp
        renderer.cpp
    DEPENDS
        rr::assets
        rr::core
        rr::platform
        glm::glm
        SDL3::SDL3
)
target_compile_definitions(rr_render PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

rr_add_shader(rr_render MeshVertex vs "${PROJECT_SOURCE_DIR}/shaders/mesh.vert.hlsl")
rr_add_shader(rr_render MeshFragment ps "${PROJECT_SOURCE_DIR}/shaders/mesh.frag.hlsl")
rr_add_shader(rr_render LineVertex vs "${PROJECT_SOURCE_DIR}/shaders/line.vert.hlsl")
rr_add_shader(rr_render LineFragment ps "${PROJECT_SOURCE_DIR}/shaders/line.frag.hlsl")
