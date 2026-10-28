#pragma once

#include <SDL3/SDL_events.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "platform/window.hpp"
#include "render/overlay_pass.hpp"
#include "render/renderer.hpp"
#include "settings/registry.hpp"

namespace rr::overlay {

struct DeveloperStats {
    FloatSeconds frameTime{0.0};
    int ticksPerFrame = 0;
    std::string_view startupSummary;
};

struct SceneChoice {
    std::string name;
    std::function<void()> open;
};

/// ImGui state is global, so only one may exist. `window` and `renderer` must outlive it.
class Overlay final : public render::OverlayPass {
public:
    [[nodiscard]] static Result<std::unique_ptr<Overlay>> Create(const platform::Window& window,
                                                                 render::Renderer& renderer);

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    Overlay(Overlay&&) = delete;
    Overlay& operator=(Overlay&&) = delete;
    ~Overlay() override;

    static void HandleEvent(const SDL_Event& event);
    void Toggle();
    [[nodiscard]] bool IsVisible() const { return isVisible_; }

    /// Does nothing while hidden. Returns the setting keys whose edit finished, to persist.
    [[nodiscard]] std::vector<std::string> Build(settings::Registry& registry,
                                                 const DeveloperStats& stats,
                                                 std::span<const SceneChoice> scenes);

    void Prepare(SDL_GPUCommandBuffer* commands) override;
    void Draw(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass) override;

private:
    explicit Overlay(render::Renderer& renderer) : renderer_(&renderer) {}

    render::Renderer* renderer_;
    bool isVisible_ = false;
    bool hasFrame_ = false;
    Nanoseconds frameCost_{0};
    Nanoseconds lastFrameCost_{0};
};

}
