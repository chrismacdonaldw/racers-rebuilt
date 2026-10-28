#pragma once

#include <SDL3/SDL_gpu.h>

#include <utility>

namespace rr::render {

template <typename T, void (*Release)(SDL_GPUDevice*, T*)>
class GpuResource {
public:
    GpuResource() = default;
    GpuResource(SDL_GPUDevice* device, T* resource) : device_(device), resource_(resource) {}

    GpuResource(GpuResource&& other) noexcept
        : device_(other.device_), resource_(std::exchange(other.resource_, nullptr)) {}

    GpuResource& operator=(GpuResource&& other) noexcept {
        if (this != &other) {
            Reset();
            device_ = other.device_;
            resource_ = std::exchange(other.resource_, nullptr);
        }
        return *this;
    }

    GpuResource(const GpuResource&) = delete;
    GpuResource& operator=(const GpuResource&) = delete;

    ~GpuResource() { Reset(); }

    [[nodiscard]] T* Get() const { return resource_; }

private:
    void Reset() {
        if (resource_ != nullptr) {
            Release(device_, resource_);
            resource_ = nullptr;
        }
    }

    SDL_GPUDevice* device_ = nullptr;
    T* resource_ = nullptr;
};

using GpuBuffer = GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using GpuTransferBuffer = GpuResource<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
using GpuTexture = GpuResource<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
using GpuSampler = GpuResource<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using GpuShader = GpuResource<SDL_GPUShader, SDL_ReleaseGPUShader>;
using GpuPipeline = GpuResource<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;

}
