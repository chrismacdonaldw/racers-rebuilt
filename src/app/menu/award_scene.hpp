#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/menu_parts.hpp"
#include "app/scene.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

[[nodiscard]] std::string_view AwardMusic(sim::CircuitAward award);

/// Shows a circuit's trophy over the main menu's backdrop until the award cinematics arrive.
class AwardScene final : public Scene {
public:
    /// `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<AwardScene>> Create(render::Renderer& renderer,
                                                                    const assets::Vfs& vfs,
                                                                    sim::CircuitAward award);

    /// Stands in for `winvvcar` with its music over the bare backdrop.
    [[nodiscard]] static Result<std::unique_ptr<AwardScene>> CreateRecordsUnlock(
        render::Renderer& renderer, const assets::Vfs& vfs);

    [[nodiscard]] SceneMusic Music() const override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    AwardScene(render::Renderer& renderer, std::string_view music,
               std::vector<MenuPicture> pictures, std::optional<UiImage> trophy);

    render::Renderer* renderer_;
    std::string_view music_;
    std::vector<MenuPicture> pictures_;
    std::optional<UiImage> trophy_;
    std::optional<std::string> next_;
    std::vector<render::SpriteDraw> sprites_;
};

}
