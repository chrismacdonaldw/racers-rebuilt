#pragma once

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/window.hpp"
#include "render/asset_table.hpp"
#include "render/backend.hpp"
#include "render/frame_readback.hpp"
#include "render/gpu_resource.hpp"
#include "render/pipelines.hpp"
#include "render/quad_batch.hpp"
#include "render/render_view.hpp"
#include "render/sprite_batch.hpp"

namespace rr::render {

/// Draws through SDL GPU on Direct3D 12. Must not outlive the window it draws to.
class SdlGpuBackend final : public Backend {
public:
    [[nodiscard]] static Result<std::unique_ptr<SdlGpuBackend>> Create(
        const platform::Window& window, platform::PresentMode mode);

    [[nodiscard]] Result<void> SetPresentMode(platform::PresentMode mode) override;

    [[nodiscard]] Result<MeshId> UploadMesh(const assets::GeometryAsset& geometry) override;
    [[nodiscard]] Result<TextureId> UploadTexture(const assets::ImageAsset& image) override;
    [[nodiscard]] Result<void> UpdateTexture(TextureId texture,
                                             const assets::ImageAsset& image) override;
    void ReleaseAssets() override;
    void PinTextures() override { textures_.Pin(); }

    [[nodiscard]] Result<std::optional<assets::ImageAsset>> Draw(std::span<const RenderView> views,
                                                                 const FrameSettings& settings,
                                                                 bool captures) override;

    [[nodiscard]] Result<void> StartImGui() override;
    void StopImGui() override;

    [[nodiscard]] std::string DriverName() const override;

private:
    static constexpr std::size_t kBlendModeCount = 3;
    static constexpr std::size_t kSamplerCount = 4;

    struct DeviceDeleter {
        SDL_Window* window = nullptr;

        void operator()(SDL_GPUDevice* device) const;
    };

    struct Mesh {
        GpuBuffer vertices;
        GpuBuffer indices;
        std::uint32_t indexCount = 0;
    };

    struct Texture {
        GpuTexture original;
        GpuTexture enhanced;
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

    explicit SdlGpuBackend(std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device);

    Result<void> CreatePipelines();
    Result<void> CreateResources();
    Result<GpuBuffer> CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes);
    Result<StreamBuffer> CreateStream(std::size_t sizeBytes);
    Result<GpuTransferBuffer> Stage(std::span<const std::span<const std::byte>> parts);
    static Result<void> CheckRgba(const assets::ImageAsset& image);
    Result<GpuTexture> CreateTexture(const assets::ImageAsset& image,
                                     std::span<const std::byte> pixels, std::uint32_t mipLevels);
    Result<void> GenerateMips(SDL_GPUTexture* texture);
    Result<void> CopyToTexture(SDL_GPUTexture* texture, const assets::ImageAsset& image,
                               std::span<const std::byte> pixels, bool cycles);
    [[nodiscard]] SDL_GPUTexture* Shown(TextureId texture) const;
    [[nodiscard]] SDL_GPUSampler* Sampler(TextureFilter filter, TextureAddress address) const;
    Result<void> UploadBuffers(std::span<const UploadPart> parts);
    void GatherViews(std::span<const RenderView> views);
    Result<void> UploadFrame(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views);
    Result<void> DrawFrame(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                           ImDrawData* overlay, bool captures);
    Result<void> EnsureSceneTargets(std::uint32_t widthPx, std::uint32_t heightPx);
    [[nodiscard]] bool IsScaled() const { return settings_.resolutionScale != 1.0F; }
    void DrawScenes(SDL_GPUCommandBuffer* commands, std::span<const RenderView> views,
                    SDL_GPUTexture* target) const;
    void DrawScene(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const RenderView& view,
                   const ViewRange& range) const;
    void DrawSprites(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                     std::uint32_t widthPx, std::uint32_t heightPx) const;
    static void DrawOverlay(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain,
                            ImDrawData* overlay);
    void DrawBatches(SDL_GPURenderPass* pass, const StreamBuffer& stream,
                     std::span<const SpriteBatch> batches,
                     const std::array<GpuPipeline, kBlendModeCount>& pipelines) const;
    void DrawMeshes(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const RenderView& view,
                    const glm::mat4& viewProjection) const;
    struct MeshPass {
        SDL_GPUCommandBuffer* commands;
        SDL_GPURenderPass* pass;
        glm::mat4 viewProjection;
        Color ambient;
        SDL_GPUGraphicsPipeline* bound;
    };

    [[nodiscard]] const std::array<GpuPipeline, kBlendModeCount>& MeshPipelines(
        bool isBackdrop, bool isSkinned) const;
    void DrawMesh(MeshPass& meshPass, const MeshDraw& draw, bool isBackdrop = false) const;
    void DrawLines(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const ViewRange& range,
                   const glm::mat4& viewProjection) const;

    std::unique_ptr<SDL_GPUDevice, DeviceDeleter> device_;
    std::array<GpuPipeline, kBlendModeCount> meshPipelines_;
    std::array<GpuPipeline, kBlendModeCount> skinnedMeshPipelines_;
    std::array<GpuPipeline, kBlendModeCount> backdropPipelines_;
    std::array<GpuPipeline, kBlendModeCount> quadPipelines_;
    std::array<GpuPipeline, kBlendModeCount> spritePipelines_;
    GpuPipeline linePipeline_;
    std::array<GpuSampler, kSamplerCount> samplers_;
    SDL_GPUTextureFormat colorFormat_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    FrameSettings settings_;
    GpuTexture sceneColor_;
    GpuTexture depth_;
    std::uint32_t sceneWidthPx_ = 0;
    std::uint32_t sceneHeightPx_ = 0;
    FrameReadback readback_;
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
    AssetTable<Texture, TextureId> textures_;
};

}
