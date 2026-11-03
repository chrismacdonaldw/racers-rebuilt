#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/carousel_view.hpp"
#include "app/menu/driver_kit.hpp"
#include "app/menu/driver_stand.hpp"
#include "app/menu/edit_driver_look.hpp"
#include "app/menu/edit_driver_menu.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/menu/part_carousel.hpp"
#include "app/scene.hpp"
#include "assets/driver_look.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/progress.hpp"
#include "settings/racers.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

struct EditDriverFiles {
    std::filesystem::path racers;
    std::filesystem::path progress;
};

/// BUILD DRIVER: picks a racer's hat, face, torso and legs. DONE saves the racers file.
class EditDriverScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<EditDriverScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        const EditDriverFiles& files, std::size_t racer);

    void ApplySettings(const settings::Registry& settings) override;
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
        std::size_t racer = 0;
        settings::Progress progress;
    };

    struct Views {
        std::unique_ptr<DriverKit> kit;
        std::unique_ptr<DriverStand> stand;
        std::optional<DialogLook> dialog;
    };

    EditDriverScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                    EditDriverLook look, Views views, Racers racers);

    void Handle(DriverRequest request);
    void TickDialog(MessageDialog& dialog, const sim::InputFrame& input);
    CarouselStep Step();
    void Mix();
    void Done();
    void AskLeave();
    void Leave(bool isCheering);
    void ShowDriver();
    void PlaySounds(const sim::InputFrame& input, DriverRequest request, DriverRow focusBefore,
                    CarouselStep step) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(DriverRow row) const;
    [[nodiscard]] std::size_t StateOf(DriverRow row) const;
    void LayOut();
    void DrawControls();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    EditDriverLook look_;
    Views views_;
    Racers racers_;
    assets::DriverLook saved_;
    assets::DriverLook driver_;
    std::vector<PartCarousel> carousels_;
    std::vector<CarouselView> carouselViews_;
    EditDriverMenu menu_;
    MenuText text_;
    sim::Random random_;
    std::optional<MessageDialog> dialog_;
    std::optional<std::string> leaving_;
    std::optional<std::string> opened_;
    std::vector<MenuPicture> under_;
    std::vector<render::Rect> holes_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<render::RenderView> views3d_;
};

}
