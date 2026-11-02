rr_add_module(render
    SOURCES
        camera.cpp
        frame_readback.cpp
        light_uniforms.cpp
        pipelines.cpp
        particle_bank.cpp
        quad_batch.cpp
        renderer.cpp
        renderer_uploads.cpp
        sprite_batch.cpp
        ui_layout.cpp
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
rr_add_shader(rr_render QuadVertex vs "${PROJECT_SOURCE_DIR}/shaders/quad.vert.hlsl")
rr_add_shader(rr_render SpriteVertex vs "${PROJECT_SOURCE_DIR}/shaders/sprite.vert.hlsl")
rr_add_shader(rr_render SpriteFragment ps "${PROJECT_SOURCE_DIR}/shaders/sprite.frag.hlsl")
