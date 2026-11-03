#pragma once

#include <SDL3/SDL_events.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/driver_stand.hpp"
#include "app/menu/licence_look.hpp"
#include "app/menu/licence_menu.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/message_dialog.hpp"
#include "app/menu/name_field.hpp"
#include "app/menu/stand_racer.hpp"
#include "app/scene.hpp"
#include "assets/driver_look.hpp"
#include "assets/loader_pool.hpp"
#include "assets/race_catalog.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {

/// MAKE LICENSE: the racer's name and expression. DONE saves them and checks the name for cheats.
class LicenceScene final : public Scene {
public:
    /// `renderer`, `vfs` and `cheats` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<LicenceScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
        const std::filesystem::path& racersFile, std::size_t racer, sim::Cheats& cheats);

    void ApplySettings(const settings::Registry& settings) override;
    [[nodiscard]] std::optional<std::string> TakeNextScene() override;
    void HandleEvent(const SDL_Event& event) override;
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
        std::vector<std::string> circuits;
    };

    struct Views {
        std::unique_ptr<assets::LoaderPool> pool;
        std::unique_ptr<assets::TrackLoader> loader;
        std::unique_ptr<DriverStand> stand;
        std::optional<DialogLook> dialog;
    };

    LicenceScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                 LicenceLook look, Views views, Racers racers);

    void Handle(LicenceRequest request);
    void TickDialog(MessageDialog& dialog, const sim::InputFrame& input);
    void TypeKeys();
    void Done();
    void AskLeave();
    void ShowPhoto();
    [[nodiscard]] bool HasChanges() const;
    [[nodiscard]] settings::Racer& Record();
    void PlaySounds(const sim::InputFrame& input, LicenceRequest request,
                    LicenceRow focusBefore) const;
    [[nodiscard]] const assets::SoundIds& SoundsOf(LicenceRow row) const;
    [[nodiscard]] std::size_t StateOf(LicenceRow row) const;
    void LayOut();
    void DrawName();

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    MenuSounds sounds_;
    sim::Cheats* cheats_ = nullptr;
    LicenceLook look_;
    Views views_;
    Racers racers_;
    NameField name_;
    std::uint8_t expression_ = 0;
    std::map<std::uint8_t, StandRacer> photos_;
    LicenceMenu menu_;
    MenuText text_;
    bool isNamed_ = false;
    float cursorWidth_ = 0.0F;
    std::vector<std::optional<char16_t>> keys_;
    bool isConfirmTyped_ = false;
    bool isBackTyped_ = false;
    Milliseconds blink_{0};
    std::optional<MessageDialog> dialog_;
    std::optional<std::string> opened_;
    std::vector<MenuPicture> under_;
    std::vector<render::SpriteDraw> sprites_;
};

}
