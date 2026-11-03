#include "render/sdl_gpu_backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/assert.hpp"
#include "render/gpu_error.hpp"
#include "render/pipelines.hpp"

#include <imgui_impl_sdlgpu3.h>

namespace rr::render {
namespace {

constexpr float kMaxLod = 16.0F;
constexpr std::size_t kLineVertexBytes = kMaxDebugLines * 2 * sizeof(LineVertex);
constexpr std::size_t kSpriteVertexBytes = kMaxSprites * kVerticesPerSprite * sizeof(SpriteVertex);
constexpr std::size_t kQuadVertexBytes = kMaxQuads * kVerticesPerSprite * sizeof(QuadVertex);
static_assert(kMaxQuads <= kMaxSprites);
static_assert(kMaxSprites * kVerticesPerSprite <=
              std::size_t{std::numeric_limits<std::uint16_t>::max()} + 1);

std::uint32_t Scaled(std::uint32_t sizePx, float scale) {
    return std::max(1U,
                    static_cast<std::uint32_t>(std::lround(static_cast<float>(sizePx) * scale)));
}

assets::ImageAsset WhitePixel() {
    return {.widthPx = 1, .heightPx = 1, .rgba = std::vector(4, std::byte{0xFF})};
}

std::size_t SamplerSlot(TextureFilter filter, TextureAddress address) {
    return (static_cast<std::size_t>(filter) * 2) + static_cast<std::size_t>(address);
}

GpuSampler CreateSampler(SDL_GPUDevice* device, TextureFilter filter, TextureAddress address) {
    const SDL_GPUFilter texels =
        filter == TextureFilter::Point ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
    const SDL_GPUSamplerAddressMode edges = address == TextureAddress::Clamp
                                                ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
                                                : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    const SDL_GPUSamplerCreateInfo info{.min_filter = texels,
                                        .mag_filter = texels,
                                        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
                                        .address_mode_u = edges,
                                        .address_mode_v = edges,
                                        .address_mode_w = edges,
                                        .max_lod = kMaxLod};
    return {device, SDL_CreateGPUSampler(device, &info)};
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

Result<std::unique_ptr<SdlGpuBackend>> SdlGpuBackend::Create(const platform::Window& window,
                                                             platform::PresentMode mode) {
    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, "direct3d12");
    if (device == nullptr) {
        return GpuFailure("can't create a Direct3D 12 device");
    }
    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> owned(device, DeviceDeleter{});
    if (!SDL_ClaimWindowForGPUDevice(device, window.Handle())) {
        return GpuFailure("can't attach the GPU device to the window");
    }
    owned.get_deleter().window = window.Handle();

    std::unique_ptr<SdlGpuBackend> backend(new SdlGpuBackend(std::move(owned)));
    Result<void> ready = backend->SetPresentMode(mode)
                             .and_then([&] { return backend->CreatePipelines(); })
                             .and_then([&] { return backend->CreateResources(); })
                             .and_then([&] { return backend->UploadTexture(WhitePixel()); })
                             .transform([&](TextureId white) {
                                 RR_VERIFY(white == kWhiteTexture);
                                 backend->textures_.Pin();
                             });
    if (!ready) {
        return std::unexpected(std::move(ready.error()));
    }
    return backend;
}

Result<void> SdlGpuBackend::SetPresentMode(platform::PresentMode mode) {
    SDL_Window* window = device_.get_deleter().window;
    const SDL_GPUPresentMode presentMode = ChoosePresentMode(device_.get(), window, mode);
    if (!SDL_SetGPUSwapchainParameters(device_.get(), window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       presentMode)) {
        return GpuFailure("can't set the present mode");
    }
    return {};
}

void SdlGpuBackend::ReleaseAssets() {
    meshes_.Release();
    textures_.Release();
}

Result<std::optional<assets::ImageAsset>> SdlGpuBackend::Draw(std::span<const RenderView> views,
                                                              const FrameSettings& settings,
                                                              bool captures) {
    settings_ = settings;
    ImDrawData* overlay = settings.overlay == nullptr ? nullptr : settings.overlay->TakeDrawData();
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a frame");
    }
    const Result<void> drawn = DrawFrame(commands, views, overlay, captures);
    if (!captures) {
        if (!SDL_SubmitGPUCommandBuffer(commands)) {
            return GpuFailure("can't submit a frame");
        }
        return drawn.transform([] { return std::optional<assets::ImageAsset>(); });
    }
    Result<assets::ImageAsset> frame = readback_.Submit(device_.get(), commands);
    if (!drawn) {
        return std::unexpected(drawn.error());
    }
    return frame.transform(
        [](assets::ImageAsset& image) { return std::optional(std::move(image)); });
}

Result<void> SdlGpuBackend::StartImGui() {
    ImGui_ImplSDLGPU3_InitInfo info{.Device = device_.get(), .ColorTargetFormat = colorFormat_};
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        return Fail(ErrorCode::Platform, "can't start the overlay's renderer");
    }
    return {};
}

void SdlGpuBackend::StopImGui() {
    ImGui_ImplSDLGPU3_Shutdown();
}

std::string SdlGpuBackend::DriverName() const {
    const char* driver = SDL_GetGPUDeviceDriver(device_.get());
    return driver == nullptr ? "" : driver;
}

Result<void> SdlGpuBackend::DrawFrame(SDL_GPUCommandBuffer* commands,
                                      std::span<const RenderView> views, ImDrawData* overlay,
                                      bool captures) {
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    Result<void> prepared = UploadFrame(commands, views).and_then([&]() -> Result<void> {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, device_.get_deleter().window,
                                                   &swapchain, &widthPx, &heightPx)) {
            return GpuFailure("can't acquire the swapchain");
        }
        return swapchain == nullptr
                   ? Result<void>{}
                   : EnsureSceneTargets(Scaled(widthPx, settings_.resolutionScale),
                                        Scaled(heightPx, settings_.resolutionScale));
    });
    if (prepared && swapchain != nullptr && captures) {
        prepared = readback_.Prepare(device_.get(), colorFormat_, widthPx, heightPx);
    }
    if (!prepared || swapchain == nullptr) {
        return prepared;
    }
    if (overlay != nullptr) {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDLGPU3_PrepareDrawData(overlay, commands);
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
    if (overlay != nullptr) {
        DrawOverlay(commands, swapchain, overlay);
    }
    return {};
}

SdlGpuBackend::SdlGpuBackend(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device)
    : device_(std::move(device)) {}

Result<void> SdlGpuBackend::CreatePipelines() {
    SDL_GPUDevice* device = device_.get();
    colorFormat_ = SDL_GetGPUSwapchainTextureFormat(device, device_.get_deleter().window);
    for (std::size_t mode = 0; mode < kBlendModeCount; ++mode) {
        const auto blend = static_cast<BlendMode>(mode);
        Result<GpuPipeline> meshPipeline = CreateMeshPipeline(device, colorFormat_, blend);
        Result<GpuPipeline> skinnedPipeline = meshPipeline.and_then([&](const GpuPipeline&) {
            return CreateSkinnedMeshPipeline(device, colorFormat_, blend);
        });
        Result<GpuPipeline> backdropPipeline = skinnedPipeline.and_then([&](const GpuPipeline&) {
            return CreateBackdropPipeline(device, colorFormat_, blend);
        });
        Result<GpuPipeline> quadPipeline = backdropPipeline.and_then(
            [&](const GpuPipeline&) { return CreateQuadPipeline(device, colorFormat_, blend); });
        Result<GpuPipeline> spritePipeline = quadPipeline.and_then(
            [&](const GpuPipeline&) { return CreateSpritePipeline(device, colorFormat_, blend); });
        if (!spritePipeline) {
            return std::unexpected(std::move(spritePipeline.error()));
        }
        meshPipelines_.at(mode) = std::move(*meshPipeline);
        skinnedMeshPipelines_.at(mode) = std::move(*skinnedPipeline);
        backdropPipelines_.at(mode) = std::move(*backdropPipeline);
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

Result<void> SdlGpuBackend::CreateResources() {
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
    for (const TextureFilter filter : {TextureFilter::Linear, TextureFilter::Point}) {
        for (const TextureAddress address : {TextureAddress::Repeat, TextureAddress::Clamp}) {
            GpuSampler sampler = CreateSampler(device_.get(), filter, address);
            if (sampler.Get() == nullptr) {
                return GpuFailure("can't create renderer samplers");
            }
            samplers_.at(SamplerSlot(filter, address)) = std::move(sampler);
        }
    }
    return {};
}

Result<void> SdlGpuBackend::EnsureSceneTargets(std::uint32_t widthPx, std::uint32_t heightPx) {
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

SDL_GPUTexture* SdlGpuBackend::Shown(TextureId texture) const {
    const Texture& shown = textures_.Get(texture);
    return settings_.isSmoothingTextures && shown.enhanced.Get() != nullptr ? shown.enhanced.Get()
                                                                            : shown.original.Get();
}

SDL_GPUSampler* SdlGpuBackend::Sampler(TextureFilter filter, TextureAddress address) const {
    return samplers_.at(SamplerSlot(filter, address)).Get();
}

void SdlGpuBackend::DeviceDeleter::operator()(SDL_GPUDevice* device) const {
    if (window != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }
    SDL_DestroyGPUDevice(device);
}

}
