#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "platform/frame_pacer.hpp"
#include "platform/window.hpp"
#include "render/backend.hpp"
#include "render/overlay_pass.hpp"
#include "render/render_view.hpp"
#include "render/ui_layout.hpp"

namespace rr::render {

enum class RendererKind : std::uint8_t { Standard };

/// Must not outlive the window it draws to. Main thread only.
class Renderer {
public:
    [[nodiscard]] static Result<Renderer> Create(const platform::Window& window,
                                                 platform::PresentMode mode, RendererKind kind);

    /// `Vsync` waits for the display. The other modes present immediately.
    [[nodiscard]] Result<void> SetPresentMode(platform::PresentMode mode) {
        return backend_->SetPresentMode(mode);
    }

    [[nodiscard]] Result<MeshId> UploadMesh(const assets::GeometryAsset& geometry) {
        return backend_->UploadMesh(geometry);
    }
    [[nodiscard]] Result<TextureId> UploadTexture(const assets::ImageAsset& image) {
        return backend_->UploadTexture(image);
    }
    /// `image` must be the size `texture` was uploaded at.
    [[nodiscard]] Result<void> UpdateTexture(TextureId texture, const assets::ImageAsset& image) {
        return backend_->UpdateTexture(texture, image);
    }
    /// Invalidates every `MeshId`, and every `TextureId` handed out after the last pin.
    void ReleaseAssets() { backend_->ReleaseAssets(); }

    /// Keeps every texture uploaded so far through later `ReleaseAssets` calls.
    void PinTextures() { backend_->PinTextures(); }

    /// Draws textures with their enhanced look where they have one.
    void SetSmoothTextures(bool isOn) { settings_.isSmoothingTextures = isOn; }

    /// The scene renders at this fraction of the window size, then scales to fit.
    void SetResolutionScale(float scale);

    void SetUiFit(UiFit fit) { settings_.uiFit = fit; }
    [[nodiscard]] UiFit Fit() const { return settings_.uiFit; }
    void SetSceneFit(SceneFit fit) { settings_.sceneFit = fit; }

    /// Drawn over every view's sprites until replaced.
    void SetPointer(const std::optional<SpriteDraw>& pointer) { settings_.pointer = pointer; }

    /// `overlay` must outlive the renderer or be replaced. Null removes it.
    void SetOverlay(OverlayPass* overlay) { settings_.overlay = overlay; }

    /// Needs a current ImGui context. `StopImGui` must run before the context is destroyed.
    [[nodiscard]] Result<void> StartImGui() { return backend_->StartImGui(); }
    void StopImGui() { backend_->StopImGui(); }

    [[nodiscard]] std::string DriverName() const { return backend_->DriverName(); }

    [[nodiscard]] Result<void> Draw(const RenderView& view) { return Draw(std::span(&view, 1)); }
    /// Sprites cover the whole window, after every scene. Items past their maximums are dropped.
    [[nodiscard]] Result<void> Draw(std::span<const RenderView> views);

    /// The next `Draw` waits for the GPU and keeps a copy of the frame without the overlay.
    void RequestCapture() { isCaptureRequested_ = true; }
    [[nodiscard]] std::optional<assets::ImageAsset> TakeCapture();

private:
    explicit Renderer(std::unique_ptr<Backend> backend) : backend_(std::move(backend)) {}

    std::unique_ptr<Backend> backend_;
    FrameSettings settings_;
    bool isCaptureRequested_ = false;
    std::optional<assets::ImageAsset> capture_;
};

}
