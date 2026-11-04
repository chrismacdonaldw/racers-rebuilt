#pragma once

#include <SDL3/SDL_events.h>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "overlay/input_gate.hpp"
#include "overlay/quick_config_menu.hpp"
#include "overlay/racer_tools.hpp"
#include "platform/window.hpp"
#include "render/overlay_pass.hpp"
#include "render/renderer.hpp"
#include "settings/quick_configs.hpp"
#include "settings/registry.hpp"

namespace rr::overlay {

struct DeveloperStats {
    FloatSeconds frameTime{0.0};
    int ticksPerFrame = 0;
    std::string_view startupSummary;
};

struct MenuChoice {
    std::string name;
    std::function<void()> choose;
};

struct SettingActions {
    std::span<const MenuChoice> mods;
    std::span<const MenuChoice> game;
};

/// ImGui state is global, so only one may exist. `window` and `renderer` must outlive it.
class Overlay final : public render::OverlayPass {
public:
    [[nodiscard]] static Result<std::unique_ptr<Overlay>> Create(
        const platform::Window& window, render::Renderer& renderer,
        settings::QuickConfigs quickConfigs);

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    Overlay(Overlay&&) = delete;
    Overlay& operator=(Overlay&&) = delete;
    ~Overlay() override;

    void HandleEvent(const SDL_Event& event) const { input_.Forward(event); }
    void Toggle();
    [[nodiscard]] bool IsVisible() const { return input_.IsOpen(); }

    /// Does nothing while hidden. Returns the setting keys whose edit finished, to persist.
    [[nodiscard]] std::vector<std::string> Build(settings::Registry& registry,
                                                 const DeveloperStats& stats,
                                                 std::span<const MenuChoice> scenes,
                                                 std::span<const MenuChoice> saveImports,
                                                 const RacerTools& racerTools,
                                                 const SettingActions& actions);

    [[nodiscard]] ImDrawData* TakeDrawData() override;

private:
    Overlay(render::Renderer& renderer, settings::QuickConfigs quickConfigs)
        : renderer_(&renderer), quickConfigs_(std::move(quickConfigs)) {}

    render::Renderer* renderer_;
    QuickConfigMenu quickConfigs_;
    InputGate input_;
    bool hasFrame_ = false;
    Nanoseconds frameCost_{0};
    Nanoseconds lastFrameCost_{0};
};

}
