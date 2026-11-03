#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "platform/frame_pacer.hpp"
#include "render/overlay_pass.hpp"
#include "render/render_view.hpp"
#include "render/ui_layout.hpp"

namespace rr::render {

inline constexpr std::size_t kMaxDebugLines = 8192;
inline constexpr std::size_t kMaxSprites = 8192;
inline constexpr std::size_t kMaxQuads = 4096;
inline constexpr float kMinResolutionScale = 0.25F;
inline constexpr float kMaxResolutionScale = 2.0F;

struct FrameSettings {
    float resolutionScale = 1.0F;
    UiFit uiFit = UiFit::Centered;
    SceneFit sceneFit = SceneFit::Canvas;
    bool isSmoothingTextures = true;
    std::optional<SpriteDraw> pointer;
    OverlayPass* overlay = nullptr;
};

/// What `Renderer` forwards to. Main thread only.
class Backend {
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual Result<void> SetPresentMode(platform::PresentMode mode) = 0;

    [[nodiscard]] virtual Result<MeshId> UploadMesh(const assets::GeometryAsset& geometry) = 0;
    [[nodiscard]] virtual Result<TextureId> UploadTexture(const assets::ImageAsset& image) = 0;
    [[nodiscard]] virtual Result<void> UpdateTexture(TextureId texture,
                                                     const assets::ImageAsset& image) = 0;
    virtual void ReleaseAssets() = 0;
    virtual void PinTextures() = 0;

    /// Returns the frame without the overlay when `captures`, once the GPU has drawn it.
    [[nodiscard]] virtual Result<std::optional<assets::ImageAsset>> Draw(
        std::span<const RenderView> views, const FrameSettings& settings, bool captures) = 0;

    [[nodiscard]] virtual Result<void> StartImGui() = 0;
    virtual void StopImGui() = 0;

    [[nodiscard]] virtual std::string DriverName() const = 0;
};

}
