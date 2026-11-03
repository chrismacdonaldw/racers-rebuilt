#include "render/renderer.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "render/sdl_gpu_backend.hpp"

namespace rr::render {

Result<Renderer> Renderer::Create(const platform::Window& window, platform::PresentMode mode,
                                  RendererKind kind) {
    const auto wrap = [](std::unique_ptr<Backend> backend) { return Renderer(std::move(backend)); };
    switch (kind) {
        case RendererKind::Standard:
            return SdlGpuBackend::Create(window, mode).transform(wrap);
    }
    std::unreachable();
}

void Renderer::SetResolutionScale(float scale) {
    settings_.resolutionScale = std::clamp(scale, kMinResolutionScale, kMaxResolutionScale);
}

Result<void> Renderer::Draw(std::span<const RenderView> views) {
    const bool captures = std::exchange(isCaptureRequested_, false);
    return backend_->Draw(views, settings_, captures)
        .transform([&](std::optional<assets::ImageAsset> frame) {
            if (frame) {
                capture_ = std::move(frame);
            }
        });
}

std::optional<assets::ImageAsset> Renderer::TakeCapture() {
    return std::exchange(capture_, std::nullopt);
}

}
