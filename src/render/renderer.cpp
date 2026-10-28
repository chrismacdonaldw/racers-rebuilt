#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <limits>
#include <utility>

#include "core/assert.hpp"
#include "render/gpu_error.hpp"
#include "render/pipelines.hpp"

#include <glm/vec3.hpp>

namespace rr::render {
namespace {

constexpr SDL_FColor kClearColor{.r = 0.08F, .g = 0.09F, .b = 0.12F, .a = 1.0F};
constexpr glm::vec3 kLightDirection{-0.36F, -0.86F, -0.36F};
constexpr std::size_t kLineVertexBytes = kMaxDebugLines * 2 * sizeof(LineVertex);

struct DrawUniforms {
    glm::mat4 model;
    glm::mat4 viewProjection;
};

struct MaterialUniforms {
    Color tint;
    glm::vec3 lightDirection;
    float alphaCutoff = 0.0F;
};

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
    Result<void> ready =
        renderer.SetPresentMode(mode).and_then([&] { return renderer.CreatePipelines(); });
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
        meshes_.push_back({.vertices = std::move(*vertexBuffer),
                           .indices = std::move(*indexBuffer),
                           .indexCount = static_cast<std::uint32_t>(indices.size())});
        return MeshId{static_cast<std::uint32_t>(meshes_.size() - 1)};
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
    textures_.push_back(std::move(texture));
    return TextureId{static_cast<std::uint32_t>(textures_.size() - 1)};
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
        return swapchain == nullptr ? Result<void>{} : EnsureDepthTexture(widthPx, heightPx);
    });
    if (prepared && swapchain != nullptr) {
        const SDL_GPUColorTargetInfo color{.texture = swapchain,
                                           .clear_color = kClearColor,
                                           .load_op = SDL_GPU_LOADOP_CLEAR,
                                           .store_op = SDL_GPU_STOREOP_STORE};
        const SDL_GPUDepthStencilTargetInfo depth{.texture = depth_.Get(),
                                                  .clear_depth = 1.0F,
                                                  .load_op = SDL_GPU_LOADOP_CLEAR,
                                                  .store_op = SDL_GPU_STOREOP_DONT_CARE,
                                                  .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                                                  .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE};
        const glm::mat4 viewProjection =
            ViewProjection(view.camera, static_cast<float>(widthPx) / static_cast<float>(heightPx));
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        DrawMeshes(commands, pass, view.meshes, viewProjection);
        DrawLines(commands, pass, lines.size(), viewProjection);
        SDL_EndGPURenderPass(pass);
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
    Result<GpuPipeline> meshPipeline = CreateMeshPipeline(device, colorFormat);
    Result<GpuPipeline> linePipeline = meshPipeline.and_then(
        [&](const GpuPipeline&) { return CreateLinePipeline(device, colorFormat); });
    Result<GpuBuffer> lineVertices = linePipeline.and_then([&](const GpuPipeline&) {
        return CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, kLineVertexBytes);
    });
    if (!lineVertices) {
        return std::unexpected(std::move(lineVertices.error()));
    }
    meshPipeline_ = std::move(*meshPipeline);
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

Result<void> Renderer::EnsureDepthTexture(std::uint32_t widthPx, std::uint32_t heightPx) {
    if (depth_.Get() != nullptr && widthPx == depthWidthPx_ && heightPx == depthHeightPx_) {
        return {};
    }
    const SDL_GPUTextureCreateInfo info{.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = kDepthFormat,
                                        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                                        .width = widthPx,
                                        .height = heightPx,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    depth_ = GpuTexture(device_.get(), SDL_CreateGPUTexture(device_.get(), &info));
    if (depth_.Get() == nullptr) {
        return GpuFailure("can't create the depth buffer");
    }
    depthWidthPx_ = widthPx;
    depthHeightPx_ = heightPx;
    return {};
}

void Renderer::DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                          std::span<const MeshDraw> meshes, const glm::mat4& viewProjection) const {
    SDL_BindGPUGraphicsPipeline(pass, meshPipeline_.Get());
    for (const MeshDraw& draw : meshes) {
        const auto meshIndex = static_cast<std::size_t>(draw.mesh);
        const auto textureIndex = static_cast<std::size_t>(draw.material.texture);
        RR_VERIFY(meshIndex < meshes_.size() && textureIndex < textures_.size());
        const Mesh& mesh = meshes_[meshIndex];

        const DrawUniforms drawUniforms{.model = ModelMatrix(draw.transform),
                                        .viewProjection = viewProjection};
        const MaterialUniforms materialUniforms{.tint = draw.material.tint,
                                                .lightDirection = glm::normalize(kLightDirection),
                                                .alphaCutoff = draw.material.alphaCutoff};
        SDL_PushGPUVertexUniformData(commands, 0, &drawUniforms, sizeof(drawUniforms));
        SDL_PushGPUFragmentUniformData(commands, 0, &materialUniforms, sizeof(materialUniforms));

        const SDL_GPUBufferBinding vertices{.buffer = mesh.vertices.Get()};
        const SDL_GPUBufferBinding indices{.buffer = mesh.indices.Get()};
        const SDL_GPUTextureSamplerBinding texture{.texture = textures_[textureIndex].Get(),
                                                   .sampler = sampler_.Get()};
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
        SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
        SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, 1, 0, 0, 0);
    }
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
