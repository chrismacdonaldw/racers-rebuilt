#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/window.hpp"
#include "render/asset_table.hpp"
#include "render/frame_readback.hpp"
#include "render/gpu_resource.hpp"
#include "render/overlay_pass.hpp"
#include "render/pipelines.hpp"
#include "render/quad_batch.hpp"
#include "render/render_view.hpp"
#include "render/sprite_batch.hpp"
#include "render/ui_layout.hpp"

namespace rr::render {

inline constexpr std::size_t kMaxDebugLines = 8192;
inline constexpr std::size_t kMaxSprites = 8192;
inline constexpr std::size_t kMaxQuads = 4096;
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
    /// `image` must be the size `texture` was uploaded at.
    [[nodiscard]] Result<void> UpdateTexture(TextureId texture, const assets::ImageAsset& image);
    /// Invalidates every `MeshId`, and every `TextureId` handed out after the last pin.
    void ReleaseAssets();

    /// Keeps every texture uploaded so far through later `ReleaseAssets` calls.
    void PinTextures() { textures_.Pin(); }

    /// The scene renders at this fraction of the window size, then scales to fit.
    void SetResolutionScale(float scale);

    void SetUiFit(UiFit fit) { uiFit_ = fit; }
    [[nodiscard]] UiFit Fit() const { return uiFit_; }
    void SetSceneFit(SceneFit fit) { sceneFit_ = fit; }

    /// Drawn over every view's sprites until replaced.
    void SetPointer(const std::optional<SpriteDraw>& pointer) { pointer_ = pointer; }

    /// `overlay` must outlive the renderer or be replaced. Null removes it.
    void SetOverlay(OverlayPass* overlay) { overlay_ = overlay; }

    [[nodiscard]] SDL_GPUDevice* Device() const { return device_.get(); }
    [[nodiscard]] SDL_GPUTextureFormat ColorFormat() const { return colorFormat_; }

    [[nodiscard]] Result<void> Draw(const RenderView& view) { return Draw(std::span(&view, 1)); }
    /// Sprites cover the whole window, after every scene. Items past their maximums are dropped.
    [[nodiscard]] Result<void> Draw(std::span<const RenderView> views);

    /// The next `Draw` waits for the GPU and keeps a copy of the frame without the overlay.
    void RequestCapture() { isCaptureRequested_ = true; }
    [[nodiscard]] std::optional<assets::ImageAsset> TakeCapture();

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

    struct ViewRange {
        std::size_t firstLine = 0;
        std::size_t lineCount = 0;
        std::size_t firstQuadBatch = 0;
        std::size_t quadBatchCount = 0;
    };

    explicit Renderer(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device);

    Result<void> CreatePipelines();
    Result<void> CreateResources();
    Result<GpuBuffer> CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes);
    Result<StreamBuffer> CreateStream(std::size_t sizeBytes);
    Result<GpuTransferBuffer> Stage(std::span<const std::span<const std::byte>> parts);
    static Result<void> CheckRgba(const assets::ImageAsset& image);
    Result<void> GenerateMips(SDL_GPUTexture* texture);
    Result<void> CopyToTexture(SDL_GPUTexture* texture, const assets::ImageAsset& image,
                               bool cycles);
    Result<void> UploadBuffers(std::span<const UploadPart> parts);
    void GatherViews(std::span<const RenderView> views);
    Result<void> UploadFrame(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views);
    Result<void> DrawFrame(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                           bool captures);
    Result<void> EnsureSceneTargets(std::uint32_t widthPx, std::uint32_t heightPx);
    [[nodiscard]] bool IsScaled() const { return resolutionScale_ != 1.0F; }
    void DrawScenes(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                    SDL_GPUTexture* target) const;
    void DrawScene(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const RenderView& view,
                   const ViewRange& range) const;
    void DrawSprites(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                     std::uint32_t widthPx, std::uint32_t heightPx) const;
    void DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain);
    void DrawBatches(SDL_GPURenderPass* pass, const StreamBuffer& stream,
                     std::span<const SpriteBatch> batches,
                     const std::array<GpuPipeline, kBlendModeCount>& pipelines) const;
    void DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const RenderView& view,
                    const glm::mat4& viewProjection) const;
    void DrawMesh(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const MeshDraw& draw,
                  const glm::mat4& viewProjection, const Color& ambient,
                  SDL_GPUGraphicsPipeline*& bound) const;
    void DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const ViewRange& range,
                   const glm::mat4& viewProjection) const;

    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device_;
    std::array<GpuPipeline, kBlendModeCount> meshPipelines_;
    std::array<GpuPipeline, kBlendModeCount> skinnedMeshPipelines_;
    std::array<GpuPipeline, kBlendModeCount> quadPipelines_;
    std::array<GpuPipeline, kBlendModeCount> spritePipelines_;
    GpuPipeline linePipeline_;
    GpuSampler sampler_;
    GpuSampler uiSampler_;
    SDL_GPUTextureFormat colorFormat_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    float resolutionScale_ = 1.0F;
    UiFit uiFit_ = UiFit::Centered;
    SceneFit sceneFit_ = SceneFit::Canvas;
    GpuTexture sceneColor_;
    GpuTexture depth_;
    std::uint32_t sceneWidthPx_ = 0;
    std::uint32_t sceneHeightPx_ = 0;
    OverlayPass* overlay_ = nullptr;
    std::optional<SpriteDraw> pointer_;
    FrameReadback readback_;
    bool isCaptureRequested_ = false;
    std::optional<assets::ImageAsset> capture_;
    StreamBuffer lineStream_;
    StreamBuffer quadStream_;
    StreamBuffer spriteStream_;
    GpuBuffer spriteIndices_;
    std::vector<LineVertex> lineVertices_;
    std::vector<SpriteVertex> spriteVertices_;
    std::vector<SpriteBatch> spriteBatches_;
    std::vector<QuadVertex> quadVertices_;
    std::vector<SpriteBatch> quadBatches_;
    std::vector<QuadVertex> viewQuadVertices_;
    std::vector<SpriteBatch> viewQuadBatches_;
    std::vector<SpriteDraw> sprites_;
    std::vector<ViewRange> viewRanges_;
    AssetTable<Mesh, MeshId> meshes_;
    AssetTable<GpuTexture, TextureId> textures_;
};

}
