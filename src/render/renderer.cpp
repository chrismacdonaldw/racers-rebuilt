#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "render/gpu_error.hpp"
#include "render/pipelines.hpp"

#include <glm/vec3.hpp>

namespace rr::render {
namespace {

constexpr SDL_FColor kClearColor{.r = 0.08F, .g = 0.09F, .b = 0.12F, .a = 1.0F};
constexpr glm::vec3 kLightDirection{-0.36F, -0.86F, -0.36F};
constexpr std::size_t kLineVertexBytes = kMaxDebugLines * 2 * sizeof(LineVertex);

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
    glm::vec3 lightDirection;
    float alphaCutoff = 0.0F;
    float modulateWeight = 0.0F;
    std::array<float, 3> padding{};
};

assets::ImageAsset WhitePixel() {
    return {.widthPx = 1, .heightPx = 1, .rgba = std::vector(4, std::byte{0xFF})};
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

Result<MeshId> Renderer::UploadMesh(const assets::GeometryAsset& geometry) {
    const std::span<const assets::GeometryVertex> vertices = geometry.vertices;
    const std::span<const std::uint32_t> indices = geometry.indices;
    const bool hasValidIndices =
        std::ranges::all_of(indices, [&](std::uint32_t index) { return index < vertices.size(); });
    if (indices.empty() || indices.size() % 3 != 0 || !hasValidIndices) {
        return Fail(ErrorCode::InvalidData,
                    "mesh indices aren't a triangle list into its vertices");
    }

    Result<GpuBuffer> vertexBuffer =
        CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices.size_bytes());
    Result<GpuBuffer> indexBuffer = vertexBuffer.and_then([&](const GpuBuffer&) {
        return CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indices.size_bytes());
    });
    if (!indexBuffer) {
        return std::unexpected(std::move(indexBuffer.error()));
    }
    const std::array parts{
        UploadPart{.bytes = std::as_bytes(vertices), .buffer = vertexBuffer->Get()},
        UploadPart{.bytes = std::as_bytes(indices), .buffer = indexBuffer->Get()},
    };
    return UploadBuffers(parts).transform([&] {
        return meshes_.Add({.vertices = std::move(*vertexBuffer),
                            .indices = std::move(*indexBuffer),
                            .indexCount = static_cast<std::uint32_t>(indices.size())});
    });
}

Result<TextureId> Renderer::UploadTexture(const assets::ImageAsset& image) {
    const std::size_t pixelCount = std::size_t{image.widthPx} * image.heightPx;
    if (pixelCount == 0 || image.rgba.size() != pixelCount * 4) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a {}x{} image needs {} bytes of RGBA, not {}", image.widthPx,
                                image.heightPx, pixelCount * 4, image.rgba.size()));
    }
    const SDL_GPUTextureCreateInfo info{.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        .width = image.widthPx,
                                        .height = image.heightPx,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    GpuTexture texture(device_.get(), SDL_CreateGPUTexture(device_.get(), &info));
    if (texture.Get() == nullptr) {
        return GpuFailure("can't create texture");
    }
    const std::array<std::span<const std::byte>, 1> parts{image.rgba};
    Result<GpuTransferBuffer> staged = Stage(parts);
    if (!staged) {
        return std::unexpected(std::move(staged.error()));
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a texture upload");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    const SDL_GPUTextureTransferInfo source{.transfer_buffer = staged->Get()};
    const SDL_GPUTextureRegion destination{
        .texture = texture.Get(), .w = image.widthPx, .h = image.heightPx, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        return GpuFailure("can't submit a texture upload");
    }
    return textures_.Add(std::move(texture));
}

void Renderer::ReleaseAssets() {
    meshes_.Release();
    textures_.Release();
}

Result<void> Renderer::Draw(const RenderView& view) {
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a frame");
    }
    const std::span<const DebugLine> lines =
        view.lines.first(std::min(view.lines.size(), kMaxDebugLines));
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    Result<void> prepared = UploadLines(commands, lines).and_then([&]() -> Result<void> {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, device_.get_deleter().window,
                                                   &swapchain, &widthPx, &heightPx)) {
            return GpuFailure("can't acquire the swapchain");
        }
        return swapchain == nullptr ? Result<void>{}
                                    : EnsureSceneTargets(Scaled(widthPx, resolutionScale_),
                                                         Scaled(heightPx, resolutionScale_));
    });
    if (prepared && swapchain != nullptr) {
        if (overlay_ != nullptr) {
            overlay_->Prepare(commands);
        }
        DrawScene(commands, view, lines.size(), IsScaled() ? sceneColor_.Get() : swapchain);
        if (IsScaled()) {
            const SDL_GPUBlitInfo blit{
                .source = {.texture = sceneColor_.Get(), .w = sceneWidthPx_, .h = sceneHeightPx_},
                .destination = {.texture = swapchain, .w = widthPx, .h = heightPx},
                .load_op = SDL_GPU_LOADOP_DONT_CARE,
                .filter = SDL_GPU_FILTER_LINEAR};
            SDL_BlitGPUTexture(commands, &blit);
        }
        if (overlay_ != nullptr) {
            DrawOverlay(commands, swapchain);
        }
    }
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        return GpuFailure("can't submit a frame");
    }
    return prepared;
}

Renderer::Renderer(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device)
    : device_(std::move(device)) {}

Result<void> Renderer::CreatePipelines() {
    SDL_GPUDevice* device = device_.get();
    const SDL_GPUTextureFormat colorFormat =
        SDL_GetGPUSwapchainTextureFormat(device, device_.get_deleter().window);
    colorFormat_ = colorFormat;
    for (std::size_t mode = 0; mode < kBlendModeCount; ++mode) {
        Result<GpuPipeline> meshPipeline =
            CreateMeshPipeline(device, colorFormat, static_cast<BlendMode>(mode));
        if (!meshPipeline) {
            return std::unexpected(std::move(meshPipeline.error()));
        }
        meshPipelines_.at(mode) = std::move(*meshPipeline);
    }
    Result<GpuPipeline> linePipeline = CreateLinePipeline(device, colorFormat);
    Result<GpuBuffer> lineVertices = linePipeline.and_then([&](const GpuPipeline&) {
        return CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, kLineVertexBytes);
    });
    if (!lineVertices) {
        return std::unexpected(std::move(lineVertices.error()));
    }
    linePipeline_ = std::move(*linePipeline);
    lineVertices_ = std::move(*lineVertices);

    const SDL_GPUTransferBufferCreateInfo uploadInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<std::uint32_t>(kLineVertexBytes)};
    lineUpload_ = GpuTransferBuffer(device, SDL_CreateGPUTransferBuffer(device, &uploadInfo));
    const SDL_GPUSamplerCreateInfo samplerInfo{.min_filter = SDL_GPU_FILTER_LINEAR,
                                               .mag_filter = SDL_GPU_FILTER_LINEAR,
                                               .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                                               .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                                               .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                                               .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT};
    sampler_ = GpuSampler(device, SDL_CreateGPUSampler(device, &samplerInfo));
    if (lineUpload_.Get() == nullptr || sampler_.Get() == nullptr) {
        return GpuFailure("can't create renderer resources");
    }
    return {};
}

Result<GpuBuffer> Renderer::CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes) {
    if (sizeBytes == 0 || sizeBytes > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("can't create a GPU buffer of {} bytes", sizeBytes));
    }
    const SDL_GPUBufferCreateInfo info{.usage = usage,
                                       .size = static_cast<std::uint32_t>(sizeBytes)};
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_.get(), &info);
    if (buffer == nullptr) {
        return GpuFailure("can't create a GPU buffer");
    }
    return GpuBuffer(device_.get(), buffer);
}

Result<GpuTransferBuffer> Renderer::Stage(std::span<const std::span<const std::byte>> parts) {
    std::size_t sizeBytes = 0;
    for (const std::span<const std::byte> part : parts) {
        sizeBytes += part.size();
    }
    if (sizeBytes > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(ErrorCode::InvalidData, std::format("can't stage {} bytes at once", sizeBytes));
    }
    const SDL_GPUTransferBufferCreateInfo info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                               .size = static_cast<std::uint32_t>(sizeBytes)};
    GpuTransferBuffer staging(device_.get(), SDL_CreateGPUTransferBuffer(device_.get(), &info));
    void* mapped = staging.Get() == nullptr
                       ? nullptr
                       : SDL_MapGPUTransferBuffer(device_.get(), staging.Get(), false);
    if (mapped == nullptr) {
        return GpuFailure("can't stage an upload");
    }
    auto* cursor = static_cast<std::byte*>(mapped);
    for (const std::span<const std::byte> part : parts) {
        cursor = std::ranges::copy(part, cursor).out;
    }
    SDL_UnmapGPUTransferBuffer(device_.get(), staging.Get());
    return staging;
}

Result<void> Renderer::UploadBuffers(std::span<const UploadPart> parts) {
    std::vector<std::span<const std::byte>> bytes;
    for (const UploadPart& part : parts) {
        bytes.push_back(part.bytes);
    }
    Result<GpuTransferBuffer> staged = Stage(bytes);
    if (!staged) {
        return std::unexpected(std::move(staged.error()));
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a buffer upload");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    std::uint32_t offset = 0;
    for (const UploadPart& part : parts) {
        const auto size = static_cast<std::uint32_t>(part.bytes.size());
        const SDL_GPUTransferBufferLocation source{.transfer_buffer = staged->Get(),
                                                   .offset = offset};
        const SDL_GPUBufferRegion destination{.buffer = part.buffer, .size = size};
        SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        offset += size;
    }
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        return GpuFailure("can't submit a buffer upload");
    }
    return {};
}

Result<void> Renderer::UploadLines(SDL_GPUCommandBuffer* commands,
                                   std::span<const DebugLine> lines) {
    if (lines.empty()) {
        return {};
    }
    void* mapped = SDL_MapGPUTransferBuffer(device_.get(), lineUpload_.Get(), true);
    if (mapped == nullptr) {
        return GpuFailure("can't map debug lines");
    }
    auto* vertices = static_cast<LineVertex*>(mapped);
    for (const DebugLine& line : lines) {
        *vertices++ = {.position = line.from, .color = line.color};
        *vertices++ = {.position = line.to, .color = line.color};
    }
    SDL_UnmapGPUTransferBuffer(device_.get(), lineUpload_.Get());

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    const SDL_GPUTransferBufferLocation source{.transfer_buffer = lineUpload_.Get()};
    const SDL_GPUBufferRegion destination{
        .buffer = lineVertices_.Get(),
        .size = static_cast<std::uint32_t>(lines.size() * 2 * sizeof(LineVertex))};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);
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

void Renderer::DrawScene(SDL_GPUCommandBuffer* commands, const RenderView& view,
                         std::size_t lineCount, SDL_GPUTexture* target) {
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
    const glm::mat4 viewProjection = ViewProjection(
        view.camera, static_cast<float>(sceneWidthPx_) / static_cast<float>(sceneHeightPx_));
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
    DrawMeshes(commands, pass, view.meshes, viewProjection);
    DrawLines(commands, pass, lineCount, viewProjection);
    SDL_EndGPURenderPass(pass);
}

void Renderer::DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain) {
    const SDL_GPUColorTargetInfo color{
        .texture = swapchain, .load_op = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
    overlay_->Draw(commands, pass);
    SDL_EndGPURenderPass(pass);
}

void Renderer::DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                          std::span<const MeshDraw> meshes, const glm::mat4& viewProjection) const {
    SDL_GPUGraphicsPipeline* bound = nullptr;
    for (const MeshDraw& draw : meshes | std::views::filter(IsOpaque)) {
        DrawMesh(commands, pass, draw, viewProjection, bound);
    }
    for (const MeshDraw& draw : meshes | std::views::filter(std::not_fn(IsOpaque))) {
        DrawMesh(commands, pass, draw, viewProjection, bound);
    }
}

void Renderer::DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                        const MeshDraw& draw, const glm::mat4& viewProjection,
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
    const DrawUniforms drawUniforms{.model = ModelMatrix(draw.transform),
                                    .viewProjection = viewProjection};
    const MaterialUniforms materialUniforms{
        .tint = draw.material.tint,
        .ambient = draw.material.ambient,
        .lightDirection = glm::normalize(kLightDirection),
        .alphaCutoff = draw.material.alphaCutoff,
        .modulateWeight = draw.material.modulates ? 1.0F : 0.0F};
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
                         std::size_t lineCount, const glm::mat4& viewProjection) const {
    if (lineCount == 0) {
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, linePipeline_.Get());
    SDL_PushGPUVertexUniformData(commands, 0, &viewProjection, sizeof(viewProjection));
    const SDL_GPUBufferBinding vertices{.buffer = lineVertices_.Get()};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    SDL_DrawGPUPrimitives(pass, static_cast<std::uint32_t>(lineCount * 2), 1, 0, 0);
}

void Renderer::DeviceDeleter::operator()(SDL_GPUDevice* device) const {
    if (window != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }
    SDL_DestroyGPUDevice(device);
}

}
