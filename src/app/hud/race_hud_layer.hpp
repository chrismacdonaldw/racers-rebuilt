#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_gadget.hpp"
#include "app/hud/minimap.hpp"
#include "app/hud/race_hud.hpp"
#include "app/racer/driver_name.hpp"
#include "app/ui_text/ui_font.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/race_session.hpp"
#include "sim/render_snapshot.hpp"
#include "sim/time_trial/time_trial_ghosts.hpp"

namespace rr::app {

/// Each player's HUD in the chosen language, ready to draw. The players drive the first slots.
class RaceHudLayer {
public:
    /// `renderer` and `vfs` must outlive the layer.
    RaceHudLayer(render::Renderer& renderer, const assets::Vfs& vfs, std::size_t players,
                 std::vector<DriverName> drivers);

    /// Loads only when `language` changes. The HUD stays hidden while its files fail to load.
    void SetLanguage(std::string_view language);

    /// Takes `language`'s HUD fonts, already loaded.
    void SetLanguage(std::string_view language, UiFonts fonts);

    void UseMissingGlyphs(MissingGlyphs missing);

    /// Shows the circuit results from `before`, each racer's points before this race.
    void SetCircuitPoints(std::span<const std::int32_t> before);

    void SetMap(std::optional<TrackMap> map);

    /// Shows TOTAL and the gap to `ghosts`, which must outlive the layer, and no results.
    void SetTimeTrial(const sim::TimeTrialGhosts& ghosts);

    void ShowDemo() { demoTime_ = Milliseconds{0}; }

    /// Empty until the HUD's language loads.
    [[nodiscard]] std::u16string_view RacerName(std::size_t slot) const;
    void CycleGadget(std::size_t player);

    void Tick(const sim::RaceSession& session, Milliseconds elapsed);
    void TickPause(Milliseconds elapsed);

    /// Valid until the next call. `maps` and `covers` hold an entry per player.
    [[nodiscard]] std::span<const render::SpriteDraw> Draw(
        const sim::RaceSession& session, std::span<const sim::RacerSnapshot> racers,
        std::span<const MapView> maps, std::span<const std::optional<render::SpriteDraw>> covers,
        std::optional<PauseView> pause);

private:
    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    std::size_t players_;
    std::vector<DriverName> drivers_;
    std::vector<std::int32_t> circuitPoints_;
    const sim::TimeTrialGhosts* trial_ = nullptr;
    std::optional<Milliseconds> demoTime_;
    std::string language_;
    UiFonts fonts_;
    MissingGlyphs missingGlyphs_ = MissingGlyphs::FirstGlyph;
    std::vector<std::u16string> names_;
    std::vector<UiImage> images_;
    std::vector<RaceHud> huds_;
    std::vector<HudGadget> gadgets_;
    std::vector<HudText> texts_;
    std::vector<render::SpriteDraw> sprites_;
};

}
