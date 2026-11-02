#pragma once

#include <SDL3/SDL_gpu.h>

#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/gpu_resource.hpp"
#include "render/render_view.hpp"

namespace rr::render {

inline constexpr SDL_GPUTextureFormat kDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

struct LineVertex {
    Vector3 position;
    Color color;
};

[[nodiscard]] Result<GpuPipeline> CreateMeshPipeline(SDL_GPUDevice* device,
                                                     SDL_GPUTextureFormat colorFormat,
                                                     BlendMode blend);
/// Takes each vertex's `joint` as an index into a joint pose array in vertex uniform slot 1.
[[nodiscard]] Result<GpuPipeline> CreateSkinnedMeshPipeline(SDL_GPUDevice* device,
                                                            SDL_GPUTextureFormat colorFormat,
                                                            BlendMode blend);
[[nodiscard]] Result<GpuPipeline> CreateLinePipeline(SDL_GPUDevice* device,
                                                     SDL_GPUTextureFormat colorFormat);
[[nodiscard]] Result<GpuPipeline> CreateQuadPipeline(SDL_GPUDevice* device,
                                                     SDL_GPUTextureFormat colorFormat,
                                                     BlendMode blend);
[[nodiscard]] Result<GpuPipeline> CreateSpritePipeline(SDL_GPUDevice* device,
                                                       SDL_GPUTextureFormat colorFormat,
                                                       BlendMode blend);

}
