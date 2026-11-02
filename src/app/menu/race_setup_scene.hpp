#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/race_setup_menu.hpp"
#include "app/scene.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

enum class RaceSetupMode : std::uint8_t { SingleRace, Circuit };

/// Picks a track or circuit and a racer over the main menu's backdrop, then starts the race.
class RaceSetupScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<RaceSetupScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, RaceSetupMode mode,
        const std::set<std::string>& unlockedCircuits);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    using ButtonStates = std::array<MenuButtonState, assets::kControlStates>;

    RaceSetupScene(render::Renderer& renderer, const assets::Vfs& vfs, RaceSetupMode mode,
                   std::vector<MenuPicture> pictures, ButtonStates states, RaceSetupMenu menu);

    [[nodiscard]] std::vector<std::u16string> CourseNames() const;
    void DrawRow(RaceSetupRow row, std::u16string_view label, std::u16string_view value);
    void DrawChampion();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    RaceSetupMode mode_;
    std::vector<MenuPicture> pictures_;
    ButtonStates states_;
    RaceSetupMenu menu_;
    MenuText text_;
    std::vector<std::u16string> raceNames_;
    std::vector<std::u16string> courseNames_;
    std::vector<std::vector<std::u16string>> racerNames_;
    RaceSceneSetup rules_;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
