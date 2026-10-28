#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/window.hpp"
#include "render/gpu_resource.hpp"
#include "render/render_view.hpp"

namespace rr::render {

inline constexpr std::size_t kMaxDebugLines = 4096;

/// Must not outlive the window it draws to. Main thread only.
class Renderer {
public:
    [[nodiscard]] static Result<Renderer> Create(const platform::Window& window,
                                                 platform::PresentMode mode);

    /// `Vsync` waits for the display. The other modes present immediately.
    [[nodiscard]] Result<void> SetPresentMode(platform::PresentMode mode);

    [[nodiscard]] Result<MeshId> UploadMesh(const assets::GeometryAsset& geometry);
    [[nodiscard]] Result<TextureId> UploadTexture(const assets::ImageAsset& image);

    /// Lines past `kMaxDebugLines` are dropped.
    [[nodiscard]] Result<void> Draw(const RenderView& view);

private:
    static constexpr std::size_t kBlendModeCount = 3;

    struct DeviceDeleter {
        SDL_Window* window = nullptr;

        void operator()(SDL_GPUDevice* device) const;
    };

    struct Mesh {
        GpuBuffer vertices;
        GpuBuffer indices;
        std::uint32_t indexCount = 0;
    };

    struct UploadPart {
        std::span<const std::byte> bytes;
        SDL_GPUBuffer* buffer = nullptr;
    };

    explicit Renderer(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device);

    Result<void> CreatePipelines();
    Result<GpuBuffer> CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes);
    Result<GpuTransferBuffer> Stage(std::span<const std::span<const std::byte>> parts);
    Result<void> UploadBuffers(std::span<const UploadPart> parts);
    Result<void> UploadLines(SDL_GPUCommandBuffer* commands, std::span<const DebugLine> lines);
    Result<void> EnsureDepthTexture(std::uint32_t widthPx, std::uint32_t heightPx);
    void DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                    std::span<const MeshDraw> meshes, const glm::mat4& viewProjection) const;
    void DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const MeshDraw& draw,
                  const glm::mat4& viewProjection, SDL_GPUGraphicsPipeline*& bound) const;
    void DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, std::size_t lineCount,
                   const glm::mat4& viewProjection) const;

    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device_;
    std::array<GpuPipeline, kBlendModeCount> meshPipelines_;
    GpuPipeline linePipeline_;
    GpuSampler sampler_;
    GpuTexture depth_;
    std::uint32_t depthWidthPx_ = 0;
    std::uint32_t depthHeightPx_ = 0;
    GpuBuffer lineVertices_;
    GpuTransferBuffer lineUpload_;
    std::vector<Mesh> meshes_;
    std::vector<GpuTexture> textures_;
};

}
