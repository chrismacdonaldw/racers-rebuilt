#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/menu/options_look.hpp"
#include "app/menu/options_menu.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The original's options screen. Values change the settings as they step.
class OptionsScene final : public Scene {
public:
    /// `renderer` and `vfs` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<OptionsScene>> Create(render::Renderer& renderer,
                                                                      const assets::Vfs& vfs);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    [[nodiscard]] std::vector<SettingChange> TakeSettingChanges() override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    OptionsScene(render::Renderer& renderer, const assets::Vfs& vfs, OptionsLook look,
                 std::optional<DialogLook> warningLook);

    void TickWarning(const sim::InputFrame& input);
    void Respond(OptionsRequest request, const OptionValues& before);
    void DrawTitle();
    void DrawButtons();
    void DrawLabels();
    void DrawSelector(const OptionsSelector& selector);
    void DrawSlider(const OptionsSlider& slider);
    [[nodiscard]] std::u16string_view ValueText(OptionsRow row) const;

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    OptionsLook look_;
    std::optional<DialogLook> warningLook_;
    std::optional<MessageDialog> warning_;
    OptionsMenu menu_{OptionValues{}};
    MenuText text_;
    std::u16string driverName_;
    std::optional<std::string> next_;
    std::vector<SettingChange> changes_;
    std::vector<render::SpriteDraw> sprites_;
};

}
