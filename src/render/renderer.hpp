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
#include "render/asset_table.hpp"
#include "render/gpu_resource.hpp"
#include "render/overlay_pass.hpp"
#include "render/pipelines.hpp"
#include "render/render_view.hpp"
#include "render/sprite_batch.hpp"
#include "render/ui_layout.hpp"

namespace rr::render {

inline constexpr std::size_t kMaxDebugLines = 8192;
inline constexpr std::size_t kMaxSprites = 8192;
inline constexpr float kMinResolutionScale = 0.25F;
inline constexpr float kMaxResolutionScale = 2.0F;

/// Must not outlive the window it draws to. Main thread only.
class Renderer {
public:
    [[nodiscard]] static Result<Renderer> Create(const platform::Window& window,
                                                 platform::PresentMode mode);

    /// `Vsync` waits for the display. The other modes present immediately.
    [[nodiscard]] Result<void> SetPresentMode(platform::PresentMode mode);

    [[nodiscard]] Result<MeshId> UploadMesh(const assets::GeometryAsset& geometry);
    [[nodiscard]] Result<TextureId> UploadTexture(const assets::ImageAsset& image);
    /// Invalidates every `MeshId` and `TextureId` handed out so far, except `kWhiteTexture`.
    void ReleaseAssets();

    /// The scene renders at this fraction of the window size, then scales to fit.
    void SetResolutionScale(float scale);

    void SetUiFit(UiFit fit) { uiFit_ = fit; }

    /// `overlay` must outlive the renderer or be replaced. Null removes it.
    void SetOverlay(OverlayPass* overlay) { overlay_ = overlay; }

    [[nodiscard]] SDL_GPUDevice* Device() const { return device_.get(); }
    [[nodiscard]] SDL_GPUTextureFormat ColorFormat() const { return colorFormat_; }

    /// Lines past `kMaxDebugLines` and sprites past `kMaxSprites` are dropped.
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

    struct StreamBuffer {
        GpuBuffer vertices;
        GpuTransferBuffer upload;
    };

    struct StreamUpload {
        const StreamBuffer* stream = nullptr;
        std::span<const std::byte> bytes;
    };

    explicit Renderer(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device);

    Result<void> CreatePipelines();
    Result<void> CreateResources();
    Result<GpuBuffer> CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes);
    Result<StreamBuffer> CreateStream(std::size_t sizeBytes);
    Result<GpuTransferBuffer> Stage(std::span<const std::span<const std::byte>> parts);
    Result<void> UploadBuffers(std::span<const UploadPart> parts);
    Result<void> UploadFrame(SDL_GPUCommandBuffer* commands, std::span<const DebugLine> lines,
                             std::span<const SpriteDraw> sprites);
    Result<void> EnsureSceneTargets(std::uint32_t widthPx, std::uint32_t heightPx);
    [[nodiscard]] bool IsScaled() const { return resolutionScale_ != 1.0F; }
    void DrawScene(SDL_GPUCommandBuffer* commands, const RenderView& view, std::size_t lineCount,
                   SDL_GPUTexture* target);
    void DrawSprites(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                     std::uint32_t widthPx, std::uint32_t heightPx) const;
    void DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain);
    void DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass,
                    std::span<const MeshDraw> meshes, const glm::mat4& viewProjection) const;
    void DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const MeshDraw& draw,
                  const glm::mat4& viewProjection, SDL_GPUGraphicsPipeline*& bound) const;
    void DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, std::size_t lineCount,
                   const glm::mat4& viewProjection) const;

    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device_;
    std::array<GpuPipeline, kBlendModeCount> meshPipelines_;
    std::array<GpuPipeline, kBlendModeCount> spritePipelines_;
    GpuPipeline linePipeline_;
    GpuSampler sampler_;
    GpuSampler uiSampler_;
    SDL_GPUTextureFormat colorFormat_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    float resolutionScale_ = 1.0F;
    UiFit uiFit_ = UiFit::Centered;
    GpuTexture sceneColor_;
    GpuTexture depth_;
    std::uint32_t sceneWidthPx_ = 0;
    std::uint32_t sceneHeightPx_ = 0;
    OverlayPass* overlay_ = nullptr;
    StreamBuffer lineStream_;
    StreamBuffer spriteStream_;
    GpuBuffer spriteIndices_;
    std::vector<LineVertex> lineVertices_;
    std::vector<SpriteVertex> spriteVertices_;
    std::vector<SpriteBatch> spriteBatches_;
    AssetTable<Mesh, MeshId> meshes_;
    AssetTable<GpuTexture, TextureId> textures_;
};

}
