#include "render/frame_readback.hpp"

#include <format>
#include <utility>

#include "render/gpu_error.hpp"

namespace rr::render {
namespace {

constexpr std::size_t kBytesPerPixel = 4;
constexpr std::byte kOpaque{0xFF};

bool IsBgra(SDL_GPUTextureFormat format) {
    return format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
           format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
}

bool IsRgba(SDL_GPUTextureFormat format) {
    return format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM ||
           format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
}

}

Result<std::vector<std::byte>> ToRgba(std::span<const std::byte> pixels,
                                      SDL_GPUTextureFormat format) {
    const bool swapsRedBlue = IsBgra(format);
    if (!swapsRedBlue && !IsRgba(format)) {
        return Fail(ErrorCode::Platform,
                    std::format("can't read back texture format {}", static_cast<int>(format)));
    }
    std::vector<std::byte> rgba(pixels.begin(), pixels.end());
    for (std::size_t pixel = 0; pixel + kBytesPerPixel <= rgba.size(); pixel += kBytesPerPixel) {
        if (swapsRedBlue) {
            std::swap(rgba[pixel], rgba[pixel + 2]);
        }
        rgba[pixel + 3] = kOpaque;
    }
    return rgba;
}

Result<void> FrameReadback::Prepare(SDL_GPUDevice* device, SDL_GPUTextureFormat format,
                                    std::uint32_t widthPx, std::uint32_t heightPx) {
    const SDL_GPUTextureCreateInfo texture{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = format,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = widthPx,
        .height = heightPx,
        .layer_count_or_depth = 1,
        .num_levels = 1};
    sizeBytes_ = SDL_CalculateGPUTextureFormatSize(format, widthPx, heightPx, 1);
    const SDL_GPUTransferBufferCreateInfo transfer{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                   .size = sizeBytes_};
    target_ = GpuTexture(device, SDL_CreateGPUTexture(device, &texture));
    download_ = GpuTransferBuffer(device, SDL_CreateGPUTransferBuffer(device, &transfer));
    if (target_.Get() == nullptr || download_.Get() == nullptr) {
        target_ = GpuTexture();
        download_ = GpuTransferBuffer();
        return GpuFailure("can't create the screenshot targets");
    }
    format_ = format;
    widthPx_ = widthPx;
    heightPx_ = heightPx;
    isFinished_ = false;
    return {};
}

void FrameReadback::Finish(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain) {
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    const SDL_GPUTextureRegion source{
        .texture = target_.Get(), .w = widthPx_, .h = heightPx_, .d = 1};
    const SDL_GPUTextureTransferInfo destination{.transfer_buffer = download_.Get()};
    SDL_DownloadFromGPUTexture(pass, &source, &destination);
    SDL_EndGPUCopyPass(pass);
    const SDL_GPUBlitInfo blit{.source = {.texture = target_.Get(), .w = widthPx_, .h = heightPx_},
                               .destination = {.texture = swapchain, .w = widthPx_, .h = heightPx_},
                               .load_op = SDL_GPU_LOADOP_DONT_CARE,
                               .filter = SDL_GPU_FILTER_NEAREST};
    SDL_BlitGPUTexture(commands, &blit);
    isFinished_ = true;
}

Result<assets::ImageAsset> FrameReadback::Submit(SDL_GPUDevice* device,
                                                 SDL_GPUCommandBuffer* commands) {
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == nullptr) {
        return GpuFailure("can't submit a frame");
    }
    const bool waited = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    const bool isFinished = std::exchange(isFinished_, false);
    Result<assets::ImageAsset> image = [&]() -> Result<assets::ImageAsset> {
        if (!waited) {
            return GpuFailure("can't wait for the frame");
        }
        if (!isFinished) {
            return Fail(ErrorCode::Platform, "no frame was drawn to save");
        }
        return Read(device);
    }();
    target_ = GpuTexture();
    download_ = GpuTransferBuffer();
    return image;
}

Result<assets::ImageAsset> FrameReadback::Read(SDL_GPUDevice* device) const {
    const void* mapped = SDL_MapGPUTransferBuffer(device, download_.Get(), false);
    if (mapped == nullptr) {
        return GpuFailure("can't read the frame back");
    }
    Result<std::vector<std::byte>> rgba =
        ToRgba(std::span(static_cast<const std::byte*>(mapped), sizeBytes_), format_);
    SDL_UnmapGPUTransferBuffer(device, download_.Get());
    return rgba.transform([&](std::vector<std::byte> pixels) {
        return assets::ImageAsset{
            .widthPx = widthPx_, .heightPx = heightPx_, .rgba = std::move(pixels)};
    });
}

}
