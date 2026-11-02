#pragma once

#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "render/gpu_resource.hpp"

namespace rr::render {

/// Alpha comes back opaque. Fails for formats other than 8-bit RGBA and BGRA.
[[nodiscard]] Result<std::vector<std::byte>> ToRgba(std::span<const std::byte> pixels,
                                                    SDL_GPUTextureFormat format);

/// Copies one drawn frame back to the CPU. Main thread only.
class FrameReadback {
public:
    [[nodiscard]] Result<void> Prepare(SDL_GPUDevice* device, SDL_GPUTextureFormat format,
                                       std::uint32_t widthPx, std::uint32_t heightPx);

    /// The frame is drawn here instead of the swapchain.
    [[nodiscard]] SDL_GPUTexture* Target() const { return target_.Get(); }

    /// Queues the download, then shows `Target` on `swapchain`.
    void Finish(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain);

    /// Submits `commands` and waits for the GPU. Fails when nothing was finished since `Prepare`.
    [[nodiscard]] Result<assets::ImageAsset> Submit(SDL_GPUDevice* device,
                                                    SDL_GPUCommandBuffer* commands);

private:
    [[nodiscard]] Result<assets::ImageAsset> Read(SDL_GPUDevice* device) const;

    GpuTexture target_;
    GpuTransferBuffer download_;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    std::uint32_t widthPx_ = 0;
    std::uint32_t heightPx_ = 0;
    std::uint32_t sizeBytes_ = 0;
    bool isFinished_ = false;
};

}
