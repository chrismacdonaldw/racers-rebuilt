#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/best_time_panel.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/race_setup_menu.hpp"
#include "app/menu/track_preview.hpp"
#include "app/race_scene_parts.hpp"
#include "app/scene.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/progress.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class RaceSetupMode : std::uint8_t { SingleRace, Circuit, Versus, TimeTrial };

[[nodiscard]] std::optional<RaceSetupMode> RaceSetupModeOf(std::string_view scene);

struct RaceSetupLook {
    std::vector<MenuPicture> pictures;
    std::array<MenuButtonState, assets::kControlStates> states;
    std::array<assets::SoundIds, static_cast<std::size_t>(RaceSetupRow::Count)> rowSounds{};
    assets::CarouselSounds scrollSounds{};
};

/// Picks a track or circuit and the players' racers over the main menu's backdrop, then races.
class RaceSetupScene final : public Scene {
public:
    /// `outputs` and `vfs` must outlive the scene. Versus shows no race preview.
    [[nodiscard]] static Result<std::unique_ptr<RaceSetupScene>> Create(
        const RaceOutputs& outputs, const assets::Vfs& vfs, MenuSounds sounds, RaceSetupMode mode,
        const settings::Progress& progress, std::string_view language);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    RaceSetupScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                   RaceSetupMode mode, RaceSetupLook look, RaceSetupMenu menu);

    void PlaySounds(const sim::InputFrame& input, RaceSetupRow focusBefore) const;
    [[nodiscard]] std::vector<std::u16string> CourseNames() const;
    void ShowPreview();
    /// Beside the race preview, a value stands in for a label that isn't from the game's text.
    struct RowText {
        std::u16string_view label;
        std::u16string_view value;
        bool isGameText = false;
    };

    void DrawRow(RaceSetupRow row, const RowText& text);
    void DrawValue(const MenuButtonState& state, std::u16string_view value, float top);
    [[nodiscard]] std::u16string_view RacerName(std::size_t racer) const;
    void DrawChampion();
    void ShowBestTimes();
    void DrawBestTimes();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    RaceSetupMode mode_;
    RaceSetupLook look_;
    RaceSetupMenu menu_;
    MenuText text_;
    std::vector<std::u16string> raceNames_;
    std::vector<std::u16string> courseNames_;
    std::vector<std::vector<std::u16string>> racerNames_;
    settings::Progress progress_;
    BestTimePanel bestTimes_;
    RaceSceneSetup rules_;
    std::unique_ptr<TrackPreview> preview_;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
