#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/menu/car_build_screen.hpp"
#include "app/menu/car_edit.hpp"
#include "app/menu/car_stand.hpp"
#include "app/menu/edit_car_look.hpp"
#include "app/menu/edit_car_menu.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/menu/racer_edit_flow.hpp"
#include "app/scene.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

class EditCarScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene. `partSets` are the progress file's unlocks.
    [[nodiscard]] static Result<std::unique_ptr<EditCarScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        const std::filesystem::path& racersFile, EditedRacer racer,
        const std::set<std::int32_t>& partSets);

    void OpenBuilder();

    void ApplySettings(const settings::Registry& settings) override;
    void HandleEvent(const SDL_Event& event) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Racers {
        std::filesystem::path file;
        settings::RacerStore store;
        EditedRacer racer;
    };

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Car {
        assets::PieceCatalog pieces;
        sim::BuiltCar built;
        QuickBuilds quickBuilds;
        std::vector<std::size_t> sets;
        std::size_t set = 0;
        bool isReplaceable = false;
    };

    struct Views {
        std::unique_ptr<CarStand> stand;
        std::optional<DialogLook> dialog;
    };

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Edit {
        Racers racers;
        Car car;
    };

    EditCarScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                 EditCarLook look, Views views, Edit edit);

    void Handle(CarRequest request);
    void TickBuild(const sim::InputFrame& input);
    void CloseBuilder();
    void RenderBuild();
    void TickDialog(MessageDialog& dialog, const sim::InputFrame& input);
    void Confirm(CarRequest request);
    void Apply(CarRequest request);
    void StepSet(std::size_t forward);
    void ResetCar();
    void QuickBuild();
    void Done();
    void ShowCar();
    [[nodiscard]] bool HasChanges() const;
    void PlaySounds(const sim::InputFrame& input, CarRequest request, CarRow focusBefore) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(CarRow row) const;
    [[nodiscard]] std::size_t StateOf(CarRow row) const;
    void LayOut();
    void DrawControls();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    EditCarLook look_;
    Views views_;
    Racers racers_;
    EditorButtons buttons_;
    Car car_;
    settings::Racer shown_;
    EditCarMenu menu_;
    MenuText text_;
    sim::Random random_;
    std::optional<MessageDialog> dialog_;
    std::optional<CarRequest> asked_;
    std::optional<std::string> opened_;
    std::unique_ptr<CarBuildScreen> build_;
    std::vector<render::RenderView> views3d_;
    float turn_ = 0.0F;
    std::vector<MenuPicture> under_;
    std::vector<render::SpriteDraw> sprites_;
};

}
