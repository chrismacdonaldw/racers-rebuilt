#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/garage_look.hpp"
#include "app/menu/garage_menu.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/menu/pointer_buttons.hpp"
#include "app/menu/racer_stand.hpp"
#include "app/race_scene_name.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// The player's racers. NEW, COPY and DELETE save the racers file at once, unless it won't load.
class GarageScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene. It opens on `racer` when there's one.
    [[nodiscard]] static Result<std::unique_ptr<GarageScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        std::filesystem::path racersFile, std::size_t racer);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    [[nodiscard]] bool ShowsPointer() const override { return true; }
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Racers {
        std::filesystem::path file;
        bool isLoaded = false;
        settings::RacerStore store;
        std::size_t shown = 0;
    };

    struct Views {
        std::unique_ptr<RacerStand> stand;
        std::optional<DialogLook> dialog;
    };

    GarageScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                GarageLook look, Views views, Racers racers);

    void Handle(GarageRequest request);
    void TickDialog(MessageDialog& dialog, const sim::InputFrame& input);
    void AddNew();
    void Copy();
    void AskDelete();
    void Delete();
    void TestDrive();
    bool Changed(std::size_t racer, GarageButton focus);
    void ShowRacer();
    void LockShownRacer();
    void PlaySounds(const sim::InputFrame& input, GarageRequest request,
                    GarageButton focusBefore) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(GarageButton button) const;
    [[nodiscard]] std::size_t StateOf(GarageButton button) const;
    void AimPointer();
    void DrawButtons();
    void DrawPicker();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    GarageLook look_;
    Views views_;
    Racers racers_;
    std::vector<std::u16string> names_;
    GarageMenu menu_;
    MenuText text_;
    RaceSceneSetup rules_;
    bool allowsExtraPieces_ = false;
    std::optional<MessageDialog> dialog_;
    std::optional<std::string> opened_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<PointerTarget> targets_;
    PointerButtons pointer_;
};

}
