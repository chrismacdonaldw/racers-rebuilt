#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/single_race_menu.hpp"
#include "app/scene.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Picks a track and racer over the main menu's backdrop, then starts the race.
class SingleRaceScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<SingleRaceScene>> Create(render::Renderer& renderer,
                                                                         const assets::Vfs& vfs);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    using ButtonStates = std::array<MenuButtonState, assets::kControlStates>;

    SingleRaceScene(render::Renderer& renderer, const assets::Vfs& vfs,
                    std::vector<MenuPicture> pictures, ButtonStates states, SingleRaceMenu menu);

    void DrawRow(SingleRaceRow row, std::u16string_view label, std::u16string_view value);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    std::vector<MenuPicture> pictures_;
    ButtonStates states_;
    SingleRaceMenu menu_;
    MenuText text_;
    std::vector<std::u16string> trackNames_;
    std::vector<std::vector<std::u16string>> racerNames_;
    RaceSceneSetup rules_;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
