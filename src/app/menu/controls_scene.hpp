#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/device_assignment.hpp"
#include "app/menu/control_bindings.hpp"
#include "app/menu/controls_devices.hpp"
#include "app/menu/controls_look.hpp"
#include "app/menu/controls_menu.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/pointer_buttons.hpp"
#include "app/scene.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The original's controls screen: one player picks a device and rebinds its set.
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
    [[nodiscard]] bool ShowsPointer() const override { return true; }
    void Tick(const sim::InputFrame& input) override;
    [[nodiscard]] SceneMusic Music() const override { return {.start = MusicStart::Keep}; }
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    ControlsScene(render::Renderer& renderer, const assets::Vfs& vfs, ControlsLook look,
                  std::size_t player, std::string back);

    [[nodiscard]] const DeviceEntry& Entry() const;
    void ShowDevices(std::vector<AttachedPad> pads, InputSet set);
    void Leave();
    void DrawDevicePicker();
    void AimPointer();
    void DrawRows();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    ControlsLook look_;
    std::size_t player_;
    std::string back_;
    ControlsMenu menu_;
    MenuText text_;
    std::vector<AttachedPad> pads_;
    std::vector<DeviceEntry> entries_;
    std::optional<PlayerSets> chosen_;
    std::array<ControlNames, kInputSets.size()> names_;
    std::array<std::array<std::u16string, kControlRows>, kInputSets.size()> shown_;
    std::optional<std::string> next_;
    std::vector<SettingChange> changes_;
    std::vector<render::SpriteDraw> sprites_;
    std::vector<PointerTarget> targets_;
    PointerButtons pointer_;
};

}
