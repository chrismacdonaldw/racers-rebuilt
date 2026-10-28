#pragma once

#include <SDL3/SDL_gpu.h>

namespace rr::render {

/// Drawn over the finished frame at full window resolution, without depth.
class OverlayPass {
public:
    virtual ~OverlayPass() = default;

    /// Runs before any render pass of the frame, so it may copy data to the GPU.
    virtual void Prepare(SDL_GPUCommandBuffer* commands) = 0;

    virtual void Draw(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass) = 0;
};

}
