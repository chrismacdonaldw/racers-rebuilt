#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/control_bindings.hpp"
#include "app/menu/controls_look.hpp"
#include "app/menu/controls_menu.hpp"
#include "app/menu/menu_text.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The original's controls screen, rebinding one player's race keys and buttons.
class ControlsScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene. Finishing opens `back`.
    [[nodiscard]] static Result<std::unique_ptr<ControlsScene>> Create(render::Renderer& renderer,
                                                                       const assets::Vfs& vfs,
                                                                       std::size_t player,
                                                                       std::string back);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    [[nodiscard]] std::vector<SettingChange> TakeSettingChanges() override;
    void HandleEvent(const SDL_Event& event) override;
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    ControlsScene(render::Renderer& renderer, const assets::Vfs& vfs, ControlsLook look,
                  std::size_t player, std::string back);

    [[nodiscard]] const ControlNames& DeviceNames() const;
    void DrawDevicePicker();
    void DrawRows();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    ControlsLook look_;
    std::size_t player_;
    std::string back_;
    ControlsMenu menu_;
    MenuText text_;
    ControlNames keyboardNames_;
    ControlNames gamepadNames_;
    std::array<std::u16string, kControlRows> keyboardShown_;
    std::array<std::u16string, kControlRows> gamepadShown_;
    std::optional<std::string> next_;
    std::vector<SettingChange> changes_;
    std::vector<render::SpriteDraw> sprites_;
};

}
