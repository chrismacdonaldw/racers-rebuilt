#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/race_hud.hpp"
#include "app/ui_text/ui_font.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/race_session.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// One racer's HUD in the chosen language, ready to draw.
class RaceHudLayer {
public:
    /// `nameIndexes` are lines in `DRIVERS.SRF`. `renderer` and `vfs` must outlive the layer.
    RaceHudLayer(render::Renderer& renderer, const assets::Vfs& vfs, std::size_t racer,
                 std::vector<std::int32_t> nameIndexes);

    /// Loads only when `language` changes. The HUD stays hidden while its files fail to load.
    void SetLanguage(std::string_view language);

    void Tick(const sim::RaceSession& session, Milliseconds elapsed);

    /// Valid until the next call. `pauseFocus` adds the pause menu with that row highlighted.
    [[nodiscard]] std::span<const render::SpriteDraw> Draw(const sim::RaceSession& session,
                                                           const sim::RacerSnapshot& racer,
                                                           std::optional<std::size_t> pauseFocus);

private:
    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    std::size_t racer_;
    std::vector<std::int32_t> nameIndexes_;
    std::string language_;
    UiFonts fonts_;
    std::vector<std::u16string> names_;
    std::vector<UiImage> images_;
    std::optional<RaceHud> hud_;
    std::vector<HudText> texts_;
    std::vector<render::SpriteDraw> sprites_;
};

}
