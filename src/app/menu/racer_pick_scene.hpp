#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/pointer_buttons.hpp"
#include "app/menu/racer_lock.hpp"
#include "app/menu/racer_pick_look.hpp"
#include "app/menu/racer_pick_menu.hpp"
#include "app/menu/racer_stand.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// `player` 1 picks player 2's racer, in versus.
struct RacerPickRoute {
    std::string next;
    std::size_t player = 0;
};

/// START saves player 1's pick as selected, unless the racers file won't load, then opens `next`.
class RacerPickScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<RacerPickScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        std::filesystem::path racersFile, RacerPickRoute route);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;
    [[nodiscard]] bool ShowsPointer() const override { return true; }

private:
    struct Racers {
        std::filesystem::path file;
        bool isLoaded = false;
        settings::RacerStore store;
        std::vector<settings::Racer> pickable;
        std::vector<std::u16string> names;
    };

    RacerPickScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                   RacerPickLook look, RacerPickRoute route, Racers racers);

    void Start();
    void CheerInto(std::string next);
    void ShowPicked();
    void LockPicked();
    [[nodiscard]] std::string Reopened() const;
    [[nodiscard]] std::string Left() const;
    void PlaySounds(const sim::InputFrame& input, RacerPickRequest request,
                    RacerPickRow focusBefore) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(RacerPickRow row) const;
    void AimPointer();
    void DrawPicker();
    void DrawButtons();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    RacerPickLook look_;
    RacerPickRoute route_;
    bool isVersus_;
    RacerLocks locks_;
    RacerLock shownLock_ = RacerLock::None;
    Racers racers_;
    RacerPickMenu menu_;
    MenuText text_;
    std::unique_ptr<RacerStand> stand_;
    std::optional<std::string> cheered_;
    std::optional<std::string> opened_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<PointerTarget> targets_;
    PointerButtons pointer_;
};

}
