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
[[nodiscard]] Result<GpuPipeline> CreateLinePipeline(SDL_GPUDevice* device,
                                                     SDL_GPUTextureFormat colorFormat);

}
