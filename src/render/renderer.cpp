#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "render/gpu_error.hpp"
#include "render/light_uniforms.hpp"
#include "render/pipelines.hpp"

#include <glm/vec2.hpp>

namespace rr::render {
namespace {

constexpr SDL_FColor kClearColor{.r = 0.08F, .g = 0.09F, .b = 0.12F, .a = 1.0F};
constexpr std::size_t kLineVertexBytes = kMaxDebugLines * 2 * sizeof(LineVertex);
constexpr std::size_t kSpriteVertexBytes = kMaxSprites * kVerticesPerSprite * sizeof(SpriteVertex);
constexpr std::size_t kQuadVertexBytes = kMaxQuads * kVerticesPerSprite * sizeof(QuadVertex);
static_assert(kMaxQuads <= kMaxSprites);
static_assert(kMaxSprites * kVerticesPerSprite <=
              std::size_t{std::numeric_limits<std::uint16_t>::max()} + 1);

SDL_Rect ViewportArea(const Viewport& viewport, std::uint32_t widthPx, std::uint32_t heightPx,
                      UiFit fit) {
    const PixelRect within = viewport.isOnUiCanvas
                                 ? FitUiArea(widthPx, heightPx, fit)
                                 : PixelRect{.widthPx = widthPx, .heightPx = heightPx};
    const auto edge = [](float fraction, std::uint32_t startPx, std::uint32_t sizePx) {
        return static_cast<int>(startPx) +
               static_cast<int>(std::lround(fraction * static_cast<float>(sizePx)));
    };
    const int left = edge(viewport.x, within.xPx, within.widthPx);
    const int top = edge(viewport.y, within.yPx, within.heightPx);
    return {
        .x = left,
        .y = top,
        .w = std::max(1, edge(viewport.x + viewport.width, within.xPx, within.widthPx) - left),
        .h = std::max(1, edge(viewport.y + viewport.height, within.yPx, within.heightPx) - top)};
}

std::uint32_t Scaled(std::uint32_t sizePx, float scale) {
    return std::max(1U,
                    static_cast<std::uint32_t>(std::lround(static_cast<float>(sizePx) * scale)));
}

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
};

assets::ImageAsset WhitePixel() {
    return {.widthPx = 1, .heightPx = 1, .rgba = std::vector(4, std::byte{0xFF})};
}

GpuSampler CreateSampler(SDL_GPUDevice* device, SDL_GPUSamplerAddressMode addressMode) {
    const SDL_GPUSamplerCreateInfo info{.min_filter = SDL_GPU_FILTER_LINEAR,
                                        .mag_filter = SDL_GPU_FILTER_LINEAR,
                                        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                                        .address_mode_u = addressMode,
                                        .address_mode_v = addressMode,
                                        .address_mode_w = addressMode};
    return {device, SDL_CreateGPUSampler(device, &info)};
}

bool IsOpaque(const MeshDraw& draw) {
    return draw.material.blend == BlendMode::Opaque;
}

SDL_GPUPresentMode ChoosePresentMode(SDL_GPUDevice* device, SDL_Window* window,
                                     platform::PresentMode mode) {
    if (mode != platform::PresentMode::Vsync) {
        for (const SDL_GPUPresentMode candidate :
             {SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_MAILBOX}) {
            if (SDL_WindowSupportsGPUPresentMode(device, window, candidate)) {
                return candidate;
            }
        }
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

}

Result<Renderer> Renderer::Create(const platform::Window& window, platform::PresentMode mode) {
    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, "direct3d12");
    if (device == nullptr) {
        return GpuFailure("can't create a Direct3D 12 device");
    }
    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> owned(device, DeviceDeleter{});
    if (!SDL_ClaimWindowForGPUDevice(device, window.Handle())) {
        return GpuFailure("can't attach the GPU device to the window");
    }
    owned.get_deleter().window = window.Handle();

    Renderer renderer(std::move(owned));
    Result<void> ready = renderer.SetPresentMode(mode)
                             .and_then([&] { return renderer.CreatePipelines(); })
                             .and_then([&] { return renderer.CreateResources(); })
                             .and_then([&] { return renderer.UploadTexture(WhitePixel()); })
                             .transform([&](TextureId white) {
                                 RR_VERIFY(white == kWhiteTexture);
                                 renderer.textures_.Pin();
                             });
    if (!ready) {
        return std::unexpected(std::move(ready.error()));
    }
    return renderer;
}

Result<void> Renderer::SetPresentMode(platform::PresentMode mode) {
    SDL_Window* window = device_.get_deleter().window;
    const SDL_GPUPresentMode presentMode = ChoosePresentMode(device_.get(), window, mode);
    if (!SDL_SetGPUSwapchainParameters(device_.get(), window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       presentMode)) {
        return GpuFailure("can't set the present mode");
    }
    return {};
}

void Renderer::SetResolutionScale(float scale) {
    resolutionScale_ = std::clamp(scale, kMinResolutionScale, kMaxResolutionScale);
}

void Renderer::ReleaseAssets() {
    meshes_.Release();
    textures_.Release();
}

Result<void> Renderer::Draw(std::span<const RenderView> views) {
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a frame");
    }
    const bool captures = std::exchange(isCaptureRequested_, false);
    const Result<void> drawn = DrawFrame(commands, views, captures);
    if (!captures) {
        if (!SDL_SubmitGPUCommandBuffer(commands)) {
            return GpuFailure("can't submit a frame");
        }
        return drawn;
    }
    Result<assets::ImageAsset> frame = readback_.Submit(device_.get(), commands);
    if (!drawn) {
        return drawn;
    }
    if (!frame) {
        return std::unexpected(std::move(frame.error()));
    }
    capture_ = std::move(*frame);
    return {};
}

std::optional<assets::ImageAsset> Renderer::TakeCapture() {
    return std::exchange(capture_, std::nullopt);
}

Result<void> Renderer::DrawFrame(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                                 bool captures) {
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    Result<void> prepared = UploadFrame(commands, views).and_then([&]() -> Result<void> {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, device_.get_deleter().window,
                                                   &swapchain, &widthPx, &heightPx)) {
            return GpuFailure("can't acquire the swapchain");
        }
        return swapchain == nullptr ? Result<void>{}
                                    : EnsureSceneTargets(Scaled(widthPx, resolutionScale_),
                                                         Scaled(heightPx, resolutionScale_));
    });
    if (prepared && swapchain != nullptr && captures) {
        prepared = readback_.Prepare(device_.get(), colorFormat_, widthPx, heightPx);
    }
    if (!prepared || swapchain == nullptr) {
        return prepared;
    }
    if (overlay_ != nullptr) {
        overlay_->Prepare(commands);
    }
    SDL_GPUTexture* frame = captures ? readback_.Target() : swapchain;
    DrawScenes(commands, views, IsScaled() ? sceneColor_.Get() : frame);
    if (IsScaled()) {
        const SDL_GPUBlitInfo blit{
            .source = {.texture = sceneColor_.Get(), .w = sceneWidthPx_, .h = sceneHeightPx_},
            .destination = {.texture = frame, .w = widthPx, .h = heightPx},
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .filter = SDL_GPU_FILTER_LINEAR};
        SDL_BlitGPUTexture(commands, &blit);
    }
    DrawSprites(commands, frame, widthPx, heightPx);
    if (captures) {
        readback_.Finish(commands, swapchain);
    }
    if (overlay_ != nullptr) {
        DrawOverlay(commands, swapchain);
    }
    return {};
}

Renderer::Renderer(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device)
    : device_(std::move(device)) {}

Result<void> Renderer::CreatePipelines() {
    SDL_GPUDevice* device = device_.get();
    colorFormat_ = SDL_GetGPUSwapchainTextureFormat(device, device_.get_deleter().window);
    for (std::size_t mode = 0; mode < kBlendModeCount; ++mode) {
        const auto blend = static_cast<BlendMode>(mode);
        Result<GpuPipeline> meshPipeline = CreateMeshPipeline(device, colorFormat_, blend);
        Result<GpuPipeline> quadPipeline = meshPipeline.and_then(
            [&](const GpuPipeline&) { return CreateQuadPipeline(device, colorFormat_, blend); });
        Result<GpuPipeline> spritePipeline = quadPipeline.and_then(
            [&](const GpuPipeline&) { return CreateSpritePipeline(device, colorFormat_, blend); });
        if (!spritePipeline) {
            return std::unexpected(std::move(spritePipeline.error()));
        }
        meshPipelines_.at(mode) = std::move(*meshPipeline);
        quadPipelines_.at(mode) = std::move(*quadPipeline);
        spritePipelines_.at(mode) = std::move(*spritePipeline);
    }
    Result<GpuPipeline> linePipeline = CreateLinePipeline(device, colorFormat_);
    if (!linePipeline) {
        return std::unexpected(std::move(linePipeline.error()));
    }
    linePipeline_ = std::move(*linePipeline);
    return {};
}

Result<void> Renderer::CreateResources() {
    const std::vector<std::uint16_t> indices = SpriteIndices(kMaxSprites);
    Result<StreamBuffer> lineStream = CreateStream(kLineVertexBytes);
    Result<StreamBuffer> quadStream =
        lineStream.and_then([&](const StreamBuffer&) { return CreateStream(kQuadVertexBytes); });
    Result<StreamBuffer> spriteStream =
        quadStream.and_then([&](const StreamBuffer&) { return CreateStream(kSpriteVertexBytes); });
    Result<GpuBuffer> spriteIndices = spriteStream.and_then([&](const StreamBuffer&) {
        return CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, std::span(indices).size_bytes());
    });
    if (!spriteIndices) {
        return std::unexpected(std::move(spriteIndices.error()));
    }
    const std::array parts{
        UploadPart{.bytes = std::as_bytes(std::span(indices)), .buffer = spriteIndices->Get()}};
    Result<void> uploaded = UploadBuffers(parts);
    if (!uploaded) {
        return uploaded;
    }
    lineStream_ = std::move(*lineStream);
    quadStream_ = std::move(*quadStream);
    spriteStream_ = std::move(*spriteStream);
    spriteIndices_ = std::move(*spriteIndices);
    sampler_ = CreateSampler(device_.get(), SDL_GPU_SAMPLERADDRESSMODE_REPEAT);
    uiSampler_ = CreateSampler(device_.get(), SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
    if (sampler_.Get() == nullptr || uiSampler_.Get() == nullptr) {
        return GpuFailure("can't create renderer samplers");
    }
    return {};
}

Result<void> Renderer::EnsureSceneTargets(std::uint32_t widthPx, std::uint32_t heightPx) {
    const bool needsColor = IsScaled() && sceneColor_.Get() == nullptr;
    if (depth_.Get() != nullptr && !needsColor && widthPx == sceneWidthPx_ &&
        heightPx == sceneHeightPx_) {
        return {};
    }
    SDL_GPUTextureCreateInfo info{.type = SDL_GPU_TEXTURETYPE_2D,
                                  .format = kDepthFormat,
                                  .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                                  .width = widthPx,
                                  .height = heightPx,
                                  .layer_count_or_depth = 1,
                                  .num_levels = 1};
    depth_ = GpuTexture(device_.get(), SDL_CreateGPUTexture(device_.get(), &info));
    info.format = colorFormat_;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    sceneColor_ = IsScaled() ? GpuTexture(device_.get(), SDL_CreateGPUTexture(device_.get(), &info))
                             : GpuTexture();
    if (depth_.Get() == nullptr || (IsScaled() && sceneColor_.Get() == nullptr)) {
        return GpuFailure("can't create the scene targets");
    }
    sceneWidthPx_ = widthPx;
    sceneHeightPx_ = heightPx;
    return {};
}

void Renderer::DrawScenes(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
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

void Renderer::DrawScene(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                         const RenderView& view, const ViewRange& range) const {
    const SDL_Rect area = ViewportArea(view.viewport, sceneWidthPx_, sceneHeightPx_, uiFit_);
    const SDL_GPUViewport viewport{.x = static_cast<float>(area.x),
                                   .y = static_cast<float>(area.y),
                                   .w = static_cast<float>(area.w),
                                   .h = static_cast<float>(area.h),
                                   .min_depth = 0.0F,
                                   .max_depth = 1.0F};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &area);
    const glm::mat4 viewProjection =
        ViewProjection(view.camera, static_cast<float>(area.w) / static_cast<float>(area.h));
    DrawMeshes(commands, pass, view, viewProjection);
    if (range.quadBatchCount > 0) {
        SDL_PushGPUVertexUniformData(commands, 0, &viewProjection, sizeof(viewProjection));
        DrawBatches(pass, quadStream_,
                    std::span(quadBatches_).subspan(range.firstQuadBatch, range.quadBatchCount),
                    quadPipelines_);
    }
    DrawLines(commands, pass, range, viewProjection);
}

void Renderer::DrawSprites(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                           std::uint32_t widthPx, std::uint32_t heightPx) const {
    if (spriteBatches_.empty()) {
        return;
    }
    const UiToClip uiToClip =
        UiToClipTransform(FitUiArea(widthPx, heightPx, uiFit_), widthPx, heightPx);
    SDL_PushGPUVertexUniformData(commands, 0, &uiToClip, sizeof(uiToClip));
    const SDL_GPUColorTargetInfo color{
        .texture = swapchain, .load_op = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    DrawBatches(pass, spriteStream_, spriteBatches_, spritePipelines_);
    SDL_EndGPURenderPass(pass);
}

void Renderer::DrawBatches(SDL_GPURenderPass* pass, const StreamBuffer& stream,
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
        const SDL_GPUTextureSamplerBinding texture{.texture = textures_.Get(batch.texture).Get(),
                                                   .sampler = uiSampler_.Get()};
        SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
        SDL_DrawGPUIndexedPrimitives(pass, batch.spriteCount * kIndicesPerSprite, 1,
                                     batch.firstSprite * kIndicesPerSprite, 0, 0);
    }
}

void Renderer::DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain) {
    const SDL_GPUColorTargetInfo color{
        .texture = swapchain, .load_op = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    overlay_->Draw(commands, pass);
    SDL_EndGPURenderPass(pass);
}

void Renderer::DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
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

void Renderer::DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                        const MeshDraw& draw, const glm::mat4& viewProjection, const Color& ambient,
                        SDL_GPUGraphicsPipeline*& bound) const {
    const Mesh& mesh = meshes_.Get(draw.mesh);
    RR_VERIFY(draw.firstIndex <= mesh.indexCount);
    const std::uint32_t indexCount = std::min(draw.indexCount, mesh.indexCount - draw.firstIndex);
    RR_VERIFY(draw.indexCount == kWholeMesh || indexCount == draw.indexCount);

    SDL_GPUGraphicsPipeline* pipeline =
        meshPipelines_.at(static_cast<std::size_t>(draw.material.blend)).Get();
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
        .vertexOffset = draw.material.vertexColors.offset};
    SDL_PushGPUVertexUniformData(commands, 0, &drawUniforms, sizeof(drawUniforms));
    SDL_PushGPUFragmentUniformData(commands, 0, &materialUniforms, sizeof(materialUniforms));

    const SDL_GPUBufferBinding vertices{.buffer = mesh.vertices.Get()};
    const SDL_GPUBufferBinding indices{.buffer = mesh.indices.Get()};
    const SDL_GPUTextureSamplerBinding texture{
        .texture = textures_.Get(draw.material.texture).Get(), .sampler = sampler_.Get()};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, draw.firstIndex, 0, 0);
}

void Renderer::DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
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

void Renderer::DeviceDeleter::operator()(SDL_GPUDevice* device) const {
    if (window != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }
    SDL_DestroyGPUDevice(device);
}

}
