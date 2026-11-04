#pragma once

#include <SDL3/SDL_events.h>

#include <optional>
#include <string>
#include <string_view>

#include "app/cutscene/cutscene_play.hpp"
#include "assets/circuit_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "settings/racers.hpp"

namespace rr::app {

inline constexpr std::string_view kCutsceneScreenPrefix = "screen:";
inline constexpr std::string_view kLegalScreen = "legal";
inline constexpr std::string_view kCreditsScreen = "credits";
inline constexpr std::string_view kWinCarScreen = "wincar";
inline constexpr std::string_view kWinRocketRacerCarScreen = "winrrcar";
inline constexpr std::string_view kWinRecordsCarScreen = "winvvcar";

/// `next` follows the cutscene, empty for the screen's usual one. `driver` is a racer's code.
struct CutsceneScreenSetup {
    std::string menu;
    std::string driver;
    std::string next;

    friend bool operator==(const CutsceneScreenSetup&, const CutsceneScreenSetup&) = default;
};

/// Such as `screen:c_award1:driver=CR>screen:wincar:driver=KK`.
[[nodiscard]] std::string CutsceneScreenName(const CutsceneScreenSetup& setup);

[[nodiscard]] bool IsCutsceneScreen(std::string_view scene);

/// Fails on a menu that isn't a cutscene screen, and on one with nowhere to go next.
[[nodiscard]] Result<CutsceneScreenSetup> ParseCutsceneScreen(std::string_view scene);

/// `music` is a `MusicCatalog` id, empty for silence.
struct CutsceneScreen {
    std::string_view menu;
    std::string_view layout;
    std::string_view music;
    bool loopsMusic = false;
    bool isSkippable = true;
    bool showsRacer = false;
    bool namesChampion = false;
    std::string_view next;
};

[[nodiscard]] std::optional<CutsceneScreen> FindCutsceneScreen(std::string_view menu);

/// A circuit's own intro, else the retail one for its name: `c3` has `circuit4`. Unset when it has
/// none or names no cutscene screen.
[[nodiscard]] std::optional<std::string> CircuitIntro(const assets::CircuitEntry& circuit);

/// `folder` is under `MENUDATA` and `rect` is in UI units.
struct CutsceneScreenPlacement {
    std::string folder;
    std::string cutscene;
    render::Rect rect;
};

/// Reads the screen's `MIB` element, named after the menu.
[[nodiscard]] Result<CutsceneScreenPlacement> ReadCutsceneScreen(const assets::Vfs& vfs,
                                                                 const CutsceneScreen& screen);

/// A new key, mouse or pad button press, or any release, as the original's skip test takes.
[[nodiscard]] bool IsSkipPress(const SDL_Event& event);

/// Without a driver or player racer, casts the stand-in champion or the game's first racer.
void CastScreenRacer(const assets::Vfs& vfs, const CutsceneScreen& screen, std::string_view driver,
                     const std::optional<settings::Racer>& playerRacer, CutscenePlay& play);

/// `scene` is a screen name such as `screen:legal`, played once in its `MIB` rectangle.
[[nodiscard]] Result<CutscenePlay> ScreenPlay(const assets::Vfs& vfs, std::string_view scene,
                                              const std::optional<settings::Racer>& playerRacer);

}
