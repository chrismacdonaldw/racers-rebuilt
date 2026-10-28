#include "render/pipelines.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "assets/geometry_asset.hpp"
#include "render/gpu_error.hpp"
#include "render/shader_code.hpp"

namespace rr::render {
namespace {

struct ShaderSpec {
    std::span<const std::uint8_t> code;
    SDL_GPUShaderStage stage = SDL_GPU_SHADERSTAGE_VERTEX;
    std::uint32_t samplerCount = 0;
    std::uint32_t uniformBufferCount = 1;
};

struct PipelineSpec {
    ShaderSpec vertexShader;
    ShaderSpec fragmentShader;
    std::span<const SDL_GPUVertexAttribute> attributes;
    std::uint32_t vertexPitch = 0;
    SDL_GPUPrimitiveType primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
};

constexpr std::array kMeshAttributes{
    SDL_GPUVertexAttribute{.location = 0,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                           .offset = offsetof(assets::GeometryVertex, position)},
    SDL_GPUVertexAttribute{.location = 1,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                           .offset = offsetof(assets::GeometryVertex, normal)},
    SDL_GPUVertexAttribute{.location = 2,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                           .offset = offsetof(assets::GeometryVertex, u)},
};

constexpr std::array kLineAttributes{
    SDL_GPUVertexAttribute{.location = 0,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                           .offset = offsetof(LineVertex, position)},
    SDL_GPUVertexAttribute{.location = 1,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                           .offset = offsetof(LineVertex, color)},
};

Result<GpuShader> CreateShader(SDL_GPUDevice* device, const ShaderSpec& spec) {
    const SDL_GPUShaderCreateInfo info{
        .code_size = spec.code.size(),
        .code = spec.code.data(),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_DXIL,
        .stage = spec.stage,
        .num_samplers = spec.samplerCount,
        .num_uniform_buffers = spec.uniformBufferCount,
    };
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    if (shader == nullptr) {
        return GpuFailure("can't create shader");
    }
    return GpuShader(device, shader);
}

Result<GpuPipeline> CreatePipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat colorFormat,
                                   const PipelineSpec& spec) {
    Result<GpuShader> vertexShader = CreateShader(device, spec.vertexShader);
    Result<GpuShader> fragmentShader = vertexShader.and_then(
        [&](const GpuShader&) { return CreateShader(device, spec.fragmentShader); });
    if (!fragmentShader) {
        return std::unexpected(std::move(fragmentShader.error()));
    }

    const SDL_GPUVertexBufferDescription vertexBuffer{
        .slot = 0, .pitch = spec.vertexPitch, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX};
    const SDL_GPUColorTargetDescription colorTarget{.format = colorFormat};
    const SDL_GPUGraphicsPipelineCreateInfo info{
        .vertex_shader = vertexShader->Get(),
        .fragment_shader = fragmentShader->Get(),
        .vertex_input_state = {.vertex_buffer_descriptions = &vertexBuffer,
                               .num_vertex_buffers = 1,
                               .vertex_attributes = spec.attributes.data(),
                               .num_vertex_attributes =
                                   static_cast<std::uint32_t>(spec.attributes.size())},
        .primitive_type = spec.primitive,
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                             .cull_mode = SDL_GPU_CULLMODE_NONE,
                             .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                             .enable_depth_clip = true},
        .depth_stencil_state = {.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                                .enable_depth_test = true,
                                .enable_depth_write = true},
        .target_info = {.color_target_descriptions = &colorTarget,
                        .num_color_targets = 1,
                        .depth_stencil_format = kDepthFormat,
                        .has_depth_stencil_target = true},
    };
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (pipeline == nullptr) {
        return GpuFailure("can't create graphics pipeline");
    }
    return GpuPipeline(device, pipeline);
}

}

Result<GpuPipeline> CreateMeshPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat colorFormat) {
    return CreatePipeline(
        device, colorFormat,
        {.vertexShader = {.code = shaders::kMeshVertex, .stage = SDL_GPU_SHADERSTAGE_VERTEX},
         .fragmentShader = {.code = shaders::kMeshFragment,
                            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                            .samplerCount = 1},
         .attributes = kMeshAttributes,
         .vertexPitch = sizeof(assets::GeometryVertex),
         .primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST});
}

Result<GpuPipeline> CreateLinePipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat colorFormat) {
    return CreatePipeline(
        device, colorFormat,
        {.vertexShader = {.code = shaders::kLineVertex, .stage = SDL_GPU_SHADERSTAGE_VERTEX},
         .fragmentShader = {.code = shaders::kLineFragment,
                            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                            .uniformBufferCount = 0},
         .attributes = kLineAttributes,
         .vertexPitch = sizeof(LineVertex),
         .primitive = SDL_GPU_PRIMITIVETYPE_LINELIST});
}

}
