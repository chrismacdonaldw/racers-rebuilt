#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <span>

#include "core/assert.hpp"
#include "render/joint_uniforms.hpp"
#include "render/light_uniforms.hpp"
#include "render/sdl_gpu_backend.hpp"
#include "render/ui_layout.hpp"

#include <glm/vec2.hpp>
#include <imgui_impl_sdlgpu3.h>

namespace rr::render {
namespace {

constexpr SDL_FColor kClearColor{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};

struct DrawUniforms {
    glm::mat4 model;
    glm::mat4 viewProjection;
};

struct MaterialUniforms {
    Color tint;
    Color ambient;
    float alphaCutoff = 0.0F;
    float modulateWeight = 0.0F;
    glm::vec2 uvOffset{};
    Color vertexScale;
    Color vertexOffset;
    float flatWeight = 0.0F;
    std::array<float, 3> padding{};
};

bool IsOpaque(const MeshDraw& draw) {
    return draw.material.blend == BlendMode::Opaque;
}

}

void SdlGpuBackend::DrawScenes(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                               SDL_GPUTexture* target) const {
    const SDL_GPUColorTargetInfo color{.texture = target,
                                       .clear_color = kClearColor,
                                       .load_op = SDL_GPU_LOADOP_CLEAR,
                                       .store_op = SDL_GPU_STOREOP_STORE};
    const SDL_GPUDepthStencilTargetInfo depth{.texture = depth_.Get(),
                                              .clear_depth = 1.0F,
                                              .load_op = SDL_GPU_LOADOP_CLEAR,
                                              .store_op = SDL_GPU_STOREOP_DONT_CARE,
                                              .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                                              .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
    for (const auto& [view, range] : std::views::zip(views, viewRanges_)) {
        DrawScene(commands, pass, view, range);
    }
    SDL_EndGPURenderPass(pass);
}

void SdlGpuBackend::DrawScene(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                              const RenderView& view, const ViewRange& range) const {
    const PixelRect area = ViewportArea(view.viewport, sceneWidthPx_, sceneHeightPx_,
                                        settings_.uiFit, settings_.sceneFit);
    const SDL_GPUViewport viewport{.x = static_cast<float>(area.xPx),
                                   .y = static_cast<float>(area.yPx),
                                   .w = static_cast<float>(area.widthPx),
                                   .h = static_cast<float>(area.heightPx),
                                   .min_depth = 0.0F,
                                   .max_depth = 1.0F};
    SDL_SetGPUViewport(pass, &viewport);
    const SDL_Rect scissor{.x = static_cast<int>(area.xPx),
                           .y = static_cast<int>(area.yPx),
                           .w = static_cast<int>(area.widthPx),
                           .h = static_cast<int>(area.heightPx)};
    SDL_SetGPUScissor(pass, &scissor);
    const glm::mat4 viewProjection =
        ViewProjection(view.camera, ViewAspect(view.viewport, area, settings_.sceneFit));
    DrawMeshes(commands, pass, view, viewProjection);
    if (range.quadBatchCount > 0) {
        SDL_PushGPUVertexUniformData(commands, 0, &viewProjection, sizeof(viewProjection));
        DrawBatches(pass, quadStream_,
                    std::span(quadBatches_).subspan(range.firstQuadBatch, range.quadBatchCount),
                    quadPipelines_);
    }
    DrawLines(commands, pass, range, viewProjection);
}

void SdlGpuBackend::DrawSprites(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                                std::uint32_t widthPx, std::uint32_t heightPx) const {
    if (spriteBatches_.empty()) {
        return;
    }
    const UiToClip uiToClip =
        UiToClipTransform(FitUiArea(widthPx, heightPx, settings_.uiFit), widthPx, heightPx);
    SDL_PushGPUVertexUniformData(commands, 0, &uiToClip, sizeof(uiToClip));
    const SDL_GPUColorTargetInfo color{
        .texture = swapchain, .load_op = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    DrawBatches(pass, spriteStream_, spriteBatches_, spritePipelines_);
    SDL_EndGPURenderPass(pass);
}

void SdlGpuBackend::DrawBatches(SDL_GPURenderPass* pass, const StreamBuffer& stream,
                                std::span<const SpriteBatch> batches,
                                const std::array<GpuPipeline, kBlendModeCount>& pipelines) const {
    const SDL_GPUBufferBinding vertices{.buffer = stream.vertices.Get()};
    const SDL_GPUBufferBinding indices{.buffer = spriteIndices_.Get()};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    const SDL_GPUGraphicsPipeline* bound = nullptr;
    for (const SpriteBatch& batch : batches) {
        SDL_GPUGraphicsPipeline* pipeline =
            pipelines.at(static_cast<std::size_t>(batch.blend)).Get();
        if (pipeline != bound) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            bound = pipeline;
        }
        const SDL_GPUTextureSamplerBinding texture{
            .texture = Shown(batch.texture),
            .sampler = Sampler(TextureFilter::Linear, TextureAddress::Clamp)};
        SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
        SDL_DrawGPUIndexedPrimitives(pass, batch.spriteCount * kIndicesPerSprite, 1,
                                     batch.firstSprite * kIndicesPerSprite, 0, 0);
    }
}

void SdlGpuBackend::DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                                ImDrawData* overlay) {
    const SDL_GPUColorTargetInfo color{
        .texture = swapchain, .load_op = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(overlay, commands, pass);
    SDL_EndGPURenderPass(pass);
}

void SdlGpuBackend::DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                               const RenderView& view, const glm::mat4& viewProjection) const {
    const LightUniforms lights = ToLightUniforms(view.lighting);
    SDL_PushGPUFragmentUniformData(commands, 1, &lights, sizeof(lights));
    SDL_GPUGraphicsPipeline* bound = nullptr;
    for (const MeshDraw& draw : view.meshes | std::views::filter(IsOpaque)) {
        DrawMesh(commands, pass, draw, viewProjection, view.lighting.ambient, bound);
    }
    for (const MeshDraw& draw : view.meshes | std::views::filter(std::not_fn(IsOpaque))) {
        DrawMesh(commands, pass, draw, viewProjection, view.lighting.ambient, bound);
    }
}

void SdlGpuBackend::DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                             const MeshDraw& draw, const glm::mat4& viewProjection,
                             const Color& ambient, SDL_GPUGraphicsPipeline*& bound) const {
    const Mesh& mesh = meshes_.Get(draw.mesh);
    RR_VERIFY(draw.firstIndex <= mesh.indexCount);
    const std::uint32_t indexCount = std::min(draw.indexCount, mesh.indexCount - draw.firstIndex);
    RR_VERIFY(draw.indexCount == kWholeMesh || indexCount == draw.indexCount);

    const bool isSkinned = !draw.joints.empty();
    SDL_GPUGraphicsPipeline* pipeline = (isSkinned ? skinnedMeshPipelines_ : meshPipelines_)
                                            .at(static_cast<std::size_t>(draw.material.blend))
                                            .Get();
    if (pipeline != bound) {
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        bound = pipeline;
    }
    const DrawUniforms drawUniforms{.model = ModelMatrix(draw.transform, draw.scale),
                                    .viewProjection = viewProjection};
    const MaterialUniforms materialUniforms{
        .tint = draw.material.tint,
        .ambient = draw.material.ambient * ambient,
        .alphaCutoff = draw.material.alphaCutoff,
        .modulateWeight = draw.material.modulates ? 1.0F : 0.0F,
        .uvOffset = {draw.material.uvOffset.u, draw.material.uvOffset.v},
        .vertexScale = draw.material.vertexColors.scale,
        .vertexOffset = draw.material.vertexColors.offset,
        .flatWeight = draw.material.shading == Shading::Flat ? 1.0F : 0.0F};
    SDL_PushGPUVertexUniformData(commands, 0, &drawUniforms, sizeof(drawUniforms));
    if (isSkinned) {
        const JointUniforms joints = ToJointUniforms(draw.joints);
        SDL_PushGPUVertexUniformData(commands, 1, &joints, sizeof(joints));
    }
    SDL_PushGPUFragmentUniformData(commands, 0, &materialUniforms, sizeof(materialUniforms));

    const SDL_GPUBufferBinding vertices{.buffer = mesh.vertices.Get()};
    const SDL_GPUBufferBinding indices{.buffer = mesh.indices.Get()};
    const TextureFilter filter =
        settings_.isSmoothingTextures ? TextureFilter::Linear : draw.material.filter;
    const SDL_GPUTextureSamplerBinding texture{.texture = Shown(draw.material.texture),
                                               .sampler = Sampler(filter, draw.material.address)};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, draw.firstIndex, 0, 0);
}

void SdlGpuBackend::DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                              const ViewRange& range, const glm::mat4& viewProjection) const {
    if (range.lineCount == 0) {
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, linePipeline_.Get());
    SDL_PushGPUVertexUniformData(commands, 0, &viewProjection, sizeof(viewProjection));
    const SDL_GPUBufferBinding vertices{.buffer = lineStream_.vertices.Get()};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_DrawGPUPrimitives(pass, static_cast<std::uint32_t>(range.lineCount * 2), 1,
                          static_cast<std::uint32_t>(range.firstLine * 2), 0);
}

}
